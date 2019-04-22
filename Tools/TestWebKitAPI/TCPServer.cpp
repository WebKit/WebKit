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
#include <wtf/Optional.h>

namespace TestWebKitAPI {

TCPServer::TCPServer(Function<void(Socket)>&& connectionHandler, size_t connections)
    : m_connectionHandler(WTFMove(connectionHandler))
{
    auto listeningSocket = socketBindListen(connections);
    ASSERT(listeningSocket);
    m_listeningThread = std::thread([this, listeningSocket = *listeningSocket, connections] {
        for (size_t i = 0; i < connections; ++i) {
            Socket connectionSocket = accept(listeningSocket, nullptr, nullptr);
            m_connectionThreads.append(std::thread([this, connectionSocket] {
                m_connectionHandler(connectionSocket);
                shutdown(connectionSocket, SHUT_RDWR);
                close(connectionSocket);
            }));
        }
    });
}

TCPServer::~TCPServer()
{
    m_listeningThread.join();
    for (auto& connectionThreads : m_connectionThreads)
        connectionThreads.join();
}

auto TCPServer::socketBindListen(size_t connections) -> Optional<Socket>
{
    Socket listeningSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (listeningSocket == -1)
        return WTF::nullopt;
    
    // Ports 49152-65535 are unallocated ports. Try until we find one that's free.
    for (Port port = 49152; port; port++) {
        struct sockaddr_in name;
        memset(&name, 0, sizeof(name));
        name.sin_family = AF_INET;
        name.sin_port = htons(port);
        name.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(listeningSocket, reinterpret_cast<sockaddr*>(&name), sizeof(name)) < 0) {
            // This port is busy. Try the next port.
            continue;
        }
        if (listen(listeningSocket, connections) == -1) {
            // Listening failed.
            close(listeningSocket);
            return WTF::nullopt;
        }
        m_port = port;
        return listeningSocket; // Successfully set up listening port.
    }
    
    // Couldn't find an available port.
    close(listeningSocket);
    return WTF::nullopt;
}

} // namespace TestWebKitAPI
