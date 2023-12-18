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
#include "MediaKeySession.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDM.h"
#include "CDMInstance.h"
#include "CDMKeyGroupingStrategy.h"
#include "DOMPromiseProxy.h"
#include "Document.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "JSMediaKeyStatusMap.h"
#include "Logging.h"
#include "MediaKeyMessageEvent.h"
#include "MediaKeyMessageEventInit.h"
#include "MediaKeyMessageType.h"
#include "MediaKeyStatusMap.h"
#include "MediaKeys.h"
#include "NotImplemented.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include <wtf/HashCountedSet.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/LoggerHelper.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaKeySession);

Ref<MediaKeySession> MediaKeySession::create(Document& document, WeakPtr<MediaKeys>&& keys, MediaKeySessionType sessionType, bool useDistinctiveIdentifier, Ref<CDM>&& implementation, Ref<CDMInstanceSession>&& instanceSession)
{
    auto session = adoptRef(*new MediaKeySession(document, WTFMove(keys), sessionType, useDistinctiveIdentifier, WTFMove(implementation), WTFMove(instanceSession)));
    session->suspendIfNeeded();
    return session;
}

MediaKeySession::MediaKeySession(Document& document, WeakPtr<MediaKeys>&& keys, MediaKeySessionType sessionType, bool useDistinctiveIdentifier, Ref<CDM>&& implementation, Ref<CDMInstanceSession>&& instanceSession)
    : ActiveDOMObject(&document)
#if !RELEASE_LOG_DISABLED
    , m_logger(document.logger())
    , m_logIdentifier(keys ? keys->nextChildIdentifier() : nullptr)
#endif
    , m_keys(WTFMove(keys))
    , m_expiration(std::numeric_limits<double>::quiet_NaN())
    , m_closedPromise(makeUniqueRef<ClosedPromise>())
    , m_keyStatuses(MediaKeyStatusMap::create(*this))
    , m_useDistinctiveIdentifier(useDistinctiveIdentifier)
    , m_sessionType(sessionType)
    , m_implementation(WTFMove(implementation))
    , m_instanceSession(WTFMove(instanceSession))
    , m_displayChangedObserver([this] (auto displayID) { displayChanged(displayID); })
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-createsession
    // W3C Editor's Draft 09 November 2016
    // createSession(), ctd.
    ALWAYS_LOG(LOGIDENTIFIER, "sessionType(", sessionType, "), useDistinctiveIdentifier(", useDistinctiveIdentifier, ")");

    // 3.1. Let the sessionId attribute be the empty string.
    // 3.2. Let the expiration attribute be NaN.
    // 3.3. Let the closed attribute be a new promise.
    // 3.4. Let key status be a new empty MediaKeyStatusMap object, and initialize it as follows:
    // 3.4.1. Let the size attribute be 0.
    // 3.5. Let the session type value be sessionType.
    // 3.6. Let the uninitialized value be true.
    // 3.7. Let the callable value be false.
    // 3.8. Let the use distinctive identifier value be this object's use distinctive identifier value.
    // 3.9. Let the cdm implementation value be this object's cdm implementation.
    // 3.10. Let the cdm instance value be this object's cdm instance.

    UNUSED_PARAM(m_callable);
    UNUSED_PARAM(m_sessionType);
    UNUSED_PARAM(m_useDistinctiveIdentifier);
    UNUSED_PARAM(m_closed);
    UNUSED_PARAM(m_uninitialized);

#if !RELEASE_LOG_DISABLED
    m_instanceSession->setLogIdentifier(m_logIdentifier);
#endif
    m_instanceSession->setClient(*this);

    document.addDisplayChangedObserver(m_displayChangedObserver);
}

MediaKeySession::~MediaKeySession()
{
    m_keyStatuses->detachSession();
    m_instanceSession->clearClient();
}

const String& MediaKeySession::sessionId() const
{
    return m_sessionId;
}

double MediaKeySession::expiration() const
{
    return m_expiration;
}

Ref<MediaKeyStatusMap> MediaKeySession::keyStatuses() const
{
    return m_keyStatuses.copyRef();
}

