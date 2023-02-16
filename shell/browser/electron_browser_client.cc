// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/electron_browser_client.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>
#endif

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_hints/common/network_hints.mojom.h"
#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"  // nogncheck
#include "content/browser/site_instance_impl.h"  // nogncheck
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "crypto/crypto_buildflags.h"
#include "electron/buildflags/buildflags.h"
#include "electron/grit/electron_resources.h"
#include "electron/shell/common/api/api.mojom.h"
#include "extensions/browser/api/messaging/messaging_api_message_filter.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/base/escape.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ppapi/buildflags/buildflags.h"
#include "ppapi/host/ppapi_host.h"
#include "printing/buildflags/buildflags.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "shell/app/electron_crash_reporter_client.h"
#include "shell/browser/api/electron_api_app.h"
#include "shell/browser/api/electron_api_crash_reporter.h"
#include "shell/browser/api/electron_api_protocol.h"
#include "shell/browser/api/electron_api_session.h"
#include "shell/browser/api/electron_api_web_contents.h"
#include "shell/browser/api/electron_api_web_request.h"
#include "shell/browser/badging/badge_manager.h"
#include "shell/browser/child_web_contents_tracker.h"
#include "shell/browser/electron_api_ipc_handler_impl.h"
#include "shell/browser/electron_autofill_driver_factory.h"
#include "shell/browser/electron_browser_context.h"
#include "shell/browser/electron_browser_main_parts.h"
#include "shell/browser/electron_navigation_throttle.h"
#include "shell/browser/electron_quota_permission_context.h"
#include "shell/browser/electron_speech_recognition_manager_delegate.h"
#include "shell/browser/electron_web_contents_utility_handler_impl.h"
#include "shell/browser/font_defaults.h"
#include "shell/browser/javascript_environment.h"
#include "shell/browser/media/media_capture_devices_dispatcher.h"
#include "shell/browser/native_window.h"
#include "shell/browser/net/network_context_service.h"
#include "shell/browser/net/network_context_service_factory.h"
#include "shell/browser/net/proxying_url_loader_factory.h"
#include "shell/browser/net/proxying_websocket.h"
#include "shell/browser/net/system_network_context_manager.h"
#include "shell/browser/network_hints_handler_impl.h"
#include "shell/browser/notifications/notification_presenter.h"
#include "shell/browser/notifications/platform_notification_service.h"
#include "shell/browser/protocol_registry.h"
#include "shell/browser/serial/electron_serial_delegate.h"
#include "shell/browser/session_preferences.h"
#include "shell/browser/ui/devtools_manager_delegate.h"
#include "shell/browser/web_contents_permission_helper.h"
#include "shell/browser/web_contents_preferences.h"
#include "shell/browser/window_list.h"
#include "shell/common/api/api.mojom.h"
#include "shell/common/application_info.h"
#include "shell/common/electron_paths.h"
#include "shell/common/logging.h"
#include "shell/browser/crypto_module_delegate_nss.h"
#include "shell/common/options_switches.h"
#include "shell/common/platform_util.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/native_theme/native_theme.h"
#include "v8/include/v8.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#endif

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#elif BUILDFLAG(IS_WIN)
#include "net/ssl/client_cert_store_win.h"
#elif BUILDFLAG(IS_MAC)
#include "net/ssl/client_cert_store_mac.h"
#elif defined(USE_OPENSSL)
#include "net/ssl/client_cert_store.h"
#endif

#if BUILDFLAG(ENABLE_BUILTIN_SPELLCHECKER)
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"  // nogncheck
#include "components/spellcheck/common/spellcheck.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(OVERRIDE_LOCATION_PROVIDER)
#include "shell/browser/fake_location_provider.h"
#endif  // BUILDFLAG(OVERRIDE_LOCATION_PROVIDER)

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/extensions_guest_view.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "shell/browser/extensions/electron_extension_message_filter.h"
#include "shell/browser/extensions/electron_extension_system.h"
#include "shell/browser/extensions/electron_extension_web_contents_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"  // nogncheck
#include "shell/browser/plugins/plugin_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/common/mac_helpers.h"
#include "content/public/common/child_process_host.h"
#endif

#if BUILDFLAG(IS_LINUX) && !defined(MAS_BUILD)
#include "base/debug/leak_annotations.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "components/crash/core/app/breakpad_linux.h"  // nogncheck
#include "components/crash/core/app/crash_switches.h"  // nogncheck
#include "components/crash/core/app/crashpad.h"        // nogncheck
#endif

#if BUILDFLAG(ENABLE_PICTURE_IN_PICTURE) && BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/overlay/document_overlay_window_views.h"
#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "shell/browser/browser.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/win/shell.h"
#include "ui/views/widget/widget.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "shell/browser/printing/print_view_manager_electron.h"
#endif

#if BUILDFLAG(ENABLE_PDF_VIEWER)
#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"
#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"  // nogncheck
#include "components/pdf/browser/pdf_navigation_throttle.h"
#include "components/pdf/browser/pdf_url_loader_request_interceptor.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"  // nogncheck
#include "components/pdf/common/internal_plugin_helpers.h"
#endif

using content::BrowserThread;

namespace electron {

namespace {

ElectronBrowserClient* g_browser_client = nullptr;

base::LazyInstance<std::string>::DestructorAtExit
    g_io_thread_application_locale = LAZY_INSTANCE_INITIALIZER;

base::NoDestructor<std::string> g_application_locale;

void SetApplicationLocaleOnIOThread(const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_io_thread_application_locale.Get() = locale;
}

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  NetworkHintsHandlerImpl::Create(frame_host, std::move(receiver));
}

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
// Used by the GetPrivilegeRequiredByUrl() and GetProcessPrivilege() functions
// below.  Extension, and isolated apps require different privileges to be
// granted to their RenderProcessHosts.  This classification allows us to make
// sure URLs are served by hosts with the right set of privileges.
enum class RenderProcessHostPrivilege {
  kNormal,
  kHosted,
  kIsolated,
  kExtension,
};

// Copied from chrome/browser/extensions/extension_util.cc.
bool AllowFileAccess(const std::string& extension_id,
                     content::BrowserContext* context) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             ::switches::kDisableExtensionsFileAccessCheck) ||
         extensions::ExtensionPrefs::Get(context)->AllowFileAccess(
             extension_id);
}

