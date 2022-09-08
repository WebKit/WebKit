/*
 * Copyright (C) 2012 Samsung Electronics Ltd. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/GtkVersioning.h>
#include <string.h>
#include <sys/eventfd.h>
#include <wtf/glib/GRefPtr.h>

#if USE(GTK4)
#include <webkit/webkit.h>
#else
#include <webkit2/webkit2.h>
#endif

int main(int argc, char** argv)
{
    g_setenv("WEBKIT_EXEC_PATH", WEBKIT_EXEC_PATH, FALSE);
    g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);

    gtk_init(&argc, &argv);

    int eventFD = argc == 2 ? atoi(argv[1]) : -1;

    WebKitWebView* webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    webkit_settings_set_enable_developer_extras(webkit_web_view_get_settings(webView), TRUE);
    g_signal_connect(webView, "load-changed", G_CALLBACK(+[](WebKitWebView*, WebKitLoadEvent loadEvent, int* eventFD) {
        if (loadEvent != WEBKIT_LOAD_FINISHED)
            return;

        if (*eventFD != -1) {
            uint64_t event = 1;
            write(*eventFD, &event, sizeof(uint64_t));
            close(*eventFD);
        }
    }), &eventFD);

    webkit_web_view_load_html(webView,
        "<html><body><p>WebKitGTK Inspector Test Server</p></body></html>",
        "http://127.0.0.1:2999/");

#if USE(GTK4)
    GtkWidget* window = gtk_window_new();
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(webView));
    gtk_widget_show(window);

    GRefPtr<GMainLoop> loop = adoptGRef(g_main_loop_new(nullptr, TRUE));
    g_main_loop_run(loop.get());
#else
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webView));
    gtk_widget_show_all(window);

    g_signal_connect(window, "delete-event", G_CALLBACK(gtk_main_quit), 0);

    gtk_main();
#endif
}
