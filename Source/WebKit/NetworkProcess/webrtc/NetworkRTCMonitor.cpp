/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "NetworkRTCMonitor.h"

#if USE(LIBWEBRTC)

#include "Connection.h"
#include "Logging.h"
#include "NetworkRTCProvider.h"
#include "WebRTCMonitorMessages.h"
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WeakHashSet.h>

ALLOW_COMMA_BEGIN

#include <webrtc/rtc_base/third_party/sigslot/sigslot.h>

ALLOW_COMMA_END

namespace WebKit {

#define RTC_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkRTCMonitor::" fmt, this, ##__VA_ARGS__)

class NetworkManagerWrapper final : public sigslot::has_slots<> {
public:
    void addListener(NetworkRTCMonitor&);
    void removeListener(NetworkRTCMonitor&);

private:
    void onNetworksChanged();

    std::unique_ptr<rtc::BasicNetworkManager> m_manager;
    WeakHashSet<NetworkRTCMonitor> m_observers;

    bool m_didReceiveResults { false };
    Vector<RTCNetwork> m_networkList;
    RTCNetwork::IPAddress m_ipv4;
    RTCNetwork::IPAddress m_ipv6;
};

static NetworkManagerWrapper& networkManager()
{
    static NeverDestroyed<NetworkManagerWrapper> networkManager;
    return networkManager.get();
}

void NetworkManagerWrapper::addListener(NetworkRTCMonitor& monitor)
{
    bool shouldStart = m_observers.isEmptyIgnoringNullReferences();
    m_observers.add(monitor);
    if (!shouldStart) {
        if (m_didReceiveResults)
            monitor.onNetworksChanged(m_networkList, m_ipv4, m_ipv6);
        return;
    }

    monitor.rtcProvider().callOnRTCNetworkThread([this]() {
        if (m_manager)
            return;

        RELEASE_LOG(WebRTC, "NetworkManagerWrapper startUpdating");
        m_manager = makeUniqueWithoutFastMallocCheck<rtc::BasicNetworkManager>(NetworkRTCProvider::rtcNetworkThread().socketserver());
        m_manager->SignalNetworksChanged.connect(this, &NetworkManagerWrapper::onNetworksChanged);
        m_manager->StartUpdating();
    });
}

void NetworkManagerWrapper::removeListener(NetworkRTCMonitor& monitor)
{
    m_observers.remove(monitor);
    if (!m_observers.isEmptyIgnoringNullReferences())
        return;

    monitor.rtcProvider().callOnRTCNetworkThread([this]() {
        if (!m_manager)
            return;

        RELEASE_LOG(WebRTC, "NetworkManagerWrapper stopUpdating");
        m_manager->StopUpdating();
        m_manager = nullptr;
    });
}

void NetworkManagerWrapper::onNetworksChanged()
{
    RELEASE_LOG(WebRTC, "NetworkManagerWrapper::onNetworksChanged");

    rtc::IPAddress ipv4RTC;
    m_manager->GetDefaultLocalAddress(AF_INET, &ipv4RTC);
    RTCNetwork::IPAddress ipv4(ipv4RTC);
    rtc::IPAddress ipv6RTC;
    m_manager->GetDefaultLocalAddress(AF_INET6, &ipv6RTC);
    RTCNetwork::IPAddress ipv6(ipv6RTC);

    auto networks = m_manager->GetNetworks();

    auto networkList = WTF::map(networks, [](auto& network) { return RTCNetwork { *network }; });
    callOnMainRunLoop([this, networkList = WTFMove(networkList), ipv4 = WTFMove(ipv4), ipv6 = WTFMove(ipv6)]() mutable {
        m_didReceiveResults = true;

        m_networkList = WTFMove(networkList);
        m_ipv4 = WTFMove(ipv4);
        m_ipv6 = WTFMove(ipv6);

        m_observers.forEach([this](auto& observer) {
            observer.onNetworksChanged(m_networkList, m_ipv4, m_ipv6);
        });
    });
}

NetworkRTCMonitor::~NetworkRTCMonitor()
{
    ASSERT(!m_manager);
}

void NetworkRTCMonitor::startUpdatingIfNeeded()
{
    RTC_RELEASE_LOG("startUpdatingIfNeeded m_isStarted=%d", m_isStarted);
    networkManager().addListener(*this);
}

void NetworkRTCMonitor::stopUpdating()
{
    RTC_RELEASE_LOG("stopUpdating");
    networkManager().removeListener(*this);
}

void NetworkRTCMonitor::onNetworksChanged(const Vector<RTCNetwork>& networkList, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6)
{
    RTC_RELEASE_LOG("onNetworksChanged sent");
    m_rtcProvider.connection().send(Messages::WebRTCMonitor::NetworksChanged(networkList, ipv4, ipv6), 0);
}

} // namespace WebKit

#undef RTC_RELEASE_LOG

#endif // USE(LIBWEBRTC)
