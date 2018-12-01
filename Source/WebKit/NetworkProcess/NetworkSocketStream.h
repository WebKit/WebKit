/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include <WebCore/SocketStreamHandleClient.h>
#include <WebCore/SocketStreamHandleImpl.h>
#include <pal/SessionID.h>

namespace IPC {
class Connection;
class Decoder;
class DataReference;
}

namespace WebCore {
class SocketStreamHandleImpl;
}

namespace WebKit {

class NetworkSocketStream : public RefCounted<NetworkSocketStream>, public IPC::MessageSender, public IPC::MessageReceiver, public WebCore::SocketStreamHandleClient {
public:
    static Ref<NetworkSocketStream> create(URL&&, PAL::SessionID, const String& credentialPartition, uint64_t, IPC::Connection&, WebCore::SourceApplicationAuditToken&&);
    ~NetworkSocketStream();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void sendData(const IPC::DataReference&, uint64_t);
    void sendHandshake(const IPC::DataReference&, const std::optional<WebCore::CookieRequestHeaderFieldProxy>&, uint64_t);
    void close();
    
    // SocketStreamHandleClient
    void didOpenSocketStream(WebCore::SocketStreamHandle&) final;
    void didCloseSocketStream(WebCore::SocketStreamHandle&) final;
    void didReceiveSocketStreamData(WebCore::SocketStreamHandle&, const char*, size_t) final;
    void didFailToReceiveSocketStreamData(WebCore::SocketStreamHandle&) final;
    void didUpdateBufferedAmount(WebCore::SocketStreamHandle&, size_t) final;
    void didFailSocketStream(WebCore::SocketStreamHandle&, const WebCore::SocketStreamError&) final;

private:
    IPC::Connection* messageSenderConnection() final;
    uint64_t messageSenderDestinationID() final;

    NetworkSocketStream(URL&&, PAL::SessionID, const String& credentialPartition, uint64_t, IPC::Connection&, WebCore::SourceApplicationAuditToken&&);

    uint64_t m_identifier;
    IPC::Connection& m_connection;
    Ref<WebCore::SocketStreamHandleImpl> m_impl;
};

} // namespace WebKit
