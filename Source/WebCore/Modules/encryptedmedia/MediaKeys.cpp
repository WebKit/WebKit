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
#include "MediaKeys.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDM.h"
#include "CDMInstance.h"
#include "MediaKeySession.h"
#include "NotImplemented.h"
#include <runtime/ArrayBuffer.h>

namespace WebCore {

MediaKeys::MediaKeys(bool useDistinctiveIdentifier, bool persistentStateAllowed, const Vector<MediaKeySessionType>& supportedSessionTypes, Ref<CDM>&& implementation, std::unique_ptr<CDMInstance>&& instance)
    : m_useDistinctiveIdentifier(useDistinctiveIdentifier)
    , m_persistentStateAllowed(persistentStateAllowed)
    , m_supportedSessionTypes(supportedSessionTypes)
    , m_implementation(WTFMove(implementation))
    , m_instance(WTFMove(instance))
{
}

MediaKeys::~MediaKeys() = default;

ExceptionOr<Ref<MediaKeySession>> MediaKeys::createSession(MediaKeySessionType)
{
    notImplemented();
    return Exception { NOT_SUPPORTED_ERR };
}

void MediaKeys::setServerCertificate(const BufferSource& serverCertificate, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-setservercertificate
    // W3C Editor's Draft 09 November 2016

    // When this method is invoked, the user agent must run the following steps:
    // 1. If the Key System implementation represented by this object's cdm implementation value does not support
    //    server certificates, return a promise resolved with false.
    if (!m_implementation->supportsServerCertificates()) {
        promise->reject(false);
        return;
    }

    // 2. If serverCertificate is an empty array, return a promise rejected with a new a newly created TypeError.
    if (!serverCertificate.length()) {
        promise->reject(TypeError);
        return;
    }

    // 3. Let certificate be a copy of the contents of the serverCertificate parameter.
    auto certificate = ArrayBuffer::create(serverCertificate.data(), serverCertificate.length());

    // 4. Let promise be a new promise.
    // 5. Run the following steps in parallel:

    m_taskQueue.enqueueTask([this, certificate = WTFMove(certificate), promise = WTFMove(promise)] () mutable {
        // 5.1. Use this object's cdm instance to process certificate.
        if (m_instance->setServerCertificate(certificate) == CDMInstance::Failed) {
            // 5.2. If the preceding step failed, resolve promise with a new DOMException whose name is the appropriate error name.
            promise->reject(INVALID_STATE_ERR);
            return;
        }

        // 5.1. Resolve promise with true.
        promise->resolve<IDLBoolean>(true);
    });

    // 6. Return promise.
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
