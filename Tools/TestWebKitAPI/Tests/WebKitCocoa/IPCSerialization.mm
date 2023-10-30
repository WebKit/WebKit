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

#import "ArgumentCodersCF.h"
#import "ArgumentCodersCocoa.h"
#import "Encoder.h"
#import "MessageSenderInlines.h"
#import <wtf/RetainPtr.h>

// This test makes it trivial to test round trip encoding and decoding of a particular object type.
// The primary focus here is Objective-C and similar types - Objects that exist on the platform or in
// runtime that the WebKit project doesn't have direct control of.
// To test a new ObjC type:
// 1 - Add that type to ObjCHolderForTesting's ValueType variant
// 2 - Run a test exercising that type

class SerializationTestSender final : public IPC::MessageSender {
public:
    ~SerializationTestSender() final { };

private:
    bool performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&&, CompletionHandler<void(IPC::Decoder*)>&&) const final;

    IPC::Connection* messageSenderConnection() const final { return nullptr; }
    uint64_t messageSenderDestinationID() const final { return 0; }
};

bool SerializationTestSender::performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder, CompletionHandler<void(IPC::Decoder*)>&& completionHandler) const
{
    auto decoder = IPC::Decoder::create({ encoder->buffer(), encoder->bufferSize() }, { });
    ASSERT(decoder);

    completionHandler(decoder.get());
    return true;
}

struct CFHolderForTesting {
    void encode(IPC::Encoder&) const;
    static std::optional<CFHolderForTesting> decode(IPC::Decoder&);

    CFTypeRef valueAsCFType() const
    {
        CFTypeRef result;
        std::visit([&](auto&& arg) {
            result = arg.get();
        }, value);
        return result;
    }

    typedef std::variant<
        RetainPtr<CFStringRef>,
        RetainPtr<CFURLRef>,
        RetainPtr<CFDataRef>
    > ValueType;

    ValueType value;
};

void CFHolderForTesting::encode(IPC::Encoder& encoder) const
{
    encoder << value;
}

std::optional<CFHolderForTesting> CFHolderForTesting::decode(IPC::Decoder& decoder)
{
    std::optional<ValueType> value;
    decoder >> value;
    if (!value)
        return std::nullopt;

    return { {
        WTFMove(*value)
    } };
}

inline bool operator==(const CFHolderForTesting& a, const CFHolderForTesting& b)
{
    auto aObject = a.valueAsCFType();
    auto bObject = b.valueAsCFType();

    EXPECT_TRUE(aObject);
    EXPECT_TRUE(bObject);

    return CFEqual(aObject, bObject);
}

struct ObjCHolderForTesting {
    void encode(IPC::Encoder&) const;
    static std::optional<ObjCHolderForTesting> decode(IPC::Decoder&);

    id valueAsID() const
    {
        id result;
        std::visit([&](auto&& arg) {
            result = arg.get();
        }, value);
        return result;
    }

    typedef std::variant<
        RetainPtr<NSDate>,
        RetainPtr<NSString>,
        RetainPtr<NSURL>,
        RetainPtr<NSData>
    > ValueType;

    ValueType value;
};

void ObjCHolderForTesting::encode(IPC::Encoder& encoder) const
{
    encoder << value;
}

std::optional<ObjCHolderForTesting> ObjCHolderForTesting::decode(IPC::Decoder& decoder)
{
    std::optional<ValueType> value;
    decoder >> value;
    if (!value)
        return std::nullopt;

    return { {
        WTFMove(*value)
    } };
}

inline bool operator==(const ObjCHolderForTesting& a, const ObjCHolderForTesting& b)
{
    id aObject = a.valueAsID();
    id bObject = b.valueAsID();

    EXPECT_TRUE(aObject != nil);
    EXPECT_TRUE(bObject != nil);

    return [aObject isEqual:bObject];
}

class ObjCPingBackMessage {
public:
    using Arguments = std::tuple<ObjCHolderForTesting>;
    using ReplyArguments = std::tuple<ObjCHolderForTesting>;

    // We can use any MessageName here
    static IPC::MessageName name() { return IPC::MessageName::IPCTester_AsyncPing; }
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::IPCTester_AsyncPingReply; }

    static constexpr bool isSync = false;

    ObjCPingBackMessage(const ObjCHolderForTesting& holder)
        : m_arguments(holder)
    {
    }

    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<const ObjCHolderForTesting&> m_arguments;
};

class CFPingBackMessage {
public:
    using Arguments = std::tuple<CFHolderForTesting>;
    using ReplyArguments = std::tuple<CFHolderForTesting>;

    // We can use any MessageName here
    static IPC::MessageName name() { return IPC::MessageName::IPCTester_AsyncPing; }
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::IPCTester_AsyncPingReply; }

    static constexpr bool isSync = false;

    CFPingBackMessage(const CFHolderForTesting& holder)
        : m_arguments(holder)
    {
    }

    auto&& arguments()
    {
        return WTFMove(m_arguments);
    }

private:
    std::tuple<const CFHolderForTesting&> m_arguments;
};

TEST(IPCSerialization, Basic)
{
    auto runTestNS = [](ObjCHolderForTesting&& holderArg) {
        __block bool done = false;
        __block ObjCHolderForTesting holder = WTFMove(holderArg);
        auto sender = SerializationTestSender { };
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(ObjCPingBackMessage(holder), ^(ObjCHolderForTesting&& result) {
            EXPECT_TRUE(holder == result);
            done = true;
        });

        // The completion handler should be called synchronously, so this should be true already.
        EXPECT_TRUE(done);
    };

    auto runTestCF = [](CFHolderForTesting&& holderArg) {
        __block bool done = false;
        __block CFHolderForTesting holder = WTFMove(holderArg);
        auto sender = SerializationTestSender { };
        sender.sendWithAsyncReplyWithoutUsingIPCConnection(CFPingBackMessage(holder), ^(CFHolderForTesting&& result) {
            EXPECT_TRUE(holder == result);
            done = true;
        });

        // The completion handler should be called synchronously, so this should be true already.
        EXPECT_TRUE(done);
    };

    runTestNS({ @"Hello world" });
    runTestCF({ CFSTR("Hello world") });

    runTestNS({ [NSURL URLWithString:@"https://webkit.org/"] });
    auto cfURL = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, CFSTR("https://webkit.org/"), NULL));
    runTestCF({ cfURL.get() });

    runTestNS({ [NSData dataWithBytes:"Data test" length:strlen("Data test")] });
    auto cfData = adoptCF(CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"Data test", strlen("Data test")));
    runTestCF({ cfData.get() });

    NSDateComponents *dateComponents = [[NSDateComponents alloc] init];
    [dateComponents setYear:2007];
    [dateComponents setMonth:1];
    [dateComponents setDay:9];
    [dateComponents setHour:10];
    [dateComponents setMinute:00];
    [dateComponents setSecond:0];

    runTestNS({ [[NSCalendar currentCalendar] dateFromComponents:dateComponents] });
}