RenderProcessHostPrivilege GetPrivilegeRequiredByUrl(
    const GURL& url,
    extensions::ExtensionRegistry* registry) {
  // Default to a normal renderer cause it is lower privileged. This should only
  // occur if the URL on a site instance is either malformed, or uninitialized.
  // If it is malformed, then there is no need for better privileges anyways.
  // If it is uninitialized, but eventually settles on being an a scheme other
  // than normal webrenderer, the navigation logic will correct us out of band
  // anyways.
  if (!url.is_valid())
    return RenderProcessHostPrivilege::kNormal;

  if (!url.SchemeIs(extensions::kExtensionScheme))
    return RenderProcessHostPrivilege::kNormal;

  return RenderProcessHostPrivilege::kExtension;
}

RenderProcessHostPrivilege GetProcessPrivilege(
    content::RenderProcessHost* process_host,
    extensions::ProcessMap* process_map,
    extensions::ExtensionRegistry* registry) {
  std::set<std::string> extension_ids =
      process_map->GetExtensionsInProcess(process_host->GetID());
  if (extension_ids.empty())
    return RenderProcessHostPrivilege::kNormal;

  return RenderProcessHostPrivilege::kExtension;
}

const extensions::Extension* GetEnabledExtensionFromEffectiveURL(
    content::BrowserContext* context,
    const GURL& effective_url) {
  if (!effective_url.SchemeIs(extensions::kExtensionScheme))
    return nullptr;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);
  if (!registry)
    return nullptr;

  return registry->enabled_extensions().GetByID(effective_url.host());
}
#endif  // BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)

#if BUILDFLAG(IS_LINUX)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path;
  base::PathService::Get(electron::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    bool upload = ElectronCrashReporterClient::Get()->GetCollectStatsConsent();
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(process_type, dumps_path, upload);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  if (crash_reporter::IsCrashpadEnabled()) {
    int fd;
    pid_t pid;
    return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
  }

  // Extensions have the same process type as renderers.
  if (command_line.HasSwitch(extensions::switches::kExtensionProcess)) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost("extension");
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(::switches::kProcessType);

  if (process_type == ::switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == ::switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == ::switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == ::switches::kUtilityProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // BUILDFLAG(IS_LINUX)

void MaybeAppendSecureOriginsAllowlistSwitch(base::CommandLine* cmdline) {
  // |allowlist| combines pref/policy + cmdline switch in the browser process.
  // For renderer and utility (e.g. NetworkService) processes the switch is the
  // only available source, so below the combined (pref/policy + cmdline)
  // allowlist of secure origins is injected into |cmdline| for these other
  // processes.
  std::vector<std::string> allowlist =
      network::SecureOriginAllowlist::GetInstance().GetCurrentAllowlist();
  if (!allowlist.empty()) {
    cmdline->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        base::JoinString(allowlist, ","));
  }
}

}  // namespace

// static
ElectronBrowserClient* ElectronBrowserClient::Get() {
  return g_browser_client;
}

// static
void ElectronBrowserClient::SetApplicationLocale(const std::string& locale) {
  if (!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
      !base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&SetApplicationLocaleOnIOThread, locale))) {
    g_io_thread_application_locale.Get() = locale;
  }
  *g_application_locale = locale;
}

ElectronBrowserClient::ElectronBrowserClient() {
  DCHECK(!g_browser_client);
  g_browser_client = this;
}

ElectronBrowserClient::~ElectronBrowserClient() {
  DCHECK(g_browser_client);
  g_browser_client = nullptr;
}

content::WebContents* ElectronBrowserClient::GetWebContentsFromProcessID(
    int process_id) {
  // If the process is a pending process, we should use the web contents
  // for the frame host passed into RegisterPendingProcess.
  const auto iter = pending_processes_.find(process_id);
  if (iter != std::end(pending_processes_))
    return iter->second;

  // Certain render process will be created with no associated render view,
  // for example: ServiceWorker.
  return WebContentsPreferences::GetWebContentsFromProcessID(process_id);
}

content::SiteInstance* ElectronBrowserClient::GetSiteInstanceFromAffinity(
    content::BrowserContext* browser_context,
    const GURL& url,
    content::RenderFrameHost* rfh) const {
  return nullptr;
}

bool ElectronBrowserClient::IsRendererSubFrame(int process_id) const {
  return base::Contains(renderer_is_subframe_, process_id);
}

void ElectronBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  // When a render process is crashed, it might be reused.
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  int process_id = host->GetID();

  auto* browser_context = host->GetBrowserContext();

  host->AddFilter(
      new extensions::ExtensionMessageFilter(process_id, browser_context));
  host->AddFilter(
      new ElectronExtensionMessageFilter(process_id, browser_context));
  host->AddFilter(
      new extensions::MessagingAPIMessageFilter(process_id, browser_context));
#endif

  // Remove in case the host is reused after a crash, otherwise noop.
  host->RemoveObserver(this);

  // ensure the ProcessPreferences is removed later
  host->AddObserver(this);
}

content::SpeechRecognitionManagerDelegate*
ElectronBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new ElectronSpeechRecognitionManagerDelegate;
}

content::TtsPlatform* ElectronBrowserClient::GetTtsPlatform() {
  return nullptr;
}

void ElectronBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  prefs->javascript_enabled = true;
  prefs->web_security_enabled = true;
  prefs->plugins_enabled = true;
  prefs->dom_paste_enabled = true;
  prefs->allow_scripts_to_close_windows = true;
  prefs->javascript_can_access_clipboard = true;
  prefs->local_storage_enabled = true;
  prefs->databases_enabled = true;
  prefs->allow_universal_access_from_file_urls = true;
  prefs->allow_file_access_from_file_urls = true;
  prefs->webgl1_enabled = true;
  prefs->webgl2_enabled = true;
  prefs->allow_running_insecure_content = false;
  prefs->default_minimum_page_scale_factor = 1.f;
  prefs->default_maximum_page_scale_factor = 1.f;
  prefs->navigate_on_drag_drop = false;
#if !BUILDFLAG(ENABLE_PICTURE_IN_PICTURE)
  prefs->picture_in_picture_enabled = false;
