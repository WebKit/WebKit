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

#pragma once

#include <thread>
#include <wtf/Function.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

#if HAVE(SSL)
struct SSL;
#endif // HAVE(SSL)

namespace TestWebKitAPI {

class TCPServer {
public:
    using Socket = int;
    using Port = uint16_t;
    static constexpr Port InvalidPort = 0;
    
    TCPServer(Function<void(Socket)>&&, size_t connections = 1);
#if HAVE(SSL)
    enum class Protocol : uint8_t {
        HTTPS, HTTPSProxy, HTTPSWithClientCertificateRequest
    };
    TCPServer(Protocol, Function<void(SSL*)>&&, Optional<uint16_t> maxTLSVersion = WTF::nullopt);
#endif // HAVE(SSL)
    ~TCPServer();
    
    Port port() const { return m_port; }
    
#if HAVE(SSL)
    static void respondWithOK(SSL*);
#endif
    static void respondWithChallengeThenOK(Socket);

    template<typename T> static Vector<uint8_t> read(T);
    template<typename T> static void write(T, const void*, size_t);
    
    static Vector<uint8_t> testPrivateKey();
    static Vector<uint8_t> testCertificate();
    
private:
    Optional<Socket> socketBindListen(size_t connections);
    void listenForConnections(size_t connections);

    Port m_port { InvalidPort };
    std::thread m_listeningThread;
    Vector<std::thread> m_connectionThreads;
    Function<void(Socket)> m_connectionHandler;
};

} // namespace TestWebKitAPI
