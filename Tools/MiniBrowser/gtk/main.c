/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
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

#include "BrowserWindow.h"
#include <errno.h>
#if ENABLE_WEB_AUDIO || ENABLE_VIDEO
#include <gst/gst.h>
#endif
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

#define MINI_BROWSER_ERROR (miniBrowserErrorQuark())

static const gchar **uriArguments = NULL;
static const gchar **ignoreHosts = NULL;
static WebKitAutoplayPolicy autoplayPolicy = WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND;
static GdkRGBA *backgroundColor;
static gboolean editorMode;
static const char *sessionFile;
static char *geometry;
static gboolean privateMode;
static gboolean automationMode;
static gboolean fullScreen;
static gboolean ignoreTLSErrors;
static const char *contentFilter;
static const char *cookiesFile;
static const char *cookiesPolicy;
static const char *proxy;
static gboolean darkMode;
static gboolean enableITP;
static gboolean printVersion;

typedef enum {
    MINI_BROWSER_ERROR_INVALID_ABOUT_PATH
} MiniBrowserError;

static GQuark miniBrowserErrorQuark()
{
    return g_quark_from_string("minibrowser-quark");
}

static gchar *argumentToURL(const char *filename)
{
    if (g_str_equal(filename, "about:gpu"))
        filename = "webkit://gpu";

    GFile *gfile = g_file_new_for_commandline_arg(filename);
    gchar *fileURL = g_file_get_uri(gfile);
    g_object_unref(gfile);

    return fileURL;
}

static WebKitWebView *createBrowserTab(BrowserWindow *window, WebKitSettings *webkitSettings, WebKitUserContentManager *userContentManager, WebKitWebsitePolicies *defaultWebsitePolicies)
{
    WebKitWebView *webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", browser_window_get_web_context(window),
        "settings", webkitSettings,
        "user-content-manager", userContentManager,
        "is-controlled-by-automation", automationMode,
        "website-policies", defaultWebsitePolicies,
        NULL));

    if (editorMode)
        webkit_web_view_set_editable(webView, TRUE);

    browser_window_append_view(window, webView);
    return webView;
}

static gboolean parseAutoplayPolicy(const char *optionName, const char *value, gpointer data, GError **error)
{
    if (!g_strcmp0(value, "allow")) {
        autoplayPolicy = WEBKIT_AUTOPLAY_ALLOW;
        return TRUE;
    }

    if (!g_strcmp0(value, "allow-without-sound")) {
        autoplayPolicy = WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND;
        return TRUE;
    }

    if (!g_strcmp0(value, "deny")) {
        autoplayPolicy = WEBKIT_AUTOPLAY_DENY;
        return TRUE;
    }

    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Failed to parse '%s' as an autoplay policy, valid options are allow, allow-without-sound, and deny", value);
    return FALSE;
}

static gboolean parseBackgroundColor(const char *optionName, const char *value, gpointer data, GError **error)
{
    GdkRGBA rgba;
    if (gdk_rgba_parse(&rgba, value)) {
        backgroundColor = gdk_rgba_copy(&rgba);
        return TRUE;
    }

    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Failed to parse '%s' as RGBA color", value);
    return FALSE;
}

