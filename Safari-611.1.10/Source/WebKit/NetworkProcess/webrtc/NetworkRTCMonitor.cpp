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
#include <webrtc/rtc_base/third_party/sigslot/sigslot.h>
#include <wtf/Function.h>
#include <wtf/WeakHashSet.h>

namespace WebKit {

#undef RELEASE_LOG_IF_ALLOWED

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_rtcProvider.canLog(), Network, "%p - NetworkRTCMonitor::" fmt, this, ##__VA_ARGS__)

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
    bool shouldStart = m_observers.computesEmpty();
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
        m_manager = makeUniqueWithoutFastMallocCheck<rtc::BasicNetworkManager>();
        m_manager->SignalNetworksChanged.connect(this, &NetworkManagerWrapper::onNetworksChanged);
        m_manager->StartUpdating();
    });
}

void NetworkManagerWrapper::removeListener(NetworkRTCMonitor& monitor)
{
    m_observers.remove(monitor);
    if (!m_observers.computesEmpty())
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

    rtc::BasicNetworkManager::NetworkList networks;
    m_manager->GetNetworks(&networks);
    auto networkList = WTF::map(networks, [](auto& network) { return RTCNetwork { *network }; });

    RTCNetwork::IPAddress ipv4;
    m_manager->GetDefaultLocalAddress(AF_INET, &ipv4.value);
    RTCNetwork::IPAddress ipv6;
    m_manager->GetDefaultLocalAddress(AF_INET6, &ipv6.value);

    callOnMainThread([this, networkList = WTFMove(networkList), ipv4 = WTFMove(ipv4), ipv6 = WTFMove(ipv6)]() mutable {
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
    RELEASE_LOG_IF_ALLOWED("startUpdatingIfNeeded %d", m_isStarted);
    networkManager().addListener(*this);
}

void NetworkRTCMonitor::stopUpdating()
{
    RELEASE_LOG_IF_ALLOWED("stopUpdating");
    networkManager().removeListener(*this);
}

void NetworkRTCMonitor::onNetworksChanged(const Vector<RTCNetwork>& networkList, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6)
{
    RELEASE_LOG_IF_ALLOWED("onNetworksChanged sent");
    m_rtcProvider.connection().send(Messages::WebRTCMonitor::NetworksChanged(networkList, ipv4, ipv6), 0);
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
