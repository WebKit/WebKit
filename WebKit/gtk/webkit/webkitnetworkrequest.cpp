/*
 * Copyright (C) 2007, 2008 Holger Hans Peter Freyther
 * Copyright (C) 2009 Gustavo Noronha Silva
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
#include "webkitnetworkrequest.h"

#include "CString.h"
#include "GOwnPtr.h"
#include "ResourceRequest.h"
#include "webkitprivate.h"

#include <glib/gi18n-lib.h>

namespace WTF {

template <> void freeOwnedGPtr<SoupMessage>(SoupMessage* soupMessage)
{
    if (soupMessage)
        g_object_unref(soupMessage);
}

}

/**
 * SECTION:webkitnetworkrequest
 * @short_description: The target of a navigation request
 * @see_also: #WebKitWebView::navigation-requested
 *
 * This class represents the network related aspects of a navigation
 * request. Currently this is only the uri of the target. In the future
 * the state of the web form might be added.
 * Currently this object is only used along with the
 * #WebKitWebView::navigation-requested signal.
 *
 */

G_DEFINE_TYPE(WebKitNetworkRequest, webkit_network_request, G_TYPE_OBJECT);

struct _WebKitNetworkRequestPrivate {
    gchar* uri;
    SoupMessage* message;
};

#define WEBKIT_NETWORK_REQUEST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_NETWORK_REQUEST, WebKitNetworkRequestPrivate))

static void webkit_network_request_finalize(GObject* object)
{
    WebKitNetworkRequest* request = WEBKIT_NETWORK_REQUEST(object);
    WebKitNetworkRequestPrivate* priv = request->priv;

    g_free(priv->uri);

    if (priv->message) {
        g_object_unref(priv->message);
        priv->message = NULL;
    }

    G_OBJECT_CLASS(webkit_network_request_parent_class)->finalize(object);
}

static void webkit_network_request_class_init(WebKitNetworkRequestClass* requestClass)
{
    G_OBJECT_CLASS(requestClass)->finalize = webkit_network_request_finalize;

    g_type_class_add_private(requestClass, sizeof(WebKitNetworkRequestPrivate));
}

static void webkit_network_request_init(WebKitNetworkRequest* request)
{
    WebKitNetworkRequestPrivate* priv = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(request);
    request->priv = priv;
}

// for internal use only
WebKitNetworkRequest* webkit_network_request_new_with_core_request(const WebCore::ResourceRequest& resourceRequest)
{
    WebKitNetworkRequest* request = WEBKIT_NETWORK_REQUEST(g_object_new(WEBKIT_TYPE_NETWORK_REQUEST, NULL));
    WebKitNetworkRequestPrivate* priv = request->priv;

    GOwnPtr<SoupMessage> soupMessage(resourceRequest.toSoupMessage());
    if (soupMessage)
        priv->message = SOUP_MESSAGE(g_object_ref(soupMessage.get()));
    priv->uri = g_strdup(resourceRequest.url().string().utf8().data());

    return request;
}

/**
 * webkit_network_request_new:
 * @uri: an URI
 *
 * Creates a new #WebKitNetworkRequest initialized with an URI.
 *
 * Returns: a new #WebKitNetworkRequest, or %NULL if the URI is
 * invalid.
 */
WebKitNetworkRequest* webkit_network_request_new(const gchar* uri)
{
    g_return_val_if_fail(uri, NULL);

    WebKitNetworkRequest* request = WEBKIT_NETWORK_REQUEST(g_object_new(WEBKIT_TYPE_NETWORK_REQUEST, NULL));
    WebKitNetworkRequestPrivate* priv = request->priv;

    priv->message = soup_message_new("GET", uri);
    if (!priv->message)
        return NULL;

    priv->uri = g_strdup(uri);

    return request;
}

/**
 * webkit_network_request_set_uri:
 * @request: a #WebKitNetworkRequest
 * @uri: an URI
 *
 * Sets the URI held and used by the given request.
 */
void webkit_network_request_set_uri(WebKitNetworkRequest* request, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_NETWORK_REQUEST(request));
    g_return_if_fail(uri);

    WebKitNetworkRequestPrivate* priv = request->priv;

    g_free(priv->uri);
    priv->uri = g_strdup(uri);
}

/**
 * webkit_network_request_get_uri:
 * @request: a #WebKitNetworkRequest
 *
 * Returns: the uri of the #WebKitNetworkRequest
 *
 * Since: 1.0.0
 */
G_CONST_RETURN gchar* webkit_network_request_get_uri(WebKitNetworkRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_NETWORK_REQUEST(request), NULL);

    WebKitNetworkRequestPrivate* priv = request->priv;
    return priv->uri;
}

// NOT YET API; For internal use only, for now
// webkit_network_request_get_soup_message:
// @request: a #WebKitNetworkRequest
//
// Obtains the #SoupMessage held and used by the given request. Notice
// that modification of the SoupMessage of a request by signal
// handlers is only supported where explicitly documented.
//
// Returns: the #SoupMessage
SoupMessage* webkit_network_request_get_message(WebKitNetworkRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_NETWORK_REQUEST(request), NULL);

    WebKitNetworkRequestPrivate* priv = request->priv;

    return priv->message;
}
