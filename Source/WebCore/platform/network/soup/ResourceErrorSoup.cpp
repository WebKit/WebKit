/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY IGALIA S.L. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceError.h"

#if USE(SOUP)

#include "LocalizedStrings.h"
#include "URLSoup.h"
#include <libsoup/soup.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

#if USE(SOUP2)
#define SOUP_HTTP_ERROR_DOMAIN SOUP_HTTP_ERROR
#else
#define SOUP_HTTP_ERROR_DOMAIN SOUP_SESSION_ERROR
#endif

ResourceError ResourceError::transportError(const URL& failingURL, int statusCode, const String& reasonPhrase)
{
    return ResourceError(String::fromLatin1(g_quark_to_string(SOUP_HTTP_ERROR_DOMAIN)), statusCode, failingURL, reasonPhrase);
}

ResourceError ResourceError::httpError(SoupMessage* message, GError* error)
{
    ASSERT(message);
#if USE(SOUP2)
    if (SOUP_STATUS_IS_TRANSPORT_ERROR(message->status_code))
        return transportError(soupURIToURL(soup_message_get_uri(message)), message->status_code, String::fromUTF8(message->reason_phrase));
#endif
    return genericGError(soupURIToURL(soup_message_get_uri(message)), error);
}

ResourceError ResourceError::authenticationError(SoupMessage* message)
{
    ASSERT(message);
#if USE(SOUP2)
    return ResourceError(String::fromLatin1(g_quark_to_string(SOUP_HTTP_ERROR_DOMAIN)), message->status_code,
        soupURIToURL(soup_message_get_uri(message)), String::fromUTF8(message->reason_phrase));
#else
    return ResourceError(String::fromLatin1(g_quark_to_string(SOUP_SESSION_ERROR)), soup_message_get_status(message),
        soup_message_get_uri(message), String::fromUTF8(soup_message_get_reason_phrase(message)));
#endif
}

ResourceError ResourceError::genericGError(const URL& failingURL, GError* error)
{
    return ResourceError(String::fromLatin1(g_quark_to_string(error->domain)), error->code, failingURL, String::fromUTF8(error->message));
}

ResourceError ResourceError::tlsError(const URL& failingURL, unsigned tlsErrors, GTlsCertificate* certificate)
{
    ResourceError resourceError(String::fromLatin1(g_quark_to_string(G_TLS_ERROR)), G_TLS_ERROR_BAD_CERTIFICATE, failingURL, unacceptableTLSCertificate());
    resourceError.setTLSErrors(tlsErrors);
    resourceError.setCertificate(certificate);
    return resourceError;
}

ResourceError ResourceError::timeoutError(const URL& failingURL)
{
    // FIXME: This should probably either be integrated into ErrorsGtk.h or the
    // networking errors from that file should be moved here.

    // Use the same value as in NSURLError.h
    static constexpr int timeoutError = -1001;
    static constexpr auto errorDomain = "WebKitNetworkError"_s;
    return ResourceError(errorDomain, timeoutError, failingURL, "Request timed out"_s, ResourceError::Type::Timeout);
}

void ResourceError::doPlatformIsolatedCopy(const ResourceError& other)
{
    m_certificate = other.m_certificate;
    m_tlsErrors = other.m_tlsErrors;
}

bool ResourceError::platformCompare(const ResourceError& a, const ResourceError& b)
{
    return a.tlsErrors() == b.tlsErrors();
}

} // namespace WebCore

#endif
