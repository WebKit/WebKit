/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlatformUtilities.h"

#import <WebKit/XPCEndpoint.h>
#import <WebKit/XPCEndpointClient.h>
#import <wtf/text/WTFString.h>

static bool clientConnectedToEndpoint = false;
static bool endpointReceivedMessageFromClient = false;
static bool clientReceivedMessageFromEndpoint = false;

static constexpr auto testMessageFromEndpoint = "test-message-from-endpoint";
static constexpr auto testMessageFromClient = "test-message-from-client";

class XPCEndpoint : public WebKit::XPCEndpoint {
private:
    const char* xpcEndpointMessageNameKey() const override
    {
        return nullptr;
    }
    const char* xpcEndpointMessageName() const override
    {
        return nullptr;
    }
    const char* xpcEndpointNameKey() const override
    {
        return nullptr;
    }
    void handleEvent(xpc_connection_t connection, xpc_object_t event) override
    {
        String messageName = xpc_dictionary_get_string(event, XPCEndpoint::xpcMessageNameKey);
        if (messageName == testMessageFromClient) {
            endpointReceivedMessageFromClient = true;

            auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
            xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, testMessageFromEndpoint);
            xpc_connection_send_message(connection, message.get());
        }
    }
};

class XPCEndpointClient : public WebKit::XPCEndpointClient {
private:
    void handleEvent(xpc_object_t event) override
    {
        String messageName = xpc_dictionary_get_string(event, XPCEndpoint::xpcMessageNameKey);
        if (messageName == testMessageFromEndpoint)
            clientReceivedMessageFromEndpoint = true;
    }
    void didConnect() override
    {
        auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, testMessageFromClient);
        xpc_connection_send_message(connection().get(), message.get());

        clientConnectedToEndpoint = true;
    }
};

TEST(WebKit, XPCEndpoint)
{
    XPCEndpoint xpcEndpoint;
    XPCEndpointClient xpcEndpointClient;

    xpcEndpointClient.setEndpoint(xpcEndpoint.endpoint().get());

    TestWebKitAPI::Util::run(&clientConnectedToEndpoint);
    TestWebKitAPI::Util::run(&endpointReceivedMessageFromClient);
    TestWebKitAPI::Util::run(&clientReceivedMessageFromEndpoint);
}
