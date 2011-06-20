/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2011 Lukasz Slachciak
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <webkit/webkit.h>

static gint windowCount = 0;

static GtkWidget* createWindow(WebKitWebView** outWebView);

static void activateUriEntryCb(GtkWidget* entry, gpointer data)
{
    WebKitWebView *webView = g_object_get_data(G_OBJECT(entry), "web-view");
    const gchar* uri = gtk_entry_get_text(GTK_ENTRY(entry));
    g_assert(uri);
    webkit_web_view_load_uri(webView, uri);
}

#ifndef WEBKIT2
static void updateTitle(GtkWindow* window, WebKitWebView* webView)
{
    GString *string = g_string_new(webkit_web_view_get_title(webView));
    gdouble loadProgress = webkit_web_view_get_progress(webView) * 100;
    g_string_append(string, " - WebKit Launcher");
    if (loadProgress < 100)
        g_string_append_printf(string, " (%f%%)", loadProgress);
    gchar *title = g_string_free(string, FALSE);
    gtk_window_set_title(window, title);
    g_free(title);
}

static void linkHoverCb(WebKitWebView* page, const gchar* title, const gchar* link, GtkStatusbar* statusbar)
{
    guint statusContextId =
      GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(statusbar), "link-hover-context"));
    /* underflow is allowed */
    gtk_statusbar_pop(statusbar, statusContextId);
    if (link)
        gtk_statusbar_push(statusbar, statusContextId, link);
}

static void notifyTitleCb(WebKitWebView* webView, GParamSpec* pspec, GtkWidget* window)
{
    updateTitle(GTK_WINDOW(window), webView);
}

static void notifyLoadStatusCb(WebKitWebView* webView, GParamSpec* pspec, GtkWidget* uriEntry)
{
    if (webkit_web_view_get_load_status(webView) == WEBKIT_LOAD_COMMITTED) {
        WebKitWebFrame *frame = webkit_web_view_get_main_frame(webView);
        const gchar *uri = webkit_web_frame_get_uri(frame);
        if (uri)
            gtk_entry_set_text(GTK_ENTRY(uriEntry), uri);
    }
}

static void notifyProgressCb(WebKitWebView* webView, GParamSpec* pspec, GtkWidget* window)
{
    updateTitle(GTK_WINDOW(window), webView);
}
#endif

static void destroyCb(GtkWidget* widget, GtkWidget* window)
{
    if (g_atomic_int_dec_and_test(&windowCount))
      gtk_main_quit();
}

static void goBackCb(GtkWidget* widget,  WebKitWebView* webView)
{
    webkit_web_view_go_back(webView);
}

static void goForwardCb(GtkWidget* widget, WebKitWebView* webView)
{
    webkit_web_view_go_forward(webView);
}

#ifndef WEBKIT2
static WebKitWebView*
createWebViewCb(WebKitWebView* webView, WebKitWebFrame* web_frame, GtkWidget* window)
{
    WebKitWebView *newWebView;
    createWindow(&newWebView);
    return newWebView;
}

static gboolean webViewReadyCb(WebKitWebView* webView, GtkWidget* window)
{
    gtk_widget_grab_focus(GTK_WIDGET(webView));
    gtk_widget_show_all(window);
    return FALSE;
}

static gboolean closeWebViewCb(WebKitWebView* webView, GtkWidget* window)
{
    gtk_widget_destroy(window);
    return TRUE;
}
#endif

static GtkWidget* createBrowser(GtkWidget* window, GtkWidget* uriEntry, GtkWidget* statusbar, WebKitWebView* webView)
{
    GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(scrolledWindow), GTK_WIDGET(webView));

#ifndef WEBKIT2
    g_signal_connect(webView, "notify::title", G_CALLBACK(notifyTitleCb), window);
    g_signal_connect(webView, "notify::load-status", G_CALLBACK(notifyLoadStatusCb), uriEntry);
    g_signal_connect(webView, "notify::progress", G_CALLBACK(notifyProgressCb), window);
    g_signal_connect(webView, "hovering-over-link", G_CALLBACK(linkHoverCb), statusbar);
    g_signal_connect(webView, "create-web-view", G_CALLBACK(createWebViewCb), window);
    g_signal_connect(webView, "web-view-ready", G_CALLBACK(webViewReadyCb), window);
    g_signal_connect(webView, "close-web-view", G_CALLBACK(closeWebViewCb), window);
#endif

    return scrolledWindow;
}

