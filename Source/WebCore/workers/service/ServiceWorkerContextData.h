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

#include "ContentSecurityPolicyResponseHeaders.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerJobDataIdentifier.h"
#include "ServiceWorkerRegistrationData.h"
#include "WorkerType.h"
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>

#if ENABLE(SERVICE_WORKER)

namespace WebCore {

struct ServiceWorkerContextData {

    struct ImportedScript {
        String script;
        URL responseURL;
        String mimeType;

        ImportedScript isolatedCopy() const { return { script.isolatedCopy(), responseURL.isolatedCopy(), mimeType.isolatedCopy() }; }

        template<class Encoder> void encode(Encoder& encoder) const
        {
            encoder << script << responseURL << mimeType;
        }

        template<class Decoder> static bool decode(Decoder& decoder, ImportedScript& script)
        {
            ImportedScript importedScript;
            if (!decoder.decode(importedScript.script))
                return false;

            if (!decoder.decode(importedScript.responseURL))
                return false;

            if (!decoder.decode(importedScript.mimeType))
                return false;

            script = WTFMove(importedScript);
            return true;
        }
    };

    Optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier;
    ServiceWorkerRegistrationData registration;
    ServiceWorkerIdentifier serviceWorkerIdentifier;
    String script;
    ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
    String referrerPolicy;
    URL scriptURL;
    WorkerType workerType;
    PAL::SessionID sessionID;
    bool loadedFromDisk;
    HashMap<URL, ImportedScript> scriptResourceMap;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ServiceWorkerContextData> decode(Decoder&);

    ServiceWorkerContextData isolatedCopy() const;
};

template<class Encoder>
void ServiceWorkerContextData::encode(Encoder& encoder) const
{
    encoder << jobDataIdentifier << registration << serviceWorkerIdentifier << script << contentSecurityPolicy << referrerPolicy << scriptURL << workerType << sessionID << loadedFromDisk;
    encoder << scriptResourceMap;
}

template<class Decoder>
Optional<ServiceWorkerContextData> ServiceWorkerContextData::decode(Decoder& decoder)
{
    Optional<Optional<ServiceWorkerJobDataIdentifier>> jobDataIdentifier;
    decoder >> jobDataIdentifier;
    if (!jobDataIdentifier)
        return WTF::nullopt;

    Optional<ServiceWorkerRegistrationData> registration;
    decoder >> registration;
    if (!registration)
        return WTF::nullopt;

    auto serviceWorkerIdentifier = ServiceWorkerIdentifier::decode(decoder);
    if (!serviceWorkerIdentifier)
        return WTF::nullopt;

    String script;
    if (!decoder.decode(script))
        return WTF::nullopt;

    ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
    if (!decoder.decode(contentSecurityPolicy))
        return WTF::nullopt;

    String referrerPolicy;
    if (!decoder.decode(referrerPolicy))
        return WTF::nullopt;

    URL scriptURL;
    if (!decoder.decode(scriptURL))
        return WTF::nullopt;
    
    WorkerType workerType;
    if (!decoder.decodeEnum(workerType))
        return WTF::nullopt;

    PAL::SessionID sessionID;
    if (!decoder.decode(sessionID))
        return WTF::nullopt;

    bool loadedFromDisk;
    if (!decoder.decode(loadedFromDisk))
        return WTF::nullopt;

    HashMap<URL, ImportedScript> scriptResourceMap;
    if (!decoder.decode(scriptResourceMap))
        return WTF::nullopt;

    return {{ WTFMove(*jobDataIdentifier), WTFMove(*registration), WTFMove(*serviceWorkerIdentifier), WTFMove(script), WTFMove(contentSecurityPolicy), WTFMove(referrerPolicy), WTFMove(scriptURL), workerType, sessionID, loadedFromDisk, WTFMove(scriptResourceMap) }};
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
