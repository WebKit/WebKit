/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceError.h"

#if USE(CURL)

#include "CurlContext.h"

namespace WebCore {

const char* const ResourceError::curlErrorDomain = "CurlErrorDomain";

ResourceError ResourceError::httpError(int errorCode, const URL& failingURL, Type type)
{
    return ResourceError(curlErrorDomain, errorCode, failingURL, CurlHandle::errorDescription(static_cast<CURLcode>(errorCode)), type);
}

ResourceError ResourceError::sslError(int errorCode, unsigned sslErrors, const URL& failingURL)
{
    ResourceError resourceError = ResourceError::httpError(errorCode, failingURL);
    resourceError.setSslErrors(sslErrors);
    return resourceError;
}

bool ResourceError::isSSLConnectError() const
{
    return errorCode() == CURLE_SSL_CONNECT_ERROR;
}

bool ResourceError::isSSLCertVerificationError() const
{
    return errorCode() == CURLE_SSL_CACERT || errorCode() == CURLE_PEER_FAILED_VERIFICATION;
}

void ResourceError::doPlatformIsolatedCopy(const ResourceError& other)
{
    m_sslErrors = other.m_sslErrors;
}

bool ResourceError::platformCompare(const ResourceError& a, const ResourceError& b)
{
    return a.sslErrors() == b.sslErrors();
}

} // namespace WebCore

#endif
