/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cmakeconfig.h"

#include "HeadlessViewBackend.h"
#include "WindowViewBackend.h"
#if ENABLE_WEB_AUDIO || ENABLE_VIDEO
#include <gst/gst.h>
#endif
#include <memory>
#include <wpe/webkit.h>

static const char** uriArguments;
static const char** ignoreHosts;
static gboolean headlessMode;
static gboolean privateMode;
static gboolean automationMode;
static gboolean ignoreTLSErrors;
static const char* contentFilter;
static const char* cookiesFile;
static const char* cookiesPolicy;
static const char* proxy;

static const GOptionEntry commandLineOptions[] =
{
    { "headless", 'h', 0, G_OPTION_ARG_NONE, &headlessMode, "Run in headless mode", nullptr },
    { "private", 'p', 0, G_OPTION_ARG_NONE, &privateMode, "Run in private browsing mode", nullptr },
    { "automation", 0, 0, G_OPTION_ARG_NONE, &automationMode, "Run in automation mode", nullptr },
    { "cookies-file", 'c', 0, G_OPTION_ARG_FILENAME, &cookiesFile, "Persistent cookie storage database file", "FILE" },
    { "cookies-policy", 0, 0, G_OPTION_ARG_STRING, &cookiesPolicy, "Cookies accept policy (always, never, no-third-party). Default: no-third-party", "POLICY" },
    { "proxy", 0, 0, G_OPTION_ARG_STRING, &proxy, "Set proxy", "PROXY" },
    { "ignore-host", 0, 0, G_OPTION_ARG_STRING_ARRAY, &ignoreHosts, "Set proxy ignore hosts", "HOSTS" },
    { "ignore-tls-errors", 0, 0, G_OPTION_ARG_NONE, &ignoreTLSErrors, "Ignore TLS errors", nullptr },
    { "content-filter", 0, 0, G_OPTION_ARG_FILENAME, &contentFilter, "JSON with content filtering rules", "FILE" },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &uriArguments, nullptr, "[URL]" },
    { nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
};

class InputClient final : public WPEToolingBackends::ViewBackend::InputClient {
public:
    InputClient(GMainLoop* loop, WebKitWebView* webView)
        : m_loop(loop)
        , m_webView(webView)
    {
    }

    bool dispatchKeyboardEvent(struct wpe_input_keyboard_event* event) override
    {
        if (!event->pressed)
            return false;

        if (event->modifiers & wpe_input_keyboard_modifier_control && event->key_code == WPE_KEY_q) {
            g_main_loop_quit(m_loop);
            return true;
        }

        if (event->modifiers & wpe_input_keyboard_modifier_alt) {
            if ((event->key_code == WPE_KEY_Left || event->key_code == WPE_KEY_KP_Left) && webkit_web_view_can_go_back(m_webView)) {
                webkit_web_view_go_back(m_webView);
                return true;
            }

            if ((event->key_code == WPE_KEY_Right || event->key_code == WPE_KEY_KP_Right) && webkit_web_view_can_go_forward(m_webView)) {
                webkit_web_view_go_forward(m_webView);
                return true;
            }
        }

        return false;
    }

private:
    GMainLoop* m_loop { nullptr };
    WebKitWebView* m_webView { nullptr };
};

static WebKitWebView* createWebViewForAutomationCallback(WebKitAutomationSession*, WebKitWebView* view)
{
    return view;
}

static void automationStartedCallback(WebKitWebContext*, WebKitAutomationSession* session, WebKitWebView* view)
{
    auto* info = webkit_application_info_new();
    webkit_application_info_set_version(info, WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION);
    webkit_automation_session_set_application_info(session, info);
    webkit_application_info_unref(info);

    g_signal_connect(session, "create-web-view", G_CALLBACK(createWebViewForAutomationCallback), view);
}

static gboolean decidePermissionRequest(WebKitWebView *, WebKitPermissionRequest *request, gpointer)
{
    g_print("Accepting %s request\n", G_OBJECT_TYPE_NAME(request));
    webkit_permission_request_allow(request);

    return TRUE;
}

static std::unique_ptr<WPEToolingBackends::ViewBackend> createViewBackend(uint32_t width, uint32_t height)
{
    if (headlessMode)
        return std::make_unique<WPEToolingBackends::HeadlessViewBackend>(width, height);
    return std::make_unique<WPEToolingBackends::WindowViewBackend>(width, height);
}

typedef struct {
    GMainLoop* mainLoop { nullptr };
    WebKitUserContentFilter* filter { nullptr };
    GError* error { nullptr };
} FilterSaveData;

static void filterSavedCallback(WebKitUserContentFilterStore *store, GAsyncResult *result, FilterSaveData *data)
{
    data->filter = webkit_user_content_filter_store_save_finish(store, result, &data->error);
    g_main_loop_quit(data->mainLoop);
}

int main(int argc, char *argv[])
{
#if ENABLE_DEVELOPER_MODE
    g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);
#endif

    GOptionContext* context = g_option_context_new(nullptr);
    g_option_context_add_main_entries(context, commandLineOptions, nullptr);
#if ENABLE_WEB_AUDIO || ENABLE_VIDEO
    g_option_context_add_group(context, gst_init_get_option_group());
