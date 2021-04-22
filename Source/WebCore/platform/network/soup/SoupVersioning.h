/*
 * Copyright (C) 2021 Igalia S.L.
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

#pragma once

#include <libsoup/soup.h>

#if USE(SOUP2)

static inline const char*
soup_message_get_method(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), nullptr);
    return message->method;
}

static inline const char*
soup_server_message_get_method(SoupMessage* message)
{
    return soup_message_get_method(message);
}

static inline SoupStatus
soup_message_get_status(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), SOUP_STATUS_NONE);
    return static_cast<SoupStatus>(message->status_code);
}

static inline void
soup_server_message_set_status(SoupMessage* message, unsigned statusCode, const char* reasonPhrase)
{
    if (reasonPhrase)
        soup_message_set_status_full(message, statusCode, reasonPhrase);
    else
        soup_message_set_status(message, statusCode);
}

static inline const char*
soup_message_get_reason_phrase(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), nullptr);
    return message->reason_phrase;
}

static inline SoupMessageHeaders*
soup_message_get_request_headers(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), nullptr);
    return message->request_headers;
}

static inline SoupMessageHeaders*
soup_server_message_get_request_headers(SoupMessage* message)
{
    return soup_message_get_request_headers(message);
}

static inline SoupMessageHeaders*
soup_message_get_response_headers(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), nullptr);
    return message->response_headers;
}

static inline SoupMessageHeaders*
soup_server_message_get_response_headers(SoupMessage* message)
{
    return soup_message_get_response_headers(message);
}

static inline SoupMessageBody*
soup_server_message_get_response_body(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), nullptr);
    return message->response_body;
}

static inline void
soup_server_message_set_response(SoupMessage* message, const char* contentType, SoupMemoryUse memoryUse, const char* responseBody, gsize length)
{
    return soup_message_set_response(message, contentType, memoryUse, responseBody, length);
}

static inline SoupURI*
soup_server_message_get_uri(SoupMessage* message)
{
    return soup_message_get_uri(message);
}

static inline GTlsCertificate*
soup_message_get_tls_peer_certificate(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), nullptr);
    GTlsCertificate* certificate = nullptr;
    soup_message_get_https_status(message, &certificate, nullptr);
    return certificate;
}

static inline GTlsCertificateFlags
soup_message_get_tls_peer_certificate_errors(SoupMessage* message)
{
    g_return_val_if_fail(SOUP_IS_MESSAGE(message), static_cast<GTlsCertificateFlags>(0));
    GTlsCertificateFlags flags = static_cast<GTlsCertificateFlags>(0);
    soup_message_get_https_status(message, nullptr, &flags);
    return flags;
}

static inline void
soup_session_send_async(SoupSession* session, SoupMessage* message, int, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    soup_session_send_async(session, message, cancellable, callback, userData);
}

static inline void
soup_session_websocket_connect_async(SoupSession* session, SoupMessage* message, const char* origin, char** protocols, int, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    soup_session_websocket_connect_async(session, message, origin, protocols, cancellable, callback, userData);
}

static inline void
soup_auth_cancel(SoupAuth*)
{
}

static inline void
soup_session_set_proxy_resolver(SoupSession* session, GProxyResolver* resolver)
{
    g_object_set(session, "proxy-resolver", resolver, nullptr);
}

static inline GProxyResolver*
soup_session_get_proxy_resolver(SoupSession* session)
{
    GRefPtr<GProxyResolver> resolver;
    g_object_get(session, "proxy-resolver", &resolver.outPtr(), nullptr);
    return resolver.get();
}

static inline void
soup_session_set_accept_language(SoupSession* session, const char* acceptLanguage)
{
    g_object_set(session, "accept-language", acceptLanguage, nullptr);
}
#endif // USE(SOUP2)
