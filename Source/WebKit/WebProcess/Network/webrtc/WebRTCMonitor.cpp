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
#include <wtf/MainThread.h>

namespace WebKit {

#define WEBRTC_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - WebRTCMonitor::" fmt, this, ##__VA_ARGS__)

void WebRTCMonitor::startUpdating()
{
    WEBRTC_RELEASE_LOG("StartUpdating - Asking network process to start updating");

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkRTCMonitor::StartUpdatingIfNeeded(), 0);
    ++m_clientCount;
}

void WebRTCMonitor::stopUpdating()
{
    WEBRTC_RELEASE_LOG("StopUpdating");

    ASSERT(m_clientCount);
    if (--m_clientCount)
        return;

    WEBRTC_RELEASE_LOG("StopUpdating - Asking network process to stop updating");
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkRTCMonitor::StopUpdating(), 0);
}

void WebRTCMonitor::networksChanged(Vector<RTCNetwork>&& networkList, RTCNetwork::IPAddress&& ipv4, RTCNetwork::IPAddress&& ipv6)
{
    ASSERT(isMainRunLoop());
    WEBRTC_RELEASE_LOG("NetworksChanged");

    m_didReceiveNetworkList = true;
    m_networkList = WTFMove(networkList);
    m_ipv4 = WTFMove(ipv4);
    m_ipv6 = WTFMove(ipv6);

    for (Ref observer : m_observers)
        observer->networksChanged(m_networkList, m_ipv4, m_ipv6);
}

void WebRTCMonitor::networkProcessCrashed()
{
    for (Ref observer : m_observers)
        observer->networkProcessCrashed();
}

} // namespace WebKit

#undef WEBRTC_RELEASE_LOG

#endif // USE(LIBWEBRTC)
