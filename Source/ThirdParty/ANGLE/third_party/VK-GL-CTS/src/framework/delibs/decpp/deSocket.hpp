#ifndef _DESOCKET_HPP
#define _DESOCKET_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief deSocket C++ wrapper.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deSocket.h"

#include <string>
#include <stdexcept>

namespace de
{

class SocketError : public std::runtime_error
{
public:
    SocketError(const std::string &message) : std::runtime_error(message)
    {
    }
};

class SocketAddress
{
public:
    SocketAddress(void);
    ~SocketAddress(void);

    void setHost(const char *host);
    void setPort(int port);
    void setFamily(deSocketFamily family);
    void setType(deSocketType type);
    void setProtocol(deSocketProtocol protocol);

    const char *getHost(void) const
    {
        return deSocketAddress_getHost(m_address);
    }
    int getPort(void) const
    {
        return deSocketAddress_getPort(m_address);
    }
    deSocketFamily getFamily(void) const
    {
        return deSocketAddress_getFamily(m_address);
    }
    deSocketType getType(void) const
    {
        return deSocketAddress_getType(m_address);
    }
    deSocketProtocol getProtocol(void) const
    {
        return deSocketAddress_getProtocol(m_address);
    }

    operator deSocketAddress *()
    {
        return m_address;
    }
    operator const deSocketAddress *() const
    {
        return m_address;
    }

    deSocketAddress *getPtr(void)
    {
        return m_address;
    }

private:
    SocketAddress(const SocketAddress &other);
    SocketAddress &operator=(const SocketAddress &other);

    deSocketAddress *m_address;
};

class Socket
{
public:
    Socket(void);
    ~Socket(void);

    void setFlags(uint32_t flags);

    deSocketState getState(void) const
    {
        return deSocket_getState(m_socket);
    }
    bool isConnected(void) const
    {
        return getState() == DE_SOCKETSTATE_CONNECTED;
    }

    void listen(const SocketAddress &address);
    Socket *accept(SocketAddress &clientAddress)
    {
        return accept(clientAddress.getPtr());
    }
    Socket *accept(void)
    {
        return accept(nullptr);
    }

    void connect(const SocketAddress &address);

    void shutdown(void);
    void shutdownSend(void);
    void shutdownReceive(void);

    bool isSendOpen(void)
    {
        return (deSocket_getOpenChannels(m_socket) & DE_SOCKETCHANNEL_SEND) != 0;
    }
    bool isReceiveOpen(void)
    {
        return (deSocket_getOpenChannels(m_socket) & DE_SOCKETCHANNEL_RECEIVE) != 0;
    }

    void close(void);

    deSocketResult send(const void *buf, size_t bufSize, size_t *numSent)
    {
        return deSocket_send(m_socket, buf, bufSize, numSent);
    }
    deSocketResult receive(void *buf, size_t bufSize, size_t *numRecv)
    {
        return deSocket_receive(m_socket, buf, bufSize, numRecv);
    }

private:
    Socket(deSocket *socket) : m_socket(socket)
    {
    }
    Socket(const Socket &other);
    Socket &operator=(const Socket &other);

    Socket *accept(deSocketAddress *clientAddress);

    deSocket *m_socket;
};

} // namespace de

#endif // _DESOCKET_HPP