static GtkWidget* createStatusbar()
{
    GtkStatusbar *statusbar = GTK_STATUSBAR(gtk_statusbar_new());
    guint statusContextId = gtk_statusbar_get_context_id(statusbar, "Link Hover");
    g_object_set_data(G_OBJECT(statusbar), "link-hover-context",
        GUINT_TO_POINTER(statusContextId));

    return GTK_WIDGET(statusbar);
}

static GtkWidget* createToolbar(GtkWidget* uriEntry, WebKitWebView* webView)
{
    GtkWidget *toolbar = gtk_toolbar_new();

#if GTK_CHECK_VERSION(2, 15, 0)
    gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);
#else
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_HORIZONTAL);
#endif
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem *item;

    /* the back button */
    item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
    g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(goBackCb), webView);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    /* The forward button */
    item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(goForwardCb), webView);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    /* The URL entry */
    item = gtk_tool_item_new();
    gtk_tool_item_set_expand(item, TRUE);
    gtk_container_add(GTK_CONTAINER(item), uriEntry);
    g_signal_connect(G_OBJECT(uriEntry), "activate", G_CALLBACK(activateUriEntryCb), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    /* The go button */
    g_object_set_data(G_OBJECT(uriEntry), "web-view", webView);
    item = gtk_tool_button_new_from_stock(GTK_STOCK_OK);
    g_signal_connect_swapped(G_OBJECT(item), "clicked", G_CALLBACK(activateUriEntryCb), (gpointer)uriEntry);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

static GtkWidget* createWindow(WebKitWebView** outWebView)
{
    WebKitWebView *webView;
    GtkWidget *vbox;
    GtkWidget *window;
    GtkWidget *uriEntry;
    GtkWidget *statusbar;

    g_atomic_int_inc(&windowCount);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_widget_set_name(window, "GtkLauncher");

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    uriEntry = gtk_entry_new();

    vbox = gtk_vbox_new(FALSE, 0);
    statusbar = createStatusbar(webView);
    gtk_box_pack_start(GTK_BOX(vbox), createToolbar(uriEntry, webView), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), createBrowser(window, uriEntry, statusbar, webView), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(destroyCb), NULL);

    if (outWebView)
        *outWebView = webView;

    return window;
}

static gchar* filenameToURL(const char* filename)
{
    if (!g_file_test(filename, G_FILE_TEST_EXISTS))
        return 0;

    GFile *gfile = g_file_new_for_path(filename);
    gchar *fileURL = g_file_get_uri(gfile);
    g_object_unref(gfile);

    return fileURL;
}

#ifndef WEBKIT2
gboolean parseOptionEntryCallback(const gchar *optionNameFull, const gchar *value, gpointer data, GError **error)
{
    WebKitWebSettings *webkitSettings = (WebKitWebSettings *)data;

    g_assert(webkitSettings);

    if (strlen(optionNameFull) <= 2)
        return FALSE;

    /* We have two -- in option name so remove them. */
    const gchar *optionName = optionNameFull + 2;
    GParamSpec *spec = g_object_class_find_property(G_OBJECT_GET_CLASS(webkitSettings), optionName);
    if (!spec)
        return FALSE;

    /* Convert string to proper type. */
    GValue valueString = {0, {{0}}};
    GValue valueProperty = {0, {{0}}};
    g_value_init(&valueString, G_TYPE_STRING);
    g_value_init(&valueProperty, G_PARAM_SPEC_VALUE_TYPE(spec));
    g_value_set_static_string(&valueString, value);
    if (!g_value_transform(&valueString, &valueProperty))
        return FALSE;

    /* Set WebKitWebSettings properties. */
    g_object_set_property(G_OBJECT(webkitSettings), optionName, &valueProperty);

    return TRUE;
}

static GArray* getOptionEntriesFromWebKitWebSettings(WebKitWebSettings *webkitSettings)
{
    GParamSpec **propertySpecs;
    GArray *optionEntriesArray;
    guint numProperties, i;

    propertySpecs = g_object_class_list_properties(G_OBJECT_GET_CLASS(webkitSettings), &numProperties);
    if (!propertySpecs)
        return 0;

    optionEntriesArray = g_array_new(TRUE, TRUE, sizeof(GOptionEntry));
    if (!optionEntriesArray) {
        g_free(propertySpecs);
        return 0;
    }

    for (i = 0; i < numProperties; i++) {
        GParamSpec *param = propertySpecs[i];

        /* Fill in structures only for writable properties. */
        if (!param || !(param->flags & G_PARAM_WRITABLE))
            continue;

        GType gParamType = G_PARAM_SPEC_VALUE_TYPE(param);
        if (gParamType == G_TYPE_BOOLEAN || gParamType == G_TYPE_STRING || gParamType == G_TYPE_INT
            || gParamType == G_TYPE_FLOAT) {
            GOptionEntry optionEntry;
            optionEntry.long_name = g_param_spec_get_name(param);
            /* There is no easy way to figure our short name for generated option entries.
               optionEntry.short_name=*/
            /* For bool arguments "enable" type make option argument not required. */
            if (gParamType == G_TYPE_BOOLEAN && (strstr(optionEntry.long_name, "enable")))
                optionEntry.flags = G_OPTION_FLAG_OPTIONAL_ARG;
            optionEntry.arg = G_OPTION_ARG_CALLBACK;
            optionEntry.arg_data = parseOptionEntryCallback;
            optionEntry.description = g_param_spec_get_blurb(param);
            optionEntry.arg_description = g_type_name(gParamType);
            g_array_append_val(optionEntriesArray, optionEntry);
        }
    }
    g_free(propertySpecs);

    return optionEntriesArray;
}

