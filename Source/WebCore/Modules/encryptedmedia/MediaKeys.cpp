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
#include "Document.h"
#include "EventLoop.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "MediaKeySession.h"
#include "SharedBuffer.h"
#include <wtf/Logger.h>
#include <wtf/LoggerHelper.h>

namespace WebCore {

#if !RELEASE_LOG_DISABLED
static WTFLogChannel& logChannel() { return LogEME; }
static const char* logClassName() { return "MediaKeys"; }
#endif

MediaKeys::MediaKeys(Document& document, bool useDistinctiveIdentifier, bool persistentStateAllowed, const Vector<MediaKeySessionType>& supportedSessionTypes, Ref<CDM>&& implementation, Ref<CDMInstance>&& instance)
    : m_useDistinctiveIdentifier(useDistinctiveIdentifier)
    , m_persistentStateAllowed(persistentStateAllowed)
    , m_supportedSessionTypes(supportedSessionTypes)
    , m_implementation(WTFMove(implementation))
    , m_instance(WTFMove(instance))
#if !RELEASE_LOG_DISABLED
    , m_logger(document.logger())
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
#if !RELEASE_LOG_DISABLED
    m_instance->setLogIdentifier(m_logIdentifier);
#else
    UNUSED_PARAM(document);
#endif
    m_instance->setClient(*this);
}

MediaKeys::~MediaKeys() = default;

ExceptionOr<Ref<MediaKeySession>> MediaKeys::createSession(Document& document, MediaKeySessionType sessionType)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-setservercertificate
    // W3C Editor's Draft 09 November 2016
    auto identifier = LOGIDENTIFIER;

    // When this method is invoked, the user agent must run the following steps:
    // 1. If this object's supported session types value does not contain sessionType, throw [WebIDL] a NotSupportedError.
    if (!m_supportedSessionTypes.contains(sessionType)) {
        ERROR_LOG(identifier, "Exception: unsupported sessionType: ", sessionType);
        return Exception(ExceptionCode::NotSupportedError);
    }

    // 2. If the implementation does not support MediaKeySession operations in the current state, throw [WebIDL] an InvalidStateError.
    if (!m_implementation->supportsSessions()) {
        ERROR_LOG(identifier, "Exception: implementation does not support sessions");
        return Exception(ExceptionCode::InvalidStateError);
    }

    auto instanceSession = m_instance->createSession();
    if (!instanceSession) {
        ERROR_LOG(identifier, "Exception: could not create session");
        return Exception(ExceptionCode::InvalidStateError);
    }

    // 3. Let session be a new MediaKeySession object, and initialize it as follows:
    // NOTE: Continued in MediaKeySession.
    // 4. Return session.
    ALWAYS_LOG(identifier);
    auto session = MediaKeySession::create(document, *this, sessionType, m_useDistinctiveIdentifier, m_implementation.copyRef(), instanceSession.releaseNonNull());
    m_sessions.append(session.copyRef());
    return session;
}

void MediaKeys::setServerCertificate(const BufferSource& serverCertificate, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-setservercertificate
    // W3C Editor's Draft 09 November 2016
    auto identifier = LOGIDENTIFIER;

    // When this method is invoked, the user agent must run the following steps:
    // 1. If the Key System implementation represented by this object's cdm implementation value does not support
    //    server certificates, return a promise resolved with false.
    if (!m_implementation->supportsServerCertificates()) {
        ALWAYS_LOG(identifier, "Resolved: !supportsServerCertificates()");
        promise->resolve<IDLBoolean>(false);
        return;
    }

    // 2. If serverCertificate is an empty array, return a promise rejected with a new a newly created TypeError.
    if (!serverCertificate.length()) {
        ERROR_LOG(identifier, "Rejected: empty serverCertificate");
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    // 3. Let certificate be a copy of the contents of the serverCertificate parameter.
    auto certificate = SharedBuffer::create(serverCertificate.data(), serverCertificate.length());

    // 4. Let promise be a new promise.
    // 5. Run the following steps in parallel:

    // 5.1. Use this object's cdm instance to process certificate.
    ALWAYS_LOG(identifier);
    m_instance->setServerCertificate(WTFMove(certificate), [this, protectedThis = Ref { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] (auto success) {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
#endif
        // 5.2. If the preceding step failed, resolve promise with a new DOMException whose name is the appropriate error name.
        // 5.1. [Else,] Resolve promise with true.
        if (success == CDMInstance::Failed) {
            ERROR_LOG(identifier, "::task() - Rejected, setServerCertificate() failed");
            promise->reject(ExceptionCode::InvalidStateError);
            return;
        }

        ALWAYS_LOG(identifier, "::task() - Resolved");
        promise->resolve<IDLBoolean>(true);
    });

    // 6. Return promise.
}

void MediaKeys::attachCDMClient(CDMClient& client)
{
    ASSERT(!m_cdmClients.contains(client));
    m_cdmClients.add(client);
}

void MediaKeys::detachCDMClient(CDMClient& client)
{
    ASSERT(m_cdmClients.contains(client));
    m_cdmClients.remove(client);
}

void MediaKeys::attemptToResumePlaybackOnClients()
{
    for (auto& cdmClient : m_cdmClients)
        cdmClient.cdmClientAttemptToResumePlaybackIfNecessary();
}

bool MediaKeys::hasOpenSessions() const
{
    return std::any_of(m_sessions.begin(), m_sessions.end(),
        [](auto& session) {
            return !session->isClosed();
        });
}

void MediaKeys::unrequestedInitializationDataReceived(const String& initDataType, Ref<SharedBuffer>&& initData)
{
    for (auto& cdmClient : m_cdmClients)
        cdmClient.cdmClientUnrequestedInitializationDataReceived(initDataType, initData.copyRef());
}

#if !RELEASE_LOG_DISABLED
const void* MediaKeys::nextChildIdentifier() const
{
    return LoggerHelper::childLogIdentifier(m_logIdentifier, ++m_childIdentifierSeed);
}
#endif

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