void MediaKeySession::generateRequest(const AtomString& initDataType, const BufferSource& initData, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeysession-generaterequest
    // W3C Editor's Draft 09 November 2016

    // When this method is invoked, the user agent must run the following steps:
    // 1. If this object is closed, return a promise rejected with an InvalidStateError.
    // 2. If this object's uninitialized value is false, return a promise rejected with an InvalidStateError.
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, "initDataType(", initDataType, "), initData.length(", initData.length(), ")");

    if (m_closed || !m_uninitialized) {
        ERROR_LOG(identifier, "Rejected: closed(", m_closed, ") or !uninitialized(", !m_uninitialized, ")");
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    // 3. Let this object's uninitialized value be false.
    m_uninitialized = false;

    // 4. If initDataType is the empty string, return a promise rejected with a newly created TypeError.
    // 5. If initData is an empty array, return a promise rejected with a newly created TypeError.
    if (initDataType.isEmpty() || !initData.length()) {
        ERROR_LOG(identifier, "Rejected: initDataType empty(", initDataType.isEmpty(), ") or initData empty(", !initData.length(), ")");
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    // 6. If the Key System implementation represented by this object's cdm implementation value does not support
    //    initDataType as an Initialization Data Type, return a promise rejected with a NotSupportedError. String
    //    comparison is case-sensitive.
    if (!m_implementation->supportsInitDataType(initDataType)) {
        ERROR_LOG(identifier, "Rejected: initDataType(", initDataType, ") unsupported");
        promise->reject(ExceptionCode::NotSupportedError);
        return;
    }

    // 7. Let init data be a copy of the contents of the initData parameter.
    // 8. Let session type be this object's session type.
    // 9. Let promise be a new promise.
    // 10. Run the following steps in parallel:
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, weakThis = WeakPtr { *this }, initData = SharedBuffer::create(initData.data(), initData.length()), initDataType, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
        // 10.1. If the init data is not valid for initDataType, reject promise with a newly created TypeError.
        // 10.2. Let sanitized init data be a validated and sanitized version of init data.
        RefPtr<SharedBuffer> sanitizedInitData = m_implementation->sanitizeInitData(initDataType, initData);

        // 10.3. If the preceding step failed, reject promise with a newly created TypeError.
        if (!sanitizedInitData) {
            ERROR_LOG(identifier, "::task() Rejected: cannot sanitize init data");
            promise->reject(ExceptionCode::TypeError);
            return;
        }

        // 10.4. If sanitized init data is empty, reject promise with a NotSupportedError.
        if (sanitizedInitData->isEmpty()) {
            ERROR_LOG(identifier, "::task() Rejected: empty sanitized init data");
            promise->reject(ExceptionCode::NotSupportedError);
            return;
        }

        // 10.5. Let session id be the empty string.
        // 10.6. Let message be null.
        // 10.7. Let message type be null.
        // 10.8. Let cdm be the CDM instance represented by this object's cdm instance value.
        // 10.9. Use the cdm to execute the following steps:
        // 10.9.1. If the sanitized init data is not supported by the cdm, reject promise with a NotSupportedError.
        if (!m_implementation->supportsInitData(initDataType, *sanitizedInitData)) {
            ERROR_LOG(identifier, "::task() Rejected: unsupported initDataType (", initDataType, ") or sanitized initData");
            promise->reject(ExceptionCode::NotSupportedError);
            return;
        }

        // 10.9.2 Follow the steps for the value of session type from the following list:
        //   ↳ "temporary"
        //     Let requested license type be a temporary non-persistable license.
        //   ↳ "persistent-license"
        //     Let requested license type be a persistable license.
        //   ↳ "persistent-usage-record"
        //     1. Initialize this object's record of key usage as follows.
        //        Set the list of key IDs known to the session to an empty list.
        //        Set the first decrypt time to null.
        //        Set the latest decrypt time to null.
        //     2. Let requested license type be a non-persistable license that will
        //        persist a record of key usage.

        if (m_sessionType == MediaKeySessionType::PersistentUsageRecord) {
            m_recordOfKeyUsage.clear();
            m_firstDecryptTime = 0;
            m_latestDecryptTime = 0;
        }

        m_instanceSession->requestLicense(m_sessionType, keyGroupingStrategy(), initDataType, sanitizedInitData.releaseNonNull(), [this, weakThis, promise = WTFMove(promise), identifier = WTFMove(identifier)] (Ref<SharedBuffer>&& message, const String& sessionId, bool needsIndividualization, CDMInstanceSession::SuccessValue succeeded) mutable {
            if (!weakThis)
                return;

            // 10.9.3. Let session id be a unique Session ID string.

            MediaKeyMessageType messageType;
            if (!needsIndividualization) {
                // 10.9.4. If a license request for the requested license type can be generated based on the sanitized init data:
                // 10.9.4.1. Let message be a license request for the requested license type generated based on the sanitized init data interpreted per initDataType.
                // 10.9.4.2. Let message type be "license-request".
                messageType = MediaKeyMessageType::LicenseRequest;
            } else {
                // 10.9.5. Otherwise:
                // 10.9.5.1. Let message be the request that needs to be processed before a license request request for the requested license
                //           type can be generated based on the sanitized init data.
                // 10.9.5.2. Let message type reflect the type of message, either "license-request" or "individualization-request".
                messageType = MediaKeyMessageType::IndividualizationRequest;
            }

            // 10.10. Queue a task to run the following steps:
            queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, promise = WTFMove(promise), message = WTFMove(message), messageType, sessionId, succeeded, identifier = WTFMove(identifier)] () mutable {
                // 10.10.1. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
                if (succeeded == CDMInstanceSession::SuccessValue::Failed) {
                    ERROR_LOG(identifier, "::task() Rejected: failed to request license");
                    promise->reject(ExceptionCode::NotSupportedError);
                    return;
                }
                // 10.10.2. Set the sessionId attribute to session id.
                m_sessionId = sessionId;

                // 10.9.3. Let this object's callable value be true.
                m_callable = true;

                // 10.9.3. Run the Queue a "message" Event algorithm on the session, providing message type and message.
                enqueueMessage(messageType, message);

                // 10.9.3. Resolve promise.
                ALWAYS_LOG(identifier, "::task() Resolved");
                promise->resolve();
            });
        });
    });

    // 11. Return promise.
}

