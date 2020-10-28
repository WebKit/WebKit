/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorSocket.h"
#include <wtf/Condition.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

class JS_EXPORT_PRIVATE RemoteInspectorSocketEndpoint {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    public:
        virtual ~Client() { }

        // These callbacks are not guaranteed to be called from the main thread.
        virtual void didReceive(RemoteInspectorSocketEndpoint&, ConnectionID, Vector<uint8_t>&&) = 0;
        virtual void didClose(RemoteInspectorSocketEndpoint&, ConnectionID) = 0;
    };

    class Listener {
    public:
        enum class Status : uint8_t {
            Listening,
            Invalid,
            Closed,
        };
        virtual ~Listener() { }

        // These callbacks are not guaranteed to be called from the main thread.
        virtual Optional<ConnectionID> doAccept(RemoteInspectorSocketEndpoint&, PlatformSocketType) = 0;
        virtual void didChangeStatus(RemoteInspectorSocketEndpoint&, ConnectionID, Status) = 0;
    };

    static RemoteInspectorSocketEndpoint& singleton();

    RemoteInspectorSocketEndpoint();
    ~RemoteInspectorSocketEndpoint();

    Optional<ConnectionID> connectInet(const char* serverAddr, uint16_t serverPort, Client&);
    Optional<ConnectionID> listenInet(const char* address, uint16_t port, Listener&);
    void invalidateClient(Client&);
    void invalidateListener(Listener&);

    void send(ConnectionID, const uint8_t* data, size_t);
    inline void send(ConnectionID id, const Vector<uint8_t>& data) { send(id, data.data(), data.size()); }
    inline void send(ConnectionID id, const char* data, size_t length) { send(id, reinterpret_cast<const uint8_t*>(data), length); }

    Optional<ConnectionID> createClient(PlatformSocketType, Client&);

    Optional<uint16_t> getPort(ConnectionID) const;

    void disconnect(ConnectionID);

protected:
    struct BaseConnection {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        BaseConnection(ConnectionID id)
            : id { id }
            , socket { INVALID_SOCKET_VALUE }
        {
        }

        bool setSocket(PlatformSocketType newSocket)
        {
            ASSERT(Socket::isValid(newSocket));

            if (!Socket::setup(newSocket))
                return false;

            if (Socket::isValid(socket))
                Socket::close(socket);

            socket = newSocket;
            poll = Socket::preparePolling(socket);
            return true;
        }

        ConnectionID id;
        PlatformSocketType socket;
        PollingDescriptor poll;
    };

    struct ClientConnection : public BaseConnection {
        ClientConnection(ConnectionID id, PlatformSocketType socket, Client& client)
            : BaseConnection(id)
            , client { client }
        {
            setSocket(socket);
        }

        Client& client;
        Vector<uint8_t> sendBuffer;
    };

    struct ListenerConnection : public BaseConnection {
        static constexpr Seconds initialRetryInterval { 200_ms };
        static constexpr Seconds maxRetryInterval { 5_s };

        ListenerConnection(ConnectionID id, Listener& listener, const char* address, uint16_t port)
            : BaseConnection(id)
            , address { address }
            , port { port }
            , listener { listener }
        {
            listen();
        }

        bool listen()
        {
            ASSERT(!isListening());

            if (nextRetryTime && *nextRetryTime > MonotonicTime::now())
                return false;

            if (auto newSocket = Socket::listen(address.utf8().data(), port)) {
                if (setSocket(*newSocket)) {
                    retryInterval = initialRetryInterval;
                    return true;
                }
                Socket::close(*newSocket);
            }

            nextRetryTime = MonotonicTime::now() + retryInterval;
            retryInterval = std::min<Seconds>(retryInterval * 2, maxRetryInterval);

            return false;
        }

        bool isListening()
        {
            return Socket::isListening(socket);
        }

        String address;
        uint16_t port;
        Listener& listener;
        Optional<MonotonicTime> nextRetryTime;
        Seconds retryInterval { initialRetryInterval };
    };

    ConnectionID generateConnectionID();

    void recvIfEnabled(ConnectionID);
    void sendIfEnabled(ConnectionID);
    void workerThread();
    void wakeupWorkerThread();
    void acceptInetSocketIfEnabled(ConnectionID);
    bool isListening(ConnectionID);
    int pollingTimeout();

    mutable Lock m_connectionsLock;
    HashMap<ConnectionID, std::unique_ptr<ClientConnection>> m_clients;
    HashMap<ConnectionID, std::unique_ptr<ListenerConnection>> m_listeners;

    PlatformSocketType m_wakeupSendSocket { INVALID_SOCKET_VALUE };
    PlatformSocketType m_wakeupReceiveSocket { INVALID_SOCKET_VALUE };

    RefPtr<Thread> m_workerThread;
    std::atomic<bool> m_shouldAbortWorkerThread { false };
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