#endif

    GError* error = nullptr;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Cannot parse arguments: %s\n", error->message);
        g_error_free(error);
        g_option_context_free(context);

        return 1;
    }
    g_option_context_free(context);

    auto* loop = g_main_loop_new(nullptr, FALSE);

    auto backend = createViewBackend(1280, 720);
    struct wpe_view_backend* wpeBackend = backend->backend();
    if (!wpeBackend) {
        g_warning("Failed to create WPE view backend");
        g_main_loop_unref(loop);
        return 1;
    }

    auto* webContext = (privateMode || automationMode) ? webkit_web_context_new_ephemeral() : webkit_web_context_get_default();

    if (cookiesPolicy) {
        auto* cookieManager = webkit_web_context_get_cookie_manager(webContext);
        auto* enumClass = static_cast<GEnumClass*>(g_type_class_ref(WEBKIT_TYPE_COOKIE_ACCEPT_POLICY));
        GEnumValue* enumValue = g_enum_get_value_by_nick(enumClass, cookiesPolicy);
        if (enumValue)
            webkit_cookie_manager_set_accept_policy(cookieManager, static_cast<WebKitCookieAcceptPolicy>(enumValue->value));
        g_type_class_unref(enumClass);
    }

    if (cookiesFile && !webkit_web_context_is_ephemeral(webContext)) {
        auto* cookieManager = webkit_web_context_get_cookie_manager(webContext);
        auto storageType = g_str_has_suffix(cookiesFile, ".txt") ? WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT : WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE;
        webkit_cookie_manager_set_persistent_storage(cookieManager, cookiesFile, storageType);
    }

    if (proxy) {
        auto* webkitProxySettings = webkit_network_proxy_settings_new(proxy, ignoreHosts);
        webkit_web_context_set_network_proxy_settings(webContext, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, webkitProxySettings);
        webkit_network_proxy_settings_free(webkitProxySettings);
    }

    const char* singleprocess = g_getenv("MINIBROWSER_SINGLEPROCESS");
    webkit_web_context_set_process_model(webContext, (singleprocess && *singleprocess) ?
        WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS : WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

    WebKitUserContentManager* userContentManager = nullptr;
    if (contentFilter) {
        GFile* contentFilterFile = g_file_new_for_commandline_arg(contentFilter);

        FilterSaveData saveData = { nullptr, };
        gchar* filtersPath = g_build_filename(g_get_user_cache_dir(), g_get_prgname(), "filters", nullptr);
        WebKitUserContentFilterStore* store = webkit_user_content_filter_store_new(filtersPath);
        g_free(filtersPath);

        webkit_user_content_filter_store_save_from_file(store, "WPEMiniBrowserFilter", contentFilterFile, nullptr, (GAsyncReadyCallback)filterSavedCallback, &saveData);
        saveData.mainLoop = g_main_loop_new(nullptr, FALSE);
        g_main_loop_run(saveData.mainLoop);
        g_object_unref(store);

        if (saveData.filter) {
            userContentManager = webkit_user_content_manager_new();
            webkit_user_content_manager_add_filter(userContentManager, saveData.filter);
        } else
            g_printerr("Cannot save filter '%s': %s\n", contentFilter, saveData.error->message);

        g_clear_pointer(&saveData.error, g_error_free);
        g_clear_pointer(&saveData.filter, webkit_user_content_filter_unref);
        g_main_loop_unref(saveData.mainLoop);
        g_object_unref(contentFilterFile);
    }

    auto* settings = webkit_settings_new_with_settings(
        "enable-developer-extras", TRUE,
        "enable-webgl", TRUE,
        "enable-media-stream", TRUE,
        "enable-encrypted-media", TRUE,
        nullptr);

    auto* backendPtr = backend.get();
    auto* viewBackend = webkit_web_view_backend_new(wpeBackend, [](gpointer data) {
        delete static_cast<WPEToolingBackends::ViewBackend*>(data);
    }, backend.release());

    auto* webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "backend", viewBackend,
        "web-context", webContext,
        "settings", settings,
        "user-content-manager", userContentManager,
        "is-controlled-by-automation", automationMode,
        nullptr));
    g_object_unref(settings);

    backendPtr->setInputClient(std::make_unique<InputClient>(loop, webView));

    webkit_web_context_set_automation_allowed(webContext, automationMode);
    g_signal_connect(webContext, "automation-started", G_CALLBACK(automationStartedCallback), webView);
    g_signal_connect(webView, "permission-request", G_CALLBACK(decidePermissionRequest), NULL);

    if (ignoreTLSErrors)
        webkit_web_context_set_tls_errors_policy(webContext, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    if (uriArguments) {
        GFile* file = g_file_new_for_commandline_arg(uriArguments[0]);
        char* url = g_file_get_uri(file);
        g_object_unref(file);
        webkit_web_view_load_uri(webView, url);
        g_free(url);
    } else if (!automationMode)
        webkit_web_view_load_uri(webView, "https://wpewebkit.org");

    g_main_loop_run(loop);

    g_object_unref(webView);
    if (privateMode || automationMode)
        g_object_unref(webContext);
    g_main_loop_unref(loop);

    return 0;
}