static const GOptionEntry commandLineOptions[] =
{
    { "autoplay-policy", 0, 0, G_OPTION_ARG_CALLBACK, parseAutoplayPolicy, "Autoplay policy. Valid options are: allow, allow-without-sound, and deny", NULL },
    { "bg-color", 0, 0, G_OPTION_ARG_CALLBACK, parseBackgroundColor, "Background color", NULL },
    { "editor-mode", 'e', 0, G_OPTION_ARG_NONE, &editorMode, "Run in editor mode", NULL },
    { "dark-mode", 'd', 0, G_OPTION_ARG_NONE, &darkMode, "Run in dark mode", NULL },
    { "session-file", 's', 0, G_OPTION_ARG_FILENAME, &sessionFile, "Session file", "FILE" },
    { "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry, "Unused. Kept for backwards-compatibility only", "GEOMETRY" },
    { "full-screen", 'f', 0, G_OPTION_ARG_NONE, &fullScreen, "Set the window to full-screen mode", NULL },
    { "private", 'p', 0, G_OPTION_ARG_NONE, &privateMode, "Run in private browsing mode", NULL },
    { "automation", 0, 0, G_OPTION_ARG_NONE, &automationMode, "Run in automation mode", NULL },
    { "cookies-file", 'c', 0, G_OPTION_ARG_FILENAME, &cookiesFile, "Persistent cookie storage database file", "FILE" },
    { "cookies-policy", 0, 0, G_OPTION_ARG_STRING, &cookiesPolicy, "Cookies accept policy (always, never, no-third-party). Default: no-third-party", "POLICY" },
    { "proxy", 0, 0, G_OPTION_ARG_STRING, &proxy, "Set proxy", "PROXY" },
    { "ignore-host", 0, 0, G_OPTION_ARG_STRING_ARRAY, &ignoreHosts, "Set proxy ignore hosts", "HOSTS" },
    { "ignore-tls-errors", 0, 0, G_OPTION_ARG_NONE, &ignoreTLSErrors, "Ignore TLS errors", NULL },
    { "content-filter", 0, 0, G_OPTION_ARG_FILENAME, &contentFilter, "JSON with content filtering rules", "FILE" },
    { "enable-itp", 0, 0, G_OPTION_ARG_NONE, &enableITP, "Enable Intelligent Tracking Prevention (ITP)", NULL },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &printVersion, "Print the WebKitGTK version", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &uriArguments, 0, "[URL…]" },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static gboolean parseOptionEntryCallback(const gchar *optionNameFull, const gchar *value, WebKitSettings *webSettings, GError **error)
{
    if (strlen(optionNameFull) <= 2) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Invalid option %s", optionNameFull);
        return FALSE;
    }

    /* We have two -- in option name so remove them. */
    const gchar *optionName = optionNameFull + 2;
    GParamSpec *spec = g_object_class_find_property(G_OBJECT_GET_CLASS(webSettings), optionName);
    if (!spec) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Cannot find web settings for option %s", optionNameFull);
        return FALSE;
    }

    switch (G_PARAM_SPEC_VALUE_TYPE(spec)) {
    case G_TYPE_BOOLEAN: {
        gboolean propertyValue = !(value && g_ascii_strcasecmp(value, "true") && strcmp(value, "1"));
        g_object_set(G_OBJECT(webSettings), optionName, propertyValue, NULL);
        break;
    }
    case G_TYPE_STRING:
        g_object_set(G_OBJECT(webSettings), optionName, value, NULL);
        break;
    case G_TYPE_INT: {
        glong propertyValue;
        gchar *end;

        errno = 0;
        propertyValue = g_ascii_strtoll(value, &end, 0);
        if (errno == ERANGE || propertyValue > G_MAXINT || propertyValue < G_MININT) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Integer value '%s' for %s out of range", value, optionNameFull);
            return FALSE;
        }
        if (errno || value == end) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Cannot parse integer value '%s' for %s", value, optionNameFull);
            return FALSE;
        }
        g_object_set(G_OBJECT(webSettings), optionName, propertyValue, NULL);
        break;
    }
    case G_TYPE_FLOAT: {
        gdouble propertyValue;
        gchar *end;

        errno = 0;
        propertyValue = g_ascii_strtod(value, &end);
        if (errno == ERANGE || propertyValue > G_MAXFLOAT || propertyValue < G_MINFLOAT) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Float value '%s' for %s out of range", value, optionNameFull);
            return FALSE;
        }
        if (errno || value == end) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Cannot parse float value '%s' for %s", value, optionNameFull);
            return FALSE;
        }
        g_object_set(G_OBJECT(webSettings), optionName, propertyValue, NULL);
        break;
    }
    default:
        g_assert_not_reached();
    }

    return TRUE;
}

