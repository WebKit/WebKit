/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "CoroutineUtilities.h"
#import <Network/Network.h>
#import <wtf/CompletionHandler.h>

namespace TestWebKitAPI {

class ReceiveHTTPRequestOperation;
class ReceiveBytesOperation;
class SendOperation;
class ConnectionGroup;
#if HAVE(WEB_TRANSPORT)
class ReceiveIncomingConnectionOperation;
#endif

class Connection {
public:
    void send(String&&, CompletionHandler<void()>&& = nullptr) const;
    void send(Vector<uint8_t>&&, CompletionHandler<void()>&& = nullptr) const;
    void send(RetainPtr<dispatch_data_t>&&, CompletionHandler<void(bool)>&& = nullptr) const;
    SendOperation awaitableSend(Vector<uint8_t>&&);
    SendOperation awaitableSend(String&&);
    SendOperation awaitableSend(RetainPtr<dispatch_data_t>&&);
    void sendAndReportError(Vector<uint8_t>&&, CompletionHandler<void(bool)>&&) const;
    void receiveBytes(CompletionHandler<void(Vector<uint8_t>&&)>&&, size_t minimumSize = 1) const;
    ReceiveBytesOperation awaitableReceiveBytes() const;
    void receiveHTTPRequest(CompletionHandler<void(Vector<char>&&)>&&, Vector<char>&& buffer = { }) const;
    ReceiveHTTPRequestOperation awaitableReceiveHTTPRequest() const;
    void webSocketHandshake(CompletionHandler<void()>&& = { });
    void terminate(CompletionHandler<void()>&& = { });
    void cancel();

private:
    friend class HTTPServer;
    friend class WebTransportServer;
    friend class ConnectionGroup;
    Connection(nw_connection_t connection)
        : m_connection(connection) { }

    RetainPtr<nw_connection_t> m_connection;
};

#if HAVE(WEB_TRANSPORT)

class ConnectionGroup {
public:
    ~ConnectionGroup();
    ConnectionGroup(const ConnectionGroup&);

    enum class ConnectionType : uint8_t { Datagram, Bidirectional, Unidirectional };
    Connection createWebTransportConnection(ConnectionType) const;
    ReceiveIncomingConnectionOperation receiveIncomingConnection() const;
    void cancel();

private:
    friend class WebTransportServer;
    friend class ReceiveIncomingConnectionOperation;
    ConnectionGroup(nw_connection_group_t);
    void receiveIncomingConnection(Connection);
    void receiveIncomingConnection(CompletionHandler<void(Connection)>&&);

    struct Data;
    Ref<Data> m_data;
};

class ReceiveIncomingConnectionOperation {
public:
    ReceiveIncomingConnectionOperation(const ConnectionGroup& group)
        : m_group(group) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>);
    Connection await_resume() { return WTFMove(*m_result); }
private:
    ConnectionGroup m_group;
    std::optional<Connection> m_result;
};

#endif

class ReceiveHTTPRequestOperation {
public:
    ReceiveHTTPRequestOperation(const Connection& connection)
        : m_connection(connection) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>);
    Vector<char> await_resume() { return WTFMove(m_result); }
private:
    Connection m_connection;
    Vector<char> m_result;
};

class ReceiveBytesOperation {
public:
    ReceiveBytesOperation(const Connection& connection)
        : m_connection(connection) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>);
    Vector<uint8_t> await_resume() { return WTFMove(m_result); }
private:
    Connection m_connection;
    Vector<uint8_t> m_result;
};

class SendOperation {
public:
    SendOperation(RetainPtr<dispatch_data_t>&& data, const Connection& connection)
        : m_data(WTFMove(data))
        , m_connection(connection) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>);
    void await_resume() { }
private:
    RetainPtr<dispatch_data_t> m_data;
    Connection m_connection;
};

} // namespace TestWebKitAPI
