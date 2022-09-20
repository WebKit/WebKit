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
#include "LibWebRTCSctpTransportBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCDtlsTransportBackend.h"
#include "LibWebRTCProvider.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/sctp_transport_interface.h>

ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

static inline RTCSctpTransportState toRTCSctpTransportState(webrtc::SctpTransportState state)
{
    switch (state) {
    case webrtc::SctpTransportState::kNew:
    case webrtc::SctpTransportState::kConnecting:
        return RTCSctpTransportState::Connecting;
    case webrtc::SctpTransportState::kConnected:
        return RTCSctpTransportState::Connected;
    case webrtc::SctpTransportState::kClosed:
        return RTCSctpTransportState::Closed;
    case webrtc::SctpTransportState::kNumValues:
        ASSERT_NOT_REACHED();
        return RTCSctpTransportState::Connecting;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

class LibWebRTCSctpTransportBackendObserver final : public ThreadSafeRefCounted<LibWebRTCSctpTransportBackendObserver>, public webrtc::SctpTransportObserverInterface {
public:
    static Ref<LibWebRTCSctpTransportBackendObserver> create(RTCSctpTransportBackend::Client& client, rtc::scoped_refptr<webrtc::SctpTransportInterface>& backend) { return adoptRef(*new LibWebRTCSctpTransportBackendObserver(client, backend)); }

    void start();
    void stop();

private:
    LibWebRTCSctpTransportBackendObserver(RTCSctpTransportBackend::Client&, rtc::scoped_refptr<webrtc::SctpTransportInterface>&);

    void OnStateChange(webrtc::SctpTransportInformation) final;

    void updateState(webrtc::SctpTransportInformation&&);

    rtc::scoped_refptr<webrtc::SctpTransportInterface> m_backend;
    WeakPtr<RTCSctpTransportBackend::Client> m_client;
};

LibWebRTCSctpTransportBackendObserver::LibWebRTCSctpTransportBackendObserver(RTCSctpTransportBackend::Client& client, rtc::scoped_refptr<webrtc::SctpTransportInterface>& backend)
    : m_backend(backend)
    , m_client(client)
{
    ASSERT(m_backend);
}

void LibWebRTCSctpTransportBackendObserver::updateState(webrtc::SctpTransportInformation&& info)
{
    if (!m_client)
        return;

    std::optional<unsigned short> maxChannels;
    if (auto value = info.MaxChannels())
        maxChannels = *value;
    std::optional<double> maxMessageSize;
    if (info.MaxMessageSize())
        maxMessageSize = *info.MaxMessageSize();
    m_client->onStateChanged(toRTCSctpTransportState(info.state()), maxMessageSize, maxChannels);
}

void LibWebRTCSctpTransportBackendObserver::start()
{
    LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }]() mutable {
        m_backend->RegisterObserver(this);
        callOnMainThread([protectedThis = WTFMove(protectedThis), info = m_backend->Information()]() mutable {
            protectedThis->updateState(WTFMove(info));
        });
    });
}

void LibWebRTCSctpTransportBackendObserver::stop()
{
    m_client = nullptr;
    LibWebRTCProvider::callOnWebRTCNetworkThread([protectedThis = Ref { *this }] {
        protectedThis->m_backend->UnregisterObserver();
    });
}

void LibWebRTCSctpTransportBackendObserver::OnStateChange(webrtc::SctpTransportInformation info)
{
    callOnMainThread([protectedThis = Ref { *this }, info = WTFMove(info)]() mutable {
        protectedThis->updateState(WTFMove(info));
    });
}

LibWebRTCSctpTransportBackend::LibWebRTCSctpTransportBackend(rtc::scoped_refptr<webrtc::SctpTransportInterface>&& backend, rtc::scoped_refptr<webrtc::DtlsTransportInterface>&& dtlsBackend)
    : m_backend(WTFMove(backend))
    , m_dtlsBackend(WTFMove(dtlsBackend))
{
    ASSERT(m_backend);
}

LibWebRTCSctpTransportBackend::~LibWebRTCSctpTransportBackend()
{
    if (m_observer)
        m_observer->stop();
}

UniqueRef<RTCDtlsTransportBackend> LibWebRTCSctpTransportBackend::dtlsTransportBackend()
{
    return makeUniqueRef<LibWebRTCDtlsTransportBackend>(rtc::scoped_refptr<webrtc::DtlsTransportInterface> { m_dtlsBackend.get() });
}

void LibWebRTCSctpTransportBackend::registerClient(Client& client)
{
    ASSERT(!m_observer);
    m_observer = LibWebRTCSctpTransportBackendObserver::create(client, m_backend);
    m_observer->start();
}

void LibWebRTCSctpTransportBackend::unregisterClient()
{
    ASSERT(m_observer);
    m_observer->stop();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
