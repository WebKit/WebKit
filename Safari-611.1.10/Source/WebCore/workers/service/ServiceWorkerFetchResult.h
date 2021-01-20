/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "ContentSecurityPolicyResponseHeaders.h"
#include "ResourceError.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"

namespace WebCore {

struct ServiceWorkerFetchResult {
    ServiceWorkerJobDataIdentifier jobDataIdentifier;
    ServiceWorkerRegistrationKey registrationKey;
    String script;
    CertificateInfo certificateInfo;
    ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
    String referrerPolicy;
    ResourceError scriptError;

    ServiceWorkerFetchResult isolatedCopy() const { return { jobDataIdentifier, registrationKey.isolatedCopy(), script.isolatedCopy(), certificateInfo.isolatedCopy(), contentSecurityPolicy.isolatedCopy(), referrerPolicy.isolatedCopy(), scriptError.isolatedCopy() }; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, ServiceWorkerFetchResult&);
};

inline ServiceWorkerFetchResult serviceWorkerFetchError(ServiceWorkerJobDataIdentifier jobDataIdentifier, ServiceWorkerRegistrationKey&& registrationKey, ResourceError&& error)
{
    return { jobDataIdentifier, WTFMove(registrationKey), { }, { }, { }, { }, WTFMove(error) };
}

template<class Encoder>
void ServiceWorkerFetchResult::encode(Encoder& encoder) const
{
    encoder << jobDataIdentifier << registrationKey << script << contentSecurityPolicy << referrerPolicy << scriptError;
    encoder << certificateInfo;
}

template<class Decoder>
bool ServiceWorkerFetchResult::decode(Decoder& decoder, ServiceWorkerFetchResult& result)
{
    Optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier;
    decoder >> jobDataIdentifier;
    if (!jobDataIdentifier)
        return false;
    result.jobDataIdentifier = WTFMove(*jobDataIdentifier);
    
    auto registrationKey = ServiceWorkerRegistrationKey::decode(decoder);
    if (!registrationKey)
        return false;
    std::swap(*registrationKey, result.registrationKey);

    if (!decoder.decode(result.script))
        return false;
    if (!decoder.decode(result.contentSecurityPolicy))
        return false;
    if (!decoder.decode(result.referrerPolicy))
        return false;
    if (!decoder.decode(result.scriptError))
        return false;
    
    Optional<CertificateInfo> certificateInfo;
    decoder >> certificateInfo;
    if (!certificateInfo)
        return false;
    result.certificateInfo = WTFMove(*certificateInfo);

    return true;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
