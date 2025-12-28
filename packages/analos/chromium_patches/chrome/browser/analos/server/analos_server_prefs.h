diff --git a/chrome/browser/analos/server/analos_server_prefs.h b/chrome/browser/analos/server/analos_server_prefs.h
new file mode 100644
index 0000000000000..0506a219c6272
--- /dev/null
+++ b/chrome/browser/analos/server/analos_server_prefs.h
@@ -0,0 +1,35 @@
+// Copyright 2024 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#ifndef CHROME_BROWSER_ANALOS_SERVER_ANALOS_SERVER_PREFS_H_
+#define CHROME_BROWSER_ANALOS_SERVER_ANALOS_SERVER_PREFS_H_
+
+class PrefRegistrySimple;
+
+namespace analos_server {
+
+// Default port values for AnalOS server (10-port spacing)
+inline constexpr int kDefaultCDPPort = 9000;
+inline constexpr int kDefaultMCPPort = 9100;
+inline constexpr int kDefaultAgentPort = 9200;
+inline constexpr int kDefaultExtensionPort = 9300;
+
+// Preference keys for AnalOS server configuration
+extern const char kCDPServerPort[];
+extern const char kMCPServerPort[];
+extern const char kAgentServerPort[];
+extern const char kExtensionServerPort[];
+extern const char kAllowRemoteInMCP[];
+extern const char kRestartServerRequested[];
+extern const char kServerVersion[];
+
+// Deprecated prefs (kept for migration, will be removed in future)
+extern const char kMCPServerEnabled[];  // DEPRECATED: no longer used
+
+// Registers AnalOS server preferences in Local State (browser-wide prefs)
+void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
+
+}  // namespace analos_server
+
+#endif  // CHROME_BROWSER_ANALOS_SERVER_ANALOS_SERVER_PREFS_H_
