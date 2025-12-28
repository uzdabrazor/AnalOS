diff --git a/chrome/browser/analos/metrics/analos_metrics_prefs.h b/chrome/browser/analos/metrics/analos_metrics_prefs.h
new file mode 100644
index 0000000000000..4600e0c848552
--- /dev/null
+++ b/chrome/browser/analos/metrics/analos_metrics_prefs.h
@@ -0,0 +1,24 @@
+// Copyright 2025 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#ifndef CHROME_BROWSER_ANALOS_METRICS_ANALOS_METRICS_PREFS_H_
+#define CHROME_BROWSER_ANALOS_METRICS_ANALOS_METRICS_PREFS_H_
+
+class PrefRegistrySimple;
+
+namespace user_prefs {
+class PrefRegistrySyncable;
+}  // namespace user_prefs
+
+namespace analos_metrics {
+
+// Registers AnalOS metrics preferences for the profile.
+void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
+
+// Registers AnalOS metrics preferences for local state.
+void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
+
+}  // namespace analos_metrics
+
+#endif  // CHROME_BROWSER_ANALOS_METRICS_ANALOS_METRICS_PREFS_H_
