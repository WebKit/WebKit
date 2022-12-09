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

static const String& curlErrorDomain()
{
    static NeverDestroyed<const String> errorDomain(MAKE_STATIC_STRING_IMPL("CurlErrorDomain"));
    return errorDomain;
}

ResourceError::ResourceError(int curlCode, const URL& failingURL, Type type)
    : ResourceErrorBase(curlErrorDomain(), curlCode, failingURL, CurlHandle::errorDescription(static_cast<CURLcode>(curlCode)), type, IsSanitized::No)
{
}

bool ResourceError::isCertificationVerificationError() const
{
    return domain() == curlErrorDomain() && errorCode() == CURLE_PEER_FAILED_VERIFICATION;
}

void ResourceError::doPlatformIsolatedCopy(const ResourceError&)
{
}

bool ResourceError::platformCompare(const ResourceError&, const ResourceError&)
{
    return true;
}

} // namespace WebCore

#endif
