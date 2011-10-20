/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitWebLoaderClient.h"

#include "WebKitError.h"
#include "WebKitMarshal.h"
#include "WebKitPrivate.h"
#include "WebKitWebView.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

enum {
    PROVISIONAL_LOAD_STARTED,
    PROVISIONAL_LOAD_RECEIVED_SERVER_REDIRECT,
    PROVISIONAL_LOAD_FAILED,

    LOAD_COMMITTED,
    LOAD_FINISHED,
    LOAD_FAILED,

    LAST_SIGNAL
};

struct _WebKitWebLoaderClientPrivate {
    GRefPtr<WebKitWebView> view;
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitWebLoaderClient, webkit_web_loader_client, G_TYPE_OBJECT)

static void didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    gboolean returnValue;
    WebKitWebView* webView = WEBKIT_WEB_VIEW(toImpl(page)->viewWidget());
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[PROVISIONAL_LOAD_STARTED], 0, webView, &returnValue);
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    gboolean returnValue;
    WebKitWebView* webView = WEBKIT_WEB_VIEW(toImpl(page)->viewWidget());
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[PROVISIONAL_LOAD_RECEIVED_SERVER_REDIRECT], 0, webView, &returnValue);
}

static void didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    const ResourceError& resourceError = toImpl(error)->platformError();
    GOwnPtr<GError> webError(g_error_new_literal(g_quark_from_string(resourceError.domain().utf8().data()),
                                                 resourceError.errorCode(),
                                                 resourceError.localizedDescription().utf8().data()));
    gboolean returnValue;
    WebKitWebView* webView = WEBKIT_WEB_VIEW(toImpl(page)->viewWidget());
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[PROVISIONAL_LOAD_FAILED], 0, webView, resourceError.failingURL().utf8().data(),
                  webError.get(), &returnValue);
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    gboolean returnValue;
    WebKitWebView* webView = WEBKIT_WEB_VIEW(toImpl(page)->viewWidget());
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[LOAD_COMMITTED], 0, webView,  &returnValue);
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    gboolean returnValue;
    WebKitWebView* webView = WEBKIT_WEB_VIEW(toImpl(page)->viewWidget());
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[LOAD_FINISHED], 0, webView, &returnValue);
}

static void didFailLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    const ResourceError& resourceError = toImpl(error)->platformError();
    GOwnPtr<GError> webError(g_error_new_literal(g_quark_from_string(resourceError.domain().utf8().data()),
                                                 resourceError.errorCode(),
                                                 resourceError.localizedDescription().utf8().data()));
    gboolean returnValue;
    WebKitWebView* webView = WEBKIT_WEB_VIEW(toImpl(page)->viewWidget());
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[LOAD_FAILED], 0, webView,
                  resourceError.failingURL().utf8().data(), webError.get(), &returnValue);
}

static void didChangeProgress(WKPageRef page, const void* clientInfo)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(toImpl(page)->viewWidget());
    webkitWebViewSetEstimatedLoadProgress(webView, WKPageGetEstimatedProgress(page));
}

void attachLoaderClientToPage(WKPageRef page, WebKitWebLoaderClient* client)
{
    WKPageLoaderClient loaderClient = {
        kWKPageLoaderClientCurrentVersion,
        client, // clientInfo
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        didFailProvisionalLoadWithErrorForFrame,
        didCommitLoadForFrame,
        0, // didFinishDocumentLoadForFrame
        didFinishLoadForFrame,
        didFailLoadWithErrorForFrame,
        0, // didSameDocumentNavigationForFrame
        0, // didReceiveTitleForFrame
        0, // didFirstLayoutForFrame
        0, // didFirstVisuallyNonEmptyLayoutForFrame
        0, // didRemoveFrameFromHierarchy
        0, // didDisplayInsecureContentForFrame
        0, // didRunInsecureContentForFrame
        0, // canAuthenticateAgainstProtectionSpaceInFrame
        0, // didReceiveAuthenticationChallengeInFrame
        didChangeProgress, // didStartProgress
        didChangeProgress,
        didChangeProgress, // didFinishProgress
        0, // didBecomeUnresponsive
        0, // didBecomeResponsive
        0, // processDidCrash
        0, // didChangeBackForwardList
        0, // shouldGoToBackForwardListItem
        0 // didFailToInitializePlugin
    };
    WKPageSetPageLoaderClient(page, &loaderClient);
}

