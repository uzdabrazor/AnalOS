diff --git a/chrome/browser/extensions/analos_external_loader.cc b/chrome/browser/extensions/analos_external_loader.cc
new file mode 100644
index 0000000000000..3bbb8c24621cc
--- /dev/null
+++ b/chrome/browser/extensions/analos_external_loader.cc
@@ -0,0 +1,709 @@
+// Copyright 2024 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#include "chrome/browser/extensions/analos_external_loader.h"
+
+#include <memory>
+#include <utility>
+
+#include "base/feature_list.h"
+#include "base/files/file_util.h"
+#include "base/functional/bind.h"
+#include "base/json/json_reader.h"
+#include "base/logging.h"
+#include "base/memory/ptr_util.h"
+#include "base/strings/string_util.h"
+#include "base/task/thread_pool.h"
+#include "base/task/single_thread_task_runner.h"
+#include "base/values.h"
+#include "chrome/browser/browser_features.h"
+#include "chrome/browser/browser_process.h"
+#include "chrome/browser/analos/core/analos_constants.h"
+#include "chrome/browser/extensions/extension_service.h"
+#include "chrome/browser/extensions/external_provider_impl.h"
+#include "chrome/browser/extensions/updater/extension_updater.h"
+#include "chrome/browser/profiles/profile.h"
+#include "chrome/browser/analos/metrics/analos_metrics.h"
+#include "content/public/browser/browser_context.h"
+#include "content/public/browser/storage_partition.h"
+#include "extensions/browser/disable_reason.h"
+#include "extensions/browser/extension_prefs.h"
+#include "extensions/browser/extension_registrar.h"
+#include "extensions/browser/extension_registry.h"
+#include "extensions/browser/uninstall_reason.h"
+#include "extensions/browser/extension_system.h"
+#include "extensions/browser/pending_extension_manager.h"
+#include "extensions/common/extension.h"
+#include "extensions/common/mojom/manifest.mojom-shared.h"
+#include "net/base/load_flags.h"
+#include "net/traffic_annotation/network_traffic_annotation.h"
+#include "services/network/public/cpp/resource_request.h"
+#include "services/network/public/cpp/simple_url_loader.h"
+#include "services/network/public/mojom/url_response_head.mojom.h"
+
+namespace extensions {
+
+namespace {
+
+// Interval for periodic maintenance
+constexpr base::TimeDelta kPeriodicMaintenanceInterval = base::Minutes(15);
+
+// Network traffic annotation for the extension configuration fetch.
+constexpr net::NetworkTrafficAnnotationTag kAnalOSExtensionsFetchTrafficAnnotation =
+    net::DefineNetworkTrafficAnnotation("analos_extensions_fetch", R"(
+        semantics {
+          sender: "AnalOS External Extension Loader"
+          description:
+            "Fetches a JSON configuration file that specifies which extensions "
+            "should be installed for AnalOS users at startup."
+          trigger:
+            "Triggered during browser startup when AnalOS mode is enabled."
+          data:
+            "No user data is sent. Only a GET request to fetch the configuration."
+          destination: OTHER
+          destination_other:
+            "The AnalOS configuration server specified by the config URL."
+        }
+        policy {
+          cookies_allowed: NO
+          setting:
+            "This feature can be controlled via command-line flags or "
+            "enterprise policies."
+          policy_exception_justification:
+            "Not implemented yet. This is a new feature for AnalOS."
+        })");
+
+// Example JSON format:
+// {
+//   "extensions": {
+//     "extension_id_1": {
+//       "external_update_url": "https://example.com/extension1/updates.xml"
+//     },
+//     "extension_id_2": {
+//       "external_crx": "https://example.com/extension2.crx",
+//       "external_version": "1.0"
+//     }
+//   }
+// }
+
+}  // namespace
+
+AnalOSExternalLoader::AnalOSExternalLoader(Profile* profile)
+    : profile_(profile) {
+  if (base::FeatureList::IsEnabled(features::kAnalOsAlphaFeatures)) {
+    config_url_ = GURL(analos::kAnalOSAlphaConfigUrl);
+  } else {
+    config_url_ = GURL(analos::kAnalOSConfigUrl);
+  }
+
+  for (const std::string& extension_id : analos::GetAnalOSExtensionIds()) {
+    analos_extension_ids_.insert(extension_id);
+  }
+}
+
+AnalOSExternalLoader::~AnalOSExternalLoader() = default;
+
+void AnalOSExternalLoader::StartLoading() {
+  LOG(INFO) << "AnalOS external extension loader starting...";
+  
+  if (!config_file_for_testing_.empty()) {
+    LoadFromFile();
+    return;
+  }
+
+  if (!config_url_.is_valid()) {
+    LOG(ERROR) << "Invalid AnalOS extensions config URL";
+    LoadFinished(base::Value::Dict());
+    return;
+  }
+  
+  LOG(INFO) << "Fetching AnalOS extensions from: " << config_url_.spec();
+
+  // Create the URL loader factory
+  url_loader_factory_ = profile_->GetDefaultStoragePartition()
+                            ->GetURLLoaderFactoryForBrowserProcess();
+
+  // Create the resource request
+  auto resource_request = std::make_unique<network::ResourceRequest>();
+  resource_request->url = config_url_;
+  resource_request->method = "GET";
+  resource_request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
+
+  // Create the URL loader
+  url_loader_ = network::SimpleURLLoader::Create(
+      std::move(resource_request), kAnalOSExtensionsFetchTrafficAnnotation);
+
+  // Start the download
+  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
+      url_loader_factory_.get(),
+      base::BindOnce(&AnalOSExternalLoader::OnURLFetchComplete,
+                     weak_ptr_factory_.GetWeakPtr()));
+}
+
+void AnalOSExternalLoader::OnURLFetchComplete(
+    std::unique_ptr<std::string> response_body) {
+  if (!response_body) {
+    LOG(ERROR) << "Failed to fetch AnalOS extensions config from " 
+               << config_url_.spec();
+    LoadFinished(base::Value::Dict());
+    return;
+  }
+
+  ParseConfiguration(*response_body);
+}
+
+void AnalOSExternalLoader::ParseConfiguration(
+    const std::string& json_content) {
+  std::optional<base::Value> parsed_json = base::JSONReader::Read(json_content);
+  
+  if (!parsed_json || !parsed_json->is_dict()) {
+    LOG(ERROR) << "Failed to parse AnalOS extensions config JSON";
+    LoadFinished(base::Value::Dict());
+    return;
+  }
+
+  const base::Value::Dict* extensions_dict = 
+      parsed_json->GetDict().FindDict("extensions");
+  
+  if (!extensions_dict) {
+    LOG(ERROR) << "No 'extensions' key found in AnalOS config";
+    LoadFinished(base::Value::Dict());
+    return;
+  }
+
+  // Create the prefs dictionary in the format expected by ExternalProviderImpl
+  base::Value::Dict prefs;
+  
+  for (const auto [extension_id, extension_config] : *extensions_dict) {
+    if (!extension_config.is_dict()) {
+      LOG(WARNING) << "Invalid config for extension " << extension_id;
+      continue;
+    }
+    
+    const base::Value::Dict& config_dict = extension_config.GetDict();
+    base::Value::Dict extension_prefs;
+    
+    // Copy supported fields
+    if (const std::string* update_url = 
+        config_dict.FindString(ExternalProviderImpl::kExternalUpdateUrl)) {
+      extension_prefs.Set(ExternalProviderImpl::kExternalUpdateUrl, *update_url);
+    }
+    
+    if (const std::string* crx_path = 
+        config_dict.FindString(ExternalProviderImpl::kExternalCrx)) {
+      extension_prefs.Set(ExternalProviderImpl::kExternalCrx, *crx_path);
+    }
+    
+    if (const std::string* version = 
+        config_dict.FindString(ExternalProviderImpl::kExternalVersion)) {
+      extension_prefs.Set(ExternalProviderImpl::kExternalVersion, *version);
+    }
+    
+    // Add other supported fields as needed
+    std::optional<bool> keep_if_present = 
+        config_dict.FindBool(ExternalProviderImpl::kKeepIfPresent);
+    if (keep_if_present.has_value()) {
+      extension_prefs.Set(ExternalProviderImpl::kKeepIfPresent, 
+                         keep_if_present.value());
+    }
+    
+    if (!extension_prefs.empty()) {
+      prefs.Set(extension_id, std::move(extension_prefs));
+    }
+  }
+  
+  LOG(INFO) << "Loaded " << prefs.size() << " extensions from AnalOS config";
+  
+  // Track the extension IDs we're managing
+  for (const auto [extension_id, _] : prefs) {
+    analos_extension_ids_.insert(extension_id);
+  }
+  
+  // Store the initial config for comparison
+  if (!extensions_dict->empty()) {
+    last_config_ = extensions_dict->Clone();
+  }
+  
+  // Pass the prefs to the external provider system
+  LoadFinished(std::move(prefs));
+  
+  // Use a delayed task to ensure the extension system is fully initialized
+  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
+      FROM_HERE,
+      base::BindOnce(&AnalOSExternalLoader::TriggerImmediateInstallation,
+                     weak_ptr_factory_.GetWeakPtr()),
+      base::Seconds(2));
+  
+  // Start periodic checking after initial load
+  StartPeriodicCheck();
+
+  // Log initial extension state at startup
+  CheckAndLogExtensionState("startup");
+}
+
+void AnalOSExternalLoader::StartPeriodicCheck() {
+  LOG(INFO) << "analos: Starting periodic maintenance (every " 
+            << kPeriodicMaintenanceInterval.InMinutes() << " minutes)";
+  
+  // Schedule the periodic maintenance
+  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
+      FROM_HERE,
+      base::BindOnce(&AnalOSExternalLoader::PeriodicMaintenance,
+                     weak_ptr_factory_.GetWeakPtr()),
+      kPeriodicMaintenanceInterval);
+}
+
+void AnalOSExternalLoader::PeriodicMaintenance() {
+  LOG(INFO) << "analos: Running periodic maintenance";
+  
+  if (!profile_) {
+    // Schedule next check even if profile isn't ready
+    StartPeriodicCheck();
+    return;
+  }
+  
+  // 1. Uninstall deprecated extensions (in kAnalOSExtensions but not in server config)
+  UninstallDeprecatedExtensions();
+
+  // 2. Check for and reinstall any uninstalled AnalOS extensions
+  ReinstallUninstalledExtensions();
+
+  // 3. Re-enable any disabled AnalOS extensions
+  ReenableDisabledExtensions();
+
+  // 4. Fetch latest config and check for changes
+  FetchAndCheckConfig();
+
+  // 5. Force immediate update check for all AnalOS extensions
+  ForceUpdateCheck();
+
+  // 6. Log extension state after all maintenance attempts
+  CheckAndLogExtensionState("periodic_maintenance");
+
+  // Schedule the next maintenance
+  StartPeriodicCheck();
+}
+
+void AnalOSExternalLoader::ReinstallUninstalledExtensions() {
+  ExtensionService* service = ExtensionSystem::Get(profile_)->extension_service();
+  if (!service) {
+    return;
+  }
+  
+  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
+  PendingExtensionManager* pending_manager = PendingExtensionManager::Get(profile_);
+  
+  if (!registry || !pending_manager || last_config_.empty()) {
+    return;
+  }
+  
+  for (const std::string& extension_id : analos_extension_ids_) {
+    // Check if extension exists (installed or disabled)
+    if (registry->GetInstalledExtension(extension_id)) {
+      continue;  // Extension is installed, skip to next
+    }
+    
+    LOG(INFO) << "analos: Extension " << extension_id 
+              << " was uninstalled, attempting to reinstall";
+    
+    // Find the extension's configuration
+    const base::Value::Dict* extension_config = last_config_.FindDict(extension_id);
+    if (!extension_config) {
+      LOG(WARNING) << "analos: No config found for " << extension_id;
+      continue;
+    }
+    
+    // Get the update URL from config
+    const std::string* update_url = 
+        extension_config->FindString(ExternalProviderImpl::kExternalUpdateUrl);
+    if (!update_url) {
+      LOG(WARNING) << "analos: No update URL found for " << extension_id;
+      continue;
+    }
+    
+    // Validate and add to pending extensions
+    GURL update_gurl(*update_url);
+    if (!update_gurl.is_valid()) {
+      LOG(WARNING) << "analos: Invalid update URL for " << extension_id;
+      continue;
+    }
+    
+    // Add as pending extension for installation
+    pending_manager->AddFromExternalUpdateUrl(
+        extension_id,
+        std::string(),  // No install param
+        update_gurl,
+        mojom::ManifestLocation::kExternalComponent,
+        Extension::WAS_INSTALLED_BY_DEFAULT,
+        false);
+    
+    LOG(INFO) << "analos: Added " << extension_id 
+              << " to pending extensions for reinstall";
+    
+    // Trigger immediate installation
+    ExtensionUpdater* updater = ExtensionUpdater::Get(profile_);
+    if (updater) {
+      ExtensionUpdater::CheckParams params;
+      params.ids = {extension_id};
+      params.install_immediately = true;
+      params.fetch_priority = DownloadFetchPriority::kForeground;
+      updater->CheckNow(std::move(params));
+      LOG(INFO) << "analos: Triggered immediate install for " << extension_id;
+    }
+  }
+}
+
+void AnalOSExternalLoader::ReenableDisabledExtensions() {
+  ExtensionService* service = ExtensionSystem::Get(profile_)->extension_service();
+  if (!service) {
+    return;
+  }
+
+  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
+  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
+
+  if (!registry || !prefs) {
+    return;
+  }
+
+  for (const std::string& extension_id : analos_extension_ids_) {
+    if (!registry->disabled_extensions().Contains(extension_id)) {
+      continue;
+    }
+
+    auto* registrar = extensions::ExtensionRegistrar::Get(profile_);
+    if (!registrar) {
+      LOG(WARNING) << "analos: Cannot re-enable " << extension_id
+                   << " because ExtensionRegistrar is unavailable";
+      continue;
+    }
+
+    LOG(INFO) << "analos: Re-enabling extension " << extension_id;
+    registrar->EnableExtension(extension_id);
+  }
+}
+
+void AnalOSExternalLoader::FetchAndCheckConfig() {
+  LOG(INFO) << "analos: Fetching latest config to check for changes";
+  
+  if (config_file_for_testing_.empty() && config_url_.is_valid()) {
+    // Fetch from URL
+    if (!url_loader_factory_) {
+      url_loader_factory_ = profile_->GetDefaultStoragePartition()
+                                ->GetURLLoaderFactoryForBrowserProcess();
+    }
+    
+    auto resource_request = std::make_unique<network::ResourceRequest>();
+    resource_request->url = config_url_;
+    resource_request->method = "GET";
+    resource_request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
+    
+    auto config_check_loader = network::SimpleURLLoader::Create(
+        std::move(resource_request), kAnalOSExtensionsFetchTrafficAnnotation);
+    
+    // Store the loader to keep it alive during the request
+    auto* loader_ptr = config_check_loader.get();
+    loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
+        url_loader_factory_.get(),
+        base::BindOnce(&AnalOSExternalLoader::OnConfigCheckComplete,
+                       weak_ptr_factory_.GetWeakPtr(),
+                       std::move(config_check_loader)));
+  }
+}
+
+void AnalOSExternalLoader::OnConfigCheckComplete(
+    std::unique_ptr<network::SimpleURLLoader> loader,
+    std::unique_ptr<std::string> response_body) {
+  if (!response_body) {
+    LOG(WARNING) << "analos: Failed to fetch config for update check";
+    return;
+  }
+  
+  std::optional<base::Value> parsed_json = base::JSONReader::Read(*response_body);
+  if (!parsed_json || !parsed_json->is_dict()) {
+    LOG(WARNING) << "analos: Invalid config JSON during update check";
+    return;
+  }
+  
+  const base::Value::Dict* extensions_dict = 
+      parsed_json->GetDict().FindDict("extensions");
+  if (!extensions_dict) {
+    return;
+  }
+  
+  // Check if config has changed
+  bool config_changed = false;
+  if (last_config_.empty()) {
+    config_changed = true;  // First time
+  } else {
+    // Compare with last config
+    for (const auto [extension_id, new_config] : *extensions_dict) {
+      const base::Value::Dict* old_config = last_config_.FindDict(extension_id);
+      if (!old_config || *old_config != new_config.GetDict()) {
+        config_changed = true;
+        LOG(INFO) << "analos: Config changed for extension " << extension_id;
+        break;
+      }
+    }
+    
+    // Check for removed extensions
+    for (const auto [extension_id, _] : last_config_) {
+      if (!extensions_dict->contains(extension_id)) {
+        config_changed = true;
+        LOG(INFO) << "analos: Extension " << extension_id << " removed from config";
+        break;
+      }
+    }
+  }
+  
+  if (config_changed) {
+    LOG(INFO) << "analos: Config has changed, reloading extensions";
+    
+    // Store the new config
+    last_config_ = extensions_dict->Clone();
+    
+    // Parse and reload with new config
+    ParseConfiguration(*response_body);
+  } else {
+    LOG(INFO) << "analos: Config unchanged";
+  }
+}
+
+void AnalOSExternalLoader::TriggerImmediateInstallation() {
+  if (!profile_ || analos_extension_ids_.empty()) {
+    return;
+  }
+  
+  LOG(INFO) << "analos: Triggering immediate installation on first start";
+  
+  // First, add all extensions to pending if they're not already installed
+  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
+  PendingExtensionManager* pending_manager = PendingExtensionManager::Get(profile_);
+  
+  if (registry && pending_manager && !last_config_.empty()) {
+    for (const std::string& extension_id : analos_extension_ids_) {
+      // Skip if already installed
+      if (registry->GetInstalledExtension(extension_id)) {
+        LOG(INFO) << "analos: Extension " << extension_id << " already installed";
+        continue;
+      }
+      
+      // Add to pending extensions
+      const base::Value::Dict* extension_config = last_config_.FindDict(extension_id);
+      if (extension_config) {
+        const std::string* update_url = 
+            extension_config->FindString(ExternalProviderImpl::kExternalUpdateUrl);
+        if (update_url) {
+          GURL update_gurl(*update_url);
+          if (update_gurl.is_valid()) {
+            pending_manager->AddFromExternalUpdateUrl(
+                extension_id,
+                std::string(),  // No install param
+                update_gurl,
+                mojom::ManifestLocation::kExternalComponent,
+                Extension::WAS_INSTALLED_BY_DEFAULT,
+                false);  // Don't mark acknowledged
+            LOG(INFO) << "analos: Added " << extension_id 
+                      << " to pending for immediate installation";
+          }
+        }
+      }
+    }
+  }
+  
+  // Now trigger immediate high-priority installation
+  ExtensionUpdater* updater = ExtensionUpdater::Get(profile_);
+  if (!updater) {
+    LOG(WARNING) << "analos: No extension updater available for immediate installation";
+    return;
+  }
+  
+  LOG(INFO) << "analos: Executing CheckNow with immediate install for " 
+            << analos_extension_ids_.size() << " AnalOS extensions";
+  
+  // Create CheckParams for immediate foreground installation
+  ExtensionUpdater::CheckParams params;
+  params.ids = std::list<ExtensionId>(analos_extension_ids_.begin(),
+                                       analos_extension_ids_.end());
+  params.install_immediately = true;
+  params.fetch_priority = DownloadFetchPriority::kForeground;
+
+  // Trigger the installation
+  updater->CheckNow(std::move(params));
+}
+
+void AnalOSExternalLoader::ForceUpdateCheck() {
+  if (!profile_ || analos_extension_ids_.empty()) {
+    return;
+  }
+  
+  ExtensionUpdater* updater = ExtensionUpdater::Get(profile_);
+  if (!updater) {
+    LOG(WARNING) << "analos: No extension updater available";
+    return;
+  }
+  
+  LOG(INFO) << "analos: Forcing immediate update check for " 
+            << analos_extension_ids_.size() << " AnalOS extensions";
+  
+  // Create CheckParams for immediate foreground update
+  ExtensionUpdater::CheckParams params;
+  params.ids = std::list<ExtensionId>(analos_extension_ids_.begin(), 
+                                       analos_extension_ids_.end());
+  params.install_immediately = true;
+  params.fetch_priority = DownloadFetchPriority::kForeground;
+  
+  // Trigger the update check
+  updater->CheckNow(std::move(params));
+}
+
+void AnalOSExternalLoader::LoadFromFile() {
+  // This runs on a background thread to avoid blocking the UI
+  base::ThreadPool::PostTaskAndReplyWithResult(
+      FROM_HERE,
+      {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
+      base::BindOnce([](const base::FilePath& path) -> std::string {
+        std::string contents;
+        if (!base::ReadFileToString(path, &contents)) {
+          LOG(ERROR) << "Failed to read AnalOS config file: " << path;
+          return std::string();
+        }
+        return contents;
+      }, config_file_for_testing_),
+      base::BindOnce(&AnalOSExternalLoader::ParseConfiguration,
+                     weak_ptr_factory_.GetWeakPtr()));
+}
+
+void AnalOSExternalLoader::CheckAndLogExtensionState(
+    const std::string& context) {
+  if (!profile_) {
+    return;
+  }
+
+  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
+  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
+
+  if (!registry || !prefs) {
+    return;
+  }
+
+  for (const std::string& extension_id : analos_extension_ids_) {
+    // If extension is enabled, it's healthy - skip logging
+    if (registry->enabled_extensions().Contains(extension_id)) {
+      continue;
+    }
+
+    // Extension is NOT enabled - gather diagnostic information
+    base::Value::Dict properties;
+    properties.Set("extension_id", extension_id);
+    properties.Set("context", context);
+
+    std::string state;
+
+    if (registry->disabled_extensions().Contains(extension_id)) {
+      state = "disabled";
+
+      // Get extension version if available
+      const Extension* extension = registry->disabled_extensions().GetByID(extension_id);
+      if (extension) {
+        properties.Set("version", extension->version().GetString());
+      }
+
+      // Get disable reasons using public API
+      DisableReasonSet disable_reasons = prefs->GetDisableReasons(extension_id);
+
+      // Convert to bitmask by ORing all reason values
+      int bitmask = 0;
+      for (disable_reason::DisableReason reason : disable_reasons) {
+        bitmask |= static_cast<int>(reason);
+      }
+      properties.Set("disable_reasons_bitmask", bitmask);
+
+      // Log individual disable reason flags for easy querying
+      properties.Set("reason_user_action",
+                     disable_reasons.contains(disable_reason::DISABLE_USER_ACTION));
+      properties.Set("reason_permissions_increase",
+                     disable_reasons.contains(disable_reason::DISABLE_PERMISSIONS_INCREASE));
+      properties.Set("reason_reload",
+                     disable_reasons.contains(disable_reason::DISABLE_RELOAD));
+      properties.Set("reason_corrupted",
+                     disable_reasons.contains(disable_reason::DISABLE_CORRUPTED));
+      properties.Set("reason_greylist",
+                     disable_reasons.contains(disable_reason::DISABLE_GREYLIST));
+      properties.Set("reason_remote_install",
+                     disable_reasons.contains(disable_reason::DISABLE_REMOTE_INSTALL));
+
+    } else if (registry->blocklisted_extensions().Contains(extension_id)) {
+      state = "blocklisted";
+
+    } else if (registry->blocked_extensions().Contains(extension_id)) {
+      state = "blocked";
+
+    } else if (registry->terminated_extensions().Contains(extension_id)) {
+      state = "terminated";
+
+    } else {
+      state = "not_installed";
+    }
+
+    properties.Set("state", state);
+
+    // Log to metrics
+    analos_metrics::AnalOSMetrics::Log("ota.extension.unexpected_state",
+                                              std::move(properties));
+
+    // Also log to Chrome logs for local debugging
+    LOG(WARNING) << "analos: Extension " << extension_id
+                 << " in unexpected state: " << state
+                 << " (context: " << context << ")";
+  }
+}
+
+void AnalOSExternalLoader::UninstallDeprecatedExtensions() {
+  if (!profile_ || last_config_.empty()) {
+    return;
+  }
+
+  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
+  if (!registry) {
+    return;
+  }
+
+  auto* registrar = ExtensionRegistrar::Get(profile_);
+  if (!registrar) {
+    return;
+  }
+
+  // Build set of extension IDs currently in server config
+  std::set<std::string> server_extension_ids;
+  for (const auto [extension_id, _] : last_config_) {
+    server_extension_ids.insert(extension_id);
+  }
+
+  // Check all AnalOS-managed extensions
+  for (const std::string& extension_id : analos::GetAnalOSExtensionIds()) {
+    // Skip if extension is in server config (still wanted)
+    if (server_extension_ids.contains(extension_id)) {
+      continue;
+    }
+
+    // Check if extension is installed
+    const Extension* extension = registry->GetInstalledExtension(extension_id);
+    if (!extension) {
+      continue;
+    }
+
+    LOG(INFO) << "analos: Uninstalling deprecated extension " << extension_id;
+
+    std::u16string error;
+    if (!registrar->UninstallExtension(extension_id,
+                                       UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION,
+                                       &error)) {
+      LOG(WARNING) << "analos: Failed to uninstall deprecated extension "
+                   << extension_id << ": " << error;
+    }
+  }
+}
+
+}  // namespace extensions
