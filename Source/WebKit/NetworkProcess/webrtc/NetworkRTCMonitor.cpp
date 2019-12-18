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

namespace WebKit {

#undef RELEASE_LOG_IF_ALLOWED

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_rtcProvider.canLog(), Network, "%p - NetworkRTCMonitor::" fmt, this, ##__VA_ARGS__)

NetworkRTCMonitor::~NetworkRTCMonitor()
{
    ASSERT(!m_manager);
}

void NetworkRTCMonitor::startUpdatingIfNeeded()
{
    RELEASE_LOG_IF_ALLOWED("startUpdatingIfNeeded %d", m_isStarted);
    if (m_isStarted)
        return;

    m_isStarted = true;
    m_rtcProvider.callOnRTCNetworkThread([this]() {
        RELEASE_LOG_IF_ALLOWED("startUpdating from network thread");

        m_manager = makeUniqueWithoutFastMallocCheck<rtc::BasicNetworkManager>();
        m_manager->SignalNetworksChanged.connect(this, &NetworkRTCMonitor::onNetworksChanged);
        m_manager->StartUpdating();
    });
}

void NetworkRTCMonitor::stopUpdating()
{
    RELEASE_LOG_IF_ALLOWED("stopUpdating");
    m_isStarted = false;
    m_rtcProvider.callOnRTCNetworkThread([this]() {
        RELEASE_LOG_IF_ALLOWED("stopUpdating from network thread %p", m_manager.get());

        if (!m_manager)
            return;
        m_manager->StopUpdating();
        m_manager = nullptr;
    });
}

void NetworkRTCMonitor::onNetworksChanged()
{
    RELEASE_LOG_IF_ALLOWED("onNetworksChanged");

    rtc::BasicNetworkManager::NetworkList networks;
    m_manager->GetNetworks(&networks);

    RTCNetwork::IPAddress ipv4;
    m_manager->GetDefaultLocalAddress(AF_INET, &ipv4.value);
    RTCNetwork::IPAddress ipv6;
    m_manager->GetDefaultLocalAddress(AF_INET6, &ipv6.value);

    Vector<RTCNetwork> networkList;
    networkList.reserveInitialCapacity(networks.size());
    for (auto* network : networks) {
        ASSERT(network);
        networkList.uncheckedAppend(RTCNetwork { *network });
    }

    m_rtcProvider.sendFromMainThread([this, networkList = WTFMove(networkList), ipv4 = WTFMove(ipv4), ipv6 = WTFMove(ipv6)](IPC::Connection& connection) {
        RELEASE_LOG_IF_ALLOWED("onNetworksChanged sent");
        connection.send(Messages::WebRTCMonitor::NetworksChanged(networkList, ipv4, ipv6), 0);
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
