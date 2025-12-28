diff --git a/chrome/browser/ui/webui/settings/analos_metrics_handler.cc b/chrome/browser/ui/webui/settings/analos_metrics_handler.cc
new file mode 100644
index 0000000000000..d043fcba5a32c
--- /dev/null
+++ b/chrome/browser/ui/webui/settings/analos_metrics_handler.cc
@@ -0,0 +1,56 @@
+// Copyright 2025 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#include "chrome/browser/ui/webui/settings/analos_metrics_handler.h"
+
+#include "base/logging.h"
+#include "base/values.h"
+#include "chrome/browser/analos/metrics/analos_metrics.h"
+
+namespace settings {
+
+AnalOSMetricsHandler::AnalOSMetricsHandler() = default;
+
+AnalOSMetricsHandler::~AnalOSMetricsHandler() = default;
+
+void AnalOSMetricsHandler::RegisterMessages() {
+  web_ui()->RegisterMessageCallback(
+      "logAnalOSMetric",
+      base::BindRepeating(&AnalOSMetricsHandler::HandleLogAnalOSMetric,
+                         base::Unretained(this)));
+}
+
+void AnalOSMetricsHandler::HandleLogAnalOSMetric(
+    const base::Value::List& args) {
+  if (args.size() < 1 || !args[0].is_string()) {
+    LOG(WARNING) << "analos: Invalid metric event name";
+    return;
+  }
+
+  const std::string& event_name = args[0].GetString();
+  
+  if (args.size() > 1) {
+    // Has properties
+    if (args[1].is_dict()) {
+      base::Value::Dict properties = args[1].GetDict().Clone();
+      analos_metrics::AnalOSMetrics::Log(event_name, std::move(properties));
+    } else {
+      LOG(WARNING) << "analos: Invalid metric properties format";
+      analos_metrics::AnalOSMetrics::Log(event_name);
+    }
+  } else {
+    // No properties
+    analos_metrics::AnalOSMetrics::Log(event_name);
+  }
+}
+
+void AnalOSMetricsHandler::OnJavascriptAllowed() {
+  // No special setup needed
+}
+
+void AnalOSMetricsHandler::OnJavascriptDisallowed() {
+  // No cleanup needed
+}
+
+}  // namespace settings
\ No newline at end of file
