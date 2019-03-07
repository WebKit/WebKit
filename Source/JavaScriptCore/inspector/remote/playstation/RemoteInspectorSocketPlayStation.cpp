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

#include "config.h"
#include "RemoteInspectorSocket.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorConnectionClient.h"
#include "RemoteInspectorMessageParser.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

constexpr size_t SocketBufferSize = 65536;

static void initializeSocket(PlatformSocketType sockfd)
{
    fcntl(sockfd, F_SETFD, FD_CLOEXEC);
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &SocketBufferSize, sizeof(SocketBufferSize));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &SocketBufferSize, sizeof(SocketBufferSize));
}

RemoteInspectorSocket::RemoteInspectorSocket(RemoteInspectorConnectionClient* inspectorClient)
    : m_inspectorClient(makeWeakPtr(inspectorClient))
{
    PlatformSocketType sockets[2];
    if (!socketpair(AF_UNIX, SOCK_STREAM, 0, sockets)) {
        m_wakeupSendSocket = sockets[0];
        m_wakeupReceiveSocket = sockets[1];
    }

    m_workerThread = Thread::create("Remote Inspector Worker", [this] {
        workerThread();
    });
}

RemoteInspectorSocket::~RemoteInspectorSocket()
{
    ASSERT(m_workerThread.get() != &Thread::current());

    m_shouldAbortWorkerThread = true;
    wakeupWorkerThread();
    m_workerThread->waitForCompletion();

    ::close(m_wakeupSendSocket);
    ::close(m_wakeupReceiveSocket);
    for (const auto& connection : m_connections.values()) {
        if (connection->socket != INVALID_SOCKET_VALUE)
            ::close(connection->socket);
    }
}

void RemoteInspectorSocket::wakeupWorkerThread()
{
    if (m_wakeupSendSocket != INVALID_SOCKET_VALUE)
        ::write(m_wakeupSendSocket, "1", 1);
}

bool RemoteInspectorSocket::isListening(ClientID id)
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_connections.get(id)) {
        int out;
        socklen_t outSize = sizeof(out);
        if (getsockopt(connection->socket, SOL_SOCKET, SO_ACCEPTCONN, &out, &outSize) != -1)
            return out != 0;

        LOG_ERROR("getsockopt errno = %d", errno);
    }
    return false;
}

void RemoteInspectorSocket::workerThread()
{
    struct pollfd wakeup;
    wakeup.fd = m_wakeupReceiveSocket;
    wakeup.events = POLLIN;

    while (!m_shouldAbortWorkerThread) {
        Vector<struct pollfd> pollfds;
        Vector<ClientID> ids;
        {
            LockHolder lock(m_connectionsLock);
            for (const auto& connection : m_connections) {
                pollfds.append(connection.value->poll);
                ids.append(connection.key);
            }
        }
        pollfds.append(wakeup);

        int ret = poll(pollfds.data(), pollfds.size(), -1);
        if (ret > 0) {
            if (pollfds.last().revents & POLLIN) {
                char wakeMessage;
                ::read(m_wakeupReceiveSocket, &wakeMessage, sizeof(wakeMessage));
                continue;
            }

            for (size_t i = 0; i < ids.size(); i++) {
                auto id = ids[i];

                if ((pollfds[i].revents & POLLIN) && isListening(id))
                    acceptInetSocketIfEnabled(id);
                else if (pollfds[i].revents & POLLIN)
                    recvIfEnabled(id);
                else if (pollfds[i].revents & POLLOUT)
                    sendIfEnabled(id);
            }
        }
    }
}

Optional<ClientID> RemoteInspectorSocket::createClient(PlatformSocketType fd)
{
    if (fd == INVALID_SOCKET_VALUE)
        return WTF::nullopt;

    LockHolder lock(m_connectionsLock);

    ClientID id;
    do {
        id = cryptographicallyRandomNumber();
    } while (!id || m_connections.contains(id));

    initializeSocket(fd);

    auto connection = std::make_unique<struct Connection>();
    connection->poll.fd = fd;
    connection->poll.events = POLLIN;
    connection->parser = std::make_unique<MessageParser>(id, SocketBufferSize);
    connection->socket = fd;
    connection->parser->setDidParseMessageListener([this](ClientID id, Vector<uint8_t>&& data) {
        if (m_inspectorClient)
            m_inspectorClient->didReceiveWebInspectorEvent(id, WTFMove(data));
    });

    m_connections.add(id, WTFMove(connection));
    wakeupWorkerThread();

    return id;
}

void RemoteInspectorSocket::recvIfEnabled(ClientID clientID)
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_connections.get(clientID)) {
        Vector<uint8_t> recvBuffer(SocketBufferSize);
        ssize_t readSize = ::read(connection->socket, recvBuffer.data(), recvBuffer.size());

        if (readSize > 0) {
            connection->parser->pushReceivedData(recvBuffer.data(), readSize);
            return;
        }
        if (readSize < 0)
            LOG_ERROR("read error (errno = %d)", errno);

        ::close(connection->socket);
        m_connections.remove(clientID);

        lock.unlockEarly();
        if (m_inspectorClient)
            m_inspectorClient->didClose(clientID);
    }
}

void RemoteInspectorSocket::sendIfEnabled(ClientID clientID)
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_connections.get(clientID)) {
        connection->poll.events &= ~POLLOUT;
        if (connection->sendBuffer.isEmpty())
            return;

        ssize_t writeSize = ::write(connection->socket, connection->sendBuffer.data(), std::min(connection->sendBuffer.size(), SocketBufferSize));
        if (writeSize > 0 && static_cast<size_t>(writeSize) == connection->sendBuffer.size()) {
            connection->sendBuffer.clear();
            return;
        }

        if (writeSize > 0)
            connection->sendBuffer.remove(0, writeSize);
        else
            LOG_ERROR("write error (errno = %d)", errno);

        connection->poll.events |= POLLOUT;
    }
}

void RemoteInspectorSocket::send(ClientID clientID, const uint8_t* data, size_t size)
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_connections.get(clientID)) {
        auto message = MessageParser::createMessage(data, size);
        if (message.isEmpty())
            return;

        ssize_t writeSize = 0;
        if (connection->sendBuffer.isEmpty()) {
            // Try to call send() directly if buffer is empty.
            writeSize = ::write(connection->socket, message.data(), std::min(message.size(), SocketBufferSize));
            if (writeSize > 0 && static_cast<size_t>(writeSize) == message.size()) {
                // All data is sent.
                return;
            }
            if (writeSize < 0) {
                LOG_ERROR("write error (errno = %d)", errno);
                writeSize = 0;
            }
        }

        // Copy remaining data to send later.
        connection->sendBuffer.appendRange(message.begin() + writeSize, message.end());
        connection->poll.events |= POLLOUT;

        wakeupWorkerThread();
    }
}

void RemoteInspectorSocket::acceptInetSocketIfEnabled(ClientID id)
{
    if (!isListening(id))
        return;

    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_connections.get(id)) {
        struct sockaddr_in address = { };

        socklen_t len = sizeof(struct sockaddr_in);
        int fd = accept(connection->socket, (struct sockaddr*)&address, &len);
        if (fd < 0) {
            LOG_ERROR("accept(inet) error (errno = %d)", errno);
            return;
        }

        // Need to unlock before calling createClient as it also attempts to lock.
        lock.unlockEarly();
        auto newID = createClient(fd);
        if (newID && m_inspectorClient)
            m_inspectorClient->didAccept(newID.value(), DomainType::Inet);
        else
            ::close(fd);
    }
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