void MediaKeySession::load(const String& sessionId, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeysession-load
    // W3C Editor's Draft 09 November 2016

    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, "sessionId(", sessionId, ")");

    // 1. If this object is closed, return a promise rejected with an InvalidStateError.
    // 2. If this object's uninitialized value is false, return a promise rejected with an InvalidStateError.
    if (m_closed || !m_uninitialized) {
        ERROR_LOG(identifier, "Rejected: closed(", m_closed, ") or !uninitialized(", !m_uninitialized, ")");
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    // 3. Let this object's uninitialized value be false.
    m_uninitialized = false;

    // 4. If sessionId is the empty string, return a promise rejected with a newly created TypeError.
    // 5. If the result of running the Is persistent session type? algorithm on this object's session type is false, return a promise rejected with a newly created TypeError.
    if (sessionId.isEmpty() || m_sessionType == MediaKeySessionType::Temporary) {
        ERROR_LOG(identifier, "Rejected: sessionID empty(", sessionId.isEmpty(), ") or sessionType == Temporary (", m_sessionType == MediaKeySessionType::Temporary, ")");
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    // 6. Let origin be the origin of this object's Document.
    // This is retrieved in the following task.

    // 7. Let promise be a new promise.
    // 8. Run the following steps in parallel:
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, weakThis = WeakPtr { *this }, sessionId, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
        // 8.1. Let sanitized session ID be a validated and/or sanitized version of sessionId.
        // 8.2. If the preceding step failed, or if sanitized session ID is empty, reject promise with a newly created TypeError.
        std::optional<String> sanitizedSessionId = m_implementation->sanitizeSessionId(sessionId);
        if (!sanitizedSessionId || sanitizedSessionId->isEmpty()) {
            ERROR_LOG(identifier, "Rejected: sanitizedSSessionID empty");
            promise->reject(ExceptionCode::TypeError);
            return;
        }

        // 8.3. If there is a MediaKeySession object that is not closed in this object's Document whose sessionId attribute is sanitized session ID, reject promise with a QuotaExceededError.
        // FIXME: This needs a global MediaKeySession tracker.

        String origin;
        if (RefPtr document = downcast<Document>(scriptExecutionContext()))
            origin = document->securityOrigin().toString();

        // 8.4. Let expiration time be NaN.
        // 8.5. Let message be null.
        // 8.6. Let message type be null.
        // 8.7. Let cdm be the CDM instance represented by this object's cdm instance value.
        // 8.8. Use the cdm to execute the following steps:
        m_instanceSession->loadSession(m_sessionType, *sanitizedSessionId, origin, [this, weakThis, promise = WTFMove(promise), sanitizedSessionId = *sanitizedSessionId, identifier = WTFMove(identifier)] (std::optional<CDMInstanceSession::KeyStatusVector>&& knownKeys, std::optional<double>&& expiration, std::optional<CDMInstanceSession::Message>&& message, CDMInstanceSession::SuccessValue succeeded, CDMInstanceSession::SessionLoadFailure failure) mutable {
            // 8.8.1. If there is no data stored for the sanitized session ID in the origin, resolve promise with false and abort these steps.
            // 8.8.2. If the stored session's session type is not the same as the current MediaKeySession session type, reject promise with a newly created TypeError.
            // 8.8.3. Let session data be the data stored for the sanitized session ID in the origin. This must not include data from other origin(s) or that is not associated with an origin.
            // 8.8.4. If there is a MediaKeySession object that is not closed in any Document and that represents the session data, reject promise with a QuotaExceededError.
            // 8.8.5. Load the session data.
            // 8.8.6. If the session data indicates an expiration time for the session, let expiration time be the expiration time in milliseconds since 01 January 1970 UTC.
            // 8.8.7. If the CDM needs to send a message:
            //   8.8.7.1. Let message be a message generated by the CDM based on the session data.
            //   8.8.7.2. Let message type be the appropriate MediaKeyMessageType for the message.
            // NOTE: Steps 8.8.1. through 8.8.7. should be implemented in CDMInstance.

            if (succeeded == CDMInstanceSession::SuccessValue::Failed) {
                switch (failure) {
                case CDMInstanceSession::SessionLoadFailure::NoSessionData:
                    ALWAYS_LOG(identifier, "::task() Resolved: NoSessionData");
                    promise->resolve<IDLBoolean>(false);
                    return;
                case CDMInstanceSession::SessionLoadFailure::MismatchedSessionType:
                    ERROR_LOG(identifier, "::task() Rejected: MismatchedSessionType");
                    promise->reject(ExceptionCode::TypeError);
                    return;
                case CDMInstanceSession::SessionLoadFailure::QuotaExceeded:
                    ERROR_LOG(identifier, "::task() Rejected: QuotaExceeded");
                    promise->reject(ExceptionCode::QuotaExceededError);
                    return;
                case CDMInstanceSession::SessionLoadFailure::None:
                case CDMInstanceSession::SessionLoadFailure::Other:
                    // In any other case, the session load failure will cause a rejection in the following task.
                    break;
                }
            }

            // 8.9. Queue a task to run the following steps:
            queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, knownKeys = WTFMove(knownKeys), expiration = WTFMove(expiration), message = WTFMove(message), sanitizedSessionId, succeeded, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
                // 8.9.1. If any of the preceding steps failed, reject promise with a the appropriate error name.
                if (succeeded == CDMInstanceSession::SuccessValue::Failed) {
                    ERROR_LOG(identifier, "::task() Rejected: Other failure");
                    promise->reject(ExceptionCode::NotSupportedError);
                    return;
                }

                // 8.9.2. Set the sessionId attribute to sanitized session ID.
                // 8.9.3. Let this object's callable value be true.
                m_sessionId = sanitizedSessionId;
                m_callable = true;

                // 8.9.4. If the loaded session contains information about any keys (there are known keys), run the Update Key Statuses algorithm on the session, providing each key's key ID along with the appropriate MediaKeyStatus.
                if (knownKeys)
                    updateKeyStatuses(WTFMove(*knownKeys));

                // 8.9.5. Run the Update Expiration algorithm on the session, providing expiration time.
                // This must be run, and NaN is the default value if the CDM instance doesn't provide one.
                updateExpiration(expiration.value_or(std::numeric_limits<double>::quiet_NaN()));

                // 8.9.6. If message is not null, run the Queue a "message" Event algorithm on the session, providing message type and message.
                if (message)
                    enqueueMessage(message->first, WTFMove(message->second));

                // 8.9.7. Resolve promise with true.
                ALWAYS_LOG(identifier, "::task() Resolved");
                promise->resolve<IDLBoolean>(true);
            });
        });
    });

    // 9. Return promise.
}

