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
#include "NetworkRTCSharedMonitor.h"
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
#include <wtf/darwin/DispatchExtras.h>
#endif

namespace WebKit {

#define RTC_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkRTCMonitor::" fmt, this, ##__VA_ARGS__)

static std::optional<std::pair<RTCNetwork::InterfaceAddress, RTCNetwork::IPAddress>> addressFromInterface(const struct ifaddrs& interface)
{
    RTCNetwork::IPAddress address { *interface.ifa_addr };
    RTCNetwork::IPAddress mask { *interface.ifa_netmask };
    return std::make_pair(RTCNetwork::InterfaceAddress { address, webrtc::IPV6_ADDRESS_FLAG_NONE }, mask);
}

HashMap<String, RTCNetwork> NetworkRTCMonitor::gatherNetworkMap()
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
            auto interfaceType = NetworkRTCSharedMonitor::singleton().adapterTypeFromInterfaceName(iterator->ifa_name);
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

std::optional<RTCNetwork::IPAddress> NetworkRTCMonitor::getDefaultIPAddress(bool useIPv4)
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

bool NetworkRTCMonitor::isEqual(const RTCNetwork::InterfaceAddress& a, const RTCNetwork::InterfaceAddress& b)
{
    return a.rtcAddress() == b.rtcAddress();
}

bool NetworkRTCMonitor::isEqual(const RTCNetwork::IPAddress& a, const RTCNetwork::IPAddress& b)
{
    return a.rtcAddress() == b.rtcAddress();
}

bool NetworkRTCMonitor::isEqual(const Vector<RTCNetwork::InterfaceAddress>& a, const Vector<RTCNetwork::InterfaceAddress>& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        if (!isEqual(a[i], b[i]))
            return false;
    }
    return true;
}

bool NetworkRTCMonitor::hasNetworkChanged(const RTCNetwork& a, const RTCNetwork& b)
{
    return !isEqual(a.prefix, b.prefix) || a.prefixLength != b.prefixLength || a.type != b.type || a.scopeID != b.scopeID || !isEqual(a.ips, b.ips);
}

bool NetworkRTCMonitor::sortNetworks(const RTCNetwork& a, const RTCNetwork& b)
{
    if (a.type != b.type)
        return a.type < b.type;

    int precedenceA = webrtc::IPAddressPrecedence(a.ips[0].rtcAddress());
    int precedenceB = webrtc::IPAddressPrecedence(b.ips[0].rtcAddress());

    if (precedenceA != precedenceB)
        return precedenceA < precedenceB;

    return codePointCompare(StringView { a.description }, StringView { b.description }) < 0;
}

NetworkRTCMonitor::NetworkRTCMonitor(NetworkRTCProvider& rtcProvider)
    : m_rtcProvider(rtcProvider)
{
}

NetworkRTCMonitor::~NetworkRTCMonitor()
{
    NetworkRTCSharedMonitor::singleton().removeListener(*this);
}

NetworkRTCProvider& NetworkRTCMonitor::rtcProvider()
{
    return m_rtcProvider.get();
}

const RTCNetwork::IPAddress& NetworkRTCMonitor::ipv4() const
{
    return NetworkRTCSharedMonitor::singleton().ipv4();
}

const RTCNetwork::IPAddress& NetworkRTCMonitor::ipv6()  const
{
    return NetworkRTCSharedMonitor::singleton().ipv6();
}

void NetworkRTCMonitor::startUpdatingIfNeeded()
{
#if ASSERT_ENABLED
    m_isStarted = true;
#endif
    NetworkRTCSharedMonitor::singleton().addListener(*this);
}

void NetworkRTCMonitor::stopUpdating()
{
#if ASSERT_ENABLED
    m_isStarted = false;
#endif
    NetworkRTCSharedMonitor::singleton().removeListener(*this);
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
