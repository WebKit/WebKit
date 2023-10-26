/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "ArgumentCodersCocoa.h"
#import "Encoder.h"
#import "MessageSenderInlines.h"
#import <wtf/RetainPtr.h>

// This test makes it trivial to test round trip encoding and decoding of a particular object type.
// The primary focus here is Objective-C and similar types - Objects that exist on the platform or in
// runtime that the WebKit project doesn't have direct control of.
// To test a new ObjC type:
// 1 - Add a member of that type to ObjCHolderForTesting
// 2 - Include that new member in ObjCHolderForTesting::encode() and ObjCHolderForTesting::decode()
// 3 - Include an equality comparison of that new member in the operator== function

class ObjCSerializationTester final : public IPC::MessageSender {
public:
    ~ObjCSerializationTester() final { };

private:
    bool performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&&, CompletionHandler<void(IPC::Decoder*)>&&) const final;

    IPC::Connection* messageSenderConnection() const final { return nullptr; }
    uint64_t messageSenderDestinationID() const final { return 0; }
};

bool ObjCSerializationTester::performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder, CompletionHandler<void(IPC::Decoder*)>&& completionHandler) const
{
    auto decoder = IPC::Decoder::create({ encoder->buffer(), encoder->bufferSize() }, { });
    ASSERT(decoder);

    completionHandler(decoder.get());
    return true;
}

struct ObjCHolderForTesting {
    void encode(IPC::Encoder&) const;
    static std::optional<ObjCHolderForTesting> decode(IPC::Decoder&);

    RetainPtr<NSString> stringValue;
};

void ObjCHolderForTesting::encode(IPC::Encoder& encoder) const
{
    encoder << stringValue;
}

std::optional<ObjCHolderForTesting> ObjCHolderForTesting::decode(IPC::Decoder& decoder)
{
    std::optional<RetainPtr<NSString>> stringValue;
    decoder >> stringValue;
    if (!stringValue)
        return std::nullopt;

    return { {
        WTFMove(*stringValue)
    } };
}

inline bool operator==(const ObjCHolderForTesting& a, const ObjCHolderForTesting& b)
{
    if (![a.stringValue.get() isEqualToString:b.stringValue.get()])
        return false;
    return true;
}

class BasicPingBackMessage {
public:
    using Arguments = std::tuple<ObjCHolderForTesting>;
    using ReplyArguments = std::tuple<ObjCHolderForTesting>;

    // We can use any MessageName here
    static IPC::MessageName name() { return IPC::MessageName::IPCTester_AsyncPing; }
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::IPCTester_AsyncPingReply; }

    static constexpr bool isSync = false;

    BasicPingBackMessage(ObjCHolderForTesting&& holder)
        : m_arguments(WTFMove(holder))
    {
    }

    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<const ObjCHolderForTesting&> m_arguments;
};

TEST(IPCSerialization, Basic)
{
    __block ObjCHolderForTesting holder;
    holder.stringValue = @"Hello world";

    auto sender = ObjCSerializationTester { };
    __block bool done = false;
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(BasicPingBackMessage(WTFMove(holder)), ^(ObjCHolderForTesting&& result) {
        EXPECT_EQ(holder, result);
        done = true;
    });

    // The completion handler should be called synchronously, so this should be true already.
    EXPECT_TRUE(done);
}