void MediaKeySession::update(const BufferSource& response, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeysession-update
    // W3C Editor's Draft 09 November 2016

    // When this method is invoked, the user agent must run the following steps:
    // 1. If this object is closed, return a promise rejected with an InvalidStateError.
    // 2. If this object's callable value is false, return a promise rejected with an InvalidStateError.
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, "response.length(", response.length(), ")");

    if (m_closed || !m_callable) {
        ERROR_LOG(identifier, "Rejected: closed(", m_closed, ") or !callable(", !m_callable, ")");
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    // 3. If response is an empty array, return a promise rejected with a newly created TypeError.
    if (!response.length()) {
        ERROR_LOG(identifier, "Rejected: empty response");
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    // 4. Let response copy be a copy of the contents of the response parameter.
    // 5. Let promise be a new promise.
    // 6. Run the following steps in parallel:
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, weakThis = WeakPtr { *this }, response = SharedBuffer::create(response.data(), response.length()), promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
        // 6.1. Let sanitized response be a validated and/or sanitized version of response copy.
        RefPtr<SharedBuffer> sanitizedResponse = m_implementation->sanitizeResponse(response);

        // 6.2. If the preceding step failed, or if sanitized response is empty, reject promise with a newly created TypeError.
        if (!sanitizedResponse || sanitizedResponse->isEmpty()) {
            ERROR_LOG(identifier, "::task - Rejected: empty sanitized response");
            promise->reject(ExceptionCode::TypeError);
            return;
        }

        // 6.3. Let message be null.
        // 6.4. Let message type be null.
        // 6.5. Let session closed be false.
        // 6.6. Let cdm be the CDM instance represented by this object's cdm instance value.
        // 6.7. Use the cdm to execute the following steps:
        m_instanceSession->updateLicense(m_sessionId, m_sessionType, sanitizedResponse.releaseNonNull(), [this, weakThis, promise = WTFMove(promise), identifier = WTFMove(identifier)] (bool sessionWasClosed, std::optional<CDMInstanceSession::KeyStatusVector>&& changedKeys, std::optional<double>&& changedExpiration, std::optional<CDMInstanceSession::Message>&& message, CDMInstanceSession::SuccessValue succeeded) mutable {
            if (!weakThis)
                return;

            // 6.7.1. If the format of sanitized response is invalid in any way, reject promise with a newly created TypeError.
            // 6.7.2. Process sanitized response, following the stipulation for the first matching condition from the following list:
            //   ↳ If sanitized response contains a license or key(s)
            //     Process sanitized response, following the stipulation for the first matching condition from the following list:
            //     ↳ If sessionType is "temporary" and sanitized response does not specify that session data, including any license, key(s), or similar session data it contains, should be stored
            //       Process sanitized response, not storing any session data.
            //     ↳ If sessionType is "persistent-license" and sanitized response contains a persistable license
            //       Process sanitized response, storing the license/key(s) and related session data contained in sanitized response. Such data must be stored such that only the origin of this object's Document can access it.
            //     ↳ If sessionType is "persistent-usage-record" and sanitized response contains a non-persistable license
            //       Run the following steps:
            //         6.7.2.3.1. Process sanitized response, not storing any session data.
            //         6.7.2.3.2. If processing sanitized response results in the addition of keys to the set of known keys, add the key IDs of these keys to this object's record of key usage.
            //     ↳ Otherwise
            //       Reject promise with a newly created TypeError.
            //   ↳ If sanitized response contains a record of license destruction acknowledgement and sessionType is "persistent-license"
            //     Run the following steps:
            //       6.7.2.1. Close the key session and clear all stored session data associated with this object, including the sessionId and record of license destruction.
            //       6.7.2.2. Set session closed to true.
            //   ↳ Otherwise
            //     Process sanitized response, not storing any session data.
            // NOTE: Steps 6.7.1. and 6.7.2. should be implemented in CDMInstance.

            if (succeeded == CDMInstanceSession::SuccessValue::Failed) {
                ERROR_LOG(identifier, "::task() Rejected: Failed");
                promise->reject(ExceptionCode::TypeError);
                return;
            }

            // 6.7.3. If a message needs to be sent to the server, execute the following steps:
            //   6.7.3.1. Let message be that message.
            //   6.7.3.2. Let message type be the appropriate MediaKeyMessageType for the message.
            // 6.8. Queue a task to run the following steps:
            queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, sessionWasClosed, changedKeys = WTFMove(changedKeys), changedExpiration = WTFMove(changedExpiration), message = WTFMove(message), promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
                // 6.8.1.
                if (sessionWasClosed) {
                    // ↳ If session closed is true:
                    //   Run the Session Closed algorithm on this object.
                    sessionClosed();
                } else {
                    // ↳ Otherwise:
                    //   Run the following steps:
                    //     6.8.1.1. If the set of keys known to the CDM for this object changed or the status of any key(s) changed, run the Update Key Statuses
                    //              algorithm on the session, providing each known key's key ID along with the appropriate MediaKeyStatus. Should additional
                    //              processing be necessary to determine with certainty the status of a key, use "status-pending". Once the additional processing
                    //              for one or more keys has completed, run the Update Key Statuses algorithm again with the actual status(es).
                    if (changedKeys)
                        updateKeyStatuses(WTFMove(*changedKeys));

                    //     6.8.1.2. If the expiration time for the session changed, run the Update Expiration algorithm on the session, providing the new expiration time.
                    if (changedExpiration)
                        updateExpiration(*changedExpiration);

                    //     6.8.1.3. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
                    // FIXME: At this point the implementations of preceding steps can't fail.

                    //     6.8.1.4. If message is not null, run the Queue a "message" Event algorithm on the session, providing message type and message.
                    if (message) {
                        MediaKeyMessageType messageType;
                        switch (message->first) {
                        case CDMInstanceSession::MessageType::LicenseRequest:
                            messageType = MediaKeyMessageType::LicenseRequest;
                            break;
                        case CDMInstanceSession::MessageType::LicenseRenewal:
                            messageType = MediaKeyMessageType::LicenseRenewal;
                            break;
                        case CDMInstanceSession::MessageType::LicenseRelease:
                            messageType = MediaKeyMessageType::LicenseRelease;
                            break;
                        case CDMInstanceSession::MessageType::IndividualizationRequest:
                            messageType = MediaKeyMessageType::IndividualizationRequest;
                            break;
                        }

                        enqueueMessage(messageType, WTFMove(message->second));
                    }
                }

                // 6.8.2. Resolve promise.
                ALWAYS_LOG(identifier, "::task() Resolved");
                promise->resolve();
            });
        });
    });

    // 7. Return promise.
}