#endif

  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  prefs->preferred_color_scheme =
      native_theme->ShouldUseDarkColors()
          ? blink::mojom::PreferredColorScheme::kDark
          : blink::mojom::PreferredColorScheme::kLight;

  auto preloads =
      SessionPreferences::GetValidPreloads(web_contents->GetBrowserContext());
  if (!preloads.empty())
    prefs->preloads = preloads;

  SetFontDefaults(prefs);

  // Custom preferences of guest page.
  auto* web_preferences = WebContentsPreferences::From(web_contents);
  if (web_preferences) {
    web_preferences->OverrideWebkitPrefs(prefs);
  }
}

void ElectronBrowserClient::RegisterPendingSiteInstance(
    content::RenderFrameHost* rfh,
    content::SiteInstance* pending_site_instance) {
  // Remember the original web contents for the pending renderer process.
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  auto* pending_process = pending_site_instance->GetProcess();
  pending_processes_[pending_process->GetID()] = web_contents;

  if (rfh->GetParent())
    renderer_is_subframe_.insert(pending_process->GetID());
  else
    renderer_is_subframe_.erase(pending_process->GetID());
}

void ElectronBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int process_id) {
  // Make sure we're about to launch a known executable
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath child_path;
    base::FilePath program =
        base::MakeAbsoluteFilePath(command_line->GetProgram());
#if BUILDFLAG(IS_MAC)
    auto renderer_child_path = content::ChildProcessHost::GetChildPath(
        content::ChildProcessHost::CHILD_RENDERER);
    auto gpu_child_path = content::ChildProcessHost::GetChildPath(
        content::ChildProcessHost::CHILD_GPU);
#if BUILDFLAG(ENABLE_PLUGINS)
    auto plugin_child_path = content::ChildProcessHost::GetChildPath(
        content::ChildProcessHost::CHILD_PLUGIN);
#endif
    if (program != renderer_child_path && program != gpu_child_path
#if BUILDFLAG(ENABLE_PLUGINS)
        && program != plugin_child_path
#endif
    ) {
      child_path = content::ChildProcessHost::GetChildPath(
          content::ChildProcessHost::CHILD_NORMAL);
      CHECK_EQ(program, child_path)
          << "Aborted from launching unexpected helper executable";
    }
#else
    if (!base::PathService::Get(content::CHILD_PROCESS_EXE, &child_path)) {
      CHECK(false) << "Unable to get child process binary name.";
    }
    SCOPED_CRASH_KEY_STRING256("ChildProcess", "child_process_exe",
                               child_path.AsUTF8Unsafe());
    SCOPED_CRASH_KEY_STRING256("ChildProcess", "program",
                               program.AsUTF8Unsafe());
    CHECK_EQ(program, child_path);
#endif
  }

  std::string process_type =
      command_line->GetSwitchValueASCII(::switches::kProcessType);

#if BUILDFLAG(IS_LINUX)
  bool enable_crash_reporter = false;
  if (crash_reporter::IsCrashpadEnabled()) {
    command_line->AppendSwitch(::switches::kEnableCrashpad);
    enable_crash_reporter = true;

    int fd;
    pid_t pid;

    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
  } else {
    enable_crash_reporter = breakpad::IsCrashReporterEnabled();
  }

  // Zygote Process gets booted before any JS runs, accessing GetClientId
  // will end up touching DIR_USER_DATA path provider and this will
  // configure default value because app.name from browser_init has
  // not run yet.
  if (enable_crash_reporter && process_type != ::switches::kZygoteProcess) {
    std::string switch_value =
        api::crash_reporter::GetClientId() + ",no_channel";
    command_line->AppendSwitchASCII(::switches::kEnableCrashReporter,
                                    switch_value);
    if (!crash_reporter::IsCrashpadEnabled()) {
      for (const auto& pair : api::crash_reporter::GetGlobalCrashKeys()) {
        if (!switch_value.empty())
          switch_value += ",";
        switch_value += pair.first;
        switch_value += "=";
        switch_value += pair.second;
      }
      command_line->AppendSwitchASCII(switches::kGlobalCrashKeys, switch_value);
    }
  }
#endif

  // The zygote process is booted before JS runs, so DIR_USER_DATA isn't usable
  // at that time. It doesn't need --user-data-dir to be correct anyway, since
  // the zygote itself doesn't access anything in that directory.
  if (process_type != ::switches::kZygoteProcess) {
    base::FilePath user_data_dir;
    if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      command_line->AppendSwitchPath(::switches::kUserDataDir, user_data_dir);
  }

  if (process_type == ::switches::kUtilityProcess ||
      process_type == ::switches::kRendererProcess) {
    // Copy following switches to child process.
    static const char* const kCommonSwitchNames[] = {
        switches::kStandardSchemes,      switches::kEnableSandbox,
        switches::kSecureSchemes,        switches::kBypassCSPSchemes,
        switches::kCORSSchemes,          switches::kFetchSchemes,
        switches::kServiceWorkerSchemes, switches::kStreamingSchemes};
    command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                   kCommonSwitchNames,
                                   std::size(kCommonSwitchNames));
    if (process_type == ::switches::kUtilityProcess ||
        content::RenderProcessHost::FromID(process_id)) {
      MaybeAppendSecureOriginsAllowlistSwitch(command_line);
    }
  }

  if (process_type == ::switches::kRendererProcess) {
#if BUILDFLAG(IS_WIN)
    // Append --app-user-model-id.
    PWSTR current_app_id;
    if (SUCCEEDED(GetCurrentProcessExplicitAppUserModelID(&current_app_id))) {
      command_line->AppendSwitchNative(switches::kAppUserModelId,
                                       current_app_id);
      CoTaskMemFree(current_app_id);
    }
#endif

    if (delegate_) {
      auto app_path = static_cast<api::App*>(delegate_)->GetAppPath();
      command_line->AppendSwitchPath(switches::kAppPath, app_path);
    }

    auto env = base::Environment::Create();
    if (env->HasVar("ELECTRON_PROFILE_INIT_SCRIPTS")) {
      command_line->AppendSwitch("profile-electron-init");
    }

    // Extension background pages don't have WebContentsPreferences, but they
    // support WebSQL by default.
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(process_id);
    if (extensions::ProcessMap::Get(process->GetBrowserContext())
            ->Contains(process_id))
      command_line->AppendSwitch(switches::kEnableWebSQL);
#endif

    content::WebContents* web_contents =
        GetWebContentsFromProcessID(process_id);
    if (web_contents) {
      auto* web_preferences = WebContentsPreferences::From(web_contents);
      if (web_preferences)
        web_preferences->AppendCommandLineSwitches(
            command_line, IsRendererSubFrame(process_id));
    }
  }
}

void ElectronBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* host) {}

// attempt to get api key from env
std::string ElectronBrowserClient::GetGeolocationApiKey() {
  auto env = base::Environment::Create();
  std::string api_key;
  env->GetVar("GOOGLE_API_KEY", &api_key);
  return api_key;
}

scoped_refptr<content::QuotaPermissionContext>
ElectronBrowserClient::CreateQuotaPermissionContext() {
  return base::MakeRefCounted<ElectronQuotaPermissionContext>();
}

content::GeneratedCodeCacheSettings
ElectronBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // TODO(deepak1556): Use platform cache directory.
  base::FilePath cache_path = context->GetPath();
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return content::GeneratedCodeCacheSettings(true, 0, cache_path);
}

void ElectronBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  if (delegate_) {
    delegate_->AllowCertificateError(web_contents, cert_error, ssl_info,
                                     request_url, is_main_frame_request,
                                     strict_enforcement, std::move(callback));
  }
}

base::OnceClosure ElectronBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  if (!client_certs.empty() && delegate_) {
    delegate_->SelectClientCertificate(web_contents, cert_request_info,
                                       std::move(client_certs),
                                       std::move(delegate));
  }
  return base::OnceClosure();
}

bool ElectronBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    const std::string& raw_features,
    const scoped_refptr<network::ResourceRequestBody>& body,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);
  WebContentsPreferences* prefs = WebContentsPreferences::From(web_contents);
  if (prefs) {
    if (prefs->ShouldDisablePopups()) {
      // <webview> without allowpopups attribute should return
      // null from window.open calls
      return false;
    } else {
      *no_javascript_access = false;
      return true;
    }
  }

  if (delegate_) {
    return delegate_->CanCreateWindow(
        opener, opener_url, opener_top_level_frame_url, source_origin,
        container_type, target_url, referrer, frame_name, disposition, features,
        raw_features, body, user_gesture, opener_suppressed,
        no_javascript_access);
  }

  return false;
}

#if BUILDFLAG(ENABLE_PICTURE_IN_PICTURE)
std::unique_ptr<content::VideoOverlayWindow>
ElectronBrowserClient::CreateWindowForVideoPictureInPicture(
    content::VideoPictureInPictureWindowController* controller) {
  auto overlay_window = content::VideoOverlayWindow::Create(controller);
#if BUILDFLAG(IS_WIN)
  std::wstring app_user_model_id = Browser::Get()->GetAppUserModelID();
  if (!app_user_model_id.empty()) {
    auto* overlay_window_view =
        static_cast<VideoOverlayWindowViews*>(overlay_window.get());
    ui::win::SetAppIdForWindow(app_user_model_id,
                               overlay_window_view->GetNativeWindow()
                                   ->GetHost()
                                   ->GetAcceleratedWidget());
  }
#endif
  return overlay_window;
}

std::unique_ptr<content::DocumentOverlayWindow>
ElectronBrowserClient::CreateWindowForDocumentPictureInPicture(
    content::DocumentPictureInPictureWindowController* controller) {
  auto overlay_window = content::DocumentOverlayWindow::Create(controller);
#if BUILDFLAG(IS_WIN)
  std::wstring app_user_model_id = Browser::Get()->GetAppUserModelID();
  if (!app_user_model_id.empty()) {
    auto* overlay_window_view =
        static_cast<DocumentOverlayWindowViews*>(overlay_window.get());
    ui::win::SetAppIdForWindow(app_user_model_id,
                               overlay_window_view->GetNativeWindow()
                                   ->GetHost()
                                   ->GetAcceleratedWidget());
  }
#endif
  return overlay_window;
}
#endif

void ElectronBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_schemes) {
  auto schemes_list = api::GetStandardSchemes();
  if (!schemes_list.empty())
    additional_schemes->insert(additional_schemes->end(), schemes_list.begin(),
                               schemes_list.end());
  additional_schemes->push_back(content::kChromeDevToolsScheme);
  additional_schemes->push_back(content::kChromeUIScheme);
}

void ElectronBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(content::kChromeDevToolsScheme);
}

void ElectronBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  auto* browser_context =
      static_cast<ElectronBrowserContext*>(site_instance->GetBrowserContext());
  if (!browser_context->IsOffTheRecord()) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser_context);
    const extensions::Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(
            site_instance->GetSiteURL());
    if (!extension)
      return;

    extensions::ProcessMap::Get(browser_context)
        ->Insert(extension->id(), site_instance->GetProcess()->GetID(),
                 site_instance->GetId());
  }
#endif  // BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
}

bool ElectronBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  auto* browser_context = process_host->GetBrowserContext();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser_context);

  // Otherwise, just make sure the process privilege matches the privilege
  // required by the site.
  RenderProcessHostPrivilege privilege_required =
      GetPrivilegeRequiredByUrl(site_url, registry);
  return GetProcessPrivilege(process_host, process_map, registry) ==
         privilege_required;
#else
  return content::ContentBrowserClient::IsSuitableHost(process_host, site_url);
#endif
}

bool ElectronBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& effective_url) {
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  const extensions::Extension* extension =
      GetEnabledExtensionFromEffectiveURL(browser_context, effective_url);
  return extension != nullptr;
#else
  return content::ContentBrowserClient::ShouldUseProcessPerSite(browser_context,
                                                                effective_url);
#endif
}

bool ElectronBrowserClient::ArePersistentMediaDeviceIDsAllowed(
    content::BrowserContext* browser_context,
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin) {
  return true;
}

base::FilePath ElectronBrowserClient::GetLoggingFileName(
    const base::CommandLine& cmd_line) {
  return logging::GetLogFileName(cmd_line);
}

void ElectronBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  // Don't do anything if we're shutting down.
  if (content::BrowserMainRunner::ExitedMainMessageLoop())
    return;

  auto* browser_context =
      static_cast<ElectronBrowserContext*>(site_instance->GetBrowserContext());
  if (!browser_context->IsOffTheRecord()) {
    // If this isn't an extension renderer there's nothing to do.
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser_context);
    const extensions::Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(
            site_instance->GetSiteURL());
    if (!extension)
      return;

    extensions::ProcessMap::Get(browser_context)
        ->Remove(extension->id(), site_instance->GetProcess()->GetID(),
                 site_instance->GetId());
  }
#endif
}

std::unique_ptr<net::ClientCertStore>
ElectronBrowserClient::CreateClientCertStore(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(USE_NSS_CERTS)
  return std::make_unique<net::ClientCertStoreNSS>(
      base::BindRepeating(&CreateCryptoModuleBlockingPasswordDelegate));
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<net::ClientCertStoreWin>();
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<net::ClientCertStoreMac>();
#elif defined(USE_OPENSSL)
  return std::unique_ptr<net::ClientCertStore>();
#endif
}

std::unique_ptr<device::LocationProvider>
ElectronBrowserClient::OverrideSystemLocationProvider() {
#if BUILDFLAG(OVERRIDE_LOCATION_PROVIDER)
  return std::make_unique<FakeLocationProvider>();
#else
  return nullptr;
#endif
}

void ElectronBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* browser_context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  DCHECK(browser_context);
  return NetworkContextServiceFactory::GetForContext(browser_context)
      ->ConfigureNetworkContextParams(network_context_params,
                                      cert_verifier_creation_params);
}

network::mojom::NetworkContext*
ElectronBrowserClient::GetSystemNetworkContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_browser_process->system_network_context_manager());
  return g_browser_process->system_network_context_manager()->GetContext();
}

std::unique_ptr<content::BrowserMainParts>
ElectronBrowserClient::CreateBrowserMainParts(
    content::MainFunctionParams params) {
  auto browser_main_parts =
      std::make_unique<ElectronBrowserMainParts>(std::move(params));

#if BUILDFLAG(IS_MAC)
  browser_main_parts_ = browser_main_parts.get();
#endif

  return browser_main_parts;
}

void ElectronBrowserClient::WebNotificationAllowed(
    content::RenderFrameHost* rfh,
    base::OnceCallback<void(bool, bool)> callback) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    std::move(callback).Run(false, false);
    return;
  }
  auto* permission_helper =
      WebContentsPermissionHelper::FromWebContents(web_contents);
  if (!permission_helper) {
    std::move(callback).Run(false, false);
    return;
  }
  permission_helper->RequestWebNotificationPermission(
      rfh, base::BindOnce(std::move(callback), web_contents->IsAudioMuted()));
}

void ElectronBrowserClient::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  int process_id = host->GetID();
  pending_processes_.erase(process_id);
  renderer_is_subframe_.erase(process_id);
  host->RemoveObserver(this);
}

void ElectronBrowserClient::RenderProcessReady(
    content::RenderProcessHost* host) {
  if (delegate_) {
    static_cast<api::App*>(delegate_)->RenderProcessReady(host);
  }
}

void ElectronBrowserClient::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  if (delegate_) {
    static_cast<api::App*>(delegate_)->RenderProcessExited(host);
  }
}

void OnOpenExternal(const GURL& escaped_url, bool allowed) {
  if (allowed) {
    platform_util::OpenExternal(
        escaped_url, platform_util::OpenExternalOptions(), base::DoNothing());
  }
}

void HandleExternalProtocolInUI(
    const GURL& url,
    content::WeakDocumentPtr document_ptr,
    content::WebContents::OnceGetter web_contents_getter,
    bool has_user_gesture) {
  content::WebContents* web_contents = std::move(web_contents_getter).Run();
  if (!web_contents)
    return;

  auto* permission_helper =
      WebContentsPermissionHelper::FromWebContents(web_contents);
  if (!permission_helper)
    return;

  content::RenderFrameHost* rfh = document_ptr.AsRenderFrameHostIfValid();
  if (!rfh) {
    // If the render frame host is not valid it means it was a top level
    // navigation and the frame has already been disposed of.  In this case we
    // take the current main frame and declare it responsible for the
    // transition.
    rfh = web_contents->GetMainFrame();
  }

  GURL escaped_url(net::EscapeExternalHandlerValue(url.spec()));
  auto callback = base::BindOnce(&OnOpenExternal, escaped_url);
  permission_helper->RequestOpenExternalPermission(rfh, std::move(callback),
                                                   has_user_gesture, url);
}

bool ElectronBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const absl::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&HandleExternalProtocolInUI, url,
                     initiator_document
                         ? initiator_document->GetWeakDocumentPtr()
                         : content::WeakDocumentPtr(),
                     std::move(web_contents_getter), has_user_gesture));
  return true;
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
ElectronBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  throttles.push_back(std::make_unique<ElectronNavigationThrottle>(handle));

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  throttles.push_back(
      std::make_unique<extensions::ExtensionNavigationThrottle>(handle));
#endif

#if BUILDFLAG(ENABLE_PDF_VIEWER)
  std::unique_ptr<content::NavigationThrottle> pdf_iframe_throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle);
  if (pdf_iframe_throttle)
    throttles.push_back(std::move(pdf_iframe_throttle));
  std::unique_ptr<content::NavigationThrottle> pdf_throttle =
      pdf::PdfNavigationThrottle::MaybeCreateThrottleFor(
          handle, std::make_unique<ChromePdfStreamDelegate>());
  if (pdf_throttle)
    throttles.push_back(std::move(pdf_throttle));
#endif

  return throttles;
}

content::MediaObserver* ElectronBrowserClient::GetMediaObserver() {
  return MediaCaptureDevicesDispatcher::GetInstance();
}

std::unique_ptr<content::DevToolsManagerDelegate>
ElectronBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<DevToolsManagerDelegate>();
}

NotificationPresenter* ElectronBrowserClient::GetNotificationPresenter() {
  if (!notification_presenter_) {
    notification_presenter_.reset(NotificationPresenter::Create());
  }
  return notification_presenter_.get();
}

content::PlatformNotificationService*
ElectronBrowserClient::GetPlatformNotificationService() {
  if (!notification_service_) {
    notification_service_ = std::make_unique<PlatformNotificationService>(this);
  }
  return notification_service_.get();
}

base::FilePath ElectronBrowserClient::GetDefaultDownloadDirectory() {
  base::FilePath download_path;
  if (base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &download_path))
    return download_path;
  return base::FilePath();
}

