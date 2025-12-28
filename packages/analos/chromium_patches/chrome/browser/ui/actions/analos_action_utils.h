diff --git a/chrome/browser/ui/actions/analos_action_utils.h b/chrome/browser/ui/actions/analos_action_utils.h
new file mode 100644
index 0000000000000..3d2ec755cd479
--- /dev/null
+++ b/chrome/browser/ui/actions/analos_action_utils.h
@@ -0,0 +1,73 @@
+// Copyright 2025 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#ifndef CHROME_BROWSER_UI_ACTIONS_ANALOS_ACTION_UTILS_H_
+#define CHROME_BROWSER_UI_ACTIONS_ANALOS_ACTION_UTILS_H_
+
+#include <string>
+#include <string_view>
+
+#include "base/containers/contains.h"
+#include "base/containers/fixed_flat_set.h"
+#include "chrome/browser/ui/actions/chrome_action_id.h"
+#include "chrome/browser/ui/ui_features.h"
+#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
+#include "chrome/common/chrome_features.h"
+#include "chrome/browser/analos/core/analos_constants.h"
+#include "ui/actions/actions.h"
+
+namespace analos {
+
+// Native action IDs for AnalOS panels that need special treatment
+// These actions will:
+// - Always be pinned
+// - Show text labels
+// - Have high flex priority (always visible)
+constexpr auto kAnalOSNativeActionIds =
+    base::MakeFixedFlatSet<actions::ActionId>({
+        kActionSidePanelShowThirdPartyLlm,
+        kActionSidePanelShowClashOfGpts,
+        kActionAnalOSAgent,
+    });
+
+// Check if an action ID is a AnalOS action (native or extension)
+inline bool IsAnalOSAction(actions::ActionId id) {
+  // Check native actions
+  if (kAnalOSNativeActionIds.contains(id)) {
+    return true;
+  }
+
+  // Only labelled extensions are considered for AnalOS actions
+  for (const auto& ext_id :
+       analos::GetAnalOSExtensionIds()) {
+    if (!analos::IsAnalOSLabelledExtension(ext_id)) {
+      continue;
+    }
+    auto ext_action_id = actions::ActionIdMap::StringToActionId(
+        SidePanelEntryKey(SidePanelEntryId::kExtension, ext_id)
+            .ToString());
+    if (ext_action_id && id == *ext_action_id) {
+      return true;
+    }
+  }
+
+  return false;
+}
+
+
+// Get the feature flag for a native AnalOS action
+inline const base::Feature* GetFeatureForAnalOSAction(actions::ActionId id) {
+  switch (id) {
+    case kActionSidePanelShowThirdPartyLlm:
+      return &features::kThirdPartyLlmPanel;
+    case kActionSidePanelShowClashOfGpts:
+      return &features::kClashOfGpts;
+    default:
+      return nullptr;
+  }
+}
+
+}  // namespace analos
+
+#endif  // CHROME_BROWSER_UI_ACTIONS_ANALOS_ACTION_UTILS_H_
