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

#import "HTTPServer.h"
#import <wtf/text/WTFString.h>

class ServiceWorkerTCPServer : public TestWebKitAPI::HTTPServer {
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
        : TestWebKitAPI::HTTPServer([this, firstConnection = WTFMove(firstConnection), secondConnection = WTFMove(secondConnection), thirdConnection = WTFMove(thirdConnection), expectedUserAgents = WTFMove(expectedUserAgents)] (TestWebKitAPI::Connection connection) mutable {
            if (!firstConnection.isEmpty())
                return respondToRequests(connection, std::exchange(firstConnection, { }), !expectedUserAgents.isEmpty() ? expectedUserAgents[0] : String());
            if (!secondConnection.isEmpty())
                return respondToRequests(connection, std::exchange(secondConnection, { }), expectedUserAgents.size() > 1 ? expectedUserAgents[1] : String());
            respondToRequests(connection, std::exchange(thirdConnection, { }), { });
        }) { }

    NSURLRequest *request()
    {
        auto url = adoptNS([[NSString alloc] initWithFormat:@"http://127.0.0.1:%d/main.html", port()]);
        return requestWithURLString(url.get());
    }

    NSURLRequest *requestWithLocalhost()
    {
        auto url = adoptNS([[NSString alloc] initWithFormat:@"http://localhost:%d/main.html", port()]);
        return requestWithURLString(url.get());
    }

    NSURLRequest *requestWithFragment()
    {
        auto url = adoptNS([[NSString alloc] initWithFormat:@"http://127.0.0.1:%d/main.html#fragment", port()]);
        return requestWithURLString(url.get());
    }

    size_t userAgentsChecked() const { return m_userAgentsChecked; }

private:
    void respondToRequests(TestWebKitAPI::Connection& connection, Vector<ResourceInfo>&& vector, const String& expectedUserAgent, size_t vectorIndex = 0)
    {
        if (vectorIndex >= vector.size()) {
            connection.terminate();
            return;
        }
        connection.receiveHTTPRequest([=, vector = WTFMove(vector)] (Vector<char>&& request) mutable {
            if (!expectedUserAgent.isNull()) {
                EXPECT_TRUE(strnstr((const char*)request.data(), makeString("User-Agent: ", expectedUserAgent).utf8().data(), request.size()));
                m_userAgentsChecked++;
            }
            constexpr NSString *format = @"HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n\r\n"
            "%s";
            auto& info = vector[vectorIndex];
            NSString *response = [NSString stringWithFormat:format, info.mimeType, strlen(info.response), info.response];
            connection.send(response, [=, vector = WTFMove(vector)] () mutable {
                respondToRequests(connection, WTFMove(vector), expectedUserAgent, vectorIndex + 1);
            });
        });
    }

    NSURLRequest *requestWithURLString(NSString *urlString) { return [NSURLRequest requestWithURL:[NSURL URLWithString:urlString]]; }
    size_t m_userAgentsChecked { 0 };
};