scoped_refptr<network::SharedURLLoaderFactory>
ElectronBrowserClient::GetSystemSharedURLLoaderFactory() {
  if (!g_browser_process)
    return nullptr;
  return g_browser_process->shared_url_loader_factory();
}

void ElectronBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  if (!g_browser_process)
    return;

  g_browser_process->system_network_context_manager()->OnNetworkServiceCreated(
      network_service);
}

std::vector<base::FilePath>
ElectronBrowserClient::GetNetworkContextsParentDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());

  return {user_data_dir};
}

std::string ElectronBrowserClient::GetProduct() {
  return "Chrome/" CHROME_VERSION_STRING;
}

std::string ElectronBrowserClient::GetUserAgent() {
  if (user_agent_override_.empty())
    return GetApplicationUserAgent();
  return user_agent_override_;
}

void ElectronBrowserClient::SetUserAgent(const std::string& user_agent) {
  user_agent_override_ = user_agent;
}

blink::UserAgentMetadata ElectronBrowserClient::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata();
}

void ElectronBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    ukm::SourceIdObj ukm_source_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  content::BrowserContext* context = web_contents->GetBrowserContext();
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          context, ukm_source_id,
          false /* we don't support extensions::WebViewGuest */));
#endif
  // Always allow navigating to file:// URLs.
  auto* protocol_registry = ProtocolRegistry::FromBrowserContext(context);
  protocol_registry->RegisterURLLoaderFactories(factories,
                                                true /* allow_file_access */);
}

void ElectronBrowserClient::
    RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  auto* protocol_registry =
      ProtocolRegistry::FromBrowserContext(browser_context);
  // Workers are not allowed to request file:// URLs, there is no particular
  // reason for it, and we could consider supporting it in future.
  protocol_registry->RegisterURLLoaderFactories(factories,
                                                false /* allow_file_access */);
}

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
namespace {

// The FileURLLoaderFactory provided to the extension background pages.
// Checks with the ChildProcessSecurityPolicy to validate the file access.
class FileURLLoaderFactory : public network::SelfDeletingURLLoaderFactory {
 public:
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      int child_id) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

    // The FileURLLoaderFactory will delete itself when there are no more
    // receivers - see the SelfDeletingURLLoaderFactory::OnDisconnect method.
    new FileURLLoaderFactory(child_id,
                             pending_remote.InitWithNewPipeAndPassReceiver());

    return pending_remote;
  }

  // disable copy
  FileURLLoaderFactory(const FileURLLoaderFactory&) = delete;
  FileURLLoaderFactory& operator=(const FileURLLoaderFactory&) = delete;

 private:
  explicit FileURLLoaderFactory(
      int child_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
      : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
        child_id_(child_id) {}
  ~FileURLLoaderFactory() override = default;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
            child_id_, request.url)) {
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_ACCESS_DENIED));
      return;
    }
    content::CreateFileURLLoaderBypassingSecurityChecks(
        request, std::move(loader), std::move(client),
        /*observer=*/nullptr,
        /* allow_directory_listing */ true);
  }

  int child_id_;
};

}  // namespace
#endif  // BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)

void ElectronBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    const absl::optional<url::Origin>& request_initiator_origin,
    NonNetworkURLLoaderFactoryMap* factories) {
  auto* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  DCHECK(render_process_host);
  if (!render_process_host || !render_process_host->GetBrowserContext())
    return;

  content::RenderFrameHost* frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);

  // Allow accessing file:// subresources from non-file protocols if web
  // security is disabled.
  bool allow_file_access = false;
  if (web_contents) {
    const auto& web_preferences = web_contents->GetOrCreateWebPreferences();
    if (!web_preferences.web_security_enabled)
      allow_file_access = true;
  }

  ProtocolRegistry::FromBrowserContext(render_process_host->GetBrowserContext())
      ->RegisterURLLoaderFactories(factories, allow_file_access);

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  auto factory = extensions::CreateExtensionURLLoaderFactory(render_process_id,
                                                             render_frame_id);
  if (factory)
    factories->emplace(extensions::kExtensionScheme, std::move(factory));

  if (!web_contents)
    return;

  extensions::ElectronExtensionWebContentsObserver* web_observer =
      extensions::ElectronExtensionWebContentsObserver::FromWebContents(
          web_contents);

  // There is nothing to do if no ElectronExtensionWebContentsObserver is
  // attached to the |web_contents|.
  if (!web_observer)
    return;

  const extensions::Extension* extension =
      web_observer->GetExtensionFromFrame(frame_host, false);
  if (!extension)
    return;

  // Support for chrome:// scheme if appropriate.
  if (extension->is_extension() &&
      extensions::Manifest::IsComponentLocation(extension->location())) {
    // Components of chrome that are implemented as extensions or platform apps
    // are allowed to use chrome://resources/ and chrome://theme/ URLs.
    factories->emplace(content::kChromeUIScheme,
                       content::CreateWebUIURLLoaderFactory(
                           frame_host, content::kChromeUIScheme,
                           {content::kChromeUIResourcesHost}));
  }

  // Extensions with the necessary permissions get access to file:// URLs that
  // gets approval from ChildProcessSecurityPolicy. Keep this logic in sync with
  // ExtensionWebContentsObserver::RenderFrameCreated.
  extensions::Manifest::Type type = extension->GetType();
  if (type == extensions::Manifest::TYPE_EXTENSION &&
      AllowFileAccess(extension->id(), web_contents->GetBrowserContext())) {
    factories->emplace(url::kFileScheme,
                       FileURLLoaderFactory::Create(render_process_id));
  }
#endif
}

void ElectronBrowserClient::
    RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(browser_context);
  DCHECK(factories);

  auto* protocol_registry =
      ProtocolRegistry::FromBrowserContext(browser_context);
  protocol_registry->RegisterURLLoaderFactories(factories,
                                                false /* allow_file_access */);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionServiceWorkerScriptURLLoaderFactory(
          browser_context));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool ElectronBrowserClient::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    base::StringPiece scheme,
    bool is_embedded_origin_secure) {
  if (is_embedded_origin_secure && scheme == content::kChromeUIScheme)
    return true;
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  return scheme == extensions::kExtensionScheme;
#else
  return false;
#endif
}

