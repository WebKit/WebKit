/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaKeySystemAccess.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDM.h"
#include "CDMInstance.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaKeys.h"
#include "MediaKeys.h"
#include "MediaKeysRequirement.h"

namespace WebCore {

Ref<MediaKeySystemAccess> MediaKeySystemAccess::create(const String& keySystem, MediaKeySystemConfiguration&& configuration, Ref<CDM>&& implementation)
{
    return adoptRef(*new MediaKeySystemAccess(keySystem, WTFMove(configuration), WTFMove(implementation)));
}

MediaKeySystemAccess::MediaKeySystemAccess(const String& keySystem, MediaKeySystemConfiguration&& configuration, Ref<CDM>&& implementation)
    : m_keySystem(keySystem)
    , m_configuration(new MediaKeySystemConfiguration(WTFMove(configuration)))
    , m_implementation(WTFMove(implementation))
{
}

MediaKeySystemAccess::~MediaKeySystemAccess() = default;

void MediaKeySystemAccess::createMediaKeys(Document& document, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeysystemaccess-createmediakeys
    // W3C Editor's Draft 09 November 2016

    // When this method is invoked, the user agent must run the following steps:
    // 1. Let promise be a new promise.
    // 2. Run the following steps in parallel:
    m_taskQueue.enqueueTask([this, weakDocument = makeWeakPtr(document), promise = WTFMove(promise)] () mutable {
        // 2.1. Let configuration be the value of this object's configuration value.
        // 2.2. Let use distinctive identifier be true if the value of configuration's distinctiveIdentifier member is "required" and false otherwise.
        bool useDistinctiveIdentifier = m_configuration->distinctiveIdentifier == MediaKeysRequirement::Required;

        // 2.3. Let persistent state allowed be true if the value of configuration's persistentState member is "required" and false otherwise.
        bool persistentStateAllowed = m_configuration->persistentState == MediaKeysRequirement::Required;

        // 2.4. Load and initialize the Key System implementation represented by this object's cdm implementation value if necessary.
        m_implementation->loadAndInitialize();

        // 2.5. Let instance be a new instance of the Key System implementation represented by this object's cdm implementation value.
        auto instance = m_implementation->createInstance();
        if (!instance) {
            promise->reject(InvalidStateError);
            return;
        }

        // 2.6. Initialize instance to enable, disable and/or select Key System features using configuration.
        // 2.7. If use distinctive identifier is false, prevent instance from using Distinctive Identifier(s) and Distinctive Permanent Identifier(s).
        // 2.8. If persistent state allowed is false, prevent instance from persisting any state related to the application or origin of this object's Document.
        auto allowDistinctiveIdentifiers = useDistinctiveIdentifier ? CDMInstance::AllowDistinctiveIdentifiers::Yes : CDMInstance::AllowDistinctiveIdentifiers::No;
        auto allowPersistentState = persistentStateAllowed ? CDMInstance::AllowPersistentState::Yes : CDMInstance::AllowPersistentState::No;

        instance->initializeWithConfiguration(*m_configuration, allowDistinctiveIdentifiers, allowPersistentState, [weakDocument = WTFMove(weakDocument), sessionTypes = m_configuration->sessionTypes, implementation = m_implementation.copyRef(), useDistinctiveIdentifier, persistentStateAllowed, instance = instance.releaseNonNull(), promise = WTFMove(promise)] (auto successValue) mutable {
            if (successValue == CDMInstance::Failed || !weakDocument) {
                promise->reject(NotAllowedError);
                return;
            }

            // 2.9. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
            // 2.10. Let media keys be a new MediaKeys object, and initialize it as follows:
            // 2.10.1. Let the use distinctive identifier value be use distinctive identifier.
            // 2.10.2. Let the persistent state allowed value be persistent state allowed.
            // 2.10.3. Let the supported session types value be be the value of configuration's sessionTypes member.
            // 2.10.4. Let the cdm implementation value be this object's cdm implementation value.
            // 2.10.5. Let the cdm instance value be instance.
            auto mediaKeys = MediaKeys::create(*weakDocument, useDistinctiveIdentifier, persistentStateAllowed, sessionTypes, WTFMove(implementation), WTFMove(instance));

            // 2.11. Resolve promise with media keys.
            promise->resolveWithNewlyCreated<IDLInterface<MediaKeys>>(WTFMove(mediaKeys));
        });
    });

    // 3. Return promise.
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