static gboolean isValidParameterType(GType gParamType)
{
    return (gParamType == G_TYPE_BOOLEAN || gParamType == G_TYPE_STRING || gParamType == G_TYPE_INT
            || gParamType == G_TYPE_FLOAT);
}

static GOptionEntry* getOptionEntriesFromWebKitSettings(WebKitSettings *webSettings)
{
    GParamSpec **propertySpecs;
    GOptionEntry *optionEntries;
    guint numProperties, numEntries, i;

    propertySpecs = g_object_class_list_properties(G_OBJECT_GET_CLASS(webSettings), &numProperties);
    if (!propertySpecs)
        return NULL;

    optionEntries = g_new0(GOptionEntry, numProperties + 1);
    numEntries = 0;
    for (i = 0; i < numProperties; i++) {
        GParamSpec *param = propertySpecs[i];

        /* Fill in structures only for writable and not construct-only properties. */
        if (!param || !(param->flags & G_PARAM_WRITABLE) || (param->flags & G_PARAM_CONSTRUCT_ONLY))
            continue;

        GType gParamType = G_PARAM_SPEC_VALUE_TYPE(param);
        if (!isValidParameterType(gParamType))
            continue;

        GOptionEntry *optionEntry = &optionEntries[numEntries++];
        optionEntry->long_name = g_param_spec_get_name(param);

        /* There is no easy way to figure our short name for generated option entries.
           optionEntry.short_name=*/
        /* For bool arguments "enable" type make option argument not required. */
        if (gParamType == G_TYPE_BOOLEAN && (strstr(optionEntry->long_name, "enable")))
            optionEntry->flags = G_OPTION_FLAG_OPTIONAL_ARG;
        optionEntry->arg = G_OPTION_ARG_CALLBACK;
        optionEntry->arg_data = parseOptionEntryCallback;
        optionEntry->description = g_param_spec_get_blurb(param);
        optionEntry->arg_description = g_type_name(gParamType);
    }
    g_free(propertySpecs);

    return optionEntries;
}

static gboolean addSettingsGroupToContext(GOptionContext *context, WebKitSettings* webkitSettings)
{
    GOptionEntry *optionEntries = getOptionEntriesFromWebKitSettings(webkitSettings);
    if (!optionEntries)
        return FALSE;

    GOptionGroup *webSettingsGroup = g_option_group_new("websettings",
                                                        "WebKitSettings writable properties for default WebKitWebView",
                                                        "WebKitSettings properties",
                                                        webkitSettings,
                                                        NULL);
    g_option_group_add_entries(webSettingsGroup, optionEntries);
    g_free(optionEntries);

    /* Option context takes ownership of the group. */
    g_option_context_add_group(context, webSettingsGroup);

    return TRUE;
}

typedef struct {
    WebKitURISchemeRequest *request;
    GList *dataList;
    GHashTable *dataMap;
} AboutDataRequest;

static GHashTable *aboutDataRequestMap;

static void aboutDataRequestFree(AboutDataRequest *request)
{
    if (!request)
        return;

    g_list_free_full(request->dataList, g_object_unref);

    if (request->request)
        g_object_unref(request->request);
    if (request->dataMap)
        g_hash_table_destroy(request->dataMap);

    g_free(request);
}

static AboutDataRequest* aboutDataRequestNew(WebKitURISchemeRequest *uriRequest)
{
    if (!aboutDataRequestMap)
        aboutDataRequestMap = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)aboutDataRequestFree);

    AboutDataRequest *request = g_new0(AboutDataRequest, 1);
    request->request = g_object_ref(uriRequest);
    g_hash_table_insert(aboutDataRequestMap, GUINT_TO_POINTER(webkit_web_view_get_page_id(webkit_uri_scheme_request_get_web_view(request->request))), request);

    return request;
}

static AboutDataRequest *aboutDataRequestForView(guint64 pageID)
{
    return aboutDataRequestMap ? g_hash_table_lookup(aboutDataRequestMap, GUINT_TO_POINTER(pageID)) : NULL;
}

