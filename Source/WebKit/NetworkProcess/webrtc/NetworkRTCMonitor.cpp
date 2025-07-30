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
#include <WebCore/Timer.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <ranges>
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WorkQueue.h>
#include <wtf/posix/SocketPOSIX.h>

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/NetworkSPI.h>
#include <wtf/BlockPtr.h>
#endif

namespace WebKit {

#define RTC_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkRTCMonitor::" fmt, this, ##__VA_ARGS__)

class CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator, WTF::DestructionThread::MainRunLoop> {
public:
    using Callback = CompletionHandler<void(RTCNetwork::IPAddress&&, RTCNetwork::IPAddress&&, HashMap<String, RTCNetwork>&&)>;
    static Ref<CallbackAggregator> create(Callback&& callback) { return adoptRef(*new CallbackAggregator(WTFMove(callback))); }

    ~CallbackAggregator()
    {
        m_callback(WTFMove(m_ipv4), WTFMove(m_ipv6), WTFMove(m_networkMap));
    }

    void setIPv4(RTCNetwork::IPAddress&& ipv4) { m_ipv4 = WTFMove(ipv4); }
    void setIPv6(RTCNetwork::IPAddress&& ipv6) { m_ipv6 = WTFMove(ipv6); }
    void setNetworkMap(HashMap<String, RTCNetwork>&& networkMap) { m_networkMap = crossThreadCopy(WTFMove(networkMap)); }

private:
    explicit CallbackAggregator(Callback&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    Callback m_callback;

    HashMap<String, RTCNetwork> m_networkMap;
    RTCNetwork::IPAddress m_ipv4;
    RTCNetwork::IPAddress m_ipv6;
};

class NetworkRTCSharedMonitor {
public:
    NetworkRTCSharedMonitor();

    void addListener(NetworkRTCMonitor&);
    void removeListener(NetworkRTCMonitor&);

    const RTCNetwork::IPAddress& ipv4() const { return m_ipv4; }
    const RTCNetwork::IPAddress& ipv6()  const { return m_ipv6; }

    webrtc::AdapterType adapterTypeFromInterfaceName(const char*) const;

#if PLATFORM(COCOA)
    void updateNetworksFromPath(nw_path_t);
#endif

private:
    void start();
    void stop();

    void updateNetworks();
    void updateNetworksOnQueue();

    void onGatheredNetworks(RTCNetwork::IPAddress&&, RTCNetwork::IPAddress&&, HashMap<String, RTCNetwork>&&);

    WeakHashSet<NetworkRTCMonitor> m_observers;

    Ref<ConcurrentWorkQueue> m_queue;
    WebCore::Timer m_updateNetworksTimer;

    bool m_didReceiveResults { false };
    Vector<RTCNetwork> m_networkList;
    RTCNetwork::IPAddress m_ipv4;
    RTCNetwork::IPAddress m_ipv6;
    int m_networkLastIndex { 0 };
    HashMap<String, RTCNetwork> m_networkMap;
#if PLATFORM(COCOA)
    RetainPtr<nw_path_monitor> m_nwMonitor;
    HashMap<String, webrtc::AdapterType> m_adapterTypes;
#endif
};

static NetworkRTCSharedMonitor& networkSharedMonitor()
{
    static NeverDestroyed<NetworkRTCSharedMonitor> networkSharedMonitor;
    return networkSharedMonitor.get();
}

#if PLATFORM(COCOA)
static RetainPtr<nw_path_monitor> createNWPathMonitor()
{
    auto nwMonitor = adoptCF(nw_path_monitor_create());
    nw_path_monitor_set_queue(nwMonitor.get(), dispatch_get_main_queue());
    nw_path_monitor_set_update_handler(nwMonitor.get(), makeBlockPtr([](nw_path_t path) {
        networkSharedMonitor().updateNetworksFromPath(path);
    }).get());
    return nwMonitor;
}
#endif

NetworkRTCSharedMonitor::NetworkRTCSharedMonitor()
    : m_queue(ConcurrentWorkQueue::create("NetworkRTCSharedMonitor queue"_s))
    , m_updateNetworksTimer([] { networkSharedMonitor().updateNetworks(); })
{
}

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
        if (auto nwMonitor = std::exchange(m_nwMonitor, { }))
            nw_path_monitor_cancel(m_nwMonitor.get());

        RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::createNWPathMonitor");

        m_nwMonitor = createNWPathMonitor();
        nw_path_monitor_start(m_nwMonitor.get());
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

static std::optional<std::pair<RTCNetwork::InterfaceAddress, RTCNetwork::IPAddress>> addressFromInterface(const struct ifaddrs& interface)
{
    RTCNetwork::IPAddress address { *interface.ifa_addr };
    RTCNetwork::IPAddress mask { *interface.ifa_netmask };
    return std::make_pair(RTCNetwork::InterfaceAddress { address, webrtc::IPV6_ADDRESS_FLAG_NONE }, mask);
}

#if PLATFORM(COCOA)
static webrtc::AdapterType interfaceAdapterType(nw_interface_t interface)
{
    switch (nw_interface_get_type(interface)) {
    case nw_interface_type_other:
        return webrtc::ADAPTER_TYPE_VPN;
    case nw_interface_type_wifi:
        return webrtc::ADAPTER_TYPE_WIFI;
    case nw_interface_type_cellular:
        return webrtc::ADAPTER_TYPE_CELLULAR;
    case nw_interface_type_wired:
        return webrtc::ADAPTER_TYPE_ETHERNET;
    case nw_interface_type_loopback:
        return webrtc::ADAPTER_TYPE_LOOPBACK;
    }
    return webrtc::ADAPTER_TYPE_UNKNOWN;
}
#endif

static HashMap<String, RTCNetwork> gatherNetworkMap()
{
    struct ifaddrs* interfaces;
    int error = getifaddrs(&interfaces);
    if (error)
        return { };

    std::unique_ptr<struct ifaddrs> toBeFreed(interfaces);

    HashMap<String, RTCNetwork> networkMap;
    for (auto* iterator = interfaces; iterator != nullptr; iterator = iterator->ifa_next) {
        if (!iterator->ifa_addr || !iterator->ifa_netmask)
            continue;

        if (iterator->ifa_addr->sa_family != AF_INET && iterator->ifa_addr->sa_family != AF_INET6)
            continue;

        if (!(iterator->ifa_flags & IFF_RUNNING))
            continue;

        auto address = addressFromInterface(*iterator);
        if (!address)
            continue;

        int scopeID = 0;
        if (auto* address = dynamicCastToIPV6SocketAddress(*iterator->ifa_addr))
            scopeID = address->sin6_scope_id;

        auto prefixLength = webrtc::CountIPMaskBits(address->second.rtcAddress());

        auto name = unsafeSpan(iterator->ifa_name);
        auto prefixString = address->second.rtcAddress().ToString();
        auto networkKey = makeString(name, "-"_s, prefixLength, "-"_s, std::span { prefixString });

        networkMap.ensure(networkKey, [&] {
            auto interfaceType = networkSharedMonitor().adapterTypeFromInterfaceName(iterator->ifa_name);
            return RTCNetwork { name, networkKey.utf8().span(), address->second, prefixLength, interfaceType, 0, 0, true, false, scopeID, { } };
        }).iterator->value.ips.append(address->first);
    }

    return networkMap;
}

static bool connectToRemoteAddress(int socket, bool useIPv4)
{
    // DNS server values taken from libwebrtc.
    const char* publicHost = useIPv4 ? "8.8.8.8" : "2001:4860:4860::8888";
    const int publicPort = 53;

    sockaddr_storage remoteAddressStorage;
    size_t remoteAddressStorageLength = 0;
    bool success = false;
    if (useIPv4) {
        success = initializeIPV4SocketAddress(remoteAddressStorage, [&](auto& remoteAddress) {
            remoteAddressStorageLength = sizeof(remoteAddress);
            remoteAddress.sin_family = AF_INET;
            remoteAddress.sin_port = publicPort;
            return ::inet_pton(AF_INET, publicHost, &remoteAddress.sin_addr);
        });
    } else {
        success = initializeIPV6SocketAddress(remoteAddressStorage, [&](auto& remoteAddress) {
            remoteAddressStorageLength = sizeof(remoteAddress);
            remoteAddress.sin6_family = AF_INET6;
            remoteAddress.sin6_port = publicPort;
            return ::inet_pton(AF_INET6, publicHost, &remoteAddress.sin6_addr);
        });
    }
    if (!success) {
        RELEASE_LOG_ERROR(WebRTC, "getDefaultIPAddress inet_pton failed, useIPv4=%d", useIPv4);
        return false;
    }

    auto& remoteAddress = asSocketAddress(remoteAddressStorage);
    auto connectResult = ::connect(socket, &remoteAddress, remoteAddressStorageLength);
    if (connectResult < 0) {
        RELEASE_LOG_ERROR(WebRTC, "getDefaultIPAddress connect failed, useIPv4=%d", useIPv4);
        return false;
    }

    return true;
}

static std::optional<RTCNetwork::IPAddress> getSocketLocalAddress(int socket, bool useIPv4)
{
    sockaddr_storage localAddressStorage;
    zeroBytes(localAddressStorage);

    auto& localAddress = asSocketAddress(localAddressStorage);
    socklen_t localAddressStorageLength = sizeof(sockaddr_storage);

    if (::getsockname(socket, &localAddress, &localAddressStorageLength) < 0) {
        RELEASE_LOG_ERROR(WebRTC, "getDefaultIPAddress getsockname failed, useIPv4=%d", useIPv4);
        return { };
    }

    auto family = useIPv4 ? AF_INET : AF_INET6;
    if (localAddressStorage.ss_family != family) {
        RELEASE_LOG_ERROR(WebRTC, "getDefaultIPAddress address is not of the expected family, useIPv4=%d", useIPv4);
        return { };
    }

    return RTCNetwork::IPAddress { localAddress };
}

static std::optional<RTCNetwork::IPAddress> getDefaultIPAddress(bool useIPv4)
{
    // FIXME: Use nw API for Cocoa platforms.
    int socket = ::socket(useIPv4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
    if (socket == -1) {
        RELEASE_LOG_ERROR(WebRTC, "getDefaultIPAddress socket creation failed, useIPv4=%d", useIPv4);
        return { };
    }

    auto scope = makeScopeExit([socket] {
        ::closesocket(socket);
    });

    if (!connectToRemoteAddress(socket, useIPv4))
        return { };

    return getSocketLocalAddress(socket, useIPv4);
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
    auto aggregator = CallbackAggregator::create([] (auto&& ipv4, auto&& ipv6, auto&& networkList) mutable {
        networkSharedMonitor().onGatheredNetworks(WTFMove(ipv4), WTFMove(ipv6), WTFMove(networkList));
    });
    Ref protectedQueue = m_queue;
    protectedQueue->dispatch([aggregator] {
        bool useIPv4 = true;
        if (auto address = getDefaultIPAddress(useIPv4))
            aggregator->setIPv4(WTFMove(*address));
    });
    protectedQueue->dispatch([aggregator] {
        bool useIPv4 = false;
        if (auto address = getDefaultIPAddress(useIPv4))
            aggregator->setIPv6(WTFMove(*address));
    });
    protectedQueue->dispatch([aggregator] {
        aggregator->setNetworkMap(gatherNetworkMap());
    });
}

#if PLATFORM(COCOA)
void NetworkRTCSharedMonitor::updateNetworksFromPath(nw_path_t path)
{
    RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::updateNetworksFromPath");

    auto status = nw_path_get_status(path);
    if (status != nw_path_status_satisfied && status != nw_path_status_satisfiable)
        return;

    nw_path_enumerate_interfaces(path, makeBlockPtr([](nw_interface_t interface) -> bool {
        networkSharedMonitor().m_adapterTypes.set(String::fromUTF8(nw_interface_get_name(interface)), interfaceAdapterType(interface));
        return true;
    }).get());
    updateNetworks();
}
#endif

static bool isEqual(const RTCNetwork::InterfaceAddress& a, const RTCNetwork::InterfaceAddress& b)
{
    return a.rtcAddress() == b.rtcAddress();
}

static bool isEqual(const RTCNetwork::IPAddress& a, const RTCNetwork::IPAddress& b)
{
    return a.rtcAddress() == b.rtcAddress();
}

static bool isEqual(const Vector<RTCNetwork::InterfaceAddress>& a, const Vector<RTCNetwork::InterfaceAddress>& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        if (!isEqual(a[i], b[i]))
            return false;
    }
    return true;
}

static bool hasNetworkChanged(const RTCNetwork& a, const RTCNetwork& b)
{
    return !isEqual(a.prefix, b.prefix) || a.prefixLength != b.prefixLength || a.type != b.type || a.scopeID != b.scopeID || !isEqual(a.ips, b.ips);
}

static bool sortNetworks(const RTCNetwork& a, const RTCNetwork& b)
{
    if (a.type != b.type)
        return a.type < b.type;

    int precedenceA = webrtc::IPAddressPrecedence(a.ips[0].rtcAddress());
    int precedenceB = webrtc::IPAddressPrecedence(b.ips[0].rtcAddress());

    if (precedenceA != precedenceB)
        return precedenceA < precedenceB;

    return codePointCompare(StringView { a.description.span() }, StringView { b.description.span() }) < 0;
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
            didChange |= !isFound || hasNetworkChanged(keyValue.value, iterator->value);
        }
        if (!didChange) {
            for (auto& keyValue : m_networkMap) {
                if (!networkMap.contains(keyValue.key)) {
                    didChange = true;
                    break;
                }
            }
        }
        if (!didChange && (ipv4.isUnspecified() || isEqual(ipv4, m_ipv4)) && (ipv6.isUnspecified() || isEqual(ipv6, m_ipv6)))
            return;