void MediaKeySession::close(Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeysession-close
    // W3C Editor's Draft 09 November 2016

    auto identifier = LOGIDENTIFIER;

    // 1. Let session be the associated MediaKeySession object.
    // 2. If session is closed, return a resolved promise.
    ALWAYS_LOG(identifier, "EME - closing session ", m_sessionId.utf8().data());

    if (m_closed) {
        ALWAYS_LOG(identifier, "Resolved: already closed");
        promise->resolve();
        return;
    }

    // 3. If session's callable value is false, return a promise rejected with an InvalidStateError.
    if (!m_callable) {
        ERROR_LOG(identifier, "Rejected: !callable");
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    // 4. Let promise be a new promise.
    // 5. Run the following steps in parallel:
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, weakThis = WeakPtr { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
        // 5.1. Let cdm be the CDM instance represented by session's cdm instance value.
        // 5.2. Use cdm to close the key session associated with session.
        m_instanceSession->closeSession(m_sessionId, [this, weakThis, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
            if (!weakThis)
                return;

            // 5.3. Queue a task to run the following steps:
            queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
                // 5.3.1. Run the Session Closed algorithm on the session.
                sessionClosed();

                // 5.3.2. Resolve promise.
                ALWAYS_LOG(identifier, "::task() Resolved");
                promise->resolve();
            });
        });
    });

    // 6. Return promise.
}