static void websiteDataRemovedCallback(WebKitWebsiteDataManager *manager, GAsyncResult *result, AboutDataRequest *dataRequest)
{
    if (webkit_website_data_manager_remove_finish(manager, result, NULL))
        webkit_web_view_reload(webkit_uri_scheme_request_get_web_view(dataRequest->request));
}

static void websiteDataClearedCallback(WebKitWebsiteDataManager *manager, GAsyncResult *result, AboutDataRequest *dataRequest)
{
    if (webkit_website_data_manager_clear_finish(manager, result, NULL))
        webkit_web_view_reload(webkit_uri_scheme_request_get_web_view(dataRequest->request));
}

static void aboutDataScriptMessageReceivedCallback(WebKitUserContentManager *userContentManager, WebKitJavascriptResult *message, WebKitWebContext *webContext)
{
    char *messageString = jsc_value_to_string(webkit_javascript_result_get_js_value(message));
    char **tokens = g_strsplit(messageString, ":", 3);
    g_free(messageString);

    unsigned tokenCount = g_strv_length(tokens);
    if (!tokens || tokenCount < 2) {
        g_strfreev(tokens);
        return;
    }

    guint64 pageID = g_ascii_strtoull(tokens[0], NULL, 10);
    AboutDataRequest *dataRequest = aboutDataRequestForView(pageID);
    if (!dataRequest) {
        g_strfreev(tokens);
        return;
    }

    WebKitWebsiteDataManager *manager = webkit_web_context_get_website_data_manager(webContext);
    guint64 types = g_ascii_strtoull(tokens[1], NULL, 10);
    if (tokenCount == 2)
        webkit_website_data_manager_clear(manager, types, 0, NULL, (GAsyncReadyCallback)websiteDataClearedCallback, dataRequest);
    else {
        guint64 domainID = g_ascii_strtoull(tokens[2], NULL, 10);
        GList *dataList = g_hash_table_lookup(dataRequest->dataMap, GUINT_TO_POINTER(types));
        WebKitWebsiteData *data = g_list_nth_data(dataList, domainID);
        if (data) {
            GList dataList = { data, NULL, NULL };
            webkit_website_data_manager_remove(manager, types, &dataList, NULL, (GAsyncReadyCallback)websiteDataRemovedCallback, dataRequest);
        }
    }
    g_strfreev(tokens);
}

static void domainListFree(GList *domains)
{
    g_list_free_full(domains, (GDestroyNotify)webkit_website_data_unref);
}

static void aboutDataFillTable(GString *result, AboutDataRequest *dataRequest, GList* dataList, const char *title, WebKitWebsiteDataTypes types, const char *dataPath, guint64 pageID)
{
    guint64 totalDataSize = 0;
    GList *domains = NULL;
    GList *l;
    for (l = dataList; l; l = g_list_next(l)) {
        WebKitWebsiteData *data = (WebKitWebsiteData *)l->data;

        if (webkit_website_data_get_types(data) & types) {
            domains = g_list_prepend(domains, webkit_website_data_ref(data));
            totalDataSize += webkit_website_data_get_size(data, types);
        }
    }
    if (!domains)
        return;

    if (!dataRequest->dataMap)
        dataRequest->dataMap = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)domainListFree);
    g_hash_table_insert(dataRequest->dataMap, GUINT_TO_POINTER(types), domains);

    if (totalDataSize) {
        char *totalDataSizeStr = g_format_size(totalDataSize);
        g_string_append_printf(result, "<h1>%s (%s)</h1>\n<table>\n", title, totalDataSizeStr);
        g_free(totalDataSizeStr);
    } else
        g_string_append_printf(result, "<h1>%s</h1>\n<table>\n", title);
    if (dataPath)
        g_string_append_printf(result, "<tr><td colspan=\"2\">Path: %s</td></tr>\n", dataPath);

    unsigned index;
    for (l = domains, index = 0; l; l = g_list_next(l), index++) {
        WebKitWebsiteData *data = (WebKitWebsiteData *)l->data;
        const char *displayName = webkit_website_data_get_name(data);
        guint64 dataSize = webkit_website_data_get_size(data, types);
        if (dataSize) {
            char *dataSizeStr = g_format_size(dataSize);
            g_string_append_printf(result, "<tr><td>%s (%s)</td>", displayName, dataSizeStr);
            g_free(dataSizeStr);
        } else
            g_string_append_printf(result, "<tr><td>%s</td>", displayName);
        g_string_append_printf(result, "<td><input type=\"button\" value=\"Remove\" onclick=\"removeData('%"G_GUINT64_FORMAT":%u:%u');\"></td></tr>\n",
            pageID, types, index);
    }
    g_string_append_printf(result, "<tr><td><input type=\"button\" value=\"Clear all\" onclick=\"clearData('%"G_GUINT64_FORMAT":%u');\"></td></tr></table>\n",
        pageID, types);
}

