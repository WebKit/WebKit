/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "WebKitMediaKeySession.h"

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "Document.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Page.h"
#include "SecurityOriginData.h"
#include "Settings.h"
#include "WebKitMediaKeyError.h"
#include "WebKitMediaKeyMessageEvent.h"
#include "WebKitMediaKeys.h"
#include <wtf/FileSystem.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebKitMediaKeySession);

Ref<WebKitMediaKeySession> WebKitMediaKeySession::create(ScriptExecutionContext& context, WebKitMediaKeys& keys, const String& keySystem)
{
    auto session = adoptRef(*new WebKitMediaKeySession(context, keys, keySystem));
    session->suspendIfNeeded();
    return session;
}

WebKitMediaKeySession::WebKitMediaKeySession(ScriptExecutionContext& context, WebKitMediaKeys& keys, const String& keySystem)
    : ActiveDOMObject(&context)
    , m_keys(&keys)
    , m_keySystem(keySystem)
    , m_session(keys.cdm().createSession(*this))
    , m_keyRequestTimer(*this, &WebKitMediaKeySession::keyRequestTimerFired)
    , m_addKeyTimer(*this, &WebKitMediaKeySession::addKeyTimerFired)
{
    if (m_session)
        m_sessionId = m_session->sessionId();
}

WebKitMediaKeySession::~WebKitMediaKeySession()
{
    if (m_session)
        m_session->setClient(nullptr);
}

void WebKitMediaKeySession::close()
{
    if (m_session) {
        m_session->releaseKeys();
        m_session = nullptr;
    }
}

RefPtr<ArrayBuffer> WebKitMediaKeySession::cachedKeyForKeyId(const String& keyId) const
{
    return m_session ? m_session->cachedKeyForKeyID(keyId) : nullptr;
}

void WebKitMediaKeySession::generateKeyRequest(const String& mimeType, Ref<Uint8Array>&& initData)
{
    m_pendingKeyRequests.append({ mimeType, WTFMove(initData) });
    m_keyRequestTimer.startOneShot(0_s);
}

void WebKitMediaKeySession::keyRequestTimerFired()
{
    ASSERT(m_pendingKeyRequests.size());
    if (!m_session)
        return;

    while (!m_pendingKeyRequests.isEmpty()) {
        auto request = m_pendingKeyRequests.takeFirst();

        // NOTE: Continued from step 5 in MediaKeys::createSession().
        // The user agent will asynchronously execute the following steps in the task:

        // 1. Let cdm be the cdm loaded in the MediaKeys constructor.
        // 2. Let destinationURL be null.
        String destinationURL;
        WebKitMediaKeyError::Code errorCode = 0;
        uint32_t systemCode = 0;

        // 3. Use cdm to generate a key request and follow the steps for the first matching condition from the following list:

        auto keyRequest = m_session->generateKeyRequest(request.mimeType, request.initData.ptr(), destinationURL, errorCode, systemCode);

        // Otherwise [if a request is not successfully generated]:
        if (errorCode) {
            // 3.1. Create a new MediaKeyError object with the following attributes:
            //      code = the appropriate MediaKeyError code
            //      systemCode = a Key System-specific value, if provided, and 0 otherwise
            // 3.2. Set the MediaKeySession object's error attribute to the error object created in the previous step.
            // 3.3. queue a task to fire a simple event named keyerror at the MediaKeySession object.
            sendError(errorCode, systemCode);
            // 3.4. Abort the task.
            continue;
        }

        // 4. queue a task to fire a simple event named keymessage at the new object
        //    The event is of type MediaKeyMessageEvent and has:
        //    message = key request
        //    destinationURL = destinationURL
        if (keyRequest)
            sendMessage(keyRequest.get(), destinationURL);
    }
}

ExceptionOr<void> WebKitMediaKeySession::update(Ref<Uint8Array>&& key)
{
    // From <http://dvcs.w3.org/hg/html-media/raw-file/tip/encrypted-media/encrypted-media.html#dom-addkey>:
    // The addKey(key) method must run the following steps:
    // 1. If the first or second argument [sic] is an empty array, throw an InvalidAccessError.
    // NOTE: the reference to a "second argument" is a spec bug.
    if (!key->length())
        return Exception { InvalidAccessError };

    // 2. Schedule a task to handle the call, providing key.
    m_pendingKeys.append(WTFMove(key));
    m_addKeyTimer.startOneShot(0_s);

    return { };
}

