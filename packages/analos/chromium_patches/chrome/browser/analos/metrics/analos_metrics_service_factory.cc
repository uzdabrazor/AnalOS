diff --git a/chrome/browser/analos/metrics/analos_metrics_service_factory.cc b/chrome/browser/analos/metrics/analos_metrics_service_factory.cc
new file mode 100644
index 0000000000000..794961e6e39aa
--- /dev/null
+++ b/chrome/browser/analos/metrics/analos_metrics_service_factory.cc
@@ -0,0 +1,58 @@
+// Copyright 2025 The Chromium Authors
+// Use of this source code is governed by a BSD-style license that can be
+// found in the LICENSE file.
+
+#include "chrome/browser/analos/metrics/analos_metrics_service_factory.h"
+
+#include <memory>
+
+#include "base/no_destructor.h"
+#include "chrome/browser/browser_process.h"
+#include "chrome/browser/analos/metrics/analos_metrics_service.h"
+#include "chrome/browser/profiles/profile.h"
+#include "components/keyed_service/content/browser_context_dependency_manager.h"
+#include "components/prefs/pref_service.h"
+#include "content/public/browser/browser_context.h"
+#include "content/public/browser/storage_partition.h"
+
+namespace analos_metrics {
+
+// static
+AnalOSMetricsService* AnalOSMetricsServiceFactory::GetForBrowserContext(
+    content::BrowserContext* context) {
+  return static_cast<AnalOSMetricsService*>(
+      GetInstance()->GetServiceForBrowserContext(context, true));
+}
+
+// static
+AnalOSMetricsServiceFactory*
+AnalOSMetricsServiceFactory::GetInstance() {
+  static base::NoDestructor<AnalOSMetricsServiceFactory> instance;
+  return instance.get();
+}
+
+AnalOSMetricsServiceFactory::AnalOSMetricsServiceFactory()
+    : BrowserContextKeyedServiceFactory(
+          "AnalOSMetricsService",
+          BrowserContextDependencyManager::GetInstance()) {}
+
+AnalOSMetricsServiceFactory::~AnalOSMetricsServiceFactory() = default;
+
+std::unique_ptr<KeyedService>
+AnalOSMetricsServiceFactory::BuildServiceInstanceForBrowserContext(
+    content::BrowserContext* context) const {
+  Profile* profile = Profile::FromBrowserContext(context);
+
+  // Don't create service for incognito profiles
+  if (profile->IsOffTheRecord()) {
+    return nullptr;
+  }
+
+  return std::make_unique<AnalOSMetricsService>(
+      profile->GetPrefs(),
+      g_browser_process->local_state(),
+      profile->GetDefaultStoragePartition()
+          ->GetURLLoaderFactoryForBrowserProcess());
+}
+
+}  // namespace analos_metrics
