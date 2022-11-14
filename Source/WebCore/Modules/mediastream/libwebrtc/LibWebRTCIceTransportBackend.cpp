/*
 * Copyright (C) 2021 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCIceTransportBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCProvider.h"
#include "LibWebRTCUtils.h"
#include "RTCIceCandidate.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/ice_transport_interface.h>
#include <webrtc/p2p/base/ice_transport_internal.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

static inline RTCIceTransportState toRTCIceTransportState(webrtc::IceTransportState state)
{
    switch (state) {
    case webrtc::IceTransportState::kNew:
        return RTCIceTransportState::New;
    case webrtc::IceTransportState::kChecking:
        return RTCIceTransportState::Checking;
    case webrtc::IceTransportState::kConnected:
        return RTCIceTransportState::Connected;
    case webrtc::IceTransportState::kFailed:
        return RTCIceTransportState::Failed;
    case webrtc::IceTransportState::kCompleted:
        return RTCIceTransportState::Completed;
    case webrtc::IceTransportState::kDisconnected:
        return RTCIceTransportState::Disconnected;
    case webrtc::IceTransportState::kClosed:
        return RTCIceTransportState::Closed;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static inline RTCIceGatheringState toRTCIceGatheringState(cricket::IceGatheringState state)
{
    switch (state) {
    case cricket::IceGatheringState::kIceGatheringNew:
        return RTCIceGatheringState::New;
    case cricket::IceGatheringState::kIceGatheringGathering:
        return RTCIceGatheringState::Gathering;
    case cricket::IceGatheringState::kIceGatheringComplete:
        return RTCIceGatheringState::Complete;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

class LibWebRTCIceTransportBackendObserver final : public ThreadSafeRefCounted<LibWebRTCIceTransportBackendObserver>, public sigslot::has_slots<> {
public:
    static Ref<LibWebRTCIceTransportBackendObserver> create(RTCIceTransportBackend::Client& client, rtc::scoped_refptr<webrtc::IceTransportInterface> backend) { return adoptRef(*new LibWebRTCIceTransportBackendObserver(client, WTFMove(backend))); }

    void start();
    void stop();

private:
    LibWebRTCIceTransportBackendObserver(RTCIceTransportBackend::Client&, rtc::scoped_refptr<webrtc::IceTransportInterface>&&);

    void onIceTransportStateChanged(cricket::IceTransportInternal*);
    void onGatheringStateChanged(cricket::IceTransportInternal*);
    void onSelectedCandidatePairChanged(const cricket::CandidatePairChangeEvent&);

    void processSelectedCandidatePairChanged(const cricket::Candidate&, const cricket::Candidate&);

    rtc::scoped_refptr<webrtc::IceTransportInterface> m_backend;
    WeakPtr<RTCIceTransportBackend::Client> m_client;
};

LibWebRTCIceTransportBackendObserver::LibWebRTCIceTransportBackendObserver(RTCIceTransportBackend::Client& client, rtc::scoped_refptr<webrtc::IceTransportInterface>&& backend)
    : m_backend(WTFMove(backend))
    , m_client(client)
{
    ASSERT(m_backend);
}

void LibWebRTCIceTransportBackendObserver::start()
{
    LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }]() mutable {
        auto* internal = m_backend->internal();
        if (!internal)
            return;
        internal->SignalIceTransportStateChanged.connect(this, &LibWebRTCIceTransportBackendObserver::onIceTransportStateChanged);
        internal->SignalGatheringState.connect(this, &LibWebRTCIceTransportBackendObserver::onGatheringStateChanged);
        internal->SignalCandidatePairChanged.connect(this, &LibWebRTCIceTransportBackendObserver::onSelectedCandidatePairChanged);

        auto transportState = internal->GetIceTransportState();
        // We start observing a bit late and might miss the checking state. Synthesize it as needed.
        if (transportState > webrtc::IceTransportState::kChecking && transportState != webrtc::IceTransportState::kClosed) {
            callOnMainThread([protectedThis = Ref { *this }] {
                if (protectedThis->m_client)
                    protectedThis->m_client->onStateChanged(RTCIceTransportState::Checking);
            });
        }
        callOnMainThread([protectedThis = Ref { *this }, transportState, gatheringState = internal->gathering_state()] {
            if (!protectedThis->m_client)
                return;
            protectedThis->m_client->onStateChanged(toRTCIceTransportState(transportState));
            protectedThis->m_client->onGatheringStateChanged(toRTCIceGatheringState(gatheringState));
        });

        if (auto candidatePair = internal->GetSelectedCandidatePair())
            processSelectedCandidatePairChanged(candidatePair->local, candidatePair->remote);
    });
}

void LibWebRTCIceTransportBackendObserver::stop()
{
    m_client = nullptr;
    LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }] {
        auto* internal = m_backend->internal();
        if (!internal)
            return;
        internal->SignalIceTransportStateChanged.disconnect(this);
        internal->SignalGatheringState.disconnect(this);
        internal->SignalCandidatePairChanged.disconnect(this);
    });
}

void LibWebRTCIceTransportBackendObserver::onIceTransportStateChanged(cricket::IceTransportInternal* internal)
{
    callOnMainThread([protectedThis = Ref { *this }, state = internal->GetIceTransportState()] {
        if (protectedThis->m_client)
            protectedThis->m_client->onStateChanged(toRTCIceTransportState(state));
    });
}

void LibWebRTCIceTransportBackendObserver::onGatheringStateChanged(cricket::IceTransportInternal* internal)
{
    callOnMainThread([protectedThis = Ref { *this }, state = internal->gathering_state()] {
        if (protectedThis->m_client)
            protectedThis->m_client->onGatheringStateChanged(toRTCIceGatheringState(state));
    });
}

void LibWebRTCIceTransportBackendObserver::onSelectedCandidatePairChanged(const cricket::CandidatePairChangeEvent& event)
{
    processSelectedCandidatePairChanged(event.selected_candidate_pair.local, event.selected_candidate_pair.remote);
}

void LibWebRTCIceTransportBackendObserver::processSelectedCandidatePairChanged(const cricket::Candidate& local, const cricket::Candidate& remote)
{
    callOnMainThread([protectedThis = Ref { *this }, localSdp = fromStdString(local.ToString()).isolatedCopy(), remoteSdp = fromStdString(remote.ToString()).isolatedCopy(), localFields = convertIceCandidate(local).isolatedCopy(), remoteFields = convertIceCandidate(remote).isolatedCopy()]() mutable {
        if (!protectedThis->m_client)
            return;

        auto local = RTCIceCandidate::create(localSdp, emptyString(), WTFMove(localFields));
        auto remote = RTCIceCandidate::create(remoteSdp, emptyString(), WTFMove(remoteFields));
        protectedThis->m_client->onSelectedCandidatePairChanged(WTFMove(local), WTFMove(remote));
    });
}

LibWebRTCIceTransportBackend::LibWebRTCIceTransportBackend(rtc::scoped_refptr<webrtc::IceTransportInterface>&& backend)
    : m_backend(WTFMove(backend))
{
    ASSERT(m_backend);
}

LibWebRTCIceTransportBackend::~LibWebRTCIceTransportBackend()
{
}

void LibWebRTCIceTransportBackend::registerClient(Client& client)
{
    ASSERT(!m_observer);
    m_observer = LibWebRTCIceTransportBackendObserver::create(client, m_backend);
    m_observer->start();
}

void LibWebRTCIceTransportBackend::unregisterClient()
{
    ASSERT(m_observer);
    m_observer->stop();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
