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

#include <poll.h>
#include <thread>
#include <wtf/Condition.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace Inspector {

using ClientID = unsigned;
using PlatformSocketType = int;
constexpr PlatformSocketType INVALID_SOCKET_VALUE = -1;

class MessageParser;
class RemoteInspectorConnectionClient;

class JS_EXPORT_PRIVATE RemoteInspectorSocket {
public:
    RemoteInspectorSocket(RemoteInspectorConnectionClient*);
    ~RemoteInspectorSocket();

    enum class DomainType {
        Unix,
        Inet,
    };

    void send(ClientID, const uint8_t* data, size_t);

    Optional<ClientID> createClient(PlatformSocketType fd);

protected:
    void recvIfEnabled(ClientID);
    void sendIfEnabled(ClientID);
    void workerThread();
    void wakeupWorkerThread();
    void acceptInetSocketIfEnabled(ClientID);
    bool isListening(ClientID);

    struct Connection {
        std::unique_ptr<MessageParser> parser;
        Vector<uint8_t> sendBuffer;
        struct pollfd poll;
        PlatformSocketType socket { INVALID_SOCKET_VALUE };
    };

    Lock m_connectionsLock;
    HashMap<ClientID, std::unique_ptr<struct Connection>> m_connections;

    PlatformSocketType m_wakeupSendSocket { INVALID_SOCKET_VALUE };
    PlatformSocketType m_wakeupReceiveSocket { INVALID_SOCKET_VALUE };

    RefPtr<Thread> m_workerThread;
    std::atomic<bool> m_shouldAbortWorkerThread { false };

    WeakPtr<RemoteInspectorConnectionClient> m_inspectorClient;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
