/*-------------------------------------------------------------------------
 * drawElements Quality Program Execution Server
 * ---------------------------------------------
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
 * \brief TCP Server.
 *//*--------------------------------------------------------------------*/

#include "xsTcpServer.hpp"

#include <algorithm>
#include <iterator>
#include <cstdio>

namespace xs
{

TcpServer::TcpServer(deSocketFamily family, int port) : m_socket()
{
    de::SocketAddress address;
    address.setFamily(family);
    address.setPort(port);
    address.setType(DE_SOCKETTYPE_STREAM);
    address.setProtocol(DE_SOCKETPROTOCOL_TCP);

    m_socket.listen(address);
    m_socket.setFlags(DE_SOCKET_CLOSE_ON_EXEC);
}

void TcpServer::runServer(void)
{
    de::Socket *clientSocket = nullptr;
    de::SocketAddress clientAddr;

    while ((clientSocket = m_socket.accept(clientAddr)) != nullptr)
    {
        ConnectionHandler *handler = nullptr;

        try
        {
            handler = createHandler(clientSocket, clientAddr);
        }
        catch (...)
        {
            delete clientSocket;
            throw;
        }

        try
        {
            addLiveConnection(handler);
        }
        catch (...)
        {
            delete handler;
            throw;
        }

        // Start handler.
        handler->start();

        // Perform connection list cleanup.
        deleteDoneConnections();
    }

    // One more cleanup pass.
    deleteDoneConnections();
}

void TcpServer::connectionDone(ConnectionHandler *handler)
{
    de::ScopedLock lock(m_connectionListLock);

    std::vector<ConnectionHandler *>::iterator liveListPos =
        std::find(m_liveConnections.begin(), m_liveConnections.end(), handler);
    DE_ASSERT(liveListPos != m_liveConnections.end());

    m_doneConnections.reserve(m_doneConnections.size() + 1);
    m_liveConnections.erase(liveListPos);
    m_doneConnections.push_back(handler);
}

void TcpServer::addLiveConnection(ConnectionHandler *handler)
{
    de::ScopedLock lock(m_connectionListLock);
    m_liveConnections.push_back(handler);
}

void TcpServer::deleteDoneConnections(void)
{
    de::ScopedLock lock(m_connectionListLock);

    for (std::vector<ConnectionHandler *>::iterator i = m_doneConnections.begin(); i != m_doneConnections.end(); i++)
        delete *i;

    m_doneConnections.clear();
}

void TcpServer::stopServer(void)
{
    // Close socket. This should get accept() to return null.
    m_socket.close();
}

TcpServer::~TcpServer(void)
{
    try
    {
        std::vector<ConnectionHandler *> allConnections;

        if (m_connectionListLock.tryLock())
        {
            // \note [pyry] It is possible that cleanup actually fails.
            try
            {
                std::copy(m_liveConnections.begin(), m_liveConnections.end(),
                          std::inserter(allConnections, allConnections.end()));
                std::copy(m_doneConnections.begin(), m_doneConnections.end(),
                          std::inserter(allConnections, allConnections.end()));
            }
            catch (...)
            {
            }
            m_connectionListLock.unlock();
        }

        for (std::vector<ConnectionHandler *>::const_iterator i = allConnections.begin(); i != allConnections.end();
             i++)
            delete *i;

        if (m_socket.getState() != DE_SOCKETSTATE_CLOSED)
            m_socket.close();
    }
    catch (...)
    {
        // Nada, we're at destructor.
    }
}

ConnectionHandler::~ConnectionHandler(void)
{
    delete m_socket;
}

void ConnectionHandler::run(void)
{
    try
    {
        handle();
    }
    catch (const std::exception &e)
    {
        printf("ConnectionHandler::run(): %s\n", e.what());
    }

    // Notify server that this connection is done.
    m_server->connectionDone(this);
}

} // namespace xs
