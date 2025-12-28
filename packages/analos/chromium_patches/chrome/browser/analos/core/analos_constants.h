diff --git a/chrome/browser/analos/core/analos_constants.h b/chrome/browser/analos/core/analos_constants.h
new file mode 100644
index 0000000000000..6d20e32069f92
--- /dev/null
+++ b/chrome/browser/analos/core/analos_constants.h
@@ -0,0 +1,214 @@
+// Copyright 2024 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#ifndef CHROME_BROWSER_ANALOS_CORE_ANALOS_CONSTANTS_H_
+#define CHROME_BROWSER_ANALOS_CORE_ANALOS_CONSTANTS_H_
+
+#include <cstddef>
+#include <string>
+#include <vector>
+
+#include "base/command_line.h"
+#include "chrome/browser/analos/core/analos_switches.h"
+
+namespace analos {
+
+// Check if URL overrides are disabled via command line flag
+inline bool IsURLOverridesDisabled() {
+  return base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableUrlOverrides);
+}
+
+// Agent V2 Extension ID
+inline constexpr char kAgentV2ExtensionId[] =
+    "bflpfmnmnokmjhmgnolecpppdbdophmk";
+
+// AnalOS extension config URLs
+inline constexpr char kAnalOSConfigUrl[] =
+    "https://cdn.uzdabrazor.com/extensions/extensions.json";
+inline constexpr char kAnalOSAlphaConfigUrl[] =
+    "https://cdn.uzdabrazor.com/extensions/extensions.alpha.json";
+
+// Bug Reporter Extension ID
+inline constexpr char kBugReporterExtensionId[] =
+    "adlpneommgkgeanpaekgoaolcpncohkf";
+
+// Controller Extension ID
+inline constexpr char kControllerExtensionId[] =
+    "nlnihljpboknmfagkikhkdblbedophja";
+
+// uBlock Origin Extension ID (Chrome Web Store)
+inline constexpr char kUBlockOriginExtensionId[] =
+    "cjpalhdlnbpafiamejdnhcphjbkeiagm";
+
+// AnalOS CDN update manifest URL
+// Used for extensions installed from local .crx files that don't have
+// an update_url in their manifest
+inline constexpr char kAnalOSUpdateUrl[] =
+    "https://cdn.uzdabrazor.com/extensions/update-manifest.xml";
+
+// chrome://analos host constant
+inline constexpr char kAnalOSHost[] = "analos";
+
+// URL route mapping for chrome://analos/* virtual URLs
+struct AnalOSURLRoute {
+  const char* virtual_path;    // Path in chrome://analos/*, e.g., "/ai"
+  const char* extension_id;    // Extension that handles this route
+  const char* extension_page;  // Page within extension, e.g., "options.html"
+  const char* extension_hash;  // Hash/fragment without #, e.g., "ai" (empty if none)
+};
+
+inline constexpr AnalOSURLRoute kAnalOSURLRoutes[] = {
+    {"/settings", kAgentV2ExtensionId, "options.html", ""},
+    {"/mcp", kAgentV2ExtensionId, "options.html", "mcp"},
+    {"/onboarding", kAgentV2ExtensionId, "onboarding.html", ""},
+};
+
+inline constexpr size_t kAnalOSURLRoutesCount =
+    sizeof(kAnalOSURLRoutes) / sizeof(kAnalOSURLRoutes[0]);
+
+// Find a route for a given virtual path (e.g., "/ai")
+// Returns nullptr if no matching route found
+inline const AnalOSURLRoute* FindAnalOSRoute(const std::string& path) {
+  for (const auto& route : kAnalOSURLRoutes) {
+    if (path == route.virtual_path) {
+      return &route;
+    }
+  }
+  return nullptr;
+}
+
+// Get the extension URL for a chrome://analos/* path
+// Returns empty string if no matching route or if URL overrides are disabled
+// Example: "/ai" -> "chrome-extension://bflp.../options.html#ai"
+inline std::string GetAnalOSExtensionURL(const std::string& virtual_path) {
+  if (IsURLOverridesDisabled()) {
+    return std::string();
+  }
+  const AnalOSURLRoute* route = FindAnalOSRoute(virtual_path);
+  if (!route) {
+    return std::string();
+  }
+  std::string url = std::string("chrome-extension://") + route->extension_id +
+                    "/" + route->extension_page;
+  if (route->extension_hash[0] != '\0') {
+    url += "#";
+    url += route->extension_hash;
+  }
+  return url;
+}
+
+// Check if an extension URL matches a AnalOS route
+// If matched, returns the virtual URL (chrome://analos/...)
+// Returns empty string if not a AnalOS extension URL
+// Parameters:
+//   extension_id: from url.host()
+//   extension_path: from url.path(), e.g., "/options.html"
+//   extension_ref: from url.ref(), e.g., "ai" or "/ai" (normalized internally)
+// Fallback: If no exact hash match, falls back to route with empty hash for same page
+inline std::string GetAnalOSVirtualURL(const std::string& extension_id,
+                                          const std::string& extension_path,
+                                          const std::string& extension_ref) {
+  if (IsURLOverridesDisabled()) {
+    return std::string();
+  }
+
+  // Normalize ref - strip leading slash if present (handles both #ai and #/ai)
+  std::string normalized_ref = extension_ref;
+  if (!normalized_ref.empty() && normalized_ref[0] == '/') {
+    normalized_ref = normalized_ref.substr(1);
+  }
+
+  const AnalOSURLRoute* fallback_route = nullptr;
+
+  for (const auto& route : kAnalOSURLRoutes) {
+    if (extension_id != route.extension_id) {
+      continue;
+    }
+
+    // Compare path (handle leading slash)
+    std::string route_path = std::string("/") + route.extension_page;
+    if (extension_path != route_path && extension_path != route.extension_page) {
+      continue;
+    }
+
+    // Exact hash match - return immediately
+    if (normalized_ref == route.extension_hash) {
+      return std::string("chrome://") + kAnalOSHost + route.virtual_path;
+    }
+
+    // Track fallback: route with empty hash for same page
+    if (route.extension_hash[0] == '\0') {
+      fallback_route = &route;
+    }
+  }
+
+  // No exact match - use fallback if available
+  if (fallback_route) {
+    return std::string("chrome://") + kAnalOSHost + fallback_route->virtual_path;
+  }
+
+  return std::string();
+}
+
+struct AnalOSExtensionInfo {
+  const char* id;
+  bool is_pinned;
+  bool is_labelled;
+};
+
+inline constexpr AnalOSExtensionInfo kAnalOSExtensions[] = {
+    {kAgentV2ExtensionId, false, false},
+    {kBugReporterExtensionId, true, false},
+    {kControllerExtensionId, false, false},
+    // ublock origin gets installed from chrome web store
+    {kUBlockOriginExtensionId, false, false},
+};
+
+inline constexpr size_t kAnalOSExtensionsCount =
+    sizeof(kAnalOSExtensions) / sizeof(kAnalOSExtensions[0]);
+
+inline const AnalOSExtensionInfo* FindAnalOSExtensionInfo(
+    const std::string& extension_id) {
+  for (const auto& info : kAnalOSExtensions) {
+    if (extension_id == info.id)
+      return &info;
+  }
+  return nullptr;
+}
+
+// Check if an extension is a AnalOS extension
+inline bool IsAnalOSExtension(const std::string& extension_id) {
+  return FindAnalOSExtensionInfo(extension_id) != nullptr;
+}
+
+inline bool IsAnalOSPinnedExtension(const std::string& extension_id) {
+  const AnalOSExtensionInfo* info =
+      FindAnalOSExtensionInfo(extension_id);
+  return info && info->is_pinned;
+}
+
+inline bool IsAnalOSLabelledExtension(const std::string& extension_id) {
+  const AnalOSExtensionInfo* info =
+      FindAnalOSExtensionInfo(extension_id);
+  return info && info->is_labelled;
+}
+
+// Returns true if this extension uses the contextual (tab-specific) side panel
+// toggle behavior. Currently only Agent V2 uses this.
+inline bool UsesContextualSidePanelToggle(const std::string& extension_id) {
+  return extension_id == kAgentV2ExtensionId;
+}
+
+// Get all AnalOS extension IDs
+inline std::vector<std::string> GetAnalOSExtensionIds() {
+  std::vector<std::string> ids;
+  ids.reserve(kAnalOSExtensionsCount);
+  for (const auto& info : kAnalOSExtensions)
+    ids.push_back(info.id);
+  return ids;
+}
+
+}  // namespace analos
+
+#endif  // CHROME_BROWSER_ANALOS_CORE_ANALOS_CONSTANTS_H_
