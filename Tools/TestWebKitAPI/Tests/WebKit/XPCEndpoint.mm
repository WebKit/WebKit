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

#import "XPCEndpoint.h"
#import "XPCEndpointClient.h"
#import <wtf/text/WTFString.h>

static bool clientConnectedToEndpoint = false;
static bool endpointReceivedMessageFromClient = false;
static bool clientReceivedMessageFromEndpoint = false;

static constexpr auto testMessageFromEndpoint = "test-message-from-endpoint"_s;
static constexpr auto testMessageFromClient = "test-message-from-client"_s;

class XPCEndpoint final : public WebKit::XPCEndpoint {
private:
    ASCIILiteral xpcEndpointMessageNameKey() const final
    {
        return { };
    }
    ASCIILiteral xpcEndpointMessageName() const final
    {
        return { };
    }
    ASCIILiteral xpcEndpointNameKey() const final
    {
        return { };
    }
    void handleEvent(xpc_connection_t connection, xpc_object_t event) final
    {
        String messageName = xpcDictionaryGetString(event, XPCEndpoint::xpcMessageNameKey);
        if (messageName == testMessageFromClient) {
            endpointReceivedMessageFromClient = true;

            // FIXME: This is a false positive. <rdar://164843889>
            SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
            xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, testMessageFromEndpoint);
            xpc_connection_send_message(connection, message.get());
        }
    }
};

class XPCEndpointClient final : public WebKit::XPCEndpointClient {
private:
    void handleEvent(xpc_object_t event) final
    {
        String messageName = xpcDictionaryGetString(event, XPCEndpoint::xpcMessageNameKey);
        if (messageName == testMessageFromEndpoint)
            clientReceivedMessageFromEndpoint = true;
    }
    void didConnect() final
    {
        // FIXME: This is a false positive. <rdar://164843889>
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
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
