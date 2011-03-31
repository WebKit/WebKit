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

#include <WebKit2/WebKit2.h>
#include <gtk/gtk.h>

static void activateUriEntryCallback(GtkWidget *entry, gpointer data)
{
    WKViewRef webView = g_object_get_data(G_OBJECT(entry), "web-view");
    const gchar *uri = gtk_entry_get_text(GTK_ENTRY(entry));
    WKPageLoadURL(WKViewGetPage(webView), WKURLCreateWithURL(uri));
}

static void destroyCallback(GtkWidget *widget, GtkWidget *window)
{
    gtk_main_quit();
}

static void goBackCallback(GtkWidget *widget, WKViewRef webView)
{
    WKPageGoBack(WKViewGetPage(webView));
}

static void goForwardCallback(GtkWidget *widget, WKViewRef webView)
{
    WKPageGoForward(WKViewGetPage(webView));
}

static GtkWidget *createToolbar(GtkWidget *uriEntry, WKViewRef webView)
{
    GtkWidget *toolbar = gtk_toolbar_new();

#if GTK_CHECK_VERSION(2, 15, 0)
    gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);
#else
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_HORIZONTAL);
#endif
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem *item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
    g_signal_connect(item, "clicked", G_CALLBACK(goBackCallback), (gpointer)webView);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(goForwardCallback), (gpointer)webView);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = gtk_tool_item_new();
    gtk_tool_item_set_expand(item, TRUE);
    gtk_container_add(GTK_CONTAINER(item), uriEntry);
    g_signal_connect(uriEntry, "activate", G_CALLBACK(activateUriEntryCallback), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    g_object_set_data(G_OBJECT(uriEntry), "web-view", (gpointer)webView);
    item = gtk_tool_button_new_from_stock(GTK_STOCK_OK);
    g_signal_connect_swapped(item, "clicked", G_CALLBACK(activateUriEntryCallback), (gpointer)uriEntry);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

static WKViewRef createWebView()
{
    return WKViewCreate(WKContextGetSharedProcessContext(), 0);
}

static GtkWidget *createWindow(WKViewRef webView)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_widget_set_name(window, "MiniBrowser");

    GtkWidget *uriEntry = gtk_entry_new();

    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), createToolbar(uriEntry, webView), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), WKViewGetWindow(webView), TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(destroyCallback), NULL);

    return window;
}

static gchar *argumentToURL(const char *filename)
{
    GFile *gfile = g_file_new_for_commandline_arg(filename);
    gchar *fileURL = g_file_get_uri(gfile);
    g_object_unref(gfile);

    return fileURL;
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    if (!g_thread_supported())
        g_thread_init(NULL);

    WKViewRef webView = createWebView();
    GtkWidget *mainWindow = createWindow(webView);

    gchar* url = argumentToURL(argc > 1 ? argv[1] : "http://www.webkitgtk.org/");
    WKPageLoadURL(WKViewGetPage(webView), WKURLCreateWithURL(url));
    g_free(url);

    gtk_widget_grab_focus(WKViewGetWindow(webView));
    gtk_widget_show_all(mainWindow);
    gtk_main();

    return 0;
}