void MediaKeySession::remove(Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeysession-remove
    // W3C Editor's Draft 09 November 2016

    // 1. If this object is closed, return a promise rejected with an InvalidStateError.
    // 2. If this object's callable value is false, return a promise rejected with an InvalidStateError.

    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (m_closed || !m_callable) {
        ERROR_LOG(identifier, "Rejected: closed(", m_closed, ") or !callable(", !m_callable, ")");
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    // 3. Let promise be a new promise.
    // 4. Run the following steps in parallel:
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, weakThis = WeakPtr { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
        // 4.1. Let cdm be the CDM instance represented by this object's cdm instance value.
        // 4.2. Let message be null.
        // 4.3. Let message type be null.

        // 4.4. Use the cdm to execute the following steps:
        m_instanceSession->removeSessionData(m_sessionId, m_sessionType, [this, weakThis, promise = WTFMove(promise), identifier = WTFMove(identifier)] (CDMInstanceSession::KeyStatusVector&& keys, RefPtr<SharedBuffer>&& message, CDMInstanceSession::SuccessValue succeeded) mutable {
            if (!weakThis)
                return;

            // 4.4.1. If any license(s) and/or key(s) are associated with the session:
            //   4.4.1.1. Destroy the license(s) and/or key(s) associated with the session.
            //   4.4.1.2. Follow the steps for the value of this object's session type from the following list:
            //     ↳ "temporary"
            //       4.4.1.2.1.1 Continue with the following steps.
            //     ↳ "persistent-license"
            //       4.4.1.2.2.1. Let record of license destruction be a record of license destruction for the license represented by this object.
            //       4.4.1.2.2.2. Store the record of license destruction.
            //       4.4.1.2.2.3. Let message be a message containing or reflecting the record of license destruction.
            //     ↳ "persistent-usage-record"
            //       4.4.1.2.3.1. Store this object's record of key usage.
            //       4.4.1.2.3.2. Let message be a message containing or reflecting this object's record of key usage.
            // NOTE: Step 4.4.1. should be implemented in CDMInstance.

            // 4.5. Queue a task to run the following steps:
            queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, keys = WTFMove(keys), message = WTFMove(message), succeeded, promise = WTFMove(promise), identifier = WTFMove(identifier)] () mutable {
                // 4.5.1. Run the Update Key Statuses algorithm on the session, providing all key ID(s) in the session along with the "released" MediaKeyStatus value for each.
                updateKeyStatuses(WTFMove(keys));

                // 4.5.2. Run the Update Expiration algorithm on the session, providing NaN.
                updateExpiration(std::numeric_limits<double>::quiet_NaN());

                // 4.5.3. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
                if (succeeded == CDMInstanceSession::SuccessValue::Failed) {
                    ERROR_LOG(identifier, "Rejected: failed");
                    promise->reject(ExceptionCode::NotSupportedError);
                    return;
                }

                // 4.5.4. Let message type be "license-release".
                // 4.5.5. If message is not null, run the Queue a "message" Event algorithm on the session, providing message type and message.
                if (message)
                    enqueueMessage(MediaKeyMessageType::LicenseRelease, *message);

                // 4.5.6. Resolve promise.
                ALWAYS_LOG(identifier, "Resolved");
                promise->resolve();
            });
        });
    });

    // 5. Return promise.
}

