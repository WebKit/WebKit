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
#include "RemoteInspectorSocketEndpoint.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

RemoteInspectorSocketEndpoint& RemoteInspectorSocketEndpoint::singleton()
{
    static LazyNeverDestroyed<RemoteInspectorSocketEndpoint> shared;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        shared.construct();
    });
    return shared;
}

RemoteInspectorSocketEndpoint::RemoteInspectorSocketEndpoint()
{
    Socket::init();

    if (auto sockets = Socket::createPair()) {
        m_wakeupSendSocket = sockets->at(0);
        m_wakeupReceiveSocket = sockets->at(1);
    }

    m_workerThread = Thread::create("SocketEndpoint", [this] {
        workerThread();
    });
}

RemoteInspectorSocketEndpoint::~RemoteInspectorSocketEndpoint()
{
    ASSERT(m_workerThread.get() != &Thread::current());

    m_shouldAbortWorkerThread = true;
    wakeupWorkerThread();
    m_workerThread->waitForCompletion();

    Socket::close(m_wakeupSendSocket);
    Socket::close(m_wakeupReceiveSocket);
    for (const auto& connection : m_clients.values())
        Socket::close(connection->socket);
    for (const auto& connection : m_listeners.values())
        Socket::close(connection->socket);
}

void RemoteInspectorSocketEndpoint::wakeupWorkerThread()
{
    if (Socket::isValid(m_wakeupSendSocket))
        Socket::write(m_wakeupSendSocket, "1", 1);
}

Optional<ConnectionID> RemoteInspectorSocketEndpoint::connectInet(const char* serverAddress, uint16_t serverPort, Client& client)
{
    if (auto socket = Socket::connect(serverAddress, serverPort))
        return createClient(*socket, client);
    return WTF::nullopt;
}

Optional<ConnectionID> RemoteInspectorSocketEndpoint::listenInet(const char* address, uint16_t port, Listener& listener)
{
    if (auto socket = Socket::listen(address, port))
        return createListener(*socket, listener);

    return WTF::nullopt;
}

bool RemoteInspectorSocketEndpoint::isListening(ConnectionID id)
{
    LockHolder lock(m_connectionsLock);
    if (m_listeners.contains(id))
        return true;
    return false;
}

void RemoteInspectorSocketEndpoint::workerThread()
{
    PollingDescriptor wakeup = Socket::preparePolling(m_wakeupReceiveSocket);

#if USE(GENERIC_EVENT_LOOP) || USE(WINDOWS_EVENT_LOOP)
    RunLoop::setWakeUpCallback([this] {
        wakeupWorkerThread();
    });
#endif

    while (!m_shouldAbortWorkerThread) {
#if USE(GENERIC_EVENT_LOOP) || USE(WINDOWS_EVENT_LOOP)
        RunLoop::iterate();
#endif

        Vector<PollingDescriptor> pollfds;
        Vector<ConnectionID> ids;
        {
            LockHolder lock(m_connectionsLock);
            for (const auto& connection : m_clients) {
                pollfds.append(connection.value->poll);
                ids.append(connection.key);
            }
            for (const auto& connection : m_listeners) {
                pollfds.append(connection.value->poll);
                ids.append(connection.key);
            }
        }
        pollfds.append(wakeup);

        if (!Socket::poll(pollfds, -1))
            continue;

        if (Socket::isReadable(pollfds.last())) {
            char wakeMessage;
            Socket::read(m_wakeupReceiveSocket, &wakeMessage, sizeof(wakeMessage));
            continue;
        }

        for (size_t i = 0; i < ids.size(); i++) {
            auto id = ids[i];

            if (Socket::isReadable(pollfds[i])) {
                if (isListening(id))
                    acceptInetSocketIfEnabled(id);
                else
                    recvIfEnabled(id);
            } else if (Socket::isWritable(pollfds[i]))
                sendIfEnabled(id);
        }
    }

#if USE(GENERIC_EVENT_LOOP) || USE(WINDOWS_EVENT_LOOP)
    RunLoop::setWakeUpCallback(WTF::Function<void()>());
#endif
}

ConnectionID RemoteInspectorSocketEndpoint::generateConnectionID()
{
    ASSERT(m_connectionsLock.isLocked());

    ConnectionID id;
    do {
        id = cryptographicallyRandomNumber();
    } while (!id || m_clients.contains(id) || m_listeners.contains(id));

    return id;
}

Optional<ConnectionID> RemoteInspectorSocketEndpoint::createClient(PlatformSocketType socket, Client& client)
{
    ASSERT(Socket::isValid(socket));

    LockHolder lock(m_connectionsLock);
    auto id = generateConnectionID();
    auto connection = makeUnique<ClientConnection>(id, socket, client);
    m_clients.add(id, WTFMove(connection));
    wakeupWorkerThread();

    return id;
}