static void gotWebsiteDataCallback(WebKitWebsiteDataManager *manager, GAsyncResult *asyncResult, AboutDataRequest *dataRequest)
{
    GList *dataList = webkit_website_data_manager_fetch_finish(manager, asyncResult, NULL);

    GString *result = g_string_new(
        "<html><head>"
        "<script>"
        "  function removeData(domain) {"
        "    window.webkit.messageHandlers.aboutData.postMessage(domain);"
        "  }"
        "  function clearData(dataType) {"
        "    window.webkit.messageHandlers.aboutData.postMessage(dataType);"
        "  }"
        "</script></head><body>\n");

    guint64 pageID = webkit_web_view_get_page_id(webkit_uri_scheme_request_get_web_view(dataRequest->request));
    aboutDataFillTable(result, dataRequest, dataList, "Cookies", WEBKIT_WEBSITE_DATA_COOKIES, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Device Id Hash Salt", WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Memory Cache", WEBKIT_WEBSITE_DATA_MEMORY_CACHE, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Disk Cache", WEBKIT_WEBSITE_DATA_DISK_CACHE, webkit_website_data_manager_get_disk_cache_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Session Storage", WEBKIT_WEBSITE_DATA_SESSION_STORAGE, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Local Storage", WEBKIT_WEBSITE_DATA_LOCAL_STORAGE, webkit_website_data_manager_get_local_storage_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "IndexedDB Databases", WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES, webkit_website_data_manager_get_indexeddb_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Plugins Data", WEBKIT_WEBSITE_DATA_PLUGIN_DATA, NULL, pageID);
    aboutDataFillTable(result, dataRequest, dataList, "Offline Web Applications Cache", WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE, webkit_website_data_manager_get_offline_application_cache_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "HSTS Cache", WEBKIT_WEBSITE_DATA_HSTS_CACHE, webkit_website_data_manager_get_hsts_cache_directory(manager), pageID);
    aboutDataFillTable(result, dataRequest, dataList, "ITP data", WEBKIT_WEBSITE_DATA_ITP, webkit_website_data_manager_get_itp_directory(manager), pageID);

    result = g_string_append(result, "</body></html>");
    gsize streamLength = result->len;
    GInputStream *stream = g_memory_input_stream_new_from_data(g_string_free(result, FALSE), streamLength, g_free);
    webkit_uri_scheme_request_finish(dataRequest->request, stream, streamLength, "text/html");
    g_list_free_full(dataList, (GDestroyNotify)webkit_website_data_unref);
}

static void aboutDataHandleRequest(WebKitURISchemeRequest *request, WebKitWebContext *webContext)
{
    AboutDataRequest *dataRequest = aboutDataRequestNew(request);
    WebKitWebsiteDataManager *manager = webkit_web_context_get_website_data_manager(webContext);
    webkit_website_data_manager_fetch(manager, WEBKIT_WEBSITE_DATA_ALL, NULL, (GAsyncReadyCallback)gotWebsiteDataCallback, dataRequest);
}