void MediaKeySession::enqueueMessage(MediaKeyMessageType messageType, const SharedBuffer& message)
{
    // 6.4.1 Queue a "message" Event
    // https://w3c.github.io/encrypted-media/#queue-message
    // W3C Editor's Draft 09 November 2016

    // The following steps are run:
    // 1. Let the session be the specified MediaKeySession object.
    // 2. Queue a task to create an event named message that does not bubble and is not cancellable using the MediaKeyMessageEvent
    //    interface with its type attribute set to message and its isTrusted attribute initialized to true, and dispatch it at the
    //    session.
    auto messageEvent = MediaKeyMessageEvent::create(eventNames().messageEvent, {messageType, message.tryCreateArrayBuffer()}, Event::IsTrusted::Yes);
    queueTaskToDispatchEvent(*this, TaskSource::Networking, WTFMove(messageEvent));
}

void MediaKeySession::updateKeyStatuses(CDMInstanceSession::KeyStatusVector&& inputStatuses)
{
    // https://w3c.github.io/encrypted-media/#update-key-statuses
    // W3C Editor's Draft 09 November 2016

    // 1. Let the session be the associated MediaKeySession object.
    // 2. Let the input statuses be the sequence of pairs key ID and associated MediaKeyStatus pairs.
    // 3. Let the statuses be session's keyStatuses attribute.
    // 4. Run the following steps to replace the contents of statuses:
    //   4.1. Empty statuses.
    //   4.2. For each pair in input statuses.
    //     4.2.1. Let pair be the pair.
    //     4.2.2. Insert an entry for pair's key ID into statuses with the value of pair's MediaKeyStatus value.

    static auto toMediaKeyStatus = [] (CDMInstanceSession::KeyStatus status) -> MediaKeyStatus {
        switch (status) {
        case CDMInstanceSession::KeyStatus::Usable:
            return MediaKeyStatus::Usable;
        case CDMInstanceSession::KeyStatus::Expired:
            return MediaKeyStatus::Expired;
        case CDMInstanceSession::KeyStatus::Released:
            return MediaKeyStatus::Released;
        case CDMInstanceSession::KeyStatus::OutputRestricted:
            return MediaKeyStatus::OutputRestricted;
        case CDMInstanceSession::KeyStatus::OutputDownscaled:
            return MediaKeyStatus::OutputDownscaled;
        case CDMInstanceSession::KeyStatus::StatusPending:
            return MediaKeyStatus::StatusPending;
        case CDMInstanceSession::KeyStatus::InternalError:
            return MediaKeyStatus::InternalError;
        };

        ASSERT_NOT_REACHED();
        return MediaKeyStatus::InternalError;
    };

#if !RELEASE_LOG_DISABLED
    HashCountedSet<MediaKeyStatus, IntHash<MediaKeyStatus>, WTF::StrongEnumHashTraits<MediaKeyStatus>> statusCounts;
#endif

    m_statuses = WTF::map(WTFMove(inputStatuses), [&](auto&& inputStatus) {
        auto status = std::pair { WTFMove(inputStatus.first), toMediaKeyStatus(inputStatus.second) };
#if !RELEASE_LOG_DISABLED
        statusCounts.add(status.second);
#endif
        return status;
    });

#if !RELEASE_LOG_DISABLED
    StringBuilder statusString;
    for (auto& statusCount : statusCounts) {
        if (!statusCount.value)
            continue;
        if (!statusString.isEmpty())
            statusString.append(", ");
        statusString.append(makeString(convertEnumerationToString(statusCount.key), ": ", statusCount.value));
    }
    ALWAYS_LOG(LOGIDENTIFIER, "statuses: {", statusString.toString(), "}");
#endif

    // 5. Queue a task to fire a simple event named keystatuseschange at the session.
    queueTaskToDispatchEvent(*this, TaskSource::Networking, Event::create(eventNames().keystatuseschangeEvent, Event::CanBubble::No, Event::IsCancelable::No));

    // 6. Queue a task to run the Attempt to Resume Playback If Necessary algorithm on each of the media element(s) whose mediaKeys attribute is the MediaKeys object that created the session.
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking,
        [this] () mutable {
            if (m_keys)
                m_keys->attemptToResumePlaybackOnClients();
        });
}

