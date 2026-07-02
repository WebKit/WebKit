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

#include "deSocket.hpp"

#include <new>
#include <exception>

namespace de
{

// SocketAddress

SocketAddress::SocketAddress(void)
{
    m_address = deSocketAddress_create();
    if (!m_address)
        throw std::bad_alloc();
}

SocketAddress::~SocketAddress(void)
{
    deSocketAddress_destroy(m_address);
}

void SocketAddress::setHost(const char *host)
{
    if (!deSocketAddress_setHost(m_address, host))
        throw std::runtime_error("SocketAddress::setHost()");
}

void SocketAddress::setPort(int port)
{
    if (!deSocketAddress_setPort(m_address, port))
        throw std::runtime_error("SocketAddress::setPort()");
}

void SocketAddress::setFamily(deSocketFamily family)
{
    if (!deSocketAddress_setFamily(m_address, family))
        throw std::runtime_error("SocketAddress::setFamily()");
}

void SocketAddress::setType(deSocketType type)
{
    if (!deSocketAddress_setType(m_address, type))
        throw std::runtime_error("SocketAddress::setType()");
}

void SocketAddress::setProtocol(deSocketProtocol protocol)
{
    if (!deSocketAddress_setProtocol(m_address, protocol))
        throw std::runtime_error("SocketAddress::setProtocol()");
}

// Socket

Socket::Socket(void)
{
    m_socket = deSocket_create();
    if (!m_socket)
        throw std::bad_alloc();
}

Socket::~Socket(void)
{
    deSocket_destroy(m_socket);
}

void Socket::setFlags(uint32_t flags)
{
    if (!deSocket_setFlags(m_socket, flags))
        throw SocketError("Setting socket flags failed");
}

void Socket::listen(const SocketAddress &address)
{
    if (!deSocket_listen(m_socket, address))
        throw SocketError("Listening on socket failed");
}

void Socket::connect(const SocketAddress &address)
{
    if (!deSocket_connect(m_socket, address))
        throw SocketError("Connecting socket failed");
}

void Socket::shutdown(void)
{
    if (!deSocket_shutdown(m_socket, DE_SOCKETCHANNEL_BOTH))
        throw SocketError("Socket shutdown failed");
}

void Socket::shutdownSend(void)
{
    if (!deSocket_shutdown(m_socket, DE_SOCKETCHANNEL_SEND))
        throw SocketError("Socket send channel shutdown failed");
}

void Socket::shutdownReceive(void)
{
    if (!deSocket_shutdown(m_socket, DE_SOCKETCHANNEL_RECEIVE))
        throw SocketError("Socket receive channel shutdown failed");
}

void Socket::close(void)
{
    if (!deSocket_close(m_socket))
        throw SocketError("Closing socket failed");
}

Socket *Socket::accept(deSocketAddress *clientAddress)
{
    deSocket *clientSocket = deSocket_accept(m_socket, clientAddress);
    if (!clientSocket)
        throw SocketError("Accepting connection to socket failed");

    try
    {
        return new Socket(clientSocket);
    }
    catch (...)
    {
        deSocket_destroy(clientSocket);
        throw;
    }
}

} // namespace de