        m_networkMap = WTFMove(networkMap);
        if (!ipv4.isUnspecified())
            m_ipv4 = WTFMove(ipv4);
        if (!ipv6.isUnspecified())
            m_ipv6 = WTFMove(ipv6);
    }
    RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::onGatheredNetworks - networks changed");

    auto networkList = copyToVector(m_networkMap.values());
    std::ranges::sort(networkList, sortNetworks);

    int preference = std::max(127zu, networkList.size());
    for (auto& network : networkList)
        network.preference = preference--;

    m_observers.forEach([this](auto& observer) {
        Ref { observer }->onNetworksChanged(m_networkList, m_ipv4, m_ipv6);
    });
}

NetworkRTCMonitor::NetworkRTCMonitor(NetworkRTCProvider& rtcProvider)
    : m_rtcProvider(rtcProvider)
{
}

NetworkRTCMonitor::~NetworkRTCMonitor()
{
    networkSharedMonitor().removeListener(*this);
}

NetworkRTCProvider& NetworkRTCMonitor::rtcProvider()
{
    return m_rtcProvider.get();
}

const RTCNetwork::IPAddress& NetworkRTCMonitor::ipv4() const
{
    return networkSharedMonitor().ipv4();
}

const RTCNetwork::IPAddress& NetworkRTCMonitor::ipv6()  const
{
    return networkSharedMonitor().ipv6();
}

void NetworkRTCMonitor::startUpdatingIfNeeded()
{
#if ASSERT_ENABLED
    m_isStarted = true;
#endif
    networkSharedMonitor().addListener(*this);
}

void NetworkRTCMonitor::stopUpdating()
{
#if ASSERT_ENABLED
    m_isStarted = false;
#endif
    networkSharedMonitor().removeListener(*this);
}

void NetworkRTCMonitor::onNetworksChanged(const Vector<RTCNetwork>& networkList, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6)
{
    RTC_RELEASE_LOG("onNetworksChanged sent");
    m_rtcProvider->protectedConnection()->send(Messages::WebRTCMonitor::NetworksChanged(networkList, ipv4, ipv6), 0);
}


void NetworkRTCMonitor::ref()
{
    m_rtcProvider->ref();
}

void NetworkRTCMonitor::deref()
{
    m_rtcProvider->deref();
}

std::optional<SharedPreferencesForWebProcess> NetworkRTCMonitor::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    Ref protectedProvider = m_rtcProvider.get();
    return protectedProvider->sharedPreferencesForWebProcess(connection);
}

} // namespace WebKit

#undef RTC_RELEASE_LOG

#endif // USE(LIBWEBRTC)