void MediaKeySession::sendMessage(CDMMessageType messageType, Ref<SharedBuffer>&& message)
{
    enqueueMessage(messageType, message);
}

void MediaKeySession::sessionIdChanged(const String& sessionId)
{
    m_sessionId = sessionId;
}

PlatformDisplayID MediaKeySession::displayID()
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return 0;

    if (auto* page = document->page())
        return page->displayID();

    return 0;
}

void MediaKeySession::updateExpiration(double)
{
    notImplemented();
}

void MediaKeySession::sessionClosed()
{
    // https://w3c.github.io/encrypted-media/#session-closed
    // W3C Editor's Draft 09 November 2016
    ALWAYS_LOG(LOGIDENTIFIER);

    // 1. Let session be the associated MediaKeySession object.
    // 2. If session's session type is "persistent-usage-record", execute the following steps in parallel:
    if (m_sessionType == MediaKeySessionType::PersistentUsageRecord) {
        // 2.1. Let cdm be the CDM instance represented by session's cdm instance value.
        // 2.2. Use cdm to store session's record of key usage, if it exists.
        m_instanceSession->storeRecordOfKeyUsage(m_sessionId);
    }

    // 3. Run the Update Key Statuses algorithm on the session, providing an empty sequence.
    updateKeyStatuses({ });

    // 4. Run the Update Expiration algorithm on the session, providing NaN.
    updateExpiration(std::numeric_limits<double>::quiet_NaN());

    // Let's consider the session closed before any promise on the 'closed' attribute is resolved.
    m_closed = true;

    // 5. Let promise be the closed attribute of the session.
    // 6. Resolve promise.
    m_closedPromise->resolve();
}

String MediaKeySession::mediaKeysStorageDirectory() const
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    return document ? document->mediaKeysStorageDirectory() : emptyString();
}

CDMKeyGroupingStrategy MediaKeySession::keyGroupingStrategy() const
{
#if HAVE(AVCONTENTKEYSPECIFIER)
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (document && document->settings().sampleBufferContentKeySessionSupportEnabled())
        return CDMKeyGroupingStrategy::BuiltIn;
#endif

    return CDMKeyGroupingStrategy::Platform;
}

bool MediaKeySession::virtualHasPendingActivity() const
{
    // A MediaKeySession object SHALL NOT be destroyed and SHALL continue to receive events if it is not closed and the MediaKeys object that created it remains accessible.
    return !m_closed && m_keys;
}

const char* MediaKeySession::activeDOMObjectName() const
{
    return "MediaKeySession";
}

void MediaKeySession::stop()
{
    if (m_closed) {
        ALWAYS_LOG(LOGIDENTIFIER, "Already closed");
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);

    m_instanceSession->closeSession(m_sessionId, [this, weakThis = WeakPtr { this }, logIdentifier = LOGIDENTIFIER] {
        if (!weakThis)
            return;

        ALWAYS_LOG(logIdentifier, "::lambda, closed");
        sessionClosed();
    });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaKeySession::logChannel() const
{
    return LogEME;
}
#endif

void MediaKeySession::displayChanged(PlatformDisplayID displayID)
{
    m_instanceSession->displayChanged(displayID);
}

} // namespace WebCore

#endif
