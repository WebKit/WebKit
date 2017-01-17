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
#include "EventNames.h"
#include "MediaKeyMessageEvent.h"
#include "MediaKeyMessageType.h"
#include "MediaKeyStatusMap.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<MediaKeySession> MediaKeySession::create(ScriptExecutionContext& context, MediaKeySessionType sessionType, bool useDistinctiveIdentifier, Ref<CDM>&& implementation, Ref<CDMInstance>&& instance)
{
    auto session = adoptRef(*new MediaKeySession(context, sessionType, useDistinctiveIdentifier, WTFMove(implementation), WTFMove(instance)));
    session->suspendIfNeeded();
    return session;
}

MediaKeySession::MediaKeySession(ScriptExecutionContext& context, MediaKeySessionType sessionType, bool useDistinctiveIdentifier, Ref<CDM>&& implementation, Ref<CDMInstance>&& instance)
    : ActiveDOMObject(&context)
    , m_expiration(std::numeric_limits<double>::quiet_NaN())
    , m_keyStatuses(MediaKeyStatusMap::create())
    , m_useDistinctiveIdentifier(useDistinctiveIdentifier)
    , m_sessionType(sessionType)
    , m_implementation(WTFMove(implementation))
    , m_instance(WTFMove(instance))
    , m_eventQueue(*this)
    , m_weakPtrFactory(this)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-setservercertificate
    // W3C Editor's Draft 09 November 2016
    // createSession(), ctd.

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
}

MediaKeySession::~MediaKeySession() = default;

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

void MediaKeySession::generateRequest(const AtomicString& initDataType, const BufferSource& initData, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeysession-generaterequest
    // W3C Editor's Draft 09 November 2016

    // When this method is invoked, the user agent must run the following steps:
    // 1. If this object is closed, return a promise rejected with an InvalidStateError.
    // 2. If this object's uninitialized value is false, return a promise rejected with an InvalidStateError.
    if (m_closed || !m_uninitialized) {
        promise->reject(INVALID_STATE_ERR);
        return;
    }

    // 3. Let this object's uninitialized value be false.
    m_uninitialized = false;

    // 4. If initDataType is the empty string, return a promise rejected with a newly created TypeError.
    // 5. If initData is an empty array, return a promise rejected with a newly created TypeError.
    if (initDataType.isEmpty() || !initData.length()) {
        promise->reject(TypeError);
        return;
    }

    // 6. If the Key System implementation represented by this object's cdm implementation value does not support
    //    initDataType as an Initialization Data Type, return a promise rejected with a NotSupportedError. String
    //    comparison is case-sensitive.
    if (!m_implementation->supportsInitDataType(initDataType)) {
        promise->reject(NOT_SUPPORTED_ERR);
        return;
    }

    // 7. Let init data be a copy of the contents of the initData parameter.
    // 8. Let session type be this object's session type.
    // 9. Let promise be a new promise.
    // 10. Run the following steps in parallel:
    m_taskQueue.enqueueTask([this, initData = SharedBuffer::create(initData.data(), initData.length()), initDataType, promise = WTFMove(promise)] () mutable {
        // 10.1. If the init data is not valid for initDataType, reject promise with a newly created TypeError.
        // 10.2. Let sanitized init data be a validated and sanitized version of init data.
        RefPtr<SharedBuffer> sanitizedInitData = m_implementation->sanitizeInitData(initDataType, initData);

        // 10.3. If the preceding step failed, reject promise with a newly created TypeError.
        if (!sanitizedInitData) {
            promise->reject(TypeError);
            return;
        }

        // 10.4. If sanitized init data is empty, reject promise with a NotSupportedError.
        if (sanitizedInitData->isEmpty()) {
            promise->reject(NOT_SUPPORTED_ERR);
            return;
        }

        // 10.5. Let session id be the empty string.
        // 10.6. Let message be null.
        // 10.7. Let message type be null.
        // 10.8. Let cdm be the CDM instance represented by this object's cdm instance value.
        // 10.9. Use the cdm to execute the following steps:
        // 10.9.1. If the sanitized init data is not supported by the cdm, reject promise with a NotSupportedError.
        if (!m_implementation->supportsInitData(initDataType, *sanitizedInitData)) {
            promise->reject(NOT_SUPPORTED_ERR);
            return;
        }

        // 10.9.2 Follow the steps for the value of session type from the following list:
        CDMInstance::LicenseType requestedLicenseType;
        switch (m_sessionType) {
        case MediaKeySessionType::Temporary:
            // ↳ "temporary"
            // Let requested license type be a temporary non-persistable license.
            requestedLicenseType = CDMInstance::LicenseType::Temporary;
            break;
        case MediaKeySessionType::PersistentLicense:
            // ↳ "persistent-license"
            // Let requested license type be a persistable license.
            requestedLicenseType = CDMInstance::LicenseType::Persistable;
            break;
        case MediaKeySessionType::PersistentUsageRecord:
            // ↳ "persistent-usage-record"
            // 1. Initialize this object's record of key usage as follows.
            //    Set the list of key IDs known to the session to an empty list.
            m_recordOfKeyUsage.clear();

            //    Set the first decrypt time to null.
            m_firstDecryptTime = 0;

            //    Set the latest decrypt time to null.
            m_latestDecryptTime = 0;

            // 2. Let requested license type be a non-persistable license that will
            //    persist a record of key usage.
            requestedLicenseType = CDMInstance::LicenseType::UsageRecord;
            break;
        }

        m_instance->requestLicense(requestedLicenseType, initDataType, WTFMove(initData), [this, weakThis = m_weakPtrFactory.createWeakPtr(), promise = WTFMove(promise)] (Ref<SharedBuffer>&& message, const String& sessionId, bool needsIndividualization, CDMInstance::SuccessValue succeeded) mutable {
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
            m_taskQueue.enqueueTask([this, promise = WTFMove(promise), message = WTFMove(message), messageType, sessionId, succeeded] () mutable {
                // 10.10.1. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
                if (succeeded == CDMInstance::SuccessValue::Failed) {
                    promise->reject(NOT_SUPPORTED_ERR);
                    return;
                }
                // 10.10.2. Set the sessionId attribute to session id.
                m_sessionId = sessionId;

                // 10.9.3. Let this object's callable value be true.
                m_callable = true;

                // 10.9.3. Run the Queue a "message" Event algorithm on the session, providing message type and message.
                enqueueMessage(messageType, message);

                // 10.9.3. Resolve promise.
                promise->resolve();
            });
        });
    });

    // 11. Return promise.
}

void MediaKeySession::load(const String&, Ref<DeferredPromise>&&)
{
    notImplemented();
}

void MediaKeySession::update(const BufferSource&, Ref<DeferredPromise>&&)
{
    notImplemented();
}

void MediaKeySession::close(Ref<DeferredPromise>&&)
{
    notImplemented();
}

void MediaKeySession::remove(Ref<DeferredPromise>&&)
{
    notImplemented();
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
    auto messageEvent = MediaKeyMessageEvent::create(eventNames().messageEvent, {messageType, message.createArrayBuffer()}, Event::IsTrusted::Yes);
    m_eventQueue.enqueueEvent(WTFMove(messageEvent));
}

bool MediaKeySession::hasPendingActivity() const
{
    notImplemented();
    return false;
}

const char* MediaKeySession::activeDOMObjectName() const
{
    notImplemented();
    return "MediaKeySession";
}

bool MediaKeySession::canSuspendForDocumentSuspension() const
{
    notImplemented();
    return false;
}

void MediaKeySession::stop()
{
    notImplemented();
}

} // namespace WebCore

#endif
