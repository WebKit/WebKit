/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/ProcessQualified.h>
#include <wtf/Identified.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

#if PLATFORM(COCOA)
#include <Network/Network.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkTransportBidirectionalStream;
class NetworkTransportReceiveStream;
class NetworkTransportSendStream;

struct SharedPreferencesForWebProcess;
struct WebTransportSessionIdentifierType;
struct WebTransportStreamIdentifierType;

using WebTransportSessionIdentifier = ObjectIdentifier<WebTransportSessionIdentifierType>;
using WebTransportStreamIdentifier = ObjectIdentifier<WebTransportStreamIdentifierType>;

class NetworkTransportSession : public RefCounted<NetworkTransportSession>, public IPC::MessageReceiver, public IPC::MessageSender, public Identified<WebTransportSessionIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(NetworkTransportSession);
public:
    static void initialize(NetworkConnectionToWebProcess&, URL&&, CompletionHandler<void(RefPtr<NetworkTransportSession>&&)>&&);

    ~NetworkTransportSession();

    void sendDatagram(std::span<const uint8_t>, CompletionHandler<void()>&&);
    void createOutgoingUnidirectionalStream(CompletionHandler<void(std::optional<WebTransportStreamIdentifier>)>&&);
    void createBidirectionalStream(CompletionHandler<void(std::optional<WebTransportStreamIdentifier>)>&&);
    void destroyOutgoingUnidirectionalStream(WebTransportStreamIdentifier);
    void destroyBidirectionalStream(WebTransportStreamIdentifier);
    void sendStreamSendBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin, CompletionHandler<void()>&&);
    void streamSendBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin, CompletionHandler<void()>&&);
    void terminate(uint32_t, CString&&);

    void receiveDatagram(std::span<const uint8_t>);
    void streamReceiveBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin);
    void receiveIncomingUnidirectionalStream();
    void receiveBidirectionalStream();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;
private:
    template<typename... Args> static Ref<NetworkTransportSession> create(Args&&... args) { return adoptRef(*new NetworkTransportSession(std::forward<Args>(args)...)); }
#if PLATFORM(COCOA)
    NetworkTransportSession(NetworkConnectionToWebProcess&, nw_connection_group_t, nw_endpoint_t);
#endif

    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    HashMap<WebTransportStreamIdentifier, Ref<NetworkTransportBidirectionalStream>> m_bidirectionalStreams;
    HashMap<WebTransportStreamIdentifier, Ref<NetworkTransportReceiveStream>> m_receiveStreams;
    HashMap<WebTransportStreamIdentifier, UniqueRef<NetworkTransportSendStream>> m_sendStreams;
    WeakPtr<NetworkConnectionToWebProcess> m_connectionToWebProcess;

#if PLATFORM(COCOA)
    const RetainPtr<nw_connection_group_t> m_connectionGroup;
    const RetainPtr<nw_endpoint_t> m_endpoint;
#endif
};

}
