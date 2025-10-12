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

#pragma once

#if USE(LIBWEBRTC)

#include "RTCNetwork.h"
#include <WebCore/Timer.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/NetworkSPI.h>
#endif

namespace WebKit {
class NetworkRTCMonitor;

class NetworkRTCSharedMonitor {
public:
    static NetworkRTCSharedMonitor& singleton();
    ~NetworkRTCSharedMonitor();

    void addListener(NetworkRTCMonitor&);
    void removeListener(NetworkRTCMonitor&);

    const RTCNetwork::IPAddress& ipv4() const { return m_ipv4; }
    const RTCNetwork::IPAddress& ipv6()  const { return m_ipv6; }

    webrtc::AdapterType adapterTypeFromInterfaceName(const char*) const;

#if PLATFORM(COCOA)
    void updateNetworksFromPath(nw_path_t);
#endif

private:
    friend NeverDestroyed<NetworkRTCSharedMonitor>;

    NetworkRTCSharedMonitor();

#if PLATFORM(COCOA)
    void setupNWPathMonitor();
#endif

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
    RetainPtr<nw_path_monitor_t> m_nwMonitor;
    HashMap<String, webrtc::AdapterType> m_adapterTypes;
#endif
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