static void aboutURISchemeRequestCallback(WebKitURISchemeRequest *request, WebKitWebContext *webContext)
{
    GInputStream *stream;
    gsize streamLength;
    const gchar *path;
    gchar *contents;
    GError *error;

    path = webkit_uri_scheme_request_get_path(request);
    if (!g_strcmp0(path, "minibrowser")) {
        contents = g_strdup_printf("<html><body><h1>WebKitGTK MiniBrowser</h1><p>The test browser of WebKitGTK</p><p>WebKit version: %d.%d.%d</p></body></html>",
            webkit_get_major_version(),
            webkit_get_minor_version(),
            webkit_get_micro_version());
        streamLength = strlen(contents);
        stream = g_memory_input_stream_new_from_data(contents, streamLength, g_free);

        webkit_uri_scheme_request_finish(request, stream, streamLength, "text/html");
        g_object_unref(stream);
    } else if (!g_strcmp0(path, "data"))
        aboutDataHandleRequest(request, webContext);
    else {
        error = g_error_new(MINI_BROWSER_ERROR, MINI_BROWSER_ERROR_INVALID_ABOUT_PATH, "Invalid about:%s page.", path);
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
    }
}

static GtkWidget *createWebViewForAutomationInWindowCallback(WebKitAutomationSession* session, GtkApplication *application)
{
    GtkWindow *window = gtk_application_get_active_window(application);
    return BROWSER_IS_WINDOW(window) ? GTK_WIDGET(browser_window_get_or_create_web_view_for_automation(BROWSER_WINDOW(window))) : NULL;
}

static GtkWidget *createWebViewForAutomationInTabCallback(WebKitAutomationSession* session, GtkApplication *application)
{
    GtkWindow *window = gtk_application_get_active_window(application);
    return BROWSER_IS_WINDOW(window) ? GTK_WIDGET(browser_window_create_web_view_in_new_tab_for_automation(BROWSER_WINDOW(window))) : NULL;
}

static void automationStartedCallback(WebKitWebContext *webContext, WebKitAutomationSession *session, GtkApplication *application)
{
    WebKitApplicationInfo *info = webkit_application_info_new();
    webkit_application_info_set_version(info, WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION);
    webkit_automation_session_set_application_info(session, info);
    webkit_application_info_unref(info);

    g_signal_connect(session, "create-web-view::window", G_CALLBACK(createWebViewForAutomationInWindowCallback), application);
    g_signal_connect(session, "create-web-view::tab", G_CALLBACK(createWebViewForAutomationInTabCallback), application);
}

typedef struct {
    GMainLoop *mainLoop;
    WebKitUserContentFilter *filter;
    GError *error;
} FilterSaveData;

static void filterSavedCallback(WebKitUserContentFilterStore *store, GAsyncResult *result, FilterSaveData *data)
{
    data->filter = webkit_user_content_filter_store_save_finish(store, result, &data->error);
    g_main_loop_quit(data->mainLoop);
}

static void startup(GApplication *application)
{
    const char *actionAccels[] = {
        "win.reload", "F5", "<Ctrl>R", NULL,
        "win.reload-no-cache", "<Ctrl>F5", "<Ctrl><Shift>R", NULL,
        "win.toggle-inspector", "<Ctrl><Shift>I", "F12", NULL,
        "win.open-private-window", "<Ctrl><Shift>P", NULL,
        "win.focus-location", "<Ctrl>L", NULL,
        "win.stop-load", "F6", "Escape", NULL,
        "win.load-homepage", "<Alt>Home", NULL,
        "win.zoom-in", "<Ctrl>plus", "<Ctrl>equal", "<Ctrl>KP_Add", NULL,
        "win.zoom-out", "<Ctrl>minus", "<Ctrl>KP_Subtract", NULL,
        "win.zoom-default", "<Ctrl>0", "<Ctrl>KP_0", NULL,
        "win.find", "<Ctrl>F", NULL,
        "win.new-tab", "<Ctrl>T", NULL,
        "win.toggle-fullscreen", "F11", NULL,
        "win.print", "<Ctrl>P", NULL,
        "win.close", "<Ctrl>W", NULL,
        "win.quit", "<Ctrl>Q", NULL,
        "find.next", "F3", "<Ctrl>G", NULL,
        "find.previous", "<Shift>F3", "<Ctrl><Shift>G", NULL,
        NULL
    };

    for (const gchar **it = actionAccels; it[0]; it += g_strv_length((gchar **)it) + 1)
        gtk_application_set_accels_for_action(GTK_APPLICATION(application), it[0], &it[1]);
}

