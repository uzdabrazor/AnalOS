diff --git a/chrome/browser/extensions/external_provider_impl.cc b/chrome/browser/extensions/external_provider_impl.cc
index 9c8731d3ed4ab..d8eb2512ddb15 100644
--- a/chrome/browser/extensions/external_provider_impl.cc
+++ b/chrome/browser/extensions/external_provider_impl.cc
@@ -30,6 +30,8 @@
 #include "chrome/browser/browser_features.h"
 #include "chrome/browser/browser_process.h"
 #include "chrome/browser/browser_process_platform_part.h"
+#include "chrome/browser/analos/core/analos_switches.h"
+#include "chrome/browser/extensions/analos_external_loader.h"
 #include "chrome/browser/extensions/extension_management.h"
 #include "chrome/browser/extensions/extension_migrator.h"
 #include "chrome/browser/extensions/external_component_loader.h"
@@ -915,6 +916,33 @@ void ExternalProviderImpl::CreateExternalProviders(
     provider_list->push_back(std::move(initial_external_extensions_provider));
   }
 #endif  // BUILDFLAG(ENABLE_EXTENSIONS)
+
+  // Add AnalOS external extension loader
+  // This loader fetches extension configuration from a remote URL
+  // Enabled by default for all profiles
+  auto analos_loader = base::MakeRefCounted<AnalOSExternalLoader>(profile);
+
+  // Allow custom config URL via command line
+  if (base::CommandLine::ForCurrentProcess()->HasSwitch(analos::kExtensionsUrl)) {
+    std::string config_url = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(analos::kExtensionsUrl);
+    GURL url(config_url);
+    if (url.is_valid()) {
+      analos_loader->SetConfigUrl(url);
+    }
+  }
+
+  // Allow disabling via command line flag if needed
+  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(analos::kDisableExtensions)) {
+    auto analos_provider = std::make_unique<ExternalProviderImpl>(
+        service, analos_loader, profile,
+        ManifestLocation::kInvalidLocation,
+        ManifestLocation::kExternalComponent,
+        Extension::WAS_INSTALLED_BY_DEFAULT);
+    analos_provider->set_auto_acknowledge(true);
+    analos_provider->set_allow_updates(true);
+    analos_provider->set_install_immediately(true);
+    provider_list->push_back(std::move(analos_provider));
+  }
 }
 
 }  // namespace extensions
