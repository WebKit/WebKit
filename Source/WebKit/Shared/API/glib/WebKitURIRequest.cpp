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

#include "WebKitURIRequestPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/GUniquePtrSoup.h>
#include <wtf/glib/WTFGType.h>
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
    const char* httpMethod;
    GUniquePtr<SoupMessageHeaders> httpHeaders;
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
        webkit_uri_request_set_uri(request, g_value_get_string(value));
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
                                                        "about:blank",
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT)));
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

/**
 * webkit_uri_request_set_uri:
 * @request: a #WebKitURIRequest
 * @uri: an URI
 *
 * Set the URI of @request
 */
void webkit_uri_request_set_uri(WebKitURIRequest* request, const char* uri)
{
    g_return_if_fail(WEBKIT_IS_URI_REQUEST(request));
    g_return_if_fail(uri);

    URL url(URL(), uri);
    if (url == request->priv->resourceRequest.url())
        return;

    request->priv->resourceRequest.setURL(url);
    g_object_notify(G_OBJECT(request), "uri");
}

/**
 * webkit_uri_request_get_http_headers:
 * @request: a #WebKitURIRequest
 *
 * Get the HTTP headers of a #WebKitURIRequest as a #SoupMessageHeaders.
 *
 * Returns: (transfer none): a #SoupMessageHeaders with the HTTP headers of @request
 *    or %NULL if @request is not an HTTP request.
 */
SoupMessageHeaders* webkit_uri_request_get_http_headers(WebKitURIRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_URI_REQUEST(request), 0);

    if (request->priv->httpHeaders)
        return request->priv->httpHeaders.get();

    if (!request->priv->resourceRequest.url().protocolIsInHTTPFamily())
        return 0;

    request->priv->httpHeaders.reset(soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST));
    request->priv->resourceRequest.updateSoupMessageHeaders(request->priv->httpHeaders.get());
    return request->priv->httpHeaders.get();
}

/**
 * webkit_uri_request_get_http_method:
 * @request: a #WebKitURIRequest
 *
 * Get the HTTP method of the #WebKitURIRequest.
 *
 * Returns: the HTTP method of the #WebKitURIRequest or %NULL if @request is not
 *    an HTTP request.
 *
 * Since: 2.12
 */
const gchar* webkit_uri_request_get_http_method(WebKitURIRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_URI_REQUEST(request), nullptr);

    if (!request->priv->resourceRequest.url().protocolIsInHTTPFamily())
        return nullptr;

    if (request->priv->resourceRequest.httpMethod().isEmpty())
        return nullptr;

    if (!request->priv->httpMethod)
        request->priv->httpMethod = g_intern_string(request->priv->resourceRequest.httpMethod().utf8().data());
    return request->priv->httpMethod;
}

WebKitURIRequest* webkitURIRequestCreateForResourceRequest(const ResourceRequest& resourceRequest)
{
    WebKitURIRequest* uriRequest = WEBKIT_URI_REQUEST(g_object_new(WEBKIT_TYPE_URI_REQUEST, NULL));
    uriRequest->priv->resourceRequest = resourceRequest;
    return uriRequest;
}

void webkitURIRequestGetResourceRequest(WebKitURIRequest* request, ResourceRequest& resourceRequest)
{
    resourceRequest = request->priv->resourceRequest;
    if (request->priv->httpHeaders)
        resourceRequest.updateFromSoupMessageHeaders(request->priv->httpHeaders.get());
}
