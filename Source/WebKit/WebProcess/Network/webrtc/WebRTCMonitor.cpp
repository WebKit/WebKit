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
#include "WebRTCMonitor.h"

#if USE(LIBWEBRTC)

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkRTCMonitorMessages.h"
#include "WebProcess.h"
#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/rtc_base/nethelpers.h>
#include <wtf/MainThread.h>

namespace WebKit {

void WebRTCMonitor::sendOnMainThread(Function<void(IPC::Connection&)>&& callback)
{
    callOnMainThread([callback = WTFMove(callback)]() {
        callback(WebProcess::singleton().ensureNetworkProcessConnection().connection());
    });
}

void WebRTCMonitor::StartUpdating()
{
    if (m_clientCount) {
        // Need to signal new client that we already have the network list, let's do it asynchronously
        if (m_receivedNetworkList) {
            WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this] {
                SignalNetworksChanged();
            });
        }
    } else {
        m_receivedNetworkList = false;
        sendOnMainThread([](IPC::Connection& connection) {
            connection.send(Messages::NetworkRTCMonitor::StartUpdating(), 0);
        });
    }
    ++m_clientCount;
}

void WebRTCMonitor::StopUpdating()
{
    ASSERT(m_clientCount);
    if (--m_clientCount)
        return;

    sendOnMainThread([](IPC::Connection& connection) {
        connection.send(Messages::NetworkRTCMonitor::StopUpdating(), 0);
    });
}

void WebRTCMonitor::networksChanged(const Vector<RTCNetwork>& networks, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6)
{
    // No need to protect 'this' as it has the lifetime of LibWebRTC which has the lifetime of the web process.
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, networks, ipv4, ipv6] {
        std::vector<rtc::Network*> networkList(networks.size());
        for (size_t index = 0; index < networks.size(); ++index)
            networkList[index] = new rtc::Network(networks[index].value());

        bool forceSignaling = !m_receivedNetworkList;
        m_receivedNetworkList = true;

        bool hasChanged;
        set_default_local_addresses(ipv4.value, ipv6.value);
        MergeNetworkList(networkList, &hasChanged);
        if (hasChanged || forceSignaling)
            SignalNetworksChanged();

    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