bool ElectronBrowserClient::WillInterceptWebSocket(
    content::RenderFrameHost* frame) {
  if (!frame)
    return false;

  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  auto* browser_context = frame->GetProcess()->GetBrowserContext();
  auto web_request = api::WebRequest::FromOrCreate(isolate, browser_context);

  // NOTE: Some unit test environments do not initialize
  // BrowserContextKeyedAPI factories for e.g. WebRequest.
  if (!web_request.get())
    return false;

  bool has_listener = web_request->HasListener();
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  const auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);

  if (web_request_api)
    has_listener |= web_request_api->MayHaveProxies();
#endif

  return has_listener;
}

void ElectronBrowserClient::CreateWebSocket(
    content::RenderFrameHost* frame,
    WebSocketFactory factory,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  auto* browser_context = frame->GetProcess()->GetBrowserContext();

  auto web_request = api::WebRequest::FromOrCreate(isolate, browser_context);
  DCHECK(web_request.get());

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  if (!web_request->HasListener()) {
    auto* web_request_api = extensions::BrowserContextKeyedAPIFactory<
        extensions::WebRequestAPI>::Get(browser_context);

    if (web_request_api && web_request_api->MayHaveProxies()) {
      web_request_api->ProxyWebSocket(frame, std::move(factory), url,
                                      site_for_cookies, user_agent,
                                      std::move(handshake_client));
      return;
    }
  }
#endif

  ProxyingWebSocket::StartProxying(
      web_request.get(), std::move(factory), url, site_for_cookies, user_agent,
      std::move(handshake_client), true, frame->GetProcess()->GetID(),
      frame->GetRoutingID(), frame->GetLastCommittedOrigin(), browser_context,
      &next_id_);
}

bool ElectronBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame_host,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    absl::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  auto web_request = api::WebRequest::FromOrCreate(isolate, browser_context);
  DCHECK(web_request.get());

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  if (!web_request->HasListener()) {
    auto* web_request_api = extensions::BrowserContextKeyedAPIFactory<
        extensions::WebRequestAPI>::Get(browser_context);

    DCHECK(web_request_api);
    bool use_proxy_for_web_request =
        web_request_api->MaybeProxyURLLoaderFactory(
            browser_context, frame_host, render_process_id, type, navigation_id,
            ukm_source_id, factory_receiver, header_client);

    if (bypass_redirect_checks)
      *bypass_redirect_checks = use_proxy_for_web_request;
    if (use_proxy_for_web_request)
      return true;
  }
#endif

  auto proxied_receiver = std::move(*factory_receiver);
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
  *factory_receiver = target_factory_remote.InitWithNewPipeAndPassReceiver();

  // Required by WebRequestInfoInitParams.
  //
  // Note that in Electron we allow webRequest to capture requests sent from
  // browser process, so creation of |navigation_ui_data| is different from
  // Chromium which only does for renderer-initialized navigations.
  std::unique_ptr<extensions::ExtensionNavigationUIData> navigation_ui_data;
  if (navigation_id.has_value()) {
    navigation_ui_data =
        std::make_unique<extensions::ExtensionNavigationUIData>();
  }

  mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
      header_client_receiver;
  if (header_client)
    header_client_receiver = header_client->InitWithNewPipeAndPassReceiver();

  auto* protocol_registry =
      ProtocolRegistry::FromBrowserContext(browser_context);
  new ProxyingURLLoaderFactory(
      web_request.get(), protocol_registry->intercept_handlers(),
      render_process_id,
      frame_host ? frame_host->GetRoutingID() : MSG_ROUTING_NONE, &next_id_,
      std::move(navigation_ui_data), std::move(navigation_id),
      std::move(proxied_receiver), std::move(target_factory_remote),
      std::move(header_client_receiver), type);

  return true;
}

std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
ElectronBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory) {
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
      interceptors;
#if BUILDFLAG(ENABLE_PDF_VIEWER)
  {
    std::unique_ptr<content::URLLoaderRequestInterceptor> pdf_interceptor =
        pdf::PdfURLLoaderRequestInterceptor::MaybeCreateInterceptor(
            frame_tree_node_id, std::make_unique<ChromePdfStreamDelegate>());
    if (pdf_interceptor)
      interceptors.push_back(std::move(pdf_interceptor));
  }
#endif
  return interceptors;
}

void ElectronBrowserClient::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  if (factory_params->top_frame_id) {
    // Bypass CORB and CORS when web security is disabled.
    auto* rfh = content::RenderFrameHost::FromFrameToken(
        factory_params->process_id,
        blink::LocalFrameToken(factory_params->top_frame_id.value()));
    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    auto* prefs = WebContentsPreferences::From(web_contents);
    if (prefs && !prefs->IsWebSecurityEnabled()) {
      factory_params->is_corb_enabled = false;
      factory_params->disable_web_security = true;
    }
  }

  extensions::URLLoaderFactoryManager::OverrideURLLoaderFactoryParams(
      browser_context, origin, is_for_isolated_world, factory_params);
}

#if BUILDFLAG(IS_WIN)
bool ElectronBrowserClient::PreSpawnChild(sandbox::TargetPolicy* policy,
                                          sandbox::mojom::Sandbox sandbox_type,
                                          ChildSpawnFlags flags) {
  // Allow crashpad to communicate via named pipe.
  sandbox::ResultCode result = policy->AddRule(
      sandbox::TargetPolicy::SUBSYS_FILES,
      sandbox::TargetPolicy::FILES_ALLOW_ANY, L"\\??\\pipe\\crashpad_*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

void ElectronBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost&
            render_frame_host,  // NOLINT(runtime/references)
        blink::AssociatedInterfaceRegistry&
            associated_registry) {  // NOLINT(runtime/references)
  auto* contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  if (contents) {
    auto* prefs = WebContentsPreferences::From(contents);
    if (render_frame_host.GetFrameTreeNodeId() ==
            contents->GetMainFrame()->GetFrameTreeNodeId() ||
        (prefs && prefs->AllowsNodeIntegrationInSubFrames())) {
      associated_registry.AddInterface(base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<electron::mojom::ElectronApiIPC>
                 receiver) {
            ElectronApiIPCHandlerImpl::Create(render_frame_host,
                                              std::move(receiver));
          },
          &render_frame_host));
    }
  }

  associated_registry.AddInterface(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             electron::mojom::ElectronWebContentsUtility> receiver) {
        ElectronWebContentsUtilityHandlerImpl::Create(render_frame_host,
                                                      std::move(receiver));
      },
      &render_frame_host));

  associated_registry.AddInterface(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<mojom::ElectronAutofillDriver>
             receiver) {
        AutofillDriverFactory::BindAutofillDriver(std::move(receiver),
                                                  render_frame_host);
      },
      &render_frame_host));