void WebKitMediaKeySession::addKeyTimerFired()
{
    ASSERT(m_pendingKeys.size());
    if (!m_session)
        return;

    while (!m_pendingKeys.isEmpty()) {
        auto pendingKey = m_pendingKeys.takeFirst();
        unsigned short errorCode = 0;
        uint32_t systemCode = 0;

        // NOTE: Continued from step 2. of MediaKeySession::update()
        // 2.1. Let cdm be the cdm loaded in the MediaKeys constructor.
        // NOTE: This is m_session.
        // 2.2. Let 'did store key' be false.
        bool didStoreKey = false;
        // 2.3. Let 'next message' be null.
        RefPtr<Uint8Array> nextMessage;
        // 2.4. Use cdm to handle key.
        didStoreKey = m_session->update(pendingKey.ptr(), nextMessage, errorCode, systemCode);
        // 2.5. If did store key is true and the media element is waiting for a key, queue a task to attempt to resume playback.
        // TODO: Find and restart the media element

        // 2.6. If next message is not null, queue a task to fire a simple event named keymessage at the MediaKeySession object.
        //      The event is of type MediaKeyMessageEvent and has:
        //      message = next message
        //      destinationURL = null
        if (nextMessage)
            sendMessage(nextMessage.get(), emptyString());

        // 2.7. If did store key is true, queue a task to fire a simple event named keyadded at the MediaKeySession object.
        if (didStoreKey) {
            auto keyaddedEvent = Event::create(eventNames().webkitkeyaddedEvent, Event::CanBubble::No, Event::IsCancelable::No);
            keyaddedEvent->setTarget(this);
            queueTaskToDispatchEvent(*this, TaskSource::Networking, WTFMove(keyaddedEvent));

            ASSERT(m_keys);
            m_keys->keyAdded();
        }

        // 2.8. If any of the preceding steps in the task failed
        if (errorCode) {
            // 2.8.1. Create a new MediaKeyError object with the following attributes:
            //        code = the appropriate MediaKeyError code
            //        systemCode = a Key System-specific value, if provided, and 0 otherwise
            // 2.8.2. Set the MediaKeySession object's error attribute to the error object created in the previous step.
            // 2.8.3. queue a task to fire a simple event named keyerror at the MediaKeySession object.
            sendError(errorCode, systemCode);
            // 2.8.4. Abort the task.
            // NOTE: no-op
        }
    }
}

void WebKitMediaKeySession::sendMessage(Uint8Array* message, String destinationURL)
{
    auto event = WebKitMediaKeyMessageEvent::create(eventNames().webkitkeymessageEvent, message, destinationURL);
    event->setTarget(this);
    queueTaskToDispatchEvent(*this, TaskSource::Networking, WTFMove(event));
}

void WebKitMediaKeySession::sendError(MediaKeyErrorCode errorCode, uint32_t systemCode)
{
    m_error = WebKitMediaKeyError::create(errorCode, systemCode);

    auto keyerrorEvent = Event::create(eventNames().webkitkeyerrorEvent, Event::CanBubble::No, Event::IsCancelable::No);
    keyerrorEvent->setTarget(this);
    queueTaskToDispatchEvent(*this, TaskSource::Networking, WTFMove(keyerrorEvent));
}

String WebKitMediaKeySession::mediaKeysStorageDirectory() const
{
    auto* document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return emptyString();

    auto* page = document->page();
    if (!page || page->usesEphemeralSession())
        return emptyString();

    auto storageDirectory = document->settings().mediaKeysStorageDirectory();
    if (storageDirectory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(storageDirectory, document->securityOrigin().data().databaseIdentifier());
}

bool WebKitMediaKeySession::virtualHasPendingActivity() const
{
    return m_keys && m_session;
}

void WebKitMediaKeySession::stop()
{
    close();
}

const char* WebKitMediaKeySession::activeDOMObjectName() const
{
    return "WebKitMediaKeySession";
}

}

#endif
