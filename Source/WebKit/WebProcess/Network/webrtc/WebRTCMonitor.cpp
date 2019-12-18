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

#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkRTCMonitorMessages.h"
#include "WebProcess.h"
#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/rtc_base/net_helpers.h>
#include <wtf/MainThread.h>

namespace WebKit {

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(WebProcess::singleton().sessionID().isAlwaysOnLoggingAllowed(), Network, "%p - WebRTCMonitor::" fmt, this, ##__VA_ARGS__)

void WebRTCMonitor::sendOnMainThread(Function<void(IPC::Connection&)>&& callback)
{
    callOnMainThread([callback = WTFMove(callback)]() {
        callback(WebProcess::singleton().ensureNetworkProcessConnection().connection());
    });
}

void WebRTCMonitor::StartUpdating()
{
    RELEASE_LOG_IF_ALLOWED("StartUpdating");
    if (m_receivedNetworkList) {
        WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this] {
            SignalNetworksChanged();
        });
    }

    sendOnMainThread([this](auto& connection) {
        RELEASE_LOG_IF_ALLOWED("StartUpdating - Asking network process to start updating");
        connection.send(Messages::NetworkRTCMonitor::StartUpdatingIfNeeded(), 0);
    });
    ++m_clientCount;
}

void WebRTCMonitor::StopUpdating()
{
    RELEASE_LOG_IF_ALLOWED("StopUpdating");
    ASSERT(m_clientCount);
    if (--m_clientCount)
        return;

    sendOnMainThread([this](auto& connection) {
        RELEASE_LOG_IF_ALLOWED("StopUpdating - Asking network process to stop updating");
        connection.send(Messages::NetworkRTCMonitor::StopUpdating(), 0);
    });
}

void WebRTCMonitor::networksChanged(const Vector<RTCNetwork>& networks, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6)
{
    RELEASE_LOG_IF_ALLOWED("networksChanged");
    // No need to protect 'this' as it has the lifetime of LibWebRTC which has the lifetime of the web process.
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, networks, ipv4, ipv6] {
        RELEASE_LOG_IF_ALLOWED("networksChanged - Signaling");
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

void WebRTCMonitor::networkProcessCrashed()
{
    m_receivedNetworkList = false;
    if (!WebCore::LibWebRTCProvider::hasWebRTCThreads())
        return;

    // In case we have clients waiting for networksChanged, we call SignalNetworksChanged to make sure they do not wait for nothing.    
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this] {
        SignalNetworksChanged();
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
