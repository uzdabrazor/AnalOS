diff --git a/chrome/browser/analos/core/analos_switches.h b/chrome/browser/analos/core/analos_switches.h
new file mode 100644
index 0000000000000..4e7932596534c
--- /dev/null
+++ b/chrome/browser/analos/core/analos_switches.h
@@ -0,0 +1,83 @@
+// Copyright 2024 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#ifndef CHROME_BROWSER_ANALOS_CORE_ANALOS_SWITCHES_H_
+#define CHROME_BROWSER_ANALOS_CORE_ANALOS_SWITCHES_H_
+
+namespace analos {
+
+// =============================================================================
+// AnalOS Command-Line Switches
+// =============================================================================
+// All AnalOS-specific command-line flags are defined here.
+// Usage: --flag-name or --flag-name=value
+
+// === Server Switches ===
+
+// Disables the AnalOS server entirely.
+inline constexpr char kDisableServer[] = "disable-analos-server";
+
+// Disables the AnalOS server OTA updater.
+inline constexpr char kDisableServerUpdater[] = "disable-analos-server-updater";
+
+// Overrides the appcast URL for server updates (testing).
+inline constexpr char kServerAppcastUrl[] = "analos-server-appcast-url";
+
+// Overrides the server resources directory path.
+inline constexpr char kServerResourcesDir[] = "analos-server-resources-dir";
+
+// Overrides the CDP (Chrome DevTools Protocol) port.
+inline constexpr char kCDPPort[] = "analos-cdp-port";
+
+// Overrides the MCP (Model Context Protocol) port.
+inline constexpr char kMCPPort[] = "analos-mcp-port";
+
+// Overrides the Agent server port.
+inline constexpr char kAgentPort[] = "analos-agent-port";
+
+// Overrides the Extension server port.
+inline constexpr char kExtensionPort[] = "analos-extension-port";
+
+// === Extension Switches ===
+
+// Disables AnalOS managed extensions.
+inline constexpr char kDisableExtensions[] = "disable-analos-extensions";
+
+// Overrides the extensions config URL.
+inline constexpr char kExtensionsUrl[] = "analos-extensions-url";
+
+// === URL Override Switches ===
+
+// Disables chrome://analos/* URL overrides.
+// Useful for debugging to see raw extension URLs.
+inline constexpr char kDisableUrlOverrides[] = "analos-disable-url-overrides";
+
+// === Sparkle Switches (macOS Browser Updates) ===
+
+// Overrides the Sparkle appcast URL for browser updates.
+inline constexpr char kSparkleUrl[] = "analos-sparkle-url";
+
+// Forces an immediate Sparkle update check.
+inline constexpr char kSparkleForceCheck[] = "analos-sparkle-force-check";
+
+// Runs Sparkle in dry-run mode (no actual updates).
+inline constexpr char kSparkleDryRun[] = "sparkle-dry-run";
+
+// Skips Sparkle signature verification (testing only).
+inline constexpr char kSparkleSkipSignature[] = "sparkle-skip-signature";
+
+// Spoofs the current version for Sparkle (testing).
+inline constexpr char kSparkleSpoofVersion[] = "sparkle-spoof-version";
+
+// Enables verbose Sparkle logging.
+inline constexpr char kSparkleVerbose[] = "sparkle-verbose";
+
+// === Misc Switches ===
+
+// Indicates this is the first run of AnalOS.
+inline constexpr char kFirstRun[] = "analos-first-run";
+
+}  // namespace analos
+
+#endif  // CHROME_BROWSER_ANALOS_CORE_ANALOS_SWITCHES_H_
