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

#include "BrowserWindow.h"
#include <WebKit2/WebKit2.h>
#include <gtk/gtk.h>

static void destroyCallback(GtkWidget *widget, GtkWidget *window)
{
    gtk_main_quit();
}

static WKViewRef createWebView()
{
    return WKViewCreate(WKContextGetSharedProcessContext(), 0);
}

static GtkWidget *createWindow(WKViewRef webView)
{
    GtkWidget *window = browser_window_new(webView);

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
    WKPageLoadURL(WKViewGetPage(webView), WKURLCreateWithUTF8CString(url));
    g_free(url);

    gtk_widget_grab_focus(GTK_WIDGET(webView));
    gtk_widget_show_all(mainWindow);
    gtk_main();

    return 0;
}
