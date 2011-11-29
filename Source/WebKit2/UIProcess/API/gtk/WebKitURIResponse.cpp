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
#include "WebKitURIResponse.h"

#include "WebKitPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebURLResponse.h"
#include <glib/gi18n-lib.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

enum {
    PROP_0,

    PROP_URI,
    PROP_STATUS_CODE,
    PROP_CONTENT_LENGTH
};

using namespace WebCore;

G_DEFINE_TYPE(WebKitURIResponse, webkit_uri_response, G_TYPE_OBJECT)

struct _WebKitURIResponsePrivate {
    CString uri;
    GRefPtr<SoupMessage> message;
    guint64 contentLength;
};

static void webkitURIResponseFinalize(GObject* object)
{
    WEBKIT_URI_RESPONSE(object)->priv->~WebKitURIResponsePrivate();
    G_OBJECT_CLASS(webkit_uri_response_parent_class)->finalize(object);
}

static void webkitURIResponseGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitURIResponse* response = WEBKIT_URI_RESPONSE(object);

    switch (propId) {
    case PROP_URI:
        g_value_set_string(value, webkit_uri_response_get_uri(response));
        break;
    case PROP_STATUS_CODE:
        g_value_set_uint(value, webkit_uri_response_get_status_code(response));
        break;
    case PROP_CONTENT_LENGTH:
        g_value_set_uint64(value, webkit_uri_response_get_content_length(response));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitURIResponseSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitURIResponse* response = WEBKIT_URI_RESPONSE(object);

    switch (propId) {
    case PROP_URI:
        response->priv->uri = g_value_get_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_uri_response_class_init(WebKitURIResponseClass* responseClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(responseClass);

    objectClass->finalize = webkitURIResponseFinalize;
    objectClass->get_property = webkitURIResponseGetProperty;
    objectClass->set_property = webkitURIResponseSetProperty;

    /**
     * WebKitURIResponse:uri:
     *
     * The URI for which the response was made.
     */
    g_object_class_install_property(objectClass,
                                    PROP_URI,
                                    g_param_spec_string("uri",
                                                        _("URI"),
                                                        _("The URI for which the response was made."),
                                                        0,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    /**
     * WebKitURIResponse:status-code:
     *
     * The status code of the response as returned by the server.
     */
    g_object_class_install_property(objectClass,
                                    PROP_STATUS_CODE,
                                    g_param_spec_uint("status-code",
                                                      _("Status Code"),
                                                      _("The status code of the response as returned by the server."),
                                                      0, G_MAXUINT, SOUP_STATUS_NONE,
                                                      WEBKIT_PARAM_READABLE));

    /**
     * WebKitURIResponse:content-length:
     *
     * The expected content length of the response.
     */
    g_object_class_install_property(objectClass,
                                    PROP_CONTENT_LENGTH,
                                    g_param_spec_uint64("content-length",
                                                        _("Content Length"),
                                                        _("The expected content length of the response."),
                                                        0, G_MAXUINT64, 0,
                                                        WEBKIT_PARAM_READABLE));

    g_type_class_add_private(responseClass, sizeof(WebKitURIResponsePrivate));
}

static void webkit_uri_response_init(WebKitURIResponse* response)
{
    WebKitURIResponsePrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(response, WEBKIT_TYPE_URI_RESPONSE, WebKitURIResponsePrivate);
    response->priv = priv;
    new (priv) WebKitURIResponsePrivate();
}

/**
 * webkit_uri_response_get_uri:
 * @response: a #WebKitURIResponse
 *
 * Returns: the uri of the #WebKitURIResponse
 */
const gchar* webkit_uri_response_get_uri(WebKitURIResponse* response)
{
    g_return_val_if_fail(WEBKIT_IS_URI_RESPONSE(response), 0);

    return response->priv->uri.data();
}

/**
 * webkit_uri_response_get_status_code:
 * @response: a #WebKitURIResponse
 *
 * Get the status code of the #WebKitURIResponse as returned by
 * the server. It will normally be a #SoupKnownStatusCode, for
 * example %SOUP_STATUS_OK, though the server can respond with any
 * unsigned integer.
 *
 * Returns: the status code of @response
 */
guint webkit_uri_response_get_status_code(WebKitURIResponse* response)
{
    g_return_val_if_fail(WEBKIT_IS_URI_RESPONSE(response), SOUP_STATUS_NONE);

    if (!response->priv->message)
        return SOUP_STATUS_NONE;

    return response->priv->message->status_code;
}

/**
 * webkit_uri_response_get_content_length:
 * @response: a #WebKitURIResponse
 *
 * Get the expected content length of the #WebKitURIResponse. It can
 * be 0 if the server provided an incorrect or missing Content-Length.
 *
 * Returns: the expected content length of @response.
 */
guint64 webkit_uri_response_get_content_length(WebKitURIResponse* response)
{
    g_return_val_if_fail(WEBKIT_IS_URI_RESPONSE(response), 0);

    if (response->priv->contentLength)
        return response->priv->contentLength;

    if (!response->priv->message)
        return 0;

    SoupMessage* message = response->priv->message.get();
    return static_cast<guint64>(soup_message_headers_get_content_length(message->response_headers));
}

WebKitURIResponse* webkitURIResponseCreateForSoupMessage(SoupMessage* message)
{
    GOwnPtr<char> uri(soup_uri_to_string(soup_message_get_uri(message), FALSE));
    WebKitURIResponse* response = WEBKIT_URI_RESPONSE(g_object_new(WEBKIT_TYPE_URI_RESPONSE, "uri", uri.get(), NULL));
    response->priv->message = message;
    return response;
}

SoupMessage* webkitURIResponseGetSoupMessage(WebKitURIResponse* response)
{
    return response->priv->message.get();
}

void webkitURIResponseSetContentLength(WebKitURIResponse* response, guint64 contentLength)
{
    response->priv->contentLength = contentLength;
}
