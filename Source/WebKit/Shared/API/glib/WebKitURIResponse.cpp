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

#include "WebKitURIResponsePrivate.h"
#include <WebCore/GUniquePtrSoup.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebCore;

/**
 * WebKitURIResponse:
 *
 * Represents an URI response.
 *
 * A #WebKitURIResponse contains information such as the URI, the
 * status code, the content length, the mime type, the HTTP status or
 * the suggested filename.
 */

enum {
    PROP_0,

    PROP_URI,
    PROP_STATUS_CODE,
    PROP_CONTENT_LENGTH,
    PROP_MIME_TYPE,
    PROP_SUGGESTED_FILENAME,
    PROP_HTTP_HEADERS
};

struct _WebKitURIResponsePrivate {
    ResourceResponse resourceResponse;
    CString uri;
    CString mimeType;
    CString suggestedFilename;
    GUniquePtr<SoupMessageHeaders> httpHeaders;
};

WEBKIT_DEFINE_TYPE(WebKitURIResponse, webkit_uri_response, G_TYPE_OBJECT)

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
    case PROP_MIME_TYPE:
        g_value_set_string(value, webkit_uri_response_get_mime_type(response));
        break;
    case PROP_SUGGESTED_FILENAME:
        g_value_set_string(value, webkit_uri_response_get_suggested_filename(response));
        break;
    case PROP_HTTP_HEADERS:
        g_value_set_boxed(value, webkit_uri_response_get_http_headers(response));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_uri_response_class_init(WebKitURIResponseClass* responseClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(responseClass);
    objectClass->get_property = webkitURIResponseGetProperty;

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
                                                      WEBKIT_PARAM_READABLE));
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

    /**
     * WebKitURIResponse:mime-type:
     *
     * The MIME type of the response.
     */
    g_object_class_install_property(objectClass,
                                    PROP_MIME_TYPE,
                                    g_param_spec_string("mime-type",
                                                        _("MIME Type"),
                                                        _("The MIME type of the response"),
                                                        0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitURIResponse:suggested-filename:
     *
     * The suggested filename for the URI response.
     */
    g_object_class_install_property(objectClass,
                                    PROP_SUGGESTED_FILENAME,
                                    g_param_spec_string("suggested-filename",
                                                        _("Suggested Filename"),
                                                        _("The suggested filename for the URI response"),
                                                        0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitURIResponse:http-headers:
     *
     * The HTTP headers of the response, or %NULL if the response is not an HTTP response.
     *
     * Since: 2.6
     */
    g_object_class_install_property(
        objectClass,
        PROP_HTTP_HEADERS,
        g_param_spec_boxed(
            "http-headers",
            _("HTTP Headers"),
            _("The HTTP headers of the response"),
            SOUP_TYPE_MESSAGE_HEADERS,
            WEBKIT_PARAM_READABLE));
}

/**
 * webkit_uri_response_get_uri:
 * @response: a #WebKitURIResponse
 *
 * Gets the URI which resulted in the response.
 *
 * Returns: response URI, as a string.
 */
const gchar* webkit_uri_response_get_uri(WebKitURIResponse* response)
{
    g_return_val_if_fail(WEBKIT_IS_URI_RESPONSE(response), 0);

    response->priv->uri = response->priv->resourceResponse.url().string().utf8();
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

    return response->priv->resourceResponse.httpStatusCode();
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

    return response->priv->resourceResponse.expectedContentLength();
}

/**
 * webkit_uri_response_get_mime_type:
 * @response: a #WebKitURIResponse
 *
 * Gets the MIME type of the response.
 *
 * Returns: MIME type, as a string.
 */
const gchar* webkit_uri_response_get_mime_type(WebKitURIResponse* response)
{
    g_return_val_if_fail(WEBKIT_IS_URI_RESPONSE(response), 0);

    response->priv->mimeType = response->priv->resourceResponse.mimeType().utf8();
    return response->priv->mimeType.data();
}

/**
 * webkit_uri_response_get_suggested_filename:
 * @response: a #WebKitURIResponse
 *
 * Get the suggested filename for @response, as specified by
 * the 'Content-Disposition' HTTP header, or %NULL if it's not
 * present.
 *
 * Returns: (transfer none): the suggested filename or %NULL if
 *    the 'Content-Disposition' HTTP header is not present.
 */
const gchar* webkit_uri_response_get_suggested_filename(WebKitURIResponse* response)
{
    g_return_val_if_fail(WEBKIT_IS_URI_RESPONSE(response), 0);

    if (response->priv->resourceResponse.suggestedFilename().isEmpty())
        return 0;

    response->priv->suggestedFilename = response->priv->resourceResponse.suggestedFilename().utf8();
    return response->priv->suggestedFilename.data();
}

/**
 * webkit_uri_response_get_http_headers:
 * @response: a #WebKitURIResponse
 *
 * Get the HTTP headers of a #WebKitURIResponse as a #SoupMessageHeaders.
 *
 * Returns: (transfer none): a #SoupMessageHeaders with the HTTP headers of @response
 *    or %NULL if @response is not an HTTP response.
 * Since: 2.6
 */
SoupMessageHeaders* webkit_uri_response_get_http_headers(WebKitURIResponse* response)
{
    g_return_val_if_fail(WEBKIT_IS_URI_RESPONSE(response), nullptr);

    if (response->priv->httpHeaders)
        return response->priv->httpHeaders.get();

    if (!response->priv->resourceResponse.url().protocolIsInHTTPFamily())
        return nullptr;

    response->priv->httpHeaders.reset(soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE));
    response->priv->resourceResponse.updateSoupMessageHeaders(response->priv->httpHeaders.get());
    return response->priv->httpHeaders.get();
}

WebKitURIResponse* webkitURIResponseCreateForResourceResponse(const WebCore::ResourceResponse& resourceResponse)
{
    WebKitURIResponse* uriResponse = WEBKIT_URI_RESPONSE(g_object_new(WEBKIT_TYPE_URI_RESPONSE, NULL));
    uriResponse->priv->resourceResponse = resourceResponse;
    return uriResponse;
}

const WebCore::ResourceResponse& webkitURIResponseGetResourceResponse(WebKitURIResponse* uriResponse)
{
    return uriResponse->priv->resourceResponse;
}

