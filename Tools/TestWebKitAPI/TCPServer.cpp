/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "TCPServer.h"

#include <netinet/in.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace TestWebKitAPI {

TCPServer::TCPServer(Function<void(Socket)>&& socketHandler)
    : m_socketHandler(WTFMove(socketHandler))
{
    socketBindListen();
    m_thread = std::thread(&TCPServer::waitForAndReplyToRequests, this);
}

TCPServer::~TCPServer()
{
    m_thread.join();
    if (m_listeningSocket != InvalidSocket) {
        close(m_listeningSocket);
        m_listeningSocket = InvalidSocket;
    }
    if (m_connectionSocket != InvalidSocket) {
        close(m_connectionSocket);
        m_connectionSocket = InvalidSocket;
    }
}

void TCPServer::socketBindListen()
{
    m_listeningSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (m_listeningSocket == InvalidSocket)
        return;
    
    // Ports 49152-65535 are unallocated ports. Try until we find one that's free.
    for (Port port = 49152; port; port++) {
        struct sockaddr_in name;
        memset(&name, 0, sizeof(name));
        name.sin_family = AF_INET;
        name.sin_port = htons(port);
        name.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(m_listeningSocket, reinterpret_cast<sockaddr*>(&name), sizeof(name)) < 0) {
            // This port is busy. Try the next port.
            continue;
        }
        const unsigned maxConnections = 1;
        if (listen(m_listeningSocket, maxConnections) == -1) {
            // Listening failed.
            close(m_listeningSocket);
            m_listeningSocket = InvalidSocket;
            return;
        }
        m_port = port;
        return; // Successfully set up listening port.
    }
    
    // Couldn't find an available port.
    close(m_listeningSocket);
    m_listeningSocket = InvalidSocket;
}

void TCPServer::waitForAndReplyToRequests()
{
    if (m_listeningSocket == InvalidSocket)
        return;
    
    m_connectionSocket = accept(m_listeningSocket, nullptr, nullptr);
    m_socketHandler(m_connectionSocket);
    shutdown(m_connectionSocket, SHUT_RDWR);
}

} // namespace TestWebKitAPI