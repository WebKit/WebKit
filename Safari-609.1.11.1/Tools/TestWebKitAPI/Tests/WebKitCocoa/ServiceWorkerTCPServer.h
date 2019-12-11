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

#import "TCPServer.h"
#import <wtf/text/WTFString.h>

class ServiceWorkerTCPServer : public TestWebKitAPI::TCPServer {
public:
    struct ResourceInfo {
        const char* mimeType { nullptr };
        const char* response { nullptr };
    };

    explicit ServiceWorkerTCPServer(Vector<ResourceInfo>&& vector)
        : ServiceWorkerTCPServer(WTFMove(vector), { }, 1) { }
    
    ServiceWorkerTCPServer(Vector<ResourceInfo>&& firstConnection, Vector<ResourceInfo>&& secondConnection, size_t connections = 2, Vector<String>&& expectedUserAgents = { })
        : ServiceWorkerTCPServer(WTFMove(firstConnection), WTFMove(secondConnection), { }, connections, WTFMove(expectedUserAgents)) { }

    ServiceWorkerTCPServer(Vector<ResourceInfo>&& firstConnection, Vector<ResourceInfo>&& secondConnection, Vector<ResourceInfo>&& thirdConnection, size_t connections = 3, Vector<String>&& expectedUserAgents = { })
        : TCPServer([this, firstConnection = WTFMove(firstConnection), secondConnection = WTFMove(secondConnection), thirdConnection = WTFMove(thirdConnection), expectedUserAgents = WTFMove(expectedUserAgents)] (int socket) mutable {
            auto respondToRequests = [this, socket] (Vector<ResourceInfo>&& vector, const String& expectedUserAgent) {
                for (auto& info : vector) {
                    auto request = TCPServer::read(socket);
                    if (!expectedUserAgent.isNull()) {
                        EXPECT_TRUE(strstr((const char*)request.data(), makeString("User-Agent: ", expectedUserAgent).utf8().data()));
                        m_userAgentsChecked++;
                    }
                    NSString *format = @"HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %d\r\n\r\n"
                    "%s";
                    NSString *response = [NSString stringWithFormat:format, info.mimeType, strlen(info.response), info.response];
                    TCPServer::write(socket, response.UTF8String, response.length);
                }
            };
            if (!firstConnection.isEmpty())
                return respondToRequests(std::exchange(firstConnection, { }), !expectedUserAgents.isEmpty() ? expectedUserAgents[0] : String());
            if (!secondConnection.isEmpty())
                return respondToRequests(std::exchange(secondConnection, { }), expectedUserAgents.size() > 1 ? expectedUserAgents[1] : String());
            respondToRequests(std::exchange(thirdConnection, { }), { });
        }, connections) { }

    NSURLRequest *request() { return requestWithFormat(@"http://127.0.0.1:%d/main.html"); }
    NSURLRequest *requestWithLocalhost() { return requestWithFormat(@"http://localhost:%d/main.html"); }
    NSURLRequest *requestWithFragment() { return requestWithFormat(@"http://127.0.0.1:%d/main.html#fragment"); }
    size_t userAgentsChecked() const { return m_userAgentsChecked; }

private:
    NSURLRequest *requestWithFormat(NSString *format) { return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:format, port()]]]; }
    std::atomic<size_t> m_userAgentsChecked { 0 };
};