#if BUILDFLAG(ENABLE_PRINTING)
  associated_registry.AddInterface(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
             receiver) {
        PrintViewManagerElectron::BindPrintManagerHost(std::move(receiver),
                                                       render_frame_host);
      },
      &render_frame_host));
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  associated_registry.AddInterface(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<extensions::mojom::LocalFrameHost>
             receiver) {
        extensions::ExtensionWebContentsObserver::BindLocalFrameHost(
            std::move(receiver), render_frame_host);
      },
      &render_frame_host));
#endif
#if BUILDFLAG(ENABLE_PDF_VIEWER)
  associated_registry.AddInterface(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<pdf::mojom::PdfService> receiver) {
        pdf::PDFWebContentsHelper::BindPdfService(std::move(receiver),
                                                  render_frame_host);
      },
      &render_frame_host));
#endif
}

std::string ElectronBrowserClient::GetApplicationLocale() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return g_io_thread_application_locale.Get();
  return *g_application_locale;
}

base::FilePath ElectronBrowserClient::GetFontLookupTableCacheDir() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("FontLookupTableCache"));
}

bool ElectronBrowserClient::ShouldEnableStrictSiteIsolation() {
  // Enable site isolation. It is off by default in Chromium <= 69.
  return true;
}

void ElectronBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(ENABLE_BUILTIN_SPELLCHECKER)
  if (auto host_receiver = receiver.As<spellcheck::mojom::SpellCheckHost>()) {
    SpellCheckHostChromeImpl::Create(render_process_host->GetID(),
                                     std::move(host_receiver));
    return;
  }
#endif
}

#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
void BindMimeHandlerService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::MimeHandlerService>
        receiver) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(contents);
  if (!guest_view)
    return;
  extensions::MimeHandlerServiceImpl::Create(guest_view->GetStreamWeakPtr(),
                                             std::move(receiver));
}

void BindBeforeUnloadControl(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::BeforeUnloadControl>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;
  guest_view->FuseBeforeUnloadControl(std::move(receiver));
}
#endif

void ElectronBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  associated_registry->AddInterface(base::BindRepeating(
      &extensions::EventRouter::BindForRenderer, render_process_host->GetID()));
  associated_registry->AddInterface(
      base::BindRepeating(&extensions::ExtensionsGuestView::CreateForExtensions,
                          render_process_host->GetID()));
#endif
}

void ElectronBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&badging::BadgeManager::BindFrameReceiver));
  map->Add<blink::mojom::KeyboardLockService>(base::BindRepeating(
      &content::KeyboardLockServiceImpl::CreateMojoService));
#if BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  map->Add<extensions::mime_handler::MimeHandlerService>(
      base::BindRepeating(&BindMimeHandlerService));
  map->Add<extensions::mime_handler::BeforeUnloadControl>(
      base::BindRepeating(&BindBeforeUnloadControl));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (!site.SchemeIs(extensions::kExtensionScheme))
    return;

  content::BrowserContext* browser_context =
      render_frame_host->GetProcess()->GetBrowserContext();
  auto* extension = extensions::ExtensionRegistry::Get(browser_context)
                        ->enabled_extensions()
                        .GetByID(site.host());
  if (!extension)
    return;
  extensions::ExtensionsBrowserClient::Get()
      ->RegisterBrowserInterfaceBindersForFrame(map, render_frame_host,
                                                extension);
#endif
}

#if BUILDFLAG(IS_LINUX)
void ElectronBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif

std::unique_ptr<content::LoginDelegate>
ElectronBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<LoginHandler>(
      auth_info, web_contents, is_main_frame, url, response_headers,
      first_auth_attempt, std::move(auth_required_callback));
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ElectronBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  result.push_back(std::make_unique<PluginResponseInterceptorURLLoaderThrottle>(
      request.destination, frame_tree_node_id));
#endif

  return result;
}

base::flat_set<std::string>
ElectronBrowserClient::GetPluginMimeTypesWithExternalHandlers(
    content::BrowserContext* browser_context) {
  base::flat_set<std::string> mime_types;
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_ELECTRON_EXTENSIONS)
  auto map = PluginUtils::GetMimeTypeToExtensionIdMap(browser_context);
  for (const auto& pair : map)
    mime_types.insert(pair.first);
#endif
#if BUILDFLAG(ENABLE_PDF_VIEWER)
  mime_types.insert(pdf::kInternalPluginMimeType);
#endif
  return mime_types;
}

content::SerialDelegate* ElectronBrowserClient::GetSerialDelegate() {
  if (!serial_delegate_)
    serial_delegate_ = std::make_unique<ElectronSerialDelegate>();
  return serial_delegate_.get();
}

content::BluetoothDelegate* ElectronBrowserClient::GetBluetoothDelegate() {
  if (!bluetooth_delegate_)
    bluetooth_delegate_ = std::make_unique<ElectronBluetoothDelegate>();
  return bluetooth_delegate_.get();
}

void BindBadgeServiceForServiceWorker(
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(info.process_id);
  if (!render_process_host)
    return;

  badging::BadgeManager::BindServiceWorkerReceiver(
      render_process_host, info.scope, std::move(receiver));
}

void ElectronBrowserClient::RegisterBrowserInterfaceBindersForServiceWorker(
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
        map) {
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&BindBadgeServiceForServiceWorker));
}

device::GeolocationManager* ElectronBrowserClient::GetGeolocationManager() {
#if BUILDFLAG(IS_MAC)
  return browser_main_parts_->GetGeolocationManager();
#else
  return nullptr;
#endif
}

content::HidDelegate* ElectronBrowserClient::GetHidDelegate() {
  if (!hid_delegate_)
    hid_delegate_ = std::make_unique<ElectronHidDelegate>();
  return hid_delegate_.get();
}

}  // namespace electron