static void activate(GApplication *application, WebKitSettings *webkitSettings)
{
    WebKitWebsiteDataManager *manager = (privateMode || automationMode) ? webkit_website_data_manager_new_ephemeral() : webkit_website_data_manager_new(NULL);
    webkit_website_data_manager_set_itp_enabled(manager, enableITP);
    WebKitWebContext *webContext = g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "website-data-manager", manager, "process-swap-on-cross-site-navigation-enabled", TRUE,
#if !GTK_CHECK_VERSION(3, 98, 0)
        "use-system-appearance-for-scrollbars", FALSE,
#endif
        NULL);
    g_object_unref(manager);

    if (cookiesPolicy) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(webContext);
        GEnumClass *enumClass = g_type_class_ref(WEBKIT_TYPE_COOKIE_ACCEPT_POLICY);
        GEnumValue *enumValue = g_enum_get_value_by_nick(enumClass, cookiesPolicy);
        if (enumValue)
            webkit_cookie_manager_set_accept_policy(cookieManager, enumValue->value);
        g_type_class_unref(enumClass);
    }

    if (cookiesFile && !webkit_web_context_is_ephemeral(webContext)) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(webContext);
        WebKitCookiePersistentStorage storageType = g_str_has_suffix(cookiesFile, ".txt") ? WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT : WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE;
        webkit_cookie_manager_set_persistent_storage(cookieManager, cookiesFile, storageType);
    }

    if (proxy) {
        WebKitNetworkProxySettings *webkitProxySettings = webkit_network_proxy_settings_new(proxy, ignoreHosts);
        webkit_web_context_set_network_proxy_settings(webContext, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, webkitProxySettings);
        webkit_network_proxy_settings_free(webkitProxySettings);
    }

    // Enable the favicon database, by specifying the default directory.
    webkit_web_context_set_favicon_database_directory(webContext, NULL);

    webkit_web_context_register_uri_scheme(webContext, BROWSER_ABOUT_SCHEME, (WebKitURISchemeRequestCallback)aboutURISchemeRequestCallback, webContext, NULL);

    WebKitUserContentManager *userContentManager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(userContentManager, "aboutData");
    g_signal_connect(userContentManager, "script-message-received::aboutData", G_CALLBACK(aboutDataScriptMessageReceivedCallback), webContext);

    WebKitWebsitePolicies *defaultWebsitePolicies = webkit_website_policies_new_with_policies(
        "autoplay", autoplayPolicy,
        NULL);

    if (contentFilter) {
        GFile *contentFilterFile = g_file_new_for_commandline_arg(contentFilter);

        FilterSaveData saveData = { NULL, NULL, NULL };
        gchar *filtersPath = g_build_filename(g_get_user_cache_dir(), g_get_prgname(), "filters", NULL);
        WebKitUserContentFilterStore *store = webkit_user_content_filter_store_new(filtersPath);
        g_free(filtersPath);

        webkit_user_content_filter_store_save_from_file(store, "GTKMiniBrowserFilter", contentFilterFile, NULL, (GAsyncReadyCallback)filterSavedCallback, &saveData);
        saveData.mainLoop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(saveData.mainLoop);
        g_object_unref(store);

        if (saveData.filter)
            webkit_user_content_manager_add_filter(userContentManager, saveData.filter);
        else
            g_printerr("Cannot save filter '%s': %s\n", contentFilter, saveData.error->message);

        g_clear_pointer(&saveData.error, g_error_free);
        g_clear_pointer(&saveData.filter, webkit_user_content_filter_unref);
        g_main_loop_unref(saveData.mainLoop);
        g_object_unref(contentFilterFile);
    }

    webkit_web_context_set_automation_allowed(webContext, automationMode);
    g_signal_connect(webContext, "automation-started", G_CALLBACK(automationStartedCallback), application);

    if (ignoreTLSErrors)
        webkit_web_context_set_tls_errors_policy(webContext, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    BrowserWindow *mainWindow = BROWSER_WINDOW(browser_window_new(NULL, webContext));
    gtk_application_add_window(GTK_APPLICATION(application), GTK_WINDOW(mainWindow));
    if (darkMode)
        g_object_set(gtk_widget_get_settings(GTK_WIDGET(mainWindow)), "gtk-application-prefer-dark-theme", TRUE, NULL);
    if (fullScreen)
        gtk_window_fullscreen(GTK_WINDOW(mainWindow));

    if (backgroundColor)
        browser_window_set_background_color(mainWindow, backgroundColor);

    GtkWidget *firstTab = NULL;
    if (uriArguments) {
        int i;

        for (i = 0; uriArguments[i]; i++) {
            WebKitWebView *webView = createBrowserTab(mainWindow, webkitSettings, userContentManager, defaultWebsitePolicies);
            if (!i)
                firstTab = GTK_WIDGET(webView);
            gchar *url = argumentToURL(uriArguments[i]);
            webkit_web_view_load_uri(webView, url);
            g_free(url);
        }
    } else {
        WebKitWebView *webView = createBrowserTab(mainWindow, webkitSettings, userContentManager, defaultWebsitePolicies);
        firstTab = GTK_WIDGET(webView);

        if (!editorMode) {
            if (sessionFile)
                browser_window_load_session(mainWindow, sessionFile);
            else if (!automationMode)
                webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
        }
    }

    g_clear_object(&webkitSettings);
    g_object_unref(webContext);
    g_object_unref(userContentManager);
    g_object_unref(defaultWebsitePolicies);

    gtk_widget_grab_focus(firstTab);
    gtk_widget_show(GTK_WIDGET(mainWindow));
}

