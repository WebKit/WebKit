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
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

// Use the same value as in NSURLError.h
static constexpr int errorCodeTimeout = -1001;

ResourceError::ResourceError(const String& domain, int errorCode, const URL& failingURL, const String& localizedDescription, Type type, IsSanitized isSanitized)
    : ResourceErrorBase(domain, errorCode, failingURL, localizedDescription, type, isSanitized)
{
}

ResourceError ResourceError::fromIPCData(std::optional<IPCData>&& ipcData)
{
    if (!ipcData)
        return { };

    ResourceError error {
        ipcData->domain,
        ipcData->errorCode,
        ipcData->failingURL,
        ipcData->localizedDescription,
        ipcData->type,
        ipcData->isSanitized
    };
    error.setCertificate(ipcData->certificateInfo.certificate().get());
    error.setTLSErrors(ipcData->certificateInfo.tlsErrors());
    return error;
}

auto ResourceError::ipcData() const -> std::optional<IPCData>
{
    if (isNull())
        return std::nullopt;

    return IPCData {
        type(),
        domain(),
        errorCode(),
        failingURL(),
        localizedDescription(),
        m_isSanitized,
        CertificateInfo { *this }
    };
}

ResourceError ResourceError::transportError(const URL& failingURL, int statusCode, const String& reasonPhrase)
{
    return ResourceError(String::fromLatin1(g_quark_to_string(SOUP_SESSION_ERROR)), statusCode, failingURL, reasonPhrase);
}

ResourceError ResourceError::httpError(SoupMessage* message, GError* error)
{
    ASSERT(message);
    return genericGError(soupURIToURL(soup_message_get_uri(message)), error);
}

ResourceError ResourceError::authenticationError(SoupMessage* message)
{
    ASSERT(message);
    return ResourceError(String::fromLatin1(g_quark_to_string(SOUP_SESSION_ERROR)), soup_message_get_status(message),
        soup_message_get_uri(message), String::fromUTF8(soup_message_get_reason_phrase(message)));
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
    return ResourceError(errorDomainWebKitNetwork, errorCodeTimeout, failingURL, "Request timed out"_s, ResourceError::Type::Timeout);
}

ResourceError::ErrorRecoveryMethod ResourceError::errorRecoveryMethod() const
{
    if (!m_failingURL.protocolIs("https"_s) || isCancellation())
        return ErrorRecoveryMethod::NoRecovery;

    static const NeverDestroyed<String> ioErrorDomain = String::fromLatin1(g_quark_to_string(G_IO_ERROR));

    // Only failures that suggest the host does not serve HTTPS at all are
    // recoverable. G_TLS_ERROR is deliberately absent: a TLS failure means
    // there is an HTTPS server, so retrying over cleartext is never right.
    bool isRecoverableError = false;
    if (m_domain == ioErrorDomain.get()) {
        switch (m_errorCode) {
        case G_IO_ERROR_CONNECTION_REFUSED:
        case G_IO_ERROR_CONNECTION_CLOSED:
        case G_IO_ERROR_NOT_CONNECTED:
        case G_IO_ERROR_TIMED_OUT:
            isRecoverableError = true;
        }
    } else if (m_domain == errorDomainWebKitNetwork)
        isRecoverableError = m_errorCode == errorCodeHTTPSUpgradeRedirectLoop || m_errorCode == errorCodeTimeout;

    return isRecoverableError ? ErrorRecoveryMethod::HTTPFallback : ErrorRecoveryMethod::NoRecovery;
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
