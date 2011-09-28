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

#include "WebKitMarshal.h"
#include "WebKitPrivate.h"
#include "WebKitWebView.h"
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

enum {
    PROP_0,

    PROP_WEB_VIEW
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
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[PROVISIONAL_LOAD_STARTED], 0, &returnValue);
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    gboolean returnValue;
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[PROVISIONAL_LOAD_RECEIVED_SERVER_REDIRECT], 0, &returnValue);
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
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[PROVISIONAL_LOAD_FAILED], 0, webError.get(), &returnValue);
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    gboolean returnValue;
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[LOAD_COMMITTED], 0, &returnValue);
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    gboolean returnValue;
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[LOAD_FINISHED], 0, &returnValue);
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
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[LOAD_FAILED], 0, webError.get(), &returnValue);
}

static void webkitWebLoaderClientConstructed(GObject* object)
{
    WebKitWebLoaderClient* client = WEBKIT_WEB_LOADER_CLIENT(object);
    WebKitWebLoaderClientPrivate* priv = client->priv;

    WKPageLoaderClient loaderClient = {
        kWKPageLoaderClientCurrentVersion,
        client,  // clientInfo
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        didFailProvisionalLoadWithErrorForFrame,
        didCommitLoadForFrame,
        0,       // didFinishDocumentLoadForFrame
        didFinishLoadForFrame,
        didFailLoadWithErrorForFrame,
        0,       // didSameDocumentNavigationForFrame
        0,       // didReceiveTitleForFrame
        0,       // didFirstLayoutForFrame
        0,       // didFirstVisuallyNonEmptyLayoutForFrame
        0,       // didRemoveFrameFromHierarchy
        0,       // didDisplayInsecureContentForFrame
        0,       // didRunInsecureContentForFrame
        0,       // canAuthenticateAgainstProtectionSpaceInFrame
        0,       // didReceiveAuthenticationChallengeInFrame
        0,       // didStartProgress
        0,       // didChangeProgress
        0,       // didFinishProgress
        0,       // didBecomeUnresponsive
        0,       // didBecomeResponsive
        0,       // processDidCrash
        0,       // didChangeBackForwardList
        0,       // shouldGoToBackForwardListItem
        0        // didFailToInitializePlugin
    };
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(priv->view.get()));
    WKPageSetPageLoaderClient(toAPI(page), &loaderClient);
}

static void webkitWebLoaderClientSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebLoaderClient* client = WEBKIT_WEB_LOADER_CLIENT(object);

    switch (propId) {
    case PROP_WEB_VIEW:
        client->priv->view = adoptGRef(WEBKIT_WEB_VIEW(g_value_dup_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebLoaderClientGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebLoaderClient* client = WEBKIT_WEB_LOADER_CLIENT(object);

    switch (propId) {
    case PROP_WEB_VIEW:
        g_value_set_object(value, client->priv->view.get());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
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

    objectClass->set_property = webkitWebLoaderClientSetProperty;
    objectClass->get_property = webkitWebLoaderClientGetProperty;
    objectClass->constructed = webkitWebLoaderClientConstructed;

    /**
     * WebKitWebView:web-view:
     *
     * The #WebKitWebView of the loader client.
     */
    g_object_class_install_property(objectClass,
                                    PROP_WEB_VIEW,
                                    g_param_spec_object("web-view",
                                                        "Web View",
                                                        "The web view for the loader client",
                                                        WEBKIT_TYPE_WEB_VIEW,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebLoaderClient::provisional-load-started:
     * @loader_client: the #WebKitWebLoader
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
                     webkit_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0,
                     G_TYPE_NONE);

    /**
     * WebKitWebLoaderClient::provisional-load-received-server-redirect:
     * @loader_client: the #WebKitWebLoader
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
                     webkit_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0,
                     G_TYPE_NONE);

    /**
     * WebKitWebLoaderClient::provisional-load-failed:
     * @loader_client: the #WebKitWebLoader
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
                     webkit_marshal_BOOLEAN__POINTER,
                     G_TYPE_BOOLEAN, 1,
                     G_TYPE_POINTER);

    /**
     * WebKitWebLoaderClient::load-committed:
     * @loader_client: the #WebKitWebLoader
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
                     webkit_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0,
                     G_TYPE_NONE);
    /**
     * WebKitWebLoaderClient::load-finished:
     * @loader_client: the #WebKitWebLoader
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
                     webkit_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0,
                     G_TYPE_NONE);

    /**
     * WebKitWebLoaderClient::load-failed:
     * @loader_client: the #WebKitWebLoader
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
                     webkit_marshal_BOOLEAN__POINTER,
                     G_TYPE_BOOLEAN, 1,
                     G_TYPE_POINTER);

    g_type_class_add_private(clientClass, sizeof(WebKitWebLoaderClientPrivate));
}
