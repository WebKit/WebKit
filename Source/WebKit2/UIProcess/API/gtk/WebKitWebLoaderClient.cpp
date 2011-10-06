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

    PROP_WEB_VIEW,
    PROP_ESTIMATED_PROGRESS
};

struct _WebKitWebLoaderClientPrivate {
    GRefPtr<WebKitWebView> view;

    gdouble estimatedProgress;
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
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[PROVISIONAL_LOAD_FAILED], 0, resourceError.failingURL().utf8().data(),
                  webError.get(), &returnValue);
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
    g_signal_emit(WEBKIT_WEB_LOADER_CLIENT(clientInfo), signals[LOAD_FAILED], 0, resourceError.failingURL().utf8().data(),
                  webError.get(), &returnValue);
}

static void didChangeProgress(WKPageRef page, const void* clientInfo)
{
    WebKitWebLoaderClient* client = WEBKIT_WEB_LOADER_CLIENT(clientInfo);
    gdouble estimatedProgress = WKPageGetEstimatedProgress(page);

    if (client->priv->estimatedProgress == estimatedProgress)
        return;

    client->priv->estimatedProgress = estimatedProgress;
    g_object_notify(G_OBJECT(clientInfo), "estimated-progress");
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
        didChangeProgress, // didStartProgress
        didChangeProgress,
        didChangeProgress, // didFinishProgress
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

static gboolean webkitWebLoaderClientLoadFailed(WebKitWebLoaderClient* client, const gchar* failingURI, GError* error)
{
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)
        || g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)
        || g_error_matches(error, WEBKIT_PLUGIN_ERROR, WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD))
        return FALSE;

    GOwnPtr<char> htmlString(g_strdup_printf("<html><body>%s</body></html>", error->message));
    webkit_web_view_load_alternate_html(client->priv->view.get(), htmlString.get(), 0, failingURI);

    return TRUE;
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
    case PROP_ESTIMATED_PROGRESS:
        g_value_set_double(value, webkit_web_loader_client_get_estimated_progress(client));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
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

    objectClass->set_property = webkitWebLoaderClientSetProperty;
    objectClass->get_property = webkitWebLoaderClientGetProperty;
    objectClass->constructed = webkitWebLoaderClientConstructed;
    objectClass->finalize = webkitWebLoaderClientFinalize;

    clientClass->provisional_load_failed = webkitWebLoaderClientLoadFailed;
    clientClass->load_failed = webkitWebLoaderClientLoadFailed;

    /**
     * WebKitWebLoaderClient:web-view:
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
     * WebKitWebLoaderClient:estimated-progress:
     *
     * An estimate of the percent completion for the current loading operation.
     * This value will range from 0.0 to 1.0 and, once a load completes,
     * will remain at 1.0 until a new load starts, at which point it
     * will be reset to 0.0.
     * The value is an estimate based on the total number of bytes expected
     * to be received for a document, including all its possible subresources.
     */
    g_object_class_install_property(objectClass,
                                    PROP_ESTIMATED_PROGRESS,
                                    g_param_spec_double("estimated-progress",
                                                        "Estimated Progress",
                                                        "An estimate of the percent completion for a document load",
                                                        0.0, 1.0, 0.0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebLoaderClient::provisional-load-started:
     * @loader_client: the #WebKitWebLoaderClient
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
     * @loader_client: the #WebKitWebLoaderClient
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
     * @loader_client: the #WebKitWebLoaderClient
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
                     webkit_marshal_BOOLEAN__STRING_POINTER,
                     G_TYPE_BOOLEAN, 2,
                     G_TYPE_STRING,
                     G_TYPE_POINTER);

    /**
     * WebKitWebLoaderClient::load-committed:
     * @loader_client: the #WebKitWebLoaderClient
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
     * @loader_client: the #WebKitWebLoaderClient
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
     * @loader_client: the #WebKitWebLoaderClient
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
                     webkit_marshal_BOOLEAN__STRING_POINTER,
                     G_TYPE_BOOLEAN, 2,
                     G_TYPE_STRING,
                     G_TYPE_POINTER);

    g_type_class_add_private(clientClass, sizeof(WebKitWebLoaderClientPrivate));
}

/**
 * webkit_web_loader_client_get_estimated_progress:
 * @client: a #WebKitWebLoaderClient
 *
 * Gets the value of #WebKitWebLoaderClient:estimated-progress.
 * You can monitor the estimated progress of a load operation by
 * connecting to the ::notify signal of @client.
 *
 * Returns: an estimate of the of the percent complete for a document
 *     load as a range from 0.0 to 1.0.
 */
gdouble webkit_web_loader_client_get_estimated_progress(WebKitWebLoaderClient* client)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_LOADER_CLIENT(client), 0.);

    return client->priv->estimatedProgress;
}
