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
#include "WebKitNetworkRequest.h"

#include "WebKitPrivate.h"
#include "WebURLRequest.h"
#include <glib/gi18n-lib.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

enum {
    PROP_0,

    PROP_URI
};

using namespace WebCore;

G_DEFINE_TYPE(WebKitNetworkRequest, webkit_network_request, G_TYPE_OBJECT)

struct _WebKitNetworkRequestPrivate {
    CString uri;
};

static void webkitNetworkRequestFinalize(GObject* object)
{
    WEBKIT_NETWORK_REQUEST(object)->priv->~WebKitNetworkRequestPrivate();
    G_OBJECT_CLASS(webkit_network_request_parent_class)->finalize(object);
}

static void webkitNetworkRequestGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitNetworkRequest* request = WEBKIT_NETWORK_REQUEST(object);

    switch (propId) {
    case PROP_URI:
        g_value_set_string(value, webkit_network_request_get_uri(request));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitNetworkRequestSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitNetworkRequest* request = WEBKIT_NETWORK_REQUEST(object);

    switch (propId) {
    case PROP_URI:
        request->priv->uri = g_value_get_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_network_request_class_init(WebKitNetworkRequestClass* requestClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(requestClass);

    objectClass->finalize = webkitNetworkRequestFinalize;
    objectClass->get_property = webkitNetworkRequestGetProperty;
    objectClass->set_property = webkitNetworkRequestSetProperty;

    /**
     * WebKitNetworkRequest:uri:
     *
     * The URI to which the request will be made.
     */
    g_object_class_install_property(objectClass, PROP_URI,
                                    g_param_spec_string("uri",
                                                        _("URI"),
                                                        _("The URI to which the request will be made."),
                                                        0,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    g_type_class_add_private(requestClass, sizeof(WebKitNetworkRequestPrivate));
}

static void webkit_network_request_init(WebKitNetworkRequest* request)
{
    WebKitNetworkRequestPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(request, WEBKIT_TYPE_NETWORK_REQUEST, WebKitNetworkRequestPrivate);
    request->priv = priv;
    new (priv) WebKitNetworkRequestPrivate();
}

/**
 * webkit_network_request_new:
 * @uri: an URI
 *
 * Creates a new #WebKitNetworkRequest for the given URI.
 *
 * Returns: a new #WebKitNetworkRequest
 */
WebKitNetworkRequest* webkit_network_request_new(const gchar* uri)
{
    g_return_val_if_fail(uri, 0);

    return WEBKIT_NETWORK_REQUEST(g_object_new(WEBKIT_TYPE_NETWORK_REQUEST, "uri", uri, NULL));
}

/**
 * webkit_network_request_get_uri:
 * @request: a #WebKitNetworkRequest
 *
 * Returns: the uri of the #WebKitNetworkRequest
 */
const gchar* webkit_network_request_get_uri(WebKitNetworkRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_NETWORK_REQUEST(request), 0);

    return request->priv->uri.data();
}

