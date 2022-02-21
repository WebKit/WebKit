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

#include "CertificateInfo.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "CrossOriginEmbedderPolicy.h"
#include "NavigationPreloadState.h"
#include "ScriptBuffer.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerJobDataIdentifier.h"
#include "ServiceWorkerRegistrationData.h"
#include "WorkerType.h"
#include <wtf/HashMap.h>
#include <wtf/URLHash.h>

#if ENABLE(SERVICE_WORKER)

namespace WebCore {

enum class LastNavigationWasAppInitiated : bool;

struct ServiceWorkerContextData {
    struct ImportedScript {
        ScriptBuffer script;
        URL responseURL;
        String mimeType;

        template<class Encoder> void encode(Encoder& encoder) const
        {
            encoder << script << responseURL << mimeType;
        }

        template<class Decoder> static std::optional<ImportedScript> decode(Decoder& decoder)
        {
            std::optional<ScriptBuffer> script;
            decoder >> script;
            if (!script)
                return std::nullopt;

            std::optional<URL> responseURL;
            decoder >> responseURL;
            if (!responseURL)
                return std::nullopt;

            std::optional<String> mimeType;
            decoder >> mimeType;
            if (!mimeType)
                return std::nullopt;

            return {{
                WTFMove(*script),
                WTFMove(*responseURL),
                WTFMove(*mimeType)
            }};
        }

        ImportedScript isolatedCopy() const { return { script.isolatedCopy(), responseURL.isolatedCopy(), mimeType.isolatedCopy() }; }
    };

    std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier;
    ServiceWorkerRegistrationData registration;
    ServiceWorkerIdentifier serviceWorkerIdentifier;
    ScriptBuffer script;
    CertificateInfo certificateInfo;
    ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
    CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    String referrerPolicy;
    URL scriptURL;
    WorkerType workerType;
    bool loadedFromDisk;
    std::optional<LastNavigationWasAppInitiated> lastNavigationWasAppInitiated;
    HashMap<URL, ImportedScript> scriptResourceMap;
    std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier;
    NavigationPreloadState navigationPreloadState;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ServiceWorkerContextData> decode(Decoder&);

    ServiceWorkerContextData isolatedCopy() const &;
    ServiceWorkerContextData isolatedCopy() &&;
};

template<class Encoder>
void ServiceWorkerContextData::encode(Encoder& encoder) const
{
    encoder << jobDataIdentifier << registration << serviceWorkerIdentifier << script << contentSecurityPolicy << crossOriginEmbedderPolicy << referrerPolicy
        << scriptURL << workerType << loadedFromDisk << lastNavigationWasAppInitiated << scriptResourceMap << certificateInfo << serviceWorkerPageIdentifier << navigationPreloadState;
}

template<class Decoder>
std::optional<ServiceWorkerContextData> ServiceWorkerContextData::decode(Decoder& decoder)
{
    std::optional<std::optional<ServiceWorkerJobDataIdentifier>> jobDataIdentifier;
    decoder >> jobDataIdentifier;
    if (!jobDataIdentifier)
        return std::nullopt;

    std::optional<ServiceWorkerRegistrationData> registration;
    decoder >> registration;
    if (!registration)
        return std::nullopt;

    auto serviceWorkerIdentifier = ServiceWorkerIdentifier::decode(decoder);
    if (!serviceWorkerIdentifier)
        return std::nullopt;

    std::optional<ScriptBuffer> script;
    decoder >> script;
    if (!script)
        return std::nullopt;

    ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
    if (!decoder.decode(contentSecurityPolicy))
        return std::nullopt;

    std::optional<CrossOriginEmbedderPolicy> crossOriginEmbedderPolicy;
    decoder >> crossOriginEmbedderPolicy;
    if (!crossOriginEmbedderPolicy)
        return std::nullopt;

    String referrerPolicy;
    if (!decoder.decode(referrerPolicy))
        return std::nullopt;

    URL scriptURL;
    if (!decoder.decode(scriptURL))
        return std::nullopt;

    WorkerType workerType;
    if (!decoder.decode(workerType))
        return std::nullopt;

    bool loadedFromDisk;
    if (!decoder.decode(loadedFromDisk))
        return std::nullopt;

    std::optional<LastNavigationWasAppInitiated> lastNavigationWasAppInitiated;
    if (!decoder.decode(lastNavigationWasAppInitiated))
        return std::nullopt;

    HashMap<URL, ImportedScript> scriptResourceMap;
    if (!decoder.decode(scriptResourceMap))
        return std::nullopt;

    std::optional<CertificateInfo> certificateInfo;
    decoder >> certificateInfo;
    if (!certificateInfo)
        return std::nullopt;

    std::optional<std::optional<ScriptExecutionContextIdentifier>> serviceWorkerPageIdentifier;
    decoder >> serviceWorkerPageIdentifier;
    if (!serviceWorkerPageIdentifier)
        return std::nullopt;

    std::optional<NavigationPreloadState> navigationPreloadState;
    decoder >> navigationPreloadState;
    if (!navigationPreloadState)
        return std::nullopt;

    return {{
        WTFMove(*jobDataIdentifier),
        WTFMove(*registration),
        WTFMove(*serviceWorkerIdentifier),
        WTFMove(*script),
        WTFMove(*certificateInfo),
        WTFMove(contentSecurityPolicy),
        WTFMove(*crossOriginEmbedderPolicy),
        WTFMove(referrerPolicy),
        WTFMove(scriptURL),
        workerType,
        loadedFromDisk,
        WTFMove(lastNavigationWasAppInitiated),
        WTFMove(scriptResourceMap),
        WTFMove(*serviceWorkerPageIdentifier),
        WTFMove(*navigationPreloadState)
    }};
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
