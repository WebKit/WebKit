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

#pragma once

#if USE(LIBWEBRTC)

#include "RTCNetwork.h"
#include <WebCore/LibWebRTCProvider.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/Forward.h>
#include <wtf/WeakHashSet.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class LibWebRTCNetwork;
struct NetworksChangedData;

class WebRTCMonitorObserver : public AbstractRefCountedAndCanMakeWeakPtr<WebRTCMonitorObserver> {
public:
    virtual ~WebRTCMonitorObserver() = default;

    virtual void networksChanged(const Vector<RTCNetwork>&, const RTCNetwork::IPAddress&, const RTCNetwork::IPAddress&) = 0;
    virtual void networkProcessCrashed() = 0;
};

class WebRTCMonitor {
public:
    explicit WebRTCMonitor(LibWebRTCNetwork&);

    void ref() const;
    void deref() const;

    void addObserver(WebRTCMonitorObserver& observer) { m_observers.add(observer); }
    void removeObserver(WebRTCMonitorObserver& observer) { m_observers.remove(observer); }
    void startUpdating();
    void stopUpdating();

    void networkProcessCrashed();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    bool didReceiveNetworkList() const { return m_didReceiveNetworkList; }
    const Vector<RTCNetwork>& networkList() const { return m_networkList; }
    const RTCNetwork::IPAddress& ipv4() const { return m_ipv4; }
    const RTCNetwork::IPAddress& ipv6() const { return m_ipv6; }

private:
    void networksChanged(Vector<RTCNetwork>&&, RTCNetwork::IPAddress&&, RTCNetwork::IPAddress&&);

    WeakRef<LibWebRTCNetwork> m_libWebRTCNetwork;
    unsigned m_clientCount { 0 };
    WeakHashSet<WebRTCMonitorObserver> m_observers;
    bool m_didReceiveNetworkList { false };
    Vector<RTCNetwork> m_networkList;
    RTCNetwork::IPAddress m_ipv4;
    RTCNetwork::IPAddress m_ipv6;
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
