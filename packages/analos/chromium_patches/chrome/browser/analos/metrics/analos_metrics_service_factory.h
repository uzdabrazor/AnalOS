diff --git a/chrome/browser/analos/metrics/analos_metrics_service_factory.h b/chrome/browser/analos/metrics/analos_metrics_service_factory.h
new file mode 100644
index 0000000000000..2caddc7598a43
--- /dev/null
+++ b/chrome/browser/analos/metrics/analos_metrics_service_factory.h
@@ -0,0 +1,48 @@
+// Copyright 2025 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#ifndef CHROME_BROWSER_ANALOS_METRICS_ANALOS_METRICS_SERVICE_FACTORY_H_
+#define CHROME_BROWSER_ANALOS_METRICS_ANALOS_METRICS_SERVICE_FACTORY_H_
+
+#include "base/no_destructor.h"
+#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
+
+namespace content {
+class BrowserContext;
+}  // namespace content
+
+namespace analos_metrics {
+
+class AnalOSMetricsService;
+
+// Factory for creating AnalOSMetricsService instances per profile.
+class AnalOSMetricsServiceFactory
+    : public BrowserContextKeyedServiceFactory {
+ public:
+  AnalOSMetricsServiceFactory(const AnalOSMetricsServiceFactory&) =
+      delete;
+  AnalOSMetricsServiceFactory& operator=(
+      const AnalOSMetricsServiceFactory&) = delete;
+
+  // Returns the AnalOSMetricsService for |context|, creating one if needed.
+  static AnalOSMetricsService* GetForBrowserContext(
+      content::BrowserContext* context);
+
+  // Returns the singleton factory instance.
+  static AnalOSMetricsServiceFactory* GetInstance();
+
+ private:
+  friend base::NoDestructor<AnalOSMetricsServiceFactory>;
+
+  AnalOSMetricsServiceFactory();
+  ~AnalOSMetricsServiceFactory() override;
+
+  // BrowserContextKeyedServiceFactory:
+  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
+      content::BrowserContext* context) const override;
+};
+
+}  // namespace analos_metrics
+
+#endif  // CHROME_BROWSER_ANALOS_METRICS_ANALOS_METRICS_SERVICE_FACTORY_H_
