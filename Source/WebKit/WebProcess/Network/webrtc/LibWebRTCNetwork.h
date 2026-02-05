/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "LibWebRTCProvider.h"
#include "LibWebRTCSocketFactory.h"
#include "WebRTCMonitor.h"
#include "WebRTCNetworkBase.h"
#include "WebRTCResolver.h"
#include <WebCore/LibWebRTCSocketIdentifier.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebKit {
class WebProcess;

class LibWebRTCNetwork final : public WebRTCNetworkBase, private FunctionDispatcher {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCNetwork);
public:
    explicit LibWebRTCNetwork(WebProcess&);
    ~LibWebRTCNetwork();

    IPC::Connection* connection() { return m_connection.get(); }
    void setConnection(RefPtr<IPC::Connection>&&);

    void networkProcessCrashed() final;
    void setAsActive() final;

    WebRTCMonitor& monitor() { return m_webNetworkMonitor; }
    LibWebRTCSocketFactory& socketFactory() { return m_socketFactory; }
    CheckedRef<LibWebRTCSocketFactory> checkedSocketFactory() { return m_socketFactory; }

    void disableNonLocalhostConnections() { socketFactory().disableNonLocalhostConnections(); }

    Ref<WebRTCResolver> resolver(LibWebRTCResolverIdentifier identifier) { return WebRTCResolver::create(checkedSocketFactory(), identifier); }

private:
    void setSocketFactoryConnection();

    void signalReadPacket(WebCore::LibWebRTCSocketIdentifier, std::span<const uint8_t>, const RTCNetwork::IPAddress&, uint16_t port, int64_t, WebRTCNetwork::EcnMarking);
    void signalSentPacket(WebCore::LibWebRTCSocketIdentifier, int64_t, int64_t);
    void signalAddressReady(WebCore::LibWebRTCSocketIdentifier, const RTCNetwork::SocketAddress&);
    void signalConnect(WebCore::LibWebRTCSocketIdentifier);
    void signalClose(WebCore::LibWebRTCSocketIdentifier, int);
    void signalUsedInterface(WebCore::LibWebRTCSocketIdentifier, String&&);

    // FunctionDispatcher
    void dispatch(Function<void()>&&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    LibWebRTCSocketFactory m_socketFactory;
    WebRTCMonitor m_webNetworkMonitor;

    RefPtr<IPC::Connection> m_connection;
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
