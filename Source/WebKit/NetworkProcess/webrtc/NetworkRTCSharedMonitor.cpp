/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)
#include "NetworkRTCSharedMonitor.h"

#include "Logging.h"
#include "NetworkRTCMonitor.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

NetworkRTCSharedMonitor& NetworkRTCSharedMonitor::singleton()
{
    static NeverDestroyed<NetworkRTCSharedMonitor> instance;
    return instance.get();
}

NetworkRTCSharedMonitor::NetworkRTCSharedMonitor()
    : m_queue(ConcurrentWorkQueue::create("NetworkRTCSharedMonitor queue"_s))
    , m_updateNetworksTimer([this] { updateNetworks(); })
{
}

NetworkRTCSharedMonitor::~NetworkRTCSharedMonitor() = default;

void NetworkRTCSharedMonitor::addListener(NetworkRTCMonitor& monitor)
{
    if (m_didReceiveResults)
        monitor.onNetworksChanged(m_networkList, m_ipv4, m_ipv6);

    bool shouldStart = m_observers.isEmptyIgnoringNullReferences();
    m_observers.add(monitor);

    RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::addListener shouldStart=%d didReceiveResults=%d listener=%p", shouldStart, m_didReceiveResults, &monitor);

    if (!shouldStart)
        return;

#if PLATFORM(COCOA)
    if (monitor.rtcProvider().webRTCInterfaceMonitoringViaNWEnabled()) {
        setupNWPathMonitor();
        return;
    }
#endif

    updateNetworks();
    m_updateNetworksTimer.startRepeating(2_s);
}

void NetworkRTCSharedMonitor::removeListener(NetworkRTCMonitor& monitor)
{
    m_observers.remove(monitor);

    bool shouldStop = m_observers.isEmptyIgnoringNullReferences();

    RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::removeListener shouldStop=%d listener=%p", shouldStop, &monitor);

    if (!shouldStop)
        return;

#if PLATFORM(COCOA)
    if (auto nwMonitor = std::exchange(m_nwMonitor, { }))
        nw_path_monitor_cancel(nwMonitor.get());
#endif

    m_updateNetworksTimer.stop();
}

webrtc::AdapterType NetworkRTCSharedMonitor::adapterTypeFromInterfaceName(const char* interfaceName) const
{
#if PLATFORM(COCOA)
    auto iterator = m_adapterTypes.find(String::fromUTF8(interfaceName));
    if (iterator != m_adapterTypes.end())
        return iterator->value;
#endif
    return webrtc::GetAdapterTypeFromName(interfaceName);
}

void NetworkRTCSharedMonitor::updateNetworks()
{
    auto aggregator = IPAddressCallbackAggregator::create([] (auto&& ipv4, auto&& ipv6, auto&& networkList) mutable {
        NetworkRTCSharedMonitor::singleton().onGatheredNetworks(WTFMove(ipv4), WTFMove(ipv6), WTFMove(networkList));
    });
    Ref protectedQueue = m_queue;
    protectedQueue->dispatch([aggregator] {
        bool useIPv4 = true;
        if (auto address = NetworkRTCMonitor::getDefaultIPAddress(useIPv4))
            aggregator->setIPv4(WTFMove(*address));
    });
    protectedQueue->dispatch([aggregator] {
        bool useIPv4 = false;
        if (auto address = NetworkRTCMonitor::getDefaultIPAddress(useIPv4))
            aggregator->setIPv6(WTFMove(*address));
    });
    protectedQueue->dispatch([aggregator] {
        aggregator->setNetworkMap(NetworkRTCMonitor::gatherNetworkMap());
    });
}

void NetworkRTCSharedMonitor::onGatheredNetworks(RTCNetwork::IPAddress&& ipv4, RTCNetwork::IPAddress&& ipv6, HashMap<String, RTCNetwork>&& networkMap)
{
    if (!m_didReceiveResults) {
        m_didReceiveResults = true;
        m_networkMap = WTFMove(networkMap);
        m_ipv4 = WTFMove(ipv4);
        m_ipv6 = WTFMove(ipv6);

        for (auto& network : m_networkMap.values())
            network.id = ++m_networkLastIndex;
    } else {
        bool didChange = networkMap.size() != networkMap.size();
        for (auto& keyValue : networkMap) {
            auto iterator = m_networkMap.find(keyValue.key);
            bool isFound = iterator != m_networkMap.end();
            keyValue.value.id = isFound ? iterator->value.id : ++m_networkLastIndex;
            didChange |= !isFound || NetworkRTCMonitor::hasNetworkChanged(keyValue.value, iterator->value);
        }
        if (!didChange) {
            for (auto& keyValue : m_networkMap) {
                if (!networkMap.contains(keyValue.key)) {
                    didChange = true;
                    break;
                }
            }
        }
        if (!didChange && (ipv4.isUnspecified() || NetworkRTCMonitor::isEqual(ipv4, m_ipv4)) && (ipv6.isUnspecified() || NetworkRTCMonitor::isEqual(ipv6, m_ipv6)))
            return;

        m_networkMap = WTFMove(networkMap);
        if (!ipv4.isUnspecified())
            m_ipv4 = WTFMove(ipv4);
        if (!ipv6.isUnspecified())
            m_ipv6 = WTFMove(ipv6);
    }
    RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::onGatheredNetworks - networks changed");

    auto networkList = copyToVector(m_networkMap.values());
    std::ranges::sort(networkList, NetworkRTCMonitor::sortNetworks);

    int preference = std::max(127zu, networkList.size());
    for (auto& network : networkList)
        network.preference = preference--;

    m_observers.forEach([this](auto& observer) {
        Ref { observer }->onNetworksChanged(m_networkList, m_ipv4, m_ipv6);
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
