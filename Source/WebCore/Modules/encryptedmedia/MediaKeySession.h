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

#ifndef MediaKeySession_h
#define MediaKeySession_h

#if ENABLE(ENCRYPTED_MEDIA_V2)

#include "ActiveDOMObject.h"
#include "CDMSession.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "GenericEventQueue.h"
#include "Timer.h"
#include <runtime/Uint8Array.h>
#include <wtf/Deque.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaKeyError;
class MediaKeys;

class MediaKeySession final : public RefCounted<MediaKeySession>, public EventTargetWithInlineData, public ActiveDOMObject, public CDMSessionClient {
public:
    static Ref<MediaKeySession> create(ScriptExecutionContext*, MediaKeys*, const String& keySystem);
    ~MediaKeySession();

    const String& keySystem() const { return m_keySystem; }
    CDMSession* session() { return m_session.get(); }
    const String& sessionId() const;

    void setError(MediaKeyError*);
    MediaKeyError* error() { return m_error.get(); }

    void setKeys(MediaKeys* keys) { m_keys = keys; }
    MediaKeys* keys() const { return m_keys; }

    void generateKeyRequest(const String& mimeType, Uint8Array* initData);
    void update(Uint8Array* key, ExceptionCode&);

    bool isClosed() const { return !m_session; }
    void close();

    RefPtr<ArrayBuffer> cachedKeyForKeyId(const String& keyId) const;

    using RefCounted<MediaKeySession>::ref;
    using RefCounted<MediaKeySession>::deref;

    void enqueueEvent(PassRefPtr<Event>);

    virtual EventTargetInterface eventTargetInterface() const override { return MediaKeySessionEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }

    // ActiveDOMObject API.
    bool hasPendingActivity() const override;

protected:
    MediaKeySession(ScriptExecutionContext*, MediaKeys*, const String& keySystem);
    void keyRequestTimerFired();
    void addKeyTimerFired();

    // CDMSessionClient
    virtual void sendMessage(Uint8Array*, String destinationURL) override;
    virtual void sendError(MediaKeyErrorCode, unsigned long systemCode) override;
    virtual String mediaKeysStorageDirectory() const override;

    MediaKeys* m_keys;
    String m_keySystem;
    String m_sessionId;
    RefPtr<MediaKeyError> m_error;
    GenericEventQueue m_asyncEventQueue;
    std::unique_ptr<CDMSession> m_session;

    struct PendingKeyRequest {
        PendingKeyRequest(const String& mimeType, Uint8Array* initData) : mimeType(mimeType), initData(initData) { }
        String mimeType;
        RefPtr<Uint8Array> initData;
    };
    Deque<PendingKeyRequest> m_pendingKeyRequests;
    Timer m_keyRequestTimer;

    Deque<RefPtr<Uint8Array>> m_pendingKeys;
    Timer m_addKeyTimer;

private:
    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

    // ActiveDOMObject API.
    void stop() override;
    bool canSuspendForDocumentSuspension() const override;
    const char* activeDOMObjectName() const override;
};

}

#endif // ENABLE(ENCRYPTED_MEDIA_V2)

#endif // MediaKeySession_h
