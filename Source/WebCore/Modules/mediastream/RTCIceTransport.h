/*
 * Copyright (C) 2016 Ericsson AB. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "RTCIceGatheringState.h"
#include "RTCIceTransportBackend.h"
#include "RTCIceTransportState.h"
#include "RTCPeerConnection.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class RTCPeerConnection;

class RTCIceTransport : public RefCounted<RTCIceTransport>, public ActiveDOMObject, public EventTargetWithInlineData, public RTCIceTransportBackend::Client {
    WTF_MAKE_ISO_ALLOCATED(RTCIceTransport);
public:
    static Ref<RTCIceTransport> create(ScriptExecutionContext&, UniqueRef<RTCIceTransportBackend>&&, RTCPeerConnection&);
    ~RTCIceTransport();

    RTCIceTransportState state() const { return m_transportState; }
    RTCIceGatheringState gatheringState() const { return m_gatheringState; }

    const RTCIceTransportBackend& backend() const { return m_backend.get(); }
    RTCPeerConnection* connection() const { return m_connection.get(); }

    using RefCounted<RTCIceTransport>::ref;
    using RefCounted<RTCIceTransport>::deref;

private:
    RTCIceTransport(ScriptExecutionContext&, UniqueRef<RTCIceTransportBackend>&&, RTCPeerConnection&);

    // EventTargetWithInlineData
    EventTargetInterface eventTargetInterface() const final { return RTCIceTransportEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    void stop() final;
    const char* activeDOMObjectName() const final { return "RTCIceTransport"; }
    bool virtualHasPendingActivity() const final;

    // RTCIceTransportBackend::Client
    void onStateChanged(RTCIceTransportState) final;
    void onGatheringStateChanged(RTCIceGatheringState) final;

    bool m_isStopped { false };
    UniqueRef<RTCIceTransportBackend> m_backend;
    WeakPtr<RTCPeerConnection> m_connection;
    RTCIceTransportState m_transportState { RTCIceTransportState::New };
    RTCIceGatheringState m_gatheringState { RTCIceGatheringState::New };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
