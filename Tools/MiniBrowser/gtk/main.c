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

static const gchar **uriArguments = NULL;

static WKContextRef createWKContextWithInjectedBundle()
{
    WKStringRef bundlePath = WKStringCreateWithUTF8CString("Libraries/.libs/libMiniBrowserWebBundle.so");
    WKContextRef processContext = WKContextCreateWithInjectedBundlePath(bundlePath);
    WKContextInjectedBundleClient bundleClient = {
        kWKContextInjectedBundleClientCurrentVersion,
        0, /* clientInfo */
        0, /* didRecieveMessageFromInjectedBundle,*/
        0
    };
    WKContextSetInjectedBundleClient(processContext, &bundleClient);
    WKRelease(bundlePath);

    return processContext;
}

static gchar *argumentToURL(const char *filename)
{
    GFile *gfile = g_file_new_for_commandline_arg(filename);
    gchar *fileURL = g_file_get_uri(gfile);
    g_object_unref(gfile);

    return fileURL;
}

static void loadURI(const gchar *uri, WKContextRef processContext)
{
    WKViewRef webView = WKViewCreate(processContext, 0);
    GtkWidget *mainWindow = browser_window_new(webView);
    gchar *url = argumentToURL(uri);
    WKURLRef wkURL = WKURLCreateWithUTF8CString(url);
    g_free(url);
    WKPageLoadURL(WKViewGetPage(webView), wkURL);
    WKRelease(wkURL);

    gtk_widget_grab_focus(GTK_WIDGET(webView));
    gtk_widget_show(mainWindow);
}

static const GOptionEntry commandLineOptions[] =
{
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &uriArguments, 0, "[URL…]" },
    { 0, 0, 0, 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
    if (!g_thread_supported())
        g_thread_init(NULL);

    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, commandLineOptions, 0);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));

    GError *error = 0;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Cannot parse arguments: %s\n", error->message);
        g_error_free(error);
        g_option_context_free(context);

        return 1;
    }
    g_option_context_free (context);

    // Prefer the not installed web and plugin processes.
    g_setenv("WEBKIT_EXEC_PATH", WEBKIT_EXEC_PATH, FALSE);

    WKContextRef processContext = createWKContextWithInjectedBundle();

    if (uriArguments) {
        int i;

        for (i = 0; uriArguments[i]; i++)
            loadURI(uriArguments[i], processContext);
    } else
        loadURI("http://www.webkitgtk.org/", processContext);

    gtk_main();

    return 0;
}
