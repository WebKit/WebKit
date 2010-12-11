/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

static GtkWidget* createBrowser(GtkWidget* window, GtkWidget* uriEntry, GtkWidget* statusbar, WebKitWebView* webView)
{
    GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(scrolledWindow), GTK_WIDGET(webView));

    g_signal_connect(webView, "notify::title", G_CALLBACK(notifyTitleCb), window);
    g_signal_connect(webView, "notify::load-status", G_CALLBACK(notifyLoadStatusCb), uriEntry);
    g_signal_connect(webView, "notify::progress", G_CALLBACK(notifyProgressCb), window);
    g_signal_connect(webView, "hovering-over-link", G_CALLBACK(linkHoverCb), statusbar);
    g_signal_connect(webView, "create-web-view", G_CALLBACK(createWebViewCb), window);
    g_signal_connect(webView, "web-view-ready", G_CALLBACK(webViewReadyCb), window);
    g_signal_connect(webView, "close-web-view", G_CALLBACK(closeWebViewCb), window);

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

int main(int argc, char* argv[])
{
    WebKitWebView *webView;
    GtkWidget *main_window;

    gtk_init(&argc, &argv);
    if (!g_thread_supported())
        g_thread_init(NULL);

    main_window = createWindow(&webView);

    gchar *uri =(gchar*)(argc > 1 ? argv[1] : "http://www.google.com/");
    gchar *fileURL = filenameToURL(uri);

    webkit_web_view_load_uri(webView, fileURL ? fileURL : uri);
    g_free(fileURL);

    gtk_widget_grab_focus(GTK_WIDGET(webView));
    gtk_widget_show_all(main_window);
    gtk_main();

    return 0;
}
