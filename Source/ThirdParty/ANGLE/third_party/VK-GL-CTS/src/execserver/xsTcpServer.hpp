#ifndef _XSTCPSERVER_HPP
#define _XSTCPSERVER_HPP
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

#include "xsDefs.hpp"
#include "deSocket.hpp"
#include "deThread.hpp"
#include "deMutex.hpp"

#include <vector>

namespace xs
{

class ConnectionHandler;

class TcpServer
{
public:
    TcpServer(deSocketFamily family, int port);
    virtual ~TcpServer(void);

    virtual ConnectionHandler *createHandler(de::Socket *socket, const de::SocketAddress &clientAddress) = 0;

    virtual void runServer(void);
    void stopServer(void);

    virtual void connectionDone(ConnectionHandler *handler);

protected:
    de::Socket m_socket;

private:
    TcpServer(const TcpServer &other);
    TcpServer &operator=(const TcpServer &other);

    void addLiveConnection(ConnectionHandler *handler);
    void deleteDoneConnections(void);

    de::Mutex m_connectionListLock;
    std::vector<ConnectionHandler *> m_liveConnections;
    std::vector<ConnectionHandler *> m_doneConnections;
};

class ConnectionHandler : public de::Thread
{
public:
    ConnectionHandler(TcpServer *server, de::Socket *socket) : m_server(server), m_socket(socket)
    {
    }
    virtual ~ConnectionHandler(void);

    void run(void);

protected:
    virtual void handle(void) = 0;

protected:
    TcpServer *m_server;
    de::Socket *m_socket;
};

} // namespace xs

#endif // _XSTCPSERVER_HPP