void RemoteInspectorSocketEndpoint::disconnect(ConnectionID id)
{
    LockHolder lock(m_connectionsLock);

    if (const auto& connection = m_listeners.get(id)) {
        m_listeners.remove(id);
        Socket::close(connection->socket);
        lock.unlockEarly();
        connection->listener.didClose(*this, id);
    } else if (const auto& connection = m_clients.get(id)) {
        m_clients.remove(id);
        Socket::close(connection->socket);
        lock.unlockEarly();
        connection->client.didClose(*this, id);
    } else
        LOG_ERROR("Error: Cannot disconnect: Invalid id");
}

Optional<ConnectionID> RemoteInspectorSocketEndpoint::createListener(PlatformSocketType socket, Listener& listener)
{
    ASSERT(Socket::isValid(socket));

    if (!Socket::setup(socket))
        return WTF::nullopt;

    LockHolder lock(m_connectionsLock);
    auto id = generateConnectionID();
    auto connection = makeUnique<ListenerConnection>(id, socket, listener);
    m_listeners.add(id, WTFMove(connection));
    wakeupWorkerThread();

    return id;
}

Optional<ConnectionID> RemoteInspectorSocketEndpoint::createListener(PlatformSocketType socket, Listener& listener, Client& client)
{
    ASSERT(Socket::isValid(socket));

    if (!Socket::setup(socket))
        return WTF::nullopt;

    LockHolder lock(m_connectionsLock);
    auto id = generateConnectionID();
    auto connection = makeUnique<ListenerConnection>(id, socket, listener);
    m_listeners.add(id, WTFMove(connection));
    wakeupWorkerThread();

    return id;
}

void RemoteInspectorSocketEndpoint::invalidateClient(Client& client)
{
    LockHolder lock(m_connectionsLock);
    m_clients.removeIf([&client](auto& keyValue) {
        const auto& connection = keyValue.value;

        if (&connection->client != &client)
            return false;

        Socket::close(connection->socket);
        // do not call client.didClose because client is already invalidating phase.
        return true;
    });
}

void RemoteInspectorSocketEndpoint::invalidateListener(Listener& listener)
{
    LockHolder lock(m_connectionsLock);
    m_listeners.removeIf([&listener](auto& keyValue) {
        const auto& connection = keyValue.value;

        if (&connection->listener == &listener) {
            Socket::close(connection->socket);
            return true;
        }

        return false;
    });
}

Optional<uint16_t> RemoteInspectorSocketEndpoint::getPort(ConnectionID id) const
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_listeners.get(id))
        return Socket::getPort(connection->socket);
    if (const auto& connection = m_clients.get(id))
        return Socket::getPort(connection->socket);

    return WTF::nullopt;
}

void RemoteInspectorSocketEndpoint::recvIfEnabled(ConnectionID id)
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_clients.get(id)) {
        Vector<uint8_t> recvBuffer(Socket::BufferSize);
        if (auto readSize = Socket::read(connection->socket, recvBuffer.data(), recvBuffer.size())) {
            if (*readSize > 0) {
                recvBuffer.shrink(*readSize);
                lock.unlockEarly();
                connection->client.didReceive(*this, id, WTFMove(recvBuffer));
                return;
            }
        }

        Socket::close(connection->socket);
        m_clients.remove(id);

        lock.unlockEarly();
        connection->client.didClose(*this, id);
    }
}

void RemoteInspectorSocketEndpoint::sendIfEnabled(ConnectionID id)
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_clients.get(id)) {
        Socket::clearWaitingWritable(connection->poll);

        auto& buffer = connection->sendBuffer;
        if (buffer.isEmpty())
            return;

        if (auto writeSize = Socket::write(connection->socket, buffer.data(), std::min(buffer.size(), Socket::BufferSize))) {
            auto size = *writeSize;
            if (size == buffer.size()) {
                buffer.clear();
                return;
            }

            if (size > 0)
                buffer.remove(0, size);
        }

        Socket::markWaitingWritable(connection->poll);
    }
}

void RemoteInspectorSocketEndpoint::send(ConnectionID id, const uint8_t* data, size_t size)
{
    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_clients.get(id)) {
        size_t offset = 0;
        if (connection->sendBuffer.isEmpty()) {
            // Try to call send() directly if buffer is empty.
            if (auto writeSize = Socket::write(connection->socket, data, std::min(size, Socket::BufferSize)))
                offset = *writeSize;
            // @TODO need to handle closed socket case?
        }

        // Check all data is sent.
        if (offset == size)
            return;

        // Copy remaining data to send later.
        connection->sendBuffer.appendRange(data + offset, data + size);
        Socket::markWaitingWritable(connection->poll);

        wakeupWorkerThread();
    }
}

void RemoteInspectorSocketEndpoint::acceptInetSocketIfEnabled(ConnectionID id)
{
    ASSERT(isListening(id));

    LockHolder lock(m_connectionsLock);
    if (const auto& connection = m_listeners.get(id)) {
        if (auto socket = Socket::accept(connection->socket)) {
            // Need to unlock before calling createClient as it also attempts to lock.
            lock.unlockEarly();
            if (connection->listener.doAccept(*this, socket.value()))
                return;
            Socket::close(*socket);
        }
    }
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