static void transformStringToBoolean(const GValue *srcValue, GValue *destValue)
{
    const char* strValue = g_value_get_string(srcValue);
    if (strValue) {
        if (!g_ascii_strcasecmp(strValue, "true") || !strcmp(strValue, "1"))
            g_value_set_boolean(destValue, TRUE);
        else
            g_value_set_boolean(destValue, FALSE);
    } else /* When no option value provided, set "TRUE" by default. */
        g_value_set_boolean(destValue, TRUE);
}

static void transformStringToInt(const GValue *srcValue, GValue *destValue)
{
    g_value_set_int(destValue, atoi(g_value_get_string(srcValue)));
}

static void transformStringToFloat(const GValue *srcValue, GValue *destValue)
{
    g_value_set_float(destValue, atof(g_value_get_string(srcValue)));
}

static gboolean parseAdditionalOptions(WebKitWebView *webView, int argc, char* argv[])
{
    g_value_register_transform_func(G_TYPE_STRING, G_TYPE_BOOLEAN, transformStringToBoolean);
    g_value_register_transform_func(G_TYPE_STRING, G_TYPE_INT, transformStringToInt);
    g_value_register_transform_func(G_TYPE_STRING, G_TYPE_FLOAT, transformStringToFloat);


    WebKitWebSettings *webkitSettings = webkit_web_view_get_settings(webView);
    GArray *optionEntriesArray = getOptionEntriesFromWebKitWebSettings(webkitSettings);

    GOptionGroup *webSettingsGroup = g_option_group_new("websettings",
                                                        "WebKitWebSettings writable properties for default WebKitWebView",
                                                        "WebKitWebSettings properties",
                                                        webkitSettings,
                                                        NULL);
    g_option_group_add_entries(webSettingsGroup, (GOptionEntry*) optionEntriesArray->data);

    GOptionContext *context = g_option_context_new("[URL]");
    g_option_context_add_group(context, webSettingsGroup);

    GError *error = 0;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("Failed to parse arguments: %s\n", error->message);
        g_error_free(error);
        g_option_context_free(context);
        g_array_free(optionEntriesArray, TRUE);
        return FALSE;
    }
    g_option_context_free(context);
    g_array_free(optionEntriesArray, TRUE);
    return TRUE;
}
#endif

int main(int argc, char* argv[])
{
    WebKitWebView *webView;
    GtkWidget *main_window;

    gtk_init(&argc, &argv);
    if (!g_thread_supported())
        g_thread_init(NULL);

#ifndef WEBKIT2
#ifdef SOUP_TYPE_PROXY_RESOLVER_DEFAULT
    soup_session_add_feature_by_type(webkit_get_default_session(), SOUP_TYPE_PROXY_RESOLVER_DEFAULT);
#else
    const char *httpProxy = g_getenv("http_proxy");
    if (httpProxy) {
        SoupURI *proxyUri = soup_uri_new(httpProxy);
        g_object_set(webkit_get_default_session(), SOUP_SESSION_PROXY_URI, proxyUri, NULL);
        soup_uri_free(proxyUri);
    }
#endif
#endif

    main_window = createWindow(&webView);

#ifndef WEBKIT2
    if (!parseAdditionalOptions(webView, argc, argv))
        return 1;
#endif

    gchar *uri =(gchar*)(argc > 1 ? argv[1] : "http://www.google.com/");
    gchar *fileURL = filenameToURL(uri);

    webkit_web_view_load_uri(webView, fileURL ? fileURL : uri);
    g_free(fileURL);

    gtk_widget_grab_focus(GTK_WIDGET(webView));
    gtk_widget_show_all(main_window);
    gtk_main();

    return 0;
}
