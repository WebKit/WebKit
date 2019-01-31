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

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "ActiveDOMObject.h"
#include "CDMInstanceSession.h"
#include "DOMPromiseProxy.h"
#include "EventTarget.h"
#include "GenericEventQueue.h"
#include "GenericTaskQueue.h"
#include "MediaKeyMessageType.h"
#include "MediaKeySessionType.h"
#include "MediaKeyStatus.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class BufferSource;
class CDM;
class MediaKeyStatusMap;
class MediaKeys;
class SharedBuffer;

class MediaKeySession final : public RefCounted<MediaKeySession>, public EventTargetWithInlineData, public ActiveDOMObject, public CanMakeWeakPtr<MediaKeySession>, public CDMInstanceSessionClient {
public:
    static Ref<MediaKeySession> create(ScriptExecutionContext&, WeakPtr<MediaKeys>&&, MediaKeySessionType, bool useDistinctiveIdentifier, Ref<CDM>&&, Ref<CDMInstanceSession>&&);
    virtual ~MediaKeySession();

    using RefCounted<MediaKeySession>::ref;
    using RefCounted<MediaKeySession>::deref;

    bool isClosed() const { return m_closed; }

    const String& sessionId() const;
    double expiration() const;
    Ref<MediaKeyStatusMap> keyStatuses() const;

    void generateRequest(const AtomicString&, const BufferSource&, Ref<DeferredPromise>&&);
    void load(const String&, Ref<DeferredPromise>&&);
    void update(const BufferSource&, Ref<DeferredPromise>&&);
    void close(Ref<DeferredPromise>&&);
    void remove(Ref<DeferredPromise>&&);

    using ClosedPromise = DOMPromiseProxy<IDLVoid>;
    ClosedPromise& closed() { return m_closedPromise; }

    const Vector<std::pair<Ref<SharedBuffer>, MediaKeyStatus>>& statuses() const { return m_statuses; }

private:
    MediaKeySession(ScriptExecutionContext&, WeakPtr<MediaKeys>&&, MediaKeySessionType, bool useDistinctiveIdentifier, Ref<CDM>&&, Ref<CDMInstanceSession>&&);
    void enqueueMessage(MediaKeyMessageType, const SharedBuffer&);
    void updateExpiration(double);
    void sessionClosed();
    String mediaKeysStorageDirectory() const;

    // CDMInstanceSessionClient
    void updateKeyStatuses(CDMInstanceSessionClient::KeyStatusVector&&) override;
    void sendMessage(CDMMessageType, Ref<SharedBuffer>&& message) final;
    void sessionIdChanged(const String&) final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const override { return MediaKeySessionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // ActiveDOMObject
    bool hasPendingActivity() const override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void stop() override;

    WeakPtr<MediaKeys> m_keys;
    String m_sessionId;
    double m_expiration;
    ClosedPromise m_closedPromise;
    Ref<MediaKeyStatusMap> m_keyStatuses;
    bool m_closed { false };
    bool m_uninitialized { true };
    bool m_callable { false };
    bool m_useDistinctiveIdentifier;
    MediaKeySessionType m_sessionType;
    Ref<CDM> m_implementation;
    Ref<CDMInstanceSession> m_instanceSession;
    GenericEventQueue m_eventQueue;
    GenericTaskQueue<Timer> m_taskQueue;
    Vector<Ref<SharedBuffer>> m_recordOfKeyUsage;
    double m_firstDecryptTime { 0 };
    double m_latestDecryptTime { 0 };
    Vector<std::pair<Ref<SharedBuffer>, MediaKeyStatus>> m_statuses;
    WeakPtrFactory<CDMInstanceSessionClient> m_cdmInstanceSessionClientWeakPtrFactory;
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
