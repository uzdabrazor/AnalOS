diff --git a/chrome/browser/analos/server/analos_server_manager.cc b/chrome/browser/analos/server/analos_server_manager.cc
new file mode 100644
index 0000000000000..dab79743c4753
--- /dev/null
+++ b/chrome/browser/analos/server/analos_server_manager.cc
@@ -0,0 +1,1190 @@
+// Copyright 2024 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#include "chrome/browser/analos/server/analos_server_manager.h"
+
+#include <optional>
+#include <set>
+
+#include "base/command_line.h"
+#include "base/files/file_path.h"
+#include "base/files/file_util.h"
+#include "base/json/json_writer.h"
+#include "base/logging.h"
+#include "base/path_service.h"
+#include "base/process/kill.h"
+#include "base/process/launch.h"
+#include "base/rand_util.h"
+#include "base/strings/string_number_conversions.h"
+#include "base/system/sys_info.h"
+#include "base/task/thread_pool.h"
+#include "base/threading/thread_restrictions.h"
+#include "build/build_config.h"
+#include "chrome/browser/browser_process.h"
+
+#if BUILDFLAG(IS_POSIX)
+#include <signal.h>
+#endif
+
+#include "chrome/browser/analos/core/analos_switches.h"
+#include "chrome/browser/analos/metrics/analos_metrics_service.h"
+#include "chrome/browser/analos/metrics/analos_metrics_service_factory.h"
+#include "chrome/browser/analos/server/analos_server_prefs.h"
+#include "chrome/browser/analos/server/analos_server_updater.h"
+#include "chrome/browser/net/system_network_context_manager.h"
+#include "chrome/browser/profiles/profile.h"
+#include "chrome/browser/profiles/profile_manager.h"
+#include "chrome/common/chrome_paths.h"
+#include "components/prefs/pref_change_registrar.h"
+#include "components/prefs/pref_service.h"
+#include "components/version_info/version_info.h"
+#include "content/public/browser/devtools_agent_host.h"
+#include "content/public/browser/devtools_socket_factory.h"
+#include "content/public/browser/storage_partition.h"
+#include "net/base/address_family.h"
+#include "net/base/ip_address.h"
+#include "net/base/ip_endpoint.h"
+#include "net/base/net_errors.h"
+#include "net/base/port_util.h"
+#include "net/log/net_log_source.h"
+#include "net/socket/tcp_server_socket.h"
+#include "net/socket/tcp_socket.h"
+#include "net/traffic_annotation/network_traffic_annotation.h"
+#include "services/network/public/cpp/resource_request.h"
+#include "services/network/public/cpp/simple_url_loader.h"
+#include "services/network/public/mojom/url_loader_factory.mojom.h"
+#include "url/gurl.h"
+
+namespace {
+
+constexpr int kBackLog = 10;
+constexpr base::FilePath::CharType kConfigFileName[] =
+    FILE_PATH_LITERAL("server_config.json");
+
+constexpr base::TimeDelta kHealthCheckInterval = base::Seconds(30);
+constexpr base::TimeDelta kHealthCheckTimeout = base::Seconds(15);
+constexpr base::TimeDelta kProcessCheckInterval = base::Seconds(10);
+
+// Crash tracking: if server crashes within grace period, count as startup failure
+constexpr base::TimeDelta kStartupGracePeriod = base::Seconds(30);
+constexpr int kMaxStartupFailures = 3;
+
+constexpr int kMaxPortAttempts = 100;
+constexpr int kMaxPort = 65535;
+
+// Holds configuration data gathered on UI thread, passed to background thread
+struct ServerConfig {
+  std::string install_id;
+  std::string analos_version;
+  std::string chromium_version;
+  bool allow_remote_in_mcp = false;
+};
+
+// Writes the server configuration to a JSON file.
+// Returns the path to the config file on success, empty path on failure.
+base::FilePath WriteConfigJson(const base::FilePath& execution_dir,
+                               const base::FilePath& resources_dir,
+                               uint16_t cdp_port,
+                               uint16_t mcp_port,
+                               uint16_t agent_port,
+                               uint16_t extension_port,
+                               const ServerConfig& server_config) {
+  base::FilePath config_path =
+      execution_dir.Append(kConfigFileName);
+
+  base::Value::Dict config;
+
+  // ports
+  base::Value::Dict ports;
+  ports.Set("cdp", static_cast<int>(cdp_port));
+  ports.Set("http_mcp", static_cast<int>(mcp_port));
+  ports.Set("agent", static_cast<int>(agent_port));
+  ports.Set("extension", static_cast<int>(extension_port));
+  config.Set("ports", std::move(ports));
+
+  // directories
+  base::Value::Dict directories;
+  directories.Set("resources", resources_dir.AsUTF8Unsafe());
+  directories.Set("execution", execution_dir.AsUTF8Unsafe());
+  config.Set("directories", std::move(directories));
+
+  // flags
+  base::Value::Dict flags;
+  flags.Set("allow_remote_in_mcp", server_config.allow_remote_in_mcp);
+  config.Set("flags", std::move(flags));
+
+  // instance
+  base::Value::Dict instance;
+  instance.Set("install_id", server_config.install_id);
+  instance.Set("analos_version", server_config.analos_version);
+  instance.Set("chromium_version", server_config.chromium_version);
+  config.Set("instance", std::move(instance));
+
+  std::optional<std::string> json_output = base::WriteJson(config);
+  if (!json_output.has_value()) {
+    LOG(ERROR) << "analos: Failed to serialize config to JSON";
+    return base::FilePath();
+  }
+
+  if (!base::WriteFile(config_path, json_output.value())) {
+    LOG(ERROR) << "analos: Failed to write config file: " << config_path;
+    return base::FilePath();
+  }
+
+  LOG(INFO) << "analos: Wrote config to " << config_path;
+  return config_path;
+}
+
+// Helper function to check for command-line port override.
+// Returns the port value if valid override is found, 0 otherwise.
+int GetPortOverrideFromCommandLine(base::CommandLine* command_line,
+                                    const char* switch_name,
+                                    const char* port_name) {
+  if (!command_line->HasSwitch(switch_name)) {
+    return 0;
+  }
+
+  std::string port_str = command_line->GetSwitchValueASCII(switch_name);
+  int port = 0;
+
+  if (!base::StringToInt(port_str, &port) || !net::IsPortValid(port) ||
+      port <= 0) {
+    LOG(WARNING) << "analos: Invalid " << port_name
+                 << " specified on command line: " << port_str
+                 << " (must be 1-65535)";
+    return 0;
+  }
+
+  // Warn about problematic ports but respect explicit user intent
+  if (net::IsWellKnownPort(port)) {
+    LOG(WARNING) << "analos: " << port_name << " " << port
+                 << " is well-known (0-1023) and may require elevated "
+                    "privileges";
+  }
+  if (!net::IsPortAllowedForScheme(port, "http")) {
+    LOG(WARNING) << "analos: " << port_name << " " << port
+                 << " is restricted by Chromium (may interfere with system "
+                    "services)";
+  }
+
+  LOG(INFO) << "analos: " << port_name
+            << " overridden via command line: " << port;
+  return port;
+}
+
+// Launches the AnalOS server process on a background thread.
+// This function performs blocking I/O operations (PathExists, WriteConfigToml,
+// LaunchProcess).
+// If the primary exe_path doesn't exist, falls back to fallback_exe_path.
+analos::AnalOSServerManager::LaunchResult LaunchProcessOnBackgroundThread(
+    const base::FilePath& exe_path,
+    const base::FilePath& resources_dir,
+    const base::FilePath& fallback_exe_path,
+    const base::FilePath& fallback_resources_dir,
+    const base::FilePath& execution_dir,
+    uint16_t cdp_port,
+    uint16_t mcp_port,
+    uint16_t agent_port,
+    uint16_t extension_port,
+    const ServerConfig& server_config) {
+  analos::AnalOSServerManager::LaunchResult result;
+  base::FilePath actual_exe_path = exe_path;
+  base::FilePath actual_resources_dir = resources_dir;
+
+  // Check if executable exists, fallback to bundled if not
+  if (!base::PathExists(actual_exe_path)) {
+    LOG(WARNING) << "analos: Binary not found at " << actual_exe_path
+                 << ", falling back to bundled";
+    actual_exe_path = fallback_exe_path;
+    actual_resources_dir = fallback_resources_dir;
+    result.used_fallback = true;
+
+    if (!base::PathExists(actual_exe_path)) {
+      LOG(ERROR) << "analos: Bundled binary also not found at: "
+                 << actual_exe_path;
+      return result;
+    }
+  }
+
+  if (execution_dir.empty()) {
+    LOG(ERROR) << "analos: Execution directory path is empty";
+    return result;
+  }
+
+  // Ensure execution directory exists (blocking I/O)
+  if (!base::CreateDirectory(execution_dir)) {
+    LOG(ERROR) << "analos: Failed to create execution directory at: "
+               << execution_dir;
+    return result;
+  }
+
+  // Write configuration to JSON file
+  base::FilePath config_path = WriteConfigJson(
+      execution_dir, actual_resources_dir, cdp_port, mcp_port, agent_port,
+      extension_port, server_config);
+  if (config_path.empty()) {
+    LOG(ERROR) << "analos: Failed to write config file, aborting launch";
+    return result;
+  }
+
+  // Build command line with --config flag and explicit port args
+  // Ports are passed via CLI to avoid config file read race conditions
+  // CLI takes precedence over config file in the server's merge logic
+  base::CommandLine cmd(actual_exe_path);
+  cmd.AppendSwitchPath("config", config_path);
+  cmd.AppendSwitchASCII("cdp-port", base::NumberToString(cdp_port));
+  cmd.AppendSwitchASCII("http-mcp-port", base::NumberToString(mcp_port));
+  cmd.AppendSwitchASCII("agent-port", base::NumberToString(agent_port));
+  cmd.AppendSwitchASCII("extension-port", base::NumberToString(extension_port));
+
+  // Set up launch options
+  base::LaunchOptions options;
+#if BUILDFLAG(IS_WIN)
+  options.start_hidden = true;
+#endif
+
+  // Launch the process (blocking I/O)
+  result.process = base::LaunchProcess(cmd, options);
+  return result;
+}
+
+// Factory for creating TCP server sockets for CDP
+class CDPServerSocketFactory : public content::DevToolsSocketFactory {
+ public:
+  explicit CDPServerSocketFactory(uint16_t port) : port_(port) {}
+
+  CDPServerSocketFactory(const CDPServerSocketFactory&) = delete;
+  CDPServerSocketFactory& operator=(const CDPServerSocketFactory&) = delete;
+
+ private:
+  std::unique_ptr<net::ServerSocket> CreateLocalHostServerSocket(int port) {
+    std::unique_ptr<net::ServerSocket> socket(
+        new net::TCPServerSocket(nullptr, net::NetLogSource()));
+    if (socket->ListenWithAddressAndPort("127.0.0.1", port, kBackLog) ==
+        net::OK) {
+      return socket;
+    }
+    if (socket->ListenWithAddressAndPort("::1", port, kBackLog) == net::OK) {
+      return socket;
+    }
+    return nullptr;
+  }
+
+  // content::DevToolsSocketFactory implementation
+  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
+    return CreateLocalHostServerSocket(port_);
+  }
+
+  std::unique_ptr<net::ServerSocket> CreateForTethering(
+      std::string* name) override {
+    return nullptr;  // Tethering not needed for AnalOS
+  }
+
+  uint16_t port_;
+};
+
+}  // namespace
+
+namespace analos {
+
+// static
+AnalOSServerManager* AnalOSServerManager::GetInstance() {
+  static base::NoDestructor<AnalOSServerManager> instance;
+  return instance.get();
+}
+
+AnalOSServerManager::AnalOSServerManager() = default;
+
+AnalOSServerManager::~AnalOSServerManager() {
+  Shutdown();
+}
+
+bool AnalOSServerManager::AcquireLock() {
+  // Allow blocking for lock file operations (short-duration I/O)
+  base::ScopedAllowBlocking allow_blocking;
+
+  base::FilePath exec_dir = GetAnalOSExecutionDir();
+  if (exec_dir.empty()) {
+    LOG(ERROR) << "analos: Failed to resolve execution directory for lock";
+    return false;
+  }
+
+  base::FilePath lock_path = exec_dir.Append(FILE_PATH_LITERAL("server.lock"));
+
+  lock_file_ = base::File(lock_path,
+                          base::File::FLAG_OPEN_ALWAYS |
+                          base::File::FLAG_READ |
+                          base::File::FLAG_WRITE);
+
+  if (!lock_file_.IsValid()) {
+    LOG(ERROR) << "analos: Failed to open lock file: " << lock_path;
+    return false;
+  }
+
+  base::File::Error lock_error =
+      lock_file_.Lock(base::File::LockMode::kExclusive);
+  if (lock_error != base::File::FILE_OK) {
+    LOG(INFO) << "analos: Server already running in another Chrome process "
+              << "(lock file: " << lock_path << ")";
+    lock_file_.Close();
+    return false;
+  }
+
+  LOG(INFO) << "analos: Acquired exclusive lock on " << lock_path;
+  return true;
+}
+
+void AnalOSServerManager::InitializePortsAndPrefs() {
+  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
+  PrefService* prefs = g_browser_process->local_state();
+
+  // Read from prefs or use defaults
+  if (!prefs) {
+    cdp_port_ = analos_server::kDefaultCDPPort;
+    mcp_port_ = analos_server::kDefaultMCPPort;
+    agent_port_ = analos_server::kDefaultAgentPort;
+    extension_port_ = analos_server::kDefaultExtensionPort;
+    allow_remote_in_mcp_ = false;
+  } else {
+    cdp_port_ = prefs->GetInteger(analos_server::kCDPServerPort);
+    if (cdp_port_ <= 0) {
+      cdp_port_ = analos_server::kDefaultCDPPort;
+    }
+
+    mcp_port_ = prefs->GetInteger(analos_server::kMCPServerPort);
+    if (mcp_port_ <= 0) {
+      mcp_port_ = analos_server::kDefaultMCPPort;
+    }
+
+    agent_port_ = prefs->GetInteger(analos_server::kAgentServerPort);
+    if (agent_port_ <= 0) {
+      agent_port_ = analos_server::kDefaultAgentPort;
+    }
+
+    extension_port_ = prefs->GetInteger(analos_server::kExtensionServerPort);
+    if (extension_port_ <= 0) {
+      extension_port_ = analos_server::kDefaultExtensionPort;
+    }
+
+    allow_remote_in_mcp_ = prefs->GetBoolean(analos_server::kAllowRemoteInMCP);
+
+    // Set up pref change observers
+    if (!pref_change_registrar_) {
+      pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
+      pref_change_registrar_->Init(prefs);
+      pref_change_registrar_->Add(
+          analos_server::kAllowRemoteInMCP,
+          base::BindRepeating(
+              &AnalOSServerManager::OnAllowRemoteInMCPChanged,
+              base::Unretained(this)));
+      pref_change_registrar_->Add(
+          analos_server::kRestartServerRequested,
+          base::BindRepeating(
+              &AnalOSServerManager::OnRestartServerRequestedChanged,
+              base::Unretained(this)));
+    }
+  }
+
+  // Find available ports, tracking assigned ports to prevent collisions
+  std::set<int> assigned_ports;
+
+  cdp_port_ = FindAvailablePort(cdp_port_, assigned_ports);
+  assigned_ports.insert(cdp_port_);
+
+  mcp_port_ = FindAvailablePort(mcp_port_, assigned_ports);
+  assigned_ports.insert(mcp_port_);
+
+  agent_port_ = FindAvailablePort(agent_port_, assigned_ports);
+  assigned_ports.insert(agent_port_);
+
+  extension_port_ = FindAvailablePort(extension_port_, assigned_ports);
+
+  // Apply command-line overrides (internal testing only)
+  int cdp_override = GetPortOverrideFromCommandLine(
+      command_line, analos::kCDPPort, "CDP port");
+  if (cdp_override > 0) {
+    cdp_port_ = cdp_override;
+  }
+
+  int mcp_override = GetPortOverrideFromCommandLine(
+      command_line, analos::kMCPPort, "MCP port");
+  if (mcp_override > 0) {
+    mcp_port_ = mcp_override;
+  }
+
+  int agent_override = GetPortOverrideFromCommandLine(
+      command_line, analos::kAgentPort, "Agent port");
+  if (agent_override > 0) {
+    agent_port_ = agent_override;
+  }
+
+  int extension_override = GetPortOverrideFromCommandLine(
+      command_line, analos::kExtensionPort, "Extension port");
+  if (extension_override > 0) {
+    extension_port_ = extension_override;
+  }
+
+  LOG(INFO) << "analos: Final ports - CDP: " << cdp_port_
+            << ", MCP: " << mcp_port_ << ", Agent: " << agent_port_
+            << ", Extension: " << extension_port_;
+}
+
+void AnalOSServerManager::SavePortsToPrefs() {
+  PrefService* prefs = g_browser_process->local_state();
+  if (!prefs) {
+    LOG(WARNING) << "analos: SavePortsToPrefs - no prefs available, skipping save";
+    return;
+  }
+
+  prefs->SetInteger(analos_server::kCDPServerPort, cdp_port_);
+  prefs->SetInteger(analos_server::kMCPServerPort, mcp_port_);
+  prefs->SetInteger(analos_server::kAgentServerPort, agent_port_);
+  prefs->SetInteger(analos_server::kExtensionServerPort, extension_port_);
+
+  LOG(INFO) << "analos: Saving to prefs - CDP: " << cdp_port_
+            << ", MCP: " << mcp_port_ << ", Agent: " << agent_port_
+            << ", Extension: " << extension_port_;
+}
+
+void AnalOSServerManager::Start() {
+  if (is_running_) {
+    LOG(INFO) << "analos: AnalOS server already running";
+    return;
+  }
+
+  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
+  // Initialize and finalize ports, even with analos-server disabled
+  // we want to update the prefs from CLI
+  InitializePortsAndPrefs();
+  SavePortsToPrefs();
+
+  if (command_line->HasSwitch(analos::kDisableServer)) {
+    LOG(INFO) << "analos: AnalOS server disabled via command line";
+    return;
+  }
+
+  // Try to acquire system-wide lock
+  if (!AcquireLock()) {
+    return;  // Another Chrome process already owns the server
+  }
+
+  LOG(INFO) << "analos: Starting AnalOS server";
+
+  // Start servers and process
+  // Note: monitoring timers are started in OnProcessLaunched() after successful launch
+  StartCDPServer();
+  LaunchAnalOSProcess();
+}
+
+void AnalOSServerManager::Stop() {
+  if (!is_running_) {
+    return;
+  }
+
+  LOG(INFO) << "analos: Stopping AnalOS server";
+  health_check_timer_.Stop();
+  process_check_timer_.Stop();
+
+  // Stop the updater
+  if (updater_) {
+    updater_->Stop();
+    updater_.reset();
+  }
+
+  // Use wait=false for shutdown - just send kill signal, don't block UI thread
+  TerminateAnalOSProcess(/*wait=*/false);
+
+  // Release lock
+  if (lock_file_.IsValid()) {
+    lock_file_.Unlock();
+    lock_file_.Close();
+    LOG(INFO) << "analos: Released lock file";
+  }
+}
+
+bool AnalOSServerManager::IsRunning() const {
+  return is_running_ && process_.IsValid();
+}
+
+void AnalOSServerManager::Shutdown() {
+  Stop();
+}
+
+void AnalOSServerManager::StartCDPServer() {
+  LOG(INFO) << "analos: Starting CDP server on port " << cdp_port_;
+
+  content::DevToolsAgentHost::StartRemoteDebuggingServer(
+      std::make_unique<CDPServerSocketFactory>(cdp_port_),
+      base::FilePath(),
+      base::FilePath());
+
+  LOG(INFO) << "analos: CDP WebSocket server started at ws://127.0.0.1:"
+            << cdp_port_;
+  LOG(INFO) << "analos: MCP server port: " << mcp_port_
+            << " (allow_remote: "
+            << (allow_remote_in_mcp_ ? "true" : "false") << ")";
+  LOG(INFO) << "analos: Agent server port: " << agent_port_;
+  LOG(INFO) << "analos: Extension server port: " << extension_port_;
+}
+
+void AnalOSServerManager::StopCDPServer() {
+  if (cdp_port_ == 0) {
+    return;
+  }
+
+  LOG(INFO) << "analos: Stopping CDP server";
+  content::DevToolsAgentHost::StopRemoteDebuggingServer();
+  cdp_port_ = 0;
+}
+
+void AnalOSServerManager::LaunchAnalOSProcess() {
+  // Bundled paths (always available as fallback)
+  base::FilePath fallback_exe_path = GetAnalOSServerExecutablePath();
+  base::FilePath fallback_resources_dir = GetAnalOSServerResourcesPath();
+
+  // Use updater's best paths if available (for OTA updates), otherwise bundled
+  base::FilePath exe_path;
+  base::FilePath resources_dir;
+  if (updater_) {
+    exe_path = updater_->GetBestServerBinaryPath();
+    resources_dir = updater_->GetBestServerResourcesPath();
+  } else {
+    exe_path = fallback_exe_path;
+    resources_dir = fallback_resources_dir;
+  }
+
+  base::FilePath execution_dir = GetAnalOSExecutionDir();
+  if (execution_dir.empty()) {
+    LOG(ERROR) << "analos: Failed to resolve execution directory";
+    return;
+  }
+
+  LOG(INFO) << "analos: Launching server - binary: " << exe_path;
+  LOG(INFO) << "analos: Launching server - resources: " << resources_dir;
+  LOG(INFO) << "analos: Launching server - execution dir: " << execution_dir;
+
+  // Capture values to pass to background thread
+  uint16_t cdp_port = cdp_port_;
+  uint16_t mcp_port = mcp_port_;
+  uint16_t agent_port = agent_port_;
+  uint16_t extension_port = extension_port_;
+
+  // Gather server config on UI thread
+  ServerConfig server_config;
+  server_config.analos_version =
+      std::string(version_info::GetAnalOSVersionNumber());
+  server_config.chromium_version =
+      std::string(version_info::GetVersionNumber());
+  server_config.allow_remote_in_mcp = allow_remote_in_mcp_;
+
+  // Get install_id from AnalOSMetricsService if available
+  ProfileManager* profile_manager = g_browser_process->profile_manager();
+  if (profile_manager) {
+    Profile* profile = profile_manager->GetLastUsedProfileIfLoaded();
+    if (profile && !profile->IsOffTheRecord()) {
+      analos_metrics::AnalOSMetricsService* metrics_service =
+          analos_metrics::AnalOSMetricsServiceFactory::GetForBrowserContext(
+              profile);
+      if (metrics_service) {
+        server_config.install_id = metrics_service->GetInstallId();
+      }
+    }
+  }
+
+  // Post blocking work to background thread, get result back on UI thread
+  base::ThreadPool::PostTaskAndReplyWithResult(
+      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
+      base::BindOnce(&LaunchProcessOnBackgroundThread, exe_path, resources_dir,
+                     fallback_exe_path, fallback_resources_dir, execution_dir,
+                     cdp_port, mcp_port, agent_port, extension_port,
+                     server_config),
+      base::BindOnce(&AnalOSServerManager::OnProcessLaunched,
+                     weak_factory_.GetWeakPtr()));
+}
+
+void AnalOSServerManager::OnProcessLaunched(LaunchResult result) {
+  bool was_updating = is_updating_;
+
+  // If we fell back to bundled binary, invalidate downloaded version
+  if (result.used_fallback && updater_) {
+    updater_->InvalidateDownloadedVersion();
+  }
+
+  if (!result.process.IsValid()) {
+    LOG(ERROR) << "analos: Failed to launch AnalOS server";
+    // Don't stop CDP server - it's independent and may be used by other things
+    // Leave system in degraded state (CDP up, no analos_server) rather than
+    // completely broken state (no CDP, no server)
+    is_restarting_ = false;
+
+    // Notify updater of failure if this was an update restart
+    if (was_updating) {
+      is_updating_ = false;
+      if (update_complete_callback_) {
+        std::move(update_complete_callback_).Run(false);
+      }
+    }
+    return;
+  }
+
+  process_ = std::move(result.process);
+  is_running_ = true;
+  last_launch_time_ = base::TimeTicks::Now();
+
+  LOG(INFO) << "analos: AnalOS server started with PID: " << process_.Pid();
+  LOG(INFO) << "analos: CDP port: " << cdp_port_;
+  LOG(INFO) << "analos: MCP port: " << mcp_port_;
+  LOG(INFO) << "analos: Agent port: " << agent_port_;
+  LOG(INFO) << "analos: Extension port: " << extension_port_;
+
+  // Start/restart monitoring timers
+  health_check_timer_.Start(FROM_HERE, kHealthCheckInterval, this,
+                            &AnalOSServerManager::CheckServerHealth);
+  process_check_timer_.Start(FROM_HERE, kProcessCheckInterval, this,
+                             &AnalOSServerManager::CheckProcessStatus);
+
+  // Reset restart flag and pref after successful launch
+  if (is_restarting_) {
+    is_restarting_ = false;
+    PrefService* prefs = g_browser_process->local_state();
+    if (prefs && prefs->GetBoolean(analos_server::kRestartServerRequested)) {
+      prefs->SetBoolean(analos_server::kRestartServerRequested, false);
+      LOG(INFO) << "analos: Restart completed, reset restart_requested pref";
+    }
+  }
+
+  // Notify updater of success if this was an update restart
+  if (was_updating) {
+    is_updating_ = false;
+    if (update_complete_callback_) {
+      std::move(update_complete_callback_).Run(true);
+    }
+  }
+
+  // Start the updater (if not already running and not disabled)
+  if (!updater_) {
+    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
+            analos::kDisableServerUpdater)) {
+      LOG(INFO) << "analos: Server updater disabled via command line";
+    } else {
+      updater_ =
+          std::make_unique<analos_server::AnalOSServerUpdater>(this);
+      updater_->Start();
+    }
+  }
+}
+
+void AnalOSServerManager::TerminateAnalOSProcess(bool wait) {
+  if (!process_.IsValid()) {
+    return;
+  }
+
+  LOG(INFO) << "analos: Terminating AnalOS server process (PID: "
+            << process_.Pid() << ", wait: " << (wait ? "true" : "false") << ")";
+
+#if BUILDFLAG(IS_POSIX)
+  base::ProcessId pid = process_.Pid();
+  if (kill(pid, SIGKILL) != 0) {
+    PLOG(ERROR) << "analos: Failed to send SIGKILL to PID " << pid;
+  } else if (wait) {
+    // Blocking wait - must be called from background thread
+    base::ScopedAllowBaseSyncPrimitives allow_sync;
+    base::ScopedAllowBlocking allow_blocking;
+    int exit_code = 0;
+    if (process_.WaitForExit(&exit_code)) {
+      LOG(INFO) << "analos: Process killed successfully";
+    } else {
+      LOG(WARNING) << "analos: WaitForExit failed";
+    }
+  } else {
+    LOG(INFO) << "analos: SIGKILL sent (not waiting for exit)";
+  }
+#else
+  // Windows: Terminate with wait parameter
+  bool terminated = process_.Terminate(0, wait);
+  if (terminated) {
+    LOG(INFO) << "analos: Process terminated successfully";
+  } else {
+    LOG(ERROR) << "analos: Failed to terminate process";
+  }
+#endif
+
+  is_running_ = false;
+}
+
+void AnalOSServerManager::OnProcessExited(int exit_code) {
+  LOG(INFO) << "analos: AnalOS server exited with code: " << exit_code;
+  is_running_ = false;
+
+  // Stop timers during restart to prevent races
+  health_check_timer_.Stop();
+  process_check_timer_.Stop();
+
+  // Crash tracking: check if this was a startup failure
+  base::TimeDelta uptime = base::TimeTicks::Now() - last_launch_time_;
+  if (uptime < kStartupGracePeriod) {
+    consecutive_startup_failures_++;
+    LOG(WARNING) << "analos: Startup failure detected (uptime: "
+                 << uptime.InSeconds() << "s, consecutive failures: "
+                 << consecutive_startup_failures_ << ")";
+
+    if (consecutive_startup_failures_ >= kMaxStartupFailures) {
+      LOG(ERROR) << "analos: Too many startup failures ("
+                 << consecutive_startup_failures_
+                 << "), invalidating downloaded version";
+      if (updater_) {
+        updater_->InvalidateDownloadedVersion();
+      }
+      consecutive_startup_failures_ = 0;
+    }
+  } else {
+    // Process ran past grace period, reset failure counter
+    consecutive_startup_failures_ = 0;
+  }
+
+  // Prevent concurrent restarts (e.g., if RestartAnalOSProcess is in progress)
+  if (is_restarting_) {
+    LOG(INFO) << "analos: Restart already in progress, skipping";
+    return;
+  }
+  is_restarting_ = true;
+
+  // Always restart - we want the server running
+  // Don't call Start() - we already hold the lock and CDP server is running
+  LOG(WARNING) << "analos: AnalOS server exited, restarting process...";
+
+  // Capture current ports for background thread
+  int cdp = cdp_port_;
+  int mcp = mcp_port_;
+  int agent = agent_port_;
+  int extension = extension_port_;
+
+  // Revalidate ports on background thread, then launch on UI thread
+  // Process is already dead, no need to terminate
+  base::ThreadPool::PostTaskAndReplyWithResult(
+      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
+      base::BindOnce(&AnalOSServerManager::RevalidatePorts,
+                     base::Unretained(this), cdp, mcp, agent, extension),
+      base::BindOnce(&AnalOSServerManager::OnPortsRevalidated,
+                     weak_factory_.GetWeakPtr()));
+}
+
+void AnalOSServerManager::CheckServerHealth() {
+  if (!is_running_) {
+    return;
+  }
+
+  // Build health check URL
+  GURL health_url("http://127.0.0.1:" + base::NumberToString(mcp_port_) + "/health");
+
+  // Create network traffic annotation
+  net::NetworkTrafficAnnotationTag traffic_annotation =
+      net::DefineNetworkTrafficAnnotation("analos_health_check", R"(
+        semantics {
+          sender: "AnalOS Server Manager"
+          description:
+            "Checks if the AnalOS MCP server is healthy by querying its "
+            "/health endpoint."
+          trigger: "Periodic health check every 60 seconds while server is running."
+          data: "No user data sent, just an HTTP GET request."
+          destination: LOCAL
+        }
+        policy {
+          cookies_allowed: NO
+          setting: "This feature cannot be disabled by settings."
+          policy_exception_justification:
+            "Internal health check for AnalOS server functionality."
+        })");
+
+  // Create resource request
+  auto resource_request = std::make_unique<network::ResourceRequest>();
+  resource_request->url = health_url;
+  resource_request->method = "GET";
+  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
+
+  auto url_loader = network::SimpleURLLoader::Create(
+      std::move(resource_request), traffic_annotation);
+  url_loader->SetTimeoutDuration(kHealthCheckTimeout);
+
+  // Get URL loader factory from default storage partition
+  auto* url_loader_factory =
+      g_browser_process->system_network_context_manager()
+          ->GetURLLoaderFactory();
+
+  // Keep a raw pointer for the callback
+  auto* url_loader_ptr = url_loader.get();
+
+  // Download response
+  url_loader_ptr->DownloadHeadersOnly(
+      url_loader_factory,
+      base::BindOnce(&AnalOSServerManager::OnHealthCheckComplete,
+                     weak_factory_.GetWeakPtr(), std::move(url_loader)));
+}
+
+void AnalOSServerManager::CheckProcessStatus() {
+  if (!is_running_ || !process_.IsValid()) {
+    return;
+  }
+
+  int exit_code = 0;
+  bool exited = process_.WaitForExitWithTimeout(base::TimeDelta(), &exit_code);
+  LOG(INFO) << "analos: CheckProcessStatus PID: " << process_.Pid()
+            << ", WaitForExitWithTimeout returned: " << exited
+            << ", exit_code: " << exit_code;
+
+  if (exited) {
+    OnProcessExited(exit_code);
+  }
+}
+
+void AnalOSServerManager::OnHealthCheckComplete(
+    std::unique_ptr<network::SimpleURLLoader> url_loader,
+    scoped_refptr<net::HttpResponseHeaders> headers) {
+  if (!is_running_) {
+    return;
+  }
+
+  // Check if we got a valid response
+  int response_code = 0;
+  if (headers) {
+    response_code = headers->response_code();
+  }
+
+  if (response_code == 200) {
+    LOG(INFO) << "analos: Health check passed";
+    return;
+  }
+
+  // Health check failed
+  int net_error = url_loader->NetError();
+  LOG(WARNING) << "analos: Health check failed - HTTP " << response_code
+               << ", net error: " << net::ErrorToString(net_error)
+               << ", restarting AnalOS server process...";
+
+  RestartAnalOSProcess();
+}
+
+void AnalOSServerManager::RestartAnalOSProcess() {
+  LOG(INFO) << "analos: Restarting AnalOS server process";
+
+  // Prevent multiple concurrent restarts
+  if (is_restarting_) {
+    LOG(INFO) << "analos: Restart already in progress, ignoring";
+    return;
+  }
+  is_restarting_ = true;
+
+  // Stop all timers during restart to prevent races
+  health_check_timer_.Stop();
+  process_check_timer_.Stop();
+
+  // Capture current ports for background thread
+  int cdp = cdp_port_;
+  int mcp = mcp_port_;
+  int agent = agent_port_;
+  int extension = extension_port_;
+
+  // Kill process on background thread, revalidate ports, then launch on UI thread
+  base::ThreadPool::PostTaskAndReplyWithResult(
+      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
+      base::BindOnce(
+          [](AnalOSServerManager* manager, int cdp, int mcp, int agent,
+             int extension) -> RevalidatedPorts {
+            manager->TerminateAnalOSProcess(/*wait=*/true);
+            return manager->RevalidatePorts(cdp, mcp, agent, extension);
+          },
+          base::Unretained(this), cdp, mcp, agent, extension),
+      base::BindOnce(&AnalOSServerManager::OnPortsRevalidated,
+                     weak_factory_.GetWeakPtr()));
+}
+
+AnalOSServerManager::RevalidatedPorts AnalOSServerManager::RevalidatePorts(
+    int cdp_port,
+    int current_mcp,
+    int current_agent,
+    int current_extension) {
+  // CDP port is excluded - it's still bound by Chrome's DevTools server
+  std::set<int> excluded_ports;
+  excluded_ports.insert(cdp_port);
+
+  RevalidatedPorts result;
+  result.mcp_port = FindAvailablePort(current_mcp, excluded_ports);
+  excluded_ports.insert(result.mcp_port);
+
+  result.agent_port = FindAvailablePort(current_agent, excluded_ports);
+  excluded_ports.insert(result.agent_port);
+
+  result.extension_port = FindAvailablePort(current_extension, excluded_ports);
+
+  return result;
+}
+
+void AnalOSServerManager::OnPortsRevalidated(RevalidatedPorts ports) {
+  bool ports_changed = (ports.mcp_port != mcp_port_) ||
+                       (ports.agent_port != agent_port_) ||
+                       (ports.extension_port != extension_port_);
+
+  if (ports_changed) {
+    LOG(INFO) << "analos: Ports changed during revalidation - "
+              << "MCP: " << mcp_port_ << " -> " << ports.mcp_port
+              << ", Agent: " << agent_port_ << " -> " << ports.agent_port
+              << ", Extension: " << extension_port_ << " -> "
+              << ports.extension_port;
+
+    mcp_port_ = ports.mcp_port;
+    agent_port_ = ports.agent_port;
+    extension_port_ = ports.extension_port;
+    SavePortsToPrefs();
+  }
+
+  // Note: is_restarting_ is cleared in OnProcessLaunched() after launch completes
+  LaunchAnalOSProcess();
+}
+
+void AnalOSServerManager::RestartServerForUpdate(
+    UpdateCompleteCallback callback) {
+  LOG(INFO) << "analos: Restarting server for OTA update";
+
+  // Prevent multiple concurrent restarts
+  if (is_restarting_ || is_updating_) {
+    LOG(WARNING) << "analos: Restart already in progress, failing update";
+    std::move(callback).Run(false);
+    return;
+  }
+
+  is_updating_ = true;
+  update_complete_callback_ = std::move(callback);
+
+  // Use same restart flow as RestartAnalOSProcess
+  is_restarting_ = true;
+  health_check_timer_.Stop();
+  process_check_timer_.Stop();
+
+  int cdp = cdp_port_;
+  int mcp = mcp_port_;
+  int agent = agent_port_;
+  int extension = extension_port_;
+
+  base::ThreadPool::PostTaskAndReplyWithResult(
+      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
+      base::BindOnce(
+          [](AnalOSServerManager* manager, int cdp, int mcp, int agent,
+             int extension) -> RevalidatedPorts {
+            manager->TerminateAnalOSProcess(/*wait=*/true);
+            return manager->RevalidatePorts(cdp, mcp, agent, extension);
+          },
+          base::Unretained(this), cdp, mcp, agent, extension),
+      base::BindOnce(&AnalOSServerManager::OnPortsRevalidated,
+                     weak_factory_.GetWeakPtr()));
+}
+
+void AnalOSServerManager::OnAllowRemoteInMCPChanged() {
+  if (!is_running_) {
+    return;
+  }
+
+  PrefService* prefs = g_browser_process->local_state();
+  if (!prefs) {
+    return;
+  }
+
+  bool new_value = prefs->GetBoolean(analos_server::kAllowRemoteInMCP);
+
+  if (new_value != allow_remote_in_mcp_) {
+    LOG(INFO) << "analos: allow_remote_in_mcp preference changed from "
+              << (allow_remote_in_mcp_ ? "true" : "false") << " to "
+              << (new_value ? "true" : "false")
+              << ", restarting server...";
+
+    allow_remote_in_mcp_ = new_value;
+
+    // Restart server to apply new config
+    RestartAnalOSProcess();
+  }
+}
+
+void AnalOSServerManager::OnRestartServerRequestedChanged() {
+  PrefService* prefs = g_browser_process->local_state();
+  if (!prefs) {
+    return;
+  }
+
+  bool restart_requested = prefs->GetBoolean(analos_server::kRestartServerRequested);
+
+  // Only process if pref is set to true
+  if (!restart_requested) {
+    return;
+  }
+
+  LOG(INFO) << "analos: Server restart requested via preference";
+  RestartAnalOSProcess();
+}
+
+int AnalOSServerManager::FindAvailablePort(
+    int starting_port,
+    const std::set<int>& excluded_ports) {
+  LOG(INFO) << "analos: Finding port starting from " << starting_port;
+
+  for (int i = 0; i < kMaxPortAttempts; i++) {
+    int port_to_try = starting_port + i;
+
+    if (port_to_try > kMaxPort) {
+      break;
+    }
+
+    // Skip ports already assigned to other AnalOS services
+    if (excluded_ports.count(port_to_try) > 0) {
+      continue;
+    }
+
+    if (IsPortAvailable(port_to_try)) {
+      if (port_to_try != starting_port) {
+        LOG(INFO) << "analos: Port " << starting_port
+                  << " was in use or excluded, using " << port_to_try
+                  << " instead";
+      } else {
+        LOG(INFO) << "analos: Using port " << port_to_try;
+      }
+      return port_to_try;
+    }
+  }
+
+  LOG(WARNING) << "analos: Could not find available port after "
+               << kMaxPortAttempts
+               << " attempts, using " << starting_port << " anyway";
+  return starting_port;
+}
+
+bool AnalOSServerManager::IsPortAvailable(int port) {
+  // Check port is in valid range
+  if (!net::IsPortValid(port) || port == 0) {
+    return false;
+  }
+
+  // Avoid well-known ports (0-1023, require elevated privileges)
+  if (net::IsWellKnownPort(port)) {
+    return false;
+  }
+
+  // Avoid restricted ports (could interfere with system services)
+  if (!net::IsPortAllowedForScheme(port, "http")) {
+    return false;
+  }
+
+  // Use TCPSocket directly instead of TCPServerSocket to avoid SO_REUSEADDR.
+  // TCPServerSocket::Listen() calls SetDefaultOptionsForServer() which sets
+  // SO_REUSEADDR, allowing bind to succeed even when another socket is bound
+  // to 0.0.0.0 (especially on macOS). By using TCPSocket directly and NOT
+  // calling SetDefaultOptionsForServer(), we get accurate port availability.
+
+  // Try binding to IPv4 localhost
+  auto socket = net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
+  int result = socket->Open(net::ADDRESS_FAMILY_IPV4);
+  if (result != net::OK) {
+    return false;
+  }
+  result = socket->Bind(net::IPEndPoint(net::IPAddress::IPv4Localhost(), port));
+  socket->Close();
+  if (result != net::OK) {
+    return false;  // IPv4 port is in use
+  }
+
+  // Try binding to IPv6 localhost
+  auto socket6 = net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
+  result = socket6->Open(net::ADDRESS_FAMILY_IPV6);
+  if (result != net::OK) {
+    return false;
+  }
+  result = socket6->Bind(net::IPEndPoint(net::IPAddress::IPv6Localhost(), port));
+  socket6->Close();
+  if (result != net::OK) {
+    return false;  // IPv6 port is in use
+  }
+
+  return true;
+}
+
+base::FilePath AnalOSServerManager::GetAnalOSServerResourcesPath() const {
+  // Check for command-line override first
+  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
+  if (command_line->HasSwitch(analos::kServerResourcesDir)) {
+    base::FilePath custom_path =
+        command_line->GetSwitchValuePath(analos::kServerResourcesDir);
+    LOG(INFO) << "analos: Using custom resources dir from command line: "
+              << custom_path;
+    return custom_path;
+  }
+
+  base::FilePath exe_dir;
+
+#if BUILDFLAG(IS_MAC)
+  // On macOS, the binary will be in the app bundle
+  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
+    LOG(ERROR) << "analos: Failed to get executable directory";
+    return base::FilePath();
+  }
+
+  // Navigate to Resources folder in the app bundle
+  // Chrome.app/Contents/MacOS -> Chrome.app/Contents/Resources
+  exe_dir = exe_dir.DirName().Append("Resources");
+
+#elif BUILDFLAG(IS_WIN)
+  // On Windows, installer places AnalOS Server under the versioned directory
+  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
+    LOG(ERROR) << "analos: Failed to get executable directory";
+    return base::FilePath();
+  }
+  // Append version directory (chrome.release places AnalOSServer under versioned dir)
+  exe_dir = exe_dir.AppendASCII(version_info::GetVersionNumber());
+
+#elif BUILDFLAG(IS_LINUX)
+  // On Linux, binary is in the same directory as chrome
+  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
+    LOG(ERROR) << "analos: Failed to get executable directory";
+    return base::FilePath();
+  }
+#endif
+
+  // Return path to resources directory
+  return exe_dir.Append(FILE_PATH_LITERAL("AnalOSServer"))
+      .Append(FILE_PATH_LITERAL("default"))
+      .Append(FILE_PATH_LITERAL("resources"));
+}
+
+base::FilePath AnalOSServerManager::GetAnalOSExecutionDir() const {
+  base::FilePath user_data_dir;
+  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
+    LOG(ERROR) << "analos: Failed to resolve DIR_USER_DATA path";
+    return base::FilePath();
+  }
+
+  base::FilePath exec_dir = user_data_dir.Append(FILE_PATH_LITERAL(".analos"));
+
+  // Ensure directory exists before returning
+  base::ScopedAllowBlocking allow_blocking;
+  if (!base::PathExists(exec_dir)) {
+    if (!base::CreateDirectory(exec_dir)) {
+      LOG(ERROR) << "analos: Failed to create execution directory: " << exec_dir;
+      return base::FilePath();
+    }
+  }
+
+  LOG(INFO) << "analos: Using execution directory: " << exec_dir;
+  return exec_dir;
+}
+
+base::FilePath AnalOSServerManager::GetAnalOSServerExecutablePath() const {
+  base::FilePath analos_exe =
+      GetAnalOSServerResourcesPath()
+          .Append(FILE_PATH_LITERAL("bin"))
+          .Append(FILE_PATH_LITERAL("analos_server"));
+
+#if BUILDFLAG(IS_WIN)
+  analos_exe = analos_exe.AddExtension(FILE_PATH_LITERAL(".exe"));
+#endif
+
+  return analos_exe;
+}
+
+}  // namespace analos
