diff --git a/chrome/browser/extensions/api/browser_os/browser_os_api.h b/chrome/browser/extensions/api/browser_os/browser_os_api.h
new file mode 100644
index 0000000000000..5f4276cf89432
--- /dev/null
+++ b/chrome/browser/extensions/api/browser_os/browser_os_api.h
@@ -0,0 +1,344 @@
+// Copyright 2024 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#ifndef CHROME_BROWSER_EXTENSIONS_API_BROWSER_OS_BROWSER_OS_API_H_
+#define CHROME_BROWSER_EXTENSIONS_API_BROWSER_OS_BROWSER_OS_API_H_
+
+#include <cstdint>
+
+#include "base/memory/raw_ptr.h"
+#include "base/values.h"
+#include "chrome/browser/extensions/api/browser_os/browser_os_api_utils.h"
+#include "chrome/browser/extensions/api/browser_os/browser_os_content_processor.h"
+#include "chrome/browser/extensions/api/browser_os/browser_os_snapshot_processor.h"
+#include "extensions/browser/extension_function.h"
+#include "third_party/skia/include/core/SkBitmap.h"
+
+namespace content {
+class WebContents;
+}
+
+namespace ui {
+struct AXTreeUpdate;
+}
+
+namespace extensions {
+namespace api {
+
+
+class AnalOSGetAccessibilityTreeFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getAccessibilityTree",
+                             BROWSER_OS_GETACCESSIBILITYTREE)
+
+  AnalOSGetAccessibilityTreeFunction() = default;
+
+ protected:
+  ~AnalOSGetAccessibilityTreeFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+
+ private:
+  void OnAccessibilityTreeReceived(ui::AXTreeUpdate& tree_update);
+};
+
+class AnalOSGetInteractiveSnapshotFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getInteractiveSnapshot",
+                             BROWSER_OS_GETINTERACTIVESNAPSHOT)
+
+  AnalOSGetInteractiveSnapshotFunction();
+
+ protected:
+  ~AnalOSGetInteractiveSnapshotFunction() override;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+
+ private:
+  void OnAccessibilityTreeReceived(ui::AXTreeUpdate& tree_update);
+  void OnSnapshotProcessed(SnapshotProcessingResult result);
+  
+  // Counter for snapshot IDs
+  static uint32_t next_snapshot_id_;
+  
+  // Tab ID for storing mappings
+  int tab_id_ = -1;
+  
+  // Web contents for processing and drawing
+  raw_ptr<content::WebContents> web_contents_ = nullptr;
+};
+
+class AnalOSClickFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.click", BROWSER_OS_CLICK)
+
+  AnalOSClickFunction() = default;
+
+ protected:
+  ~AnalOSClickFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSInputTextFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.inputText", BROWSER_OS_INPUTTEXT)
+
+  AnalOSInputTextFunction() = default;
+
+ protected:
+  ~AnalOSInputTextFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSClearFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.clear", BROWSER_OS_CLEAR)
+
+  AnalOSClearFunction() = default;
+
+ protected:
+  ~AnalOSClearFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSGetPageLoadStatusFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getPageLoadStatus", 
+                             BROWSER_OS_GETPAGELOADSTATUS)
+
+  AnalOSGetPageLoadStatusFunction() = default;
+
+ protected:
+  ~AnalOSGetPageLoadStatusFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSScrollUpFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.scrollUp", BROWSER_OS_SCROLLUP)
+
+  AnalOSScrollUpFunction() = default;
+
+ protected:
+  ~AnalOSScrollUpFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSScrollDownFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.scrollDown", BROWSER_OS_SCROLLDOWN)
+
+  AnalOSScrollDownFunction() = default;
+
+ protected:
+  ~AnalOSScrollDownFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSScrollToNodeFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.scrollToNode", BROWSER_OS_SCROLLTONODE)
+
+  AnalOSScrollToNodeFunction() = default;
+
+ protected:
+  ~AnalOSScrollToNodeFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSSendKeysFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.sendKeys", BROWSER_OS_SENDKEYS)
+
+  AnalOSSendKeysFunction() = default;
+
+ protected:
+  ~AnalOSSendKeysFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSCaptureScreenshotFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.captureScreenshot", BROWSER_OS_CAPTURESCREENSHOT)
+
+  AnalOSCaptureScreenshotFunction();
+
+ protected:
+  ~AnalOSCaptureScreenshotFunction() override;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+  
+ private:
+  void DrawHighlightsAndCapture();
+  void CaptureScreenshotNow();
+  void OnScreenshotCaptured(const SkBitmap& bitmap);
+  
+  // Store web contents and tab id for highlight operations
+  raw_ptr<content::WebContents> web_contents_ = nullptr;
+  int tab_id_ = -1;
+  gfx::Size target_size_;
+  bool show_highlights_ = false;
+  bool use_exact_dimensions_ = false;
+};
+
+class AnalOSGetSnapshotFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getSnapshot", BROWSER_OS_GETSNAPSHOT)
+
+  AnalOSGetSnapshotFunction() = default;
+
+ protected:
+  ~AnalOSGetSnapshotFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+
+ private:
+  void OnAccessibilityTreeReceived(ui::AXTreeUpdate& tree_update);
+};
+
+// Settings API functions
+class AnalOSGetPrefFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getPref", BROWSER_OS_GETPREF)
+
+  AnalOSGetPrefFunction() = default;
+
+ protected:
+  ~AnalOSGetPrefFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSSetPrefFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.setPref", BROWSER_OS_SETPREF)
+
+  AnalOSSetPrefFunction() = default;
+
+ protected:
+  ~AnalOSSetPrefFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSGetAllPrefsFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getAllPrefs", BROWSER_OS_GETALLPREFS)
+
+  AnalOSGetAllPrefsFunction() = default;
+
+ protected:
+  ~AnalOSGetAllPrefsFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSLogMetricFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.logMetric", BROWSER_OS_LOGMETRIC)
+
+  AnalOSLogMetricFunction() = default;
+
+ protected:
+  ~AnalOSLogMetricFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSGetVersionNumberFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getVersionNumber", BROWSER_OS_GETVERSIONNUMBER)
+
+  AnalOSGetVersionNumberFunction() = default;
+
+ protected:
+  ~AnalOSGetVersionNumberFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSGetAnalosVersionNumberFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.getAnalosVersionNumber", BROWSER_OS_GETANALOSVERSIONNUMBER)
+
+  AnalOSGetAnalosVersionNumberFunction() = default;
+
+ protected:
+  ~AnalOSGetAnalosVersionNumberFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSExecuteJavaScriptFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.executeJavaScript", BROWSER_OS_EXECUTEJAVASCRIPT)
+
+  AnalOSExecuteJavaScriptFunction() = default;
+
+ protected:
+  ~AnalOSExecuteJavaScriptFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+  
+ private:
+  void OnJavaScriptExecuted(base::Value result);
+};
+
+class AnalOSClickCoordinatesFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.clickCoordinates", BROWSER_OS_CLICKCOORDINATES)
+
+  AnalOSClickCoordinatesFunction() = default;
+
+ protected:
+  ~AnalOSClickCoordinatesFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+class AnalOSTypeAtCoordinatesFunction : public ExtensionFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("analOS.typeAtCoordinates", BROWSER_OS_TYPEATCOORDINATES)
+
+  AnalOSTypeAtCoordinatesFunction() = default;
+
+ protected:
+  ~AnalOSTypeAtCoordinatesFunction() override = default;
+
+  // ExtensionFunction:
+  ResponseAction Run() override;
+};
+
+}  // namespace api
+}  // namespace extensions
+
+#endif  // CHROME_BROWSER_EXTENSIONS_API_BROWSER_OS_BROWSER_OS_API_H_
\ No newline at end of file
