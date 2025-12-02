/*
 * Copyright (C) 2016 Ericsson AB. All rights reserved.
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include "EventTargetInterfaces.h"
#include "RTCIceCandidate.h"
#include "RTCIceGatheringState.h"
#include "RTCIceTransportBackend.h"
#include "RTCIceTransportState.h"
#include "RTCPeerConnection.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class RTCPeerConnection;

class RTCIceTransport : public RefCounted<RTCIceTransport>, public ActiveDOMObject, public EventTarget, public RTCIceTransportBackendClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RTCIceTransport);
public:
    static Ref<RTCIceTransport> create(ScriptExecutionContext&, UniqueRef<RTCIceTransportBackend>&&, RTCPeerConnection&);
    ~RTCIceTransport();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    RTCIceTransportState state() const { return m_transportState; }
    RTCIceGatheringState gatheringState() const { return m_gatheringState; }

    const RTCIceTransportBackend& backend() const { return m_backend.get(); }
    RefPtr<RTCPeerConnection> connection() const { return m_connection.get(); }

    struct CandidatePair {
        RefPtr<RTCIceCandidate> local;
        RefPtr<RTCIceCandidate> remote;
    };
    std::optional<CandidatePair> getSelectedCandidatePair();

private:
    RTCIceTransport(ScriptExecutionContext&, UniqueRef<RTCIceTransportBackend>&&, RTCPeerConnection&);

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::RTCIceTransport; }
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject.
    void stop() final;
    bool virtualHasPendingActivity() const final;

    // RTCIceTransportBackendClient
    void onStateChanged(RTCIceTransportState) final;
    void onGatheringStateChanged(RTCIceGatheringState) final;
    void onSelectedCandidatePairChanged(RefPtr<RTCIceCandidate>&&, RefPtr<RTCIceCandidate>&&) final;

    bool m_isStopped { false };
    const UniqueRef<RTCIceTransportBackend> m_backend;
    WeakPtr<RTCPeerConnection, WeakPtrImplWithEventTargetData> m_connection;
    RTCIceTransportState m_transportState { RTCIceTransportState::New };
    RTCIceGatheringState m_gatheringState { RTCIceGatheringState::New };
    std::optional<CandidatePair> m_selectedCandidatePair;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
