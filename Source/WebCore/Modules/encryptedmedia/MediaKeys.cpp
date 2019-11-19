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
#include "CDMClient.h"
#include "CDMInstance.h"
#include "EventLoop.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "MediaKeySession.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"

namespace WebCore {

MediaKeys::MediaKeys(bool useDistinctiveIdentifier, bool persistentStateAllowed, const Vector<MediaKeySessionType>& supportedSessionTypes, Ref<CDM>&& implementation, Ref<CDMInstance>&& instance)
    : m_useDistinctiveIdentifier(useDistinctiveIdentifier)
    , m_persistentStateAllowed(persistentStateAllowed)
    , m_supportedSessionTypes(supportedSessionTypes)
    , m_implementation(WTFMove(implementation))
    , m_instance(WTFMove(instance))
{
}

MediaKeys::~MediaKeys() = default;

ExceptionOr<Ref<MediaKeySession>> MediaKeys::createSession(ScriptExecutionContext& context, MediaKeySessionType sessionType)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-setservercertificate
    // W3C Editor's Draft 09 November 2016
    LOG(EME, "EME - check if a new session can be created");

    // When this method is invoked, the user agent must run the following steps:
    // 1. If this object's supported session types value does not contain sessionType, throw [WebIDL] a NotSupportedError.
    if (!m_supportedSessionTypes.contains(sessionType))
        return Exception(NotSupportedError);

    // 2. If the implementation does not support MediaKeySession operations in the current state, throw [WebIDL] an InvalidStateError.
    if (!m_implementation->supportsSessions())
        return Exception(InvalidStateError);

    auto instanceSession = m_instance->createSession();
    if (!instanceSession)
        return Exception(InvalidStateError);

    // 3. Let session be a new MediaKeySession object, and initialize it as follows:
    // NOTE: Continued in MediaKeySession.
    // 4. Return session.
    auto session = MediaKeySession::create(context, makeWeakPtr(*this), sessionType, m_useDistinctiveIdentifier, m_implementation.copyRef(), instanceSession.releaseNonNull());
    m_sessions.append(session.copyRef());
    return session;
}

void MediaKeys::setServerCertificate(ScriptExecutionContext& context, const BufferSource& serverCertificate, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-setservercertificate
    // W3C Editor's Draft 09 November 2016

    // When this method is invoked, the user agent must run the following steps:
    // 1. If the Key System implementation represented by this object's cdm implementation value does not support
    //    server certificates, return a promise resolved with false.
    if (!m_implementation->supportsServerCertificates()) {
        promise->resolve<IDLBoolean>(false);
        return;
    }

    // 2. If serverCertificate is an empty array, return a promise rejected with a new a newly created TypeError.
    if (!serverCertificate.length()) {
        promise->reject(TypeError);
        return;
    }

    // 3. Let certificate be a copy of the contents of the serverCertificate parameter.
    auto certificate = SharedBuffer::create(serverCertificate.data(), serverCertificate.length());

    // 4. Let promise be a new promise.
    // 5. Run the following steps in parallel:

    context.eventLoop().queueTask(TaskSource::Networking, [this, certificate = WTFMove(certificate), promise = WTFMove(promise)] () mutable {
        // 5.1. Use this object's cdm instance to process certificate.
        if (m_instance->setServerCertificate(WTFMove(certificate)) == CDMInstance::Failed) {
            // 5.2. If the preceding step failed, resolve promise with a new DOMException whose name is the appropriate error name.
            promise->reject(InvalidStateError);
            return;
        }

        // 5.1. Resolve promise with true.
        promise->resolve<IDLBoolean>(true);
    });

    // 6. Return promise.
}

void MediaKeys::attachCDMClient(CDMClient& client)
{
    ASSERT(!m_cdmClients.contains(&client));
    m_cdmClients.append(&client);
}

void MediaKeys::detachCDMClient(CDMClient& client)
{
    ASSERT(m_cdmClients.contains(&client));
    m_cdmClients.removeFirst(&client);
}

void MediaKeys::attemptToResumePlaybackOnClients()
{
    for (auto* cdmClient : m_cdmClients)
        cdmClient->cdmClientAttemptToResumePlaybackIfNecessary();
}

bool MediaKeys::hasOpenSessions() const
{
    return std::any_of(m_sessions.begin(), m_sessions.end(),
        [](auto& session) {
            return !session->isClosed();
        });
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
