/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "DaemonConnection.h"
#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import <Foundation/Foundation.h>
#import <wtf/MainThread.h>
#import <wtf/Span.h>
#import <wtf/spi/darwin/XPCSPI.h>

namespace WebPushD {

constexpr const char* protocolVersionKey = "protocol version";
constexpr uint64_t protocolVersionValue = 1;
constexpr const char* protocolMessageTypeKey = "message type";
constexpr const char* protocolEncodedMessageKey = "encoded message";
enum class MessageType : uint8_t {
    EchoTwice = 1
};

static CompletionHandler<void(Vector<uint8_t>&&)> replySender(RetainPtr<xpc_object_t>&& request)
{
    return [request = WTFMove(request)] (Vector<uint8_t>&& message) {
        auto reply = adoptNS(xpc_dictionary_create_reply(request.get()));
        ASSERT(xpc_get_type(reply.get()) == XPC_TYPE_DICTIONARY);
        xpc_dictionary_set_uint64(reply.get(), protocolVersionKey, protocolVersionValue);
        xpc_dictionary_set_value(reply.get(), protocolEncodedMessageKey, WebKit::vectorToXPCData(WTFMove(message)).get());
        xpc_connection_send_message(xpc_dictionary_get_remote_connection(request.get()), reply.get());
    };
}

static void echoTwice(Span<const uint8_t> encodedMessage, CompletionHandler<void(Vector<uint8_t>&&)>&& replySender)
{
    WebKit::Daemon::Decoder decoder(encodedMessage);
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return;

    auto reply = makeString(*string, *string);
    WebKit::Daemon::Encoder encoder;
    encoder << reply;
    replySender(encoder.takeBuffer());
}

static void decodeMessageAndSendReply(MessageType messageType, Span<const uint8_t> encodedMessage, CompletionHandler<void(Vector<uint8_t>&&)>&& replySender)
{
    switch (messageType) {
    case MessageType::EchoTwice:
        echoTwice(encodedMessage, WTFMove(replySender));
        return;
    }
}

static void connectionEventHandler(xpc_object_t request)
{
    if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
        return;
    if (xpc_dictionary_get_uint64(request, protocolVersionKey) != protocolVersionValue) {
        NSLog(@"Received request that was not the current protocol version");
        return;
    }

    auto messageType { static_cast<MessageType>(xpc_dictionary_get_uint64(request, protocolMessageTypeKey)) };
    size_t dataSize { 0 };
    const void* data = xpc_dictionary_get_data(request, protocolEncodedMessageKey, &dataSize);
    Span<const uint8_t> encodedMessage { static_cast<const uint8_t*>(data), dataSize };
    decodeMessageAndSendReply(messageType, encodedMessage, replySender(request));
}

static void connectionAdded(xpc_connection_t)
{
}

static void connectionRemoved(xpc_connection_t)
{
}

} // namespace WebPushD

int main(int argc, const char** argv)
{
    if (argc != 3 || strcmp(argv[1], "--machServiceName")) {
        NSLog(@"usage: webpushd --machServiceName <name>");
        return -1;
    }
    const char* machServiceName = argv[2];

    @autoreleasepool {
        // FIXME: Add a sandbox.
        // FIXME: Add an entitlement check.
        WebKit::startListeningForMachServiceConnections(machServiceName, nullptr, WebPushD::connectionAdded, WebPushD::connectionRemoved, WebPushD::connectionEventHandler);
        WTF::initializeMainThread();
    }
    CFRunLoopRun();
    return 0;
}