int main(int argc, char *argv[])
{
#if ENABLE_DEVELOPER_MODE
    g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);
#endif

#if GTK_CHECK_VERSION(3, 98, 0)
    gtk_init();
#else
    gtk_init(&argc, &argv);
#endif

    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, commandLineOptions, 0);
#if !GTK_CHECK_VERSION(3, 98, 0)
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
#endif
#if ENABLE_WEB_AUDIO || ENABLE_VIDEO
    g_option_context_add_group(context, gst_init_get_option_group());
#endif

    WebKitSettings *webkitSettings = webkit_settings_new();
    webkit_settings_set_enable_developer_extras(webkitSettings, TRUE);
    webkit_settings_set_enable_webgl(webkitSettings, TRUE);
    webkit_settings_set_enable_media_stream(webkitSettings, TRUE);
    if (!addSettingsGroupToContext(context, webkitSettings))
        g_clear_object(&webkitSettings);

    GError *error = 0;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Cannot parse arguments: %s\n", error->message);
        g_error_free(error);
        g_option_context_free(context);
        g_clear_object(&webkitSettings);

        return 1;
    }
    g_option_context_free(context);

    if (printVersion) {
        g_print("WebKitGTK %u.%u.%u",
            webkit_get_major_version(),
            webkit_get_minor_version(),
            webkit_get_micro_version());
        if (g_strcmp0(SVN_REVISION, "tarball"))
            g_print(" (%s)", SVN_REVISION);
        g_print("\n");
        g_clear_object(&webkitSettings);

        return 0;
    }

    GtkApplication *application = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE);
    g_signal_connect(application, "startup", G_CALLBACK(startup), NULL);
    g_signal_connect(application, "activate", G_CALLBACK(activate), webkitSettings);
    g_application_run(G_APPLICATION(application), 0, NULL);
    g_object_unref(application);

    return 0;
}
