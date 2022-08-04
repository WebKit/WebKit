/*
 * Copyright (C) 2021 Zixing Liu
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
#include "WebKitURISchemeResponsePrivate.h"

#include "WebKitPrivate.h"
#include <WebCore/GUniquePtrSoup.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

enum {
    PROP_0,
    PROP_STREAM,
    PROP_STREAM_LENGTH,
    N_PROPERTIES,
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

using namespace WebKit;
using namespace WebCore;

/**
 * WebKitURISchemeResponse:
 *
 * Represents a URI scheme response.
 *
 * If you register a particular URI scheme in a #WebKitWebContext,
 * using webkit_web_context_register_uri_scheme(), you have to provide
 * a #WebKitURISchemeRequestCallback. After that, when a URI response
 * is made with that particular scheme, your callback will be
 * called. There you will be able to provide more response parameters
 * when the methods and properties of a #WebKitURISchemeRequest is not
 * enough.
 *
 * When you finished setting up your #WebKitURISchemeResponse, call
 * webkit_uri_request_finish_with_response() with it to return the response.
 */

struct _WebKitURISchemeResponsePrivate {
    GRefPtr<GInputStream> stream;
    uint64_t streamLength;

    int statusCode { -1 };
    CString statusMessage;
    CString contentType;
    GUniquePtr<SoupMessageHeaders> headers;
};

WEBKIT_DEFINE_TYPE(WebKitURISchemeResponse, webkit_uri_scheme_response, G_TYPE_OBJECT)

static void webkitURISchemeResponseSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitURISchemeResponse* response = WEBKIT_URI_SCHEME_RESPONSE(object);

    switch (propId) {
    case PROP_STREAM:
        response->priv->stream = G_INPUT_STREAM(g_value_get_object(value));
        break;
    case PROP_STREAM_LENGTH: {
        gint64 streamLength = g_value_get_int64(value);
        // We use -1 in the API for consistency with soup when the content length is not known, but 0 internally.
        response->priv->streamLength = streamLength == -1 ? 0 : streamLength;
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_uri_scheme_response_class_init(WebKitURISchemeResponseClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->set_property = webkitURISchemeResponseSetProperty;

    /**
     * WebKitURISchemeResponse:stream:
     *
     * The input stream to read from.
     *
     * Since: 2.36
     */
    sObjProperties[PROP_STREAM] = 
        g_param_spec_object(
            "stream",
            _("Input stream"),
            _("The input stream to read from."),
            G_TYPE_INPUT_STREAM,
            static_cast<GParamFlags>(WEBKIT_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitURISchemeResponse:stream-length:
     *
     * The input stream length in bytes, `-1` for unknown length.
     *
     * Since: 2.36
     */
    sObjProperties[PROP_STREAM_LENGTH] = 
        g_param_spec_int64(
            "stream-length",
            _("Input stream length"),
            _("The input stream length in bytes. -1 for unknown length."),
            -1, INT64_MAX, -1,
            static_cast<GParamFlags>(WEBKIT_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);
}

// Private getters
int webKitURISchemeResponseGetStatusCode(const WebKitURISchemeResponse* response)
{
    return response->priv->statusCode;
}

GInputStream* webKitURISchemeResponseGetStream(const WebKitURISchemeResponse* response)
{
    return response->priv->stream.get();
}

const CString& webKitURISchemeResponseGetStatusMessage(const WebKitURISchemeResponse* response)
{
    return response->priv->statusMessage;
}

const CString& webKitURISchemeResponseGetContentType(const WebKitURISchemeResponse* response)
{
    return response->priv->contentType;
}

uint64_t webKitURISchemeResponseGetStreamLength(const WebKitURISchemeResponse* response)
{
    return response->priv->streamLength;
}

SoupMessageHeaders* webKitURISchemeResponseGetHeaders(WebKitURISchemeResponse* response)
{
    return response->priv->headers.get();
}

/**
 * webkit_uri_scheme_response_new:
 * @input_stream: a #GInputStream to read the contents of the request
 * @stream_length: the length of the stream or -1 if not known
 *
 * Create a new #WebKitURISchemeResponse
 *
 * Returns: (transfer full): the newly created #WebKitURISchemeResponse.
 *
 * Since: 2.36
 */
WebKitURISchemeResponse* webkit_uri_scheme_response_new(GInputStream* inputStream, gint64 streamLength)
{
    g_return_val_if_fail(G_IS_INPUT_STREAM(inputStream), nullptr);
    g_return_val_if_fail(streamLength == -1 || streamLength >= 0, nullptr);

    return WEBKIT_URI_SCHEME_RESPONSE(g_object_new(WEBKIT_TYPE_URI_SCHEME_RESPONSE, "stream", inputStream, "stream-length", streamLength, nullptr));
}

/**
 * webkit_uri_scheme_response_set_content_type:
 * @response: a #WebKitURISchemeResponse
 * @content_type: the content type of the stream
 *
 * Sets the content type for the @response
 *
 * Since: 2.36
 */
void webkit_uri_scheme_response_set_content_type(WebKitURISchemeResponse* response, const gchar* contentType)
{
    g_return_if_fail(WEBKIT_IS_URI_SCHEME_RESPONSE(response));

    response->priv->contentType = contentType;
}

/**
 * webkit_uri_scheme_response_set_http_headers:
 * @response: a #WebKitURISchemeResponse
 * @headers: (transfer full): the HTTP headers to be set
 *
 * Assign the provided #SoupMessageHeaders to the response.
 *
 * @headers need to be of the type %SOUP_MESSAGE_HEADERS_RESPONSE.
 * Any existing headers will be overwritten.
 *
 * Since: 2.36
 */
void webkit_uri_scheme_response_set_http_headers(WebKitURISchemeResponse* response, SoupMessageHeaders* headers)
{
    g_return_if_fail(WEBKIT_IS_URI_SCHEME_RESPONSE(response));
    g_return_if_fail(soup_message_headers_get_headers_type(headers) == SOUP_MESSAGE_HEADERS_RESPONSE);

    response->priv->headers.reset(headers);
}

/**
 * webkit_uri_scheme_response_set_status:
 * @response: a #WebKitURISchemeResponse
 * @status_code: the HTTP status code to be returned
 * @reason_phrase: (allow-none): a reason phrase
 *
 * Sets the status code and reason phrase for the @response.
 *
 * If @status_code is a known value and @reason_phrase is %NULL, the @reason_phrase will be set automatically.
 *
 * Since: 2.36
 */
void webkit_uri_scheme_response_set_status(WebKitURISchemeResponse* response, guint statusCode, const gchar* statusMessage)
{
    g_return_if_fail(WEBKIT_IS_URI_SCHEME_RESPONSE(response));

    response->priv->statusCode = static_cast<gint>(statusCode);
    if (statusMessage)
        response->priv->statusMessage = statusMessage;
    else
        response->priv->statusMessage = soup_status_get_phrase(statusCode);
}