static gboolean webkitWebLoaderClientLoadFailed(WebKitWebLoaderClient* client, WebKitWebView* webView, const gchar* failingURI, GError* error)
{
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)
        || g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)
        || g_error_matches(error, WEBKIT_PLUGIN_ERROR, WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD))
        return FALSE;

    GOwnPtr<char> htmlString(g_strdup_printf("<html><body>%s</body></html>", error->message));
    webkit_web_view_load_alternate_html(webView, htmlString.get(), 0, failingURI);

    return TRUE;
}

static void webkitWebLoaderClientFinalize(GObject* object)
{
    WEBKIT_WEB_LOADER_CLIENT(object)->priv->~WebKitWebLoaderClientPrivate();
    G_OBJECT_CLASS(webkit_web_loader_client_parent_class)->finalize(object);
}

static void webkit_web_loader_client_init(WebKitWebLoaderClient* client)
{
    WebKitWebLoaderClientPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(client, WEBKIT_TYPE_WEB_LOADER_CLIENT, WebKitWebLoaderClientPrivate);
    client->priv = priv;
    new (priv) WebKitWebLoaderClientPrivate();
}

static void webkit_web_loader_client_class_init(WebKitWebLoaderClientClass* clientClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(clientClass);
    objectClass->finalize = webkitWebLoaderClientFinalize;
    clientClass->provisional_load_failed = webkitWebLoaderClientLoadFailed;
    clientClass->load_failed = webkitWebLoaderClientLoadFailed;

    /**
     * WebKitWebLoaderClient::provisional-load-started:
     * @loader_client: the #WebKitWebLoaderClient
     * @web_view: the #WebKitWebView associated with this event
     *
     * This signal is emitted when new load request is made.
     * No data has been received yet, empty structures have
     * been allocated to perform the load; the load may still
     * fail for transport issues such as not being able to
     * resolve a name, or connect to a port.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     */
    signals[PROVISIONAL_LOAD_STARTED] =
        g_signal_new("provisional-load-started",
                     G_TYPE_FROM_CLASS(objectClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebLoaderClientClass, provisional_load_started),
                     g_signal_accumulator_true_handled, NULL,
                     webkit_marshal_BOOLEAN__OBJECT,
                     G_TYPE_BOOLEAN, 1,
                     WEBKIT_TYPE_WEB_VIEW);

    /**
     * WebKitWebLoaderClient::provisional-load-received-server-redirect:
     * @loader_client: the #WebKitWebLoaderClient
     * @web_view: the #WebKitWebView associated with this event
     *
     * This signal is emitted when a provisional data source
     * receives a server redirect.
     *
     * This signal is emitted after #WebKitWebLoaderClient::provisional-load-started.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     */
    signals[PROVISIONAL_LOAD_RECEIVED_SERVER_REDIRECT] =
        g_signal_new("provisional-load-received-server-redirect",
                     G_TYPE_FROM_CLASS(objectClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebLoaderClientClass, provisional_load_received_server_redirect),
                     g_signal_accumulator_true_handled, NULL,
                     webkit_marshal_BOOLEAN__OBJECT,
                     G_TYPE_BOOLEAN, 1,
                     WEBKIT_TYPE_WEB_VIEW);

    /**
     * WebKitWebLoaderClient::provisional-load-failed:
     * @loader_client: the #WebKitWebLoaderClient
     * @web_view: the #WebKitWebView associated with this event
     * @error: the #GError that was triggered
     *
     * This signal is emitted when an error occurs when starting to
     * load data for a page. By default, if the signal is not handled,
     * a stock error page will be displayed. You need to handle the signal
     * if you want to provide your own error page. This signal finishes the
     * load so no other signals will be emitted after this one.
     *
     * This signal is emitted after #WebKitWebLoaderClient::provisional-load-started.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     */
    signals[PROVISIONAL_LOAD_FAILED] =
        g_signal_new("provisional-load-failed",
                     G_TYPE_FROM_CLASS(objectClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebLoaderClientClass, provisional_load_failed),
                     g_signal_accumulator_true_handled, NULL,
                     webkit_marshal_BOOLEAN__OBJECT_STRING_POINTER,
                     G_TYPE_BOOLEAN, 3,
                     WEBKIT_TYPE_WEB_VIEW,
                     G_TYPE_STRING,
                     G_TYPE_POINTER);

    /**
     * WebKitWebLoaderClient::load-committed:
     * @loader_client: the #WebKitWebLoaderClient
     * @web_view: the #WebKitWebView associated with this event
     *
     * This signal is emitted when content starts arriving for a page load.
     * The necessary transport requirements are stabilished, and the
     * load is being performed.
     *
     * This signal is emitted after #WebKitWebLoaderClient::provisional-load-started.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     */
    signals[LOAD_COMMITTED] =
        g_signal_new("load-committed",
                     G_TYPE_FROM_CLASS(objectClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebLoaderClientClass, load_committed),
                     g_signal_accumulator_true_handled, NULL,
                     webkit_marshal_BOOLEAN__OBJECT,
                     G_TYPE_BOOLEAN, 1,
                     WEBKIT_TYPE_WEB_VIEW);
    /**
     * WebKitWebLoaderClient::load-finished:
     * @loader_client: the #WebKitWebLoaderClient
     * @web_view: the #WebKitWebView associated with this event
     *
     * This signal is emitted when a page load completes, that is, when all
     * the resources are done loading with no error. In case of errors
     * during loading, the load finishes when either
     * #WebKitWebLoaderClient::provisional-load-failed or #WebKitWebLoaderClient::load-failed
     * are emitted.
     *
     * This signal is emitted after #WebKitWebLoaderClient::load-committed.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     */
    signals[LOAD_FINISHED] =
        g_signal_new("load-finished",
                     G_TYPE_FROM_CLASS(objectClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebLoaderClientClass, load_finished),
                     g_signal_accumulator_true_handled, NULL,
                     webkit_marshal_BOOLEAN__OBJECT,
                     G_TYPE_BOOLEAN, 1,
                     WEBKIT_TYPE_WEB_VIEW);

    /**
     * WebKitWebLoaderClient::load-failed:
     * @loader_client: the #WebKitWebLoaderClient
     * @web_view: the #WebKitWebView associated with this event
     * @error: the #GError that was triggered
     *
     * This signal is emitted when an error occurs loading a committed data source.
     * By default, if the signal is not handled, a stock error page will be displayed.
     * You need to handle the signal if you want to provide your own error page. This
     * signal finishes the load so no other signals will be emitted after this one.
     *
     * This signal is emitted after #WebKitWebLoaderClient::load-committed.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     */
    signals[LOAD_FAILED] =
        g_signal_new("load-failed",
                     G_TYPE_FROM_CLASS(objectClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebLoaderClientClass, load_failed),
                     g_signal_accumulator_true_handled, NULL,
                     webkit_marshal_BOOLEAN__OBJECT_STRING_POINTER,
                     G_TYPE_BOOLEAN, 3,
                     WEBKIT_TYPE_WEB_VIEW,
                     G_TYPE_STRING,
                     G_TYPE_POINTER);

    g_type_class_add_private(clientClass, sizeof(WebKitWebLoaderClientPrivate));
}
