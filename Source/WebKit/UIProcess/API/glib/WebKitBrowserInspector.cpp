/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitBrowserInspector.h"

#include "BrowserInspectorPipe.h"
#include "InspectorPlaywrightAgentClientGLib.h"
#include "WebKitBrowserInspectorPrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * SECTION: WebKitBrowserInspector
 * @Short_description: Access to the WebKit browser inspector
 * @Title: WebKitBrowserInspector
 *
 * The WebKit Browser Inspector is an experimental API that provides
 * access to the inspector via the remote debugging protocol. The protocol
 * allows to create ephemeral contexts and create pages in them and then
 * manipulate them using the inspector commands. This may be useful for
 * the browser automation or remote debugging.
 *
 * Currently the protocol can be exposed to the parent process via a unix
 * pipe.
 */

enum {
    CREATE_NEW_PAGE,
    QUIT_APPLICATION,

    LAST_SIGNAL
};

struct _WebKitBrowserInspectorPrivate {
    int unused { 0 };
};

WEBKIT_DEFINE_TYPE(WebKitBrowserInspector, webkit_browser_inspector, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0, };

static void webkit_browser_inspector_class_init(WebKitBrowserInspectorClass* findClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(findClass);

    /**
     * WebKitBrowserInspector::create-new-page:
     * @inspector: the #WebKitBrowserInspector on which the signal is emitted
     *
     * Emitted when the inspector is requested to create a new page in the provided
     * #WebKitWebContext.
     *
     * This signal is emitted when inspector receives 'Browser.createPage' command
     * from its remote client. If the signal is not handled the command will fail.
     *
     * Returns: %WebKitWebView that contains created page.
     */
    signals[CREATE_NEW_PAGE] = g_signal_new(
        "create-new-page",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitBrowserInspectorClass, create_new_page),
        nullptr, nullptr,
        g_cclosure_marshal_generic,
#if PLATFORM(GTK)
        GTK_TYPE_WIDGET,
#else
        WEBKIT_TYPE_WEB_VIEW,
#endif
        1,
        WEBKIT_TYPE_WEB_CONTEXT);

    /**
     * WebKitBrowserInspector::quit-application:
     * @inspector: the #WebKitBrowserInspector on which the signal is emitted
     *
     * Emitted when the inspector is requested to close the browser application.
     *
     * This signal is emitted when inspector receives 'Browser.close' command
     * from its remote client. If the signal is not handled the command will fail.
     */
    signals[QUIT_APPLICATION] = g_signal_new(
        "quit-application",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitBrowserInspectorClass, quit_application),
        nullptr, nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

WebKit::WebPageProxy* webkitBrowserInspectorCreateNewPageInContext(WebKitWebContext* context)
{
    WebKitWebView* newWebView;
    g_signal_emit(webkit_browser_inspector_get_default(), signals[CREATE_NEW_PAGE], 0, context, &newWebView);
    if (!newWebView)
        return nullptr;
    return  &webkitWebViewGetPage(newWebView);
}

void webkitBrowserInspectorQuitApplication()
{
    g_signal_emit(webkit_browser_inspector_get_default(), signals[QUIT_APPLICATION], 0, NULL);
}

static gpointer createWebKitBrowserInspector(gpointer)
{
    static GRefPtr<WebKitBrowserInspector> browserInspector = adoptGRef(WEBKIT_BROWSER_INSPECTOR(g_object_new(WEBKIT_TYPE_BROWSER_INSPECTOR, nullptr)));
    return browserInspector.get();
}

/**
 * webkit_browser_inspector_get_default:
 *
 * Gets the default instance of the browser inspector.
 *
 * Returns: (transfer none): a #WebKitBrowserInspector
 */
WebKitBrowserInspector* webkit_browser_inspector_get_default(void)
{
    static GOnce onceInit = G_ONCE_INIT;
    return WEBKIT_BROWSER_INSPECTOR(g_once(&onceInit, createWebKitBrowserInspector, 0));
}

/**
 * webkit_browser_inspector_initialize_pipe:
 *
 * Creates browser inspector and configures pipe handler to communicate with
 * the parent process.
 */
void webkit_browser_inspector_initialize_pipe(const char* defaultProxyURI, const char* const* ignoreHosts)
{
    WebKit::initializeBrowserInspectorPipe(makeUnique<WebKit::InspectorPlaywrightAgentClientGlib>(defaultProxyURI, ignoreHosts));
}
