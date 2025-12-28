diff --git a/chrome/browser/extensions/api/side_panel/side_panel_api.h b/chrome/browser/extensions/api/side_panel/side_panel_api.h
index 72a88888eb9fc..3f0779a57b615 100644
--- a/chrome/browser/extensions/api/side_panel/side_panel_api.h
+++ b/chrome/browser/extensions/api/side_panel/side_panel_api.h
@@ -115,6 +115,36 @@ class SidePanelCloseFunction : public SidePanelApiFunction {
   ResponseAction RunFunction() override;
 };
 
+class SidePanelAnalosToggleFunction : public SidePanelApiFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("sidePanel.analosToggle",
+                             SIDEPANEL_ANALOSTOGGLE)
+  SidePanelAnalosToggleFunction() = default;
+  SidePanelAnalosToggleFunction(const SidePanelAnalosToggleFunction&) =
+      delete;
+  SidePanelAnalosToggleFunction& operator=(
+      const SidePanelAnalosToggleFunction&) = delete;
+
+ private:
+  ~SidePanelAnalosToggleFunction() override = default;
+  ResponseAction RunFunction() override;
+};
+
+class SidePanelAnalosIsOpenFunction : public SidePanelApiFunction {
+ public:
+  DECLARE_EXTENSION_FUNCTION("sidePanel.analosIsOpen",
+                             SIDEPANEL_ANALOSISOPEN)
+  SidePanelAnalosIsOpenFunction() = default;
+  SidePanelAnalosIsOpenFunction(const SidePanelAnalosIsOpenFunction&) =
+      delete;
+  SidePanelAnalosIsOpenFunction& operator=(
+      const SidePanelAnalosIsOpenFunction&) = delete;
+
+ private:
+  ~SidePanelAnalosIsOpenFunction() override = default;
+  ResponseAction RunFunction() override;
+};
+
 }  // namespace extensions
 
 #endif  // CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_API_H_
