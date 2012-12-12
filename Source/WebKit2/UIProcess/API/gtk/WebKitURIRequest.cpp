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
#include "WebKitURIRequest.h"

#include "WebKitPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/text/CString.h>

enum {
    PROP_0,

    PROP_URI
};

using namespace WebCore;

/**
 * SECTION: WebKitURIRequest
 * @Short_description: Represents a URI request
 * @Title: WebKitURIRequest
 *
 * A #WebKitURIRequest can be created with a URI using the
 * webkit_uri_request_new() method, and you can get the URI of an
 * existing request with the webkit_uri_request_get_uri() one.
 *
 */

struct _WebKitURIRequestPrivate {
    WebCore::ResourceRequest resourceRequest;
    CString uri;
};

WEBKIT_DEFINE_TYPE(WebKitURIRequest, webkit_uri_request, G_TYPE_OBJECT)

static void webkitURIRequestGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitURIRequest* request = WEBKIT_URI_REQUEST(object);

    switch (propId) {
    case PROP_URI:
        g_value_set_string(value, webkit_uri_request_get_uri(request));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitURIRequestSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitURIRequest* request = WEBKIT_URI_REQUEST(object);

    switch (propId) {
    case PROP_URI:
        request->priv->resourceRequest.setURL(KURL(KURL(), g_value_get_string(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_uri_request_class_init(WebKitURIRequestClass* requestClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(requestClass);
    objectClass->get_property = webkitURIRequestGetProperty;
    objectClass->set_property = webkitURIRequestSetProperty;

    /**
     * WebKitURIRequest:uri:
     *
     * The URI to which the request will be made.
     */
    g_object_class_install_property(objectClass, PROP_URI,
                                    g_param_spec_string("uri",
                                                        _("URI"),
                                                        _("The URI to which the request will be made."),
                                                        0,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

/**
 * webkit_uri_request_new:
 * @uri: an URI
 *
 * Creates a new #WebKitURIRequest for the given URI.
 *
 * Returns: a new #WebKitURIRequest
 */
WebKitURIRequest* webkit_uri_request_new(const gchar* uri)
{
    g_return_val_if_fail(uri, 0);

    return WEBKIT_URI_REQUEST(g_object_new(WEBKIT_TYPE_URI_REQUEST, "uri", uri, NULL));
}

/**
 * webkit_uri_request_get_uri:
 * @request: a #WebKitURIRequest
 *
 * Returns: the uri of the #WebKitURIRequest
 */
const gchar* webkit_uri_request_get_uri(WebKitURIRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_URI_REQUEST(request), 0);

    request->priv->uri = request->priv->resourceRequest.url().string().utf8();
    return request->priv->uri.data();
}

WebKitURIRequest* webkitURIRequestCreateForResourceRequest(const WebCore::ResourceRequest& resourceRequest)
{
    WebKitURIRequest* uriRequest = WEBKIT_URI_REQUEST(g_object_new(WEBKIT_TYPE_URI_REQUEST, NULL));
    uriRequest->priv->resourceRequest = resourceRequest;
    return uriRequest;
}

const WebCore::ResourceRequest& webkitURIRequestGetResourceRequest(WebKitURIRequest* uriRequest)
{
    return uriRequest->priv->resourceRequest;
}
