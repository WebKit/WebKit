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
#import <Foundation/NSValue.h>
#import <WebCore/FontCocoa.h>
#import <limits.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

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
        RetainPtr<CFDataRef>,
        RetainPtr<CFBooleanRef>,
        RetainPtr<CGColorRef>,
        RetainPtr<CFDictionaryRef>
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

    if (CFEqual(aObject, bObject))
        return true;

    // Sometimes the CF input and CF output fail the CFEqual call above (Such as CFDictionaries containing certain things)
    // In these cases, give the Obj-C equivalent equality check a chance.
    return [(NSObject *)aObject isEqual: (NSObject *)bObject];
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
        RetainPtr<NSData>,
        RetainPtr<NSNumber>,
        RetainPtr<NSArray>,
        RetainPtr<NSDictionary>,
        RetainPtr<WebCore::CocoaFont>
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

    // NSString/CFString
    runTestNS({ @"Hello world" });
    runTestCF({ CFSTR("Hello world") });

    // NSURL/CFURL
    runTestNS({ [NSURL URLWithString:@"https://webkit.org/"] });
    auto cfURL = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, CFSTR("https://webkit.org/"), NULL));
    runTestCF({ cfURL.get() });

    // NSData/CFData
    runTestNS({ [NSData dataWithBytes:"Data test" length:strlen("Data test")] });
    auto cfData = adoptCF(CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"Data test", strlen("Data test")));
    runTestCF({ cfData.get() });

    // NSDate
    NSDateComponents *dateComponents = [[NSDateComponents alloc] init];
    [dateComponents setYear:2007];
    [dateComponents setMonth:1];
    [dateComponents setDay:9];
    [dateComponents setHour:10];
    [dateComponents setMinute:00];
    [dateComponents setSecond:0];
    runTestNS({ [[NSCalendar currentCalendar] dateFromComponents:dateComponents] });

    // CFBoolean
    runTestCF({ kCFBooleanTrue });
    runTestCF({ kCFBooleanFalse });

    // CGColor
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    constexpr CGFloat testComponents[4] = { 1, .75, .5, .25 };
    auto cgColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), testComponents));
    runTestCF({ cgColor.get() });

    // NSNumber
    runTestNS({ [NSNumber numberWithChar: CHAR_MIN] });
    runTestNS({ [NSNumber numberWithUnsignedChar: CHAR_MAX] });
    runTestNS({ [NSNumber numberWithShort: SHRT_MIN] });
    runTestNS({ [NSNumber numberWithUnsignedShort: SHRT_MAX] });
    runTestNS({ [NSNumber numberWithInt: INT_MIN] });
    runTestNS({ [NSNumber numberWithUnsignedInt: UINT_MAX] });
    runTestNS({ [NSNumber numberWithLong: LONG_MIN] });
    runTestNS({ [NSNumber numberWithUnsignedLong: ULONG_MAX] });
    runTestNS({ [NSNumber numberWithLongLong: LLONG_MIN] });
    runTestNS({ [NSNumber numberWithUnsignedLongLong: ULLONG_MAX] });
    runTestNS({ [NSNumber numberWithFloat: 3.14159] });
    runTestNS({ [NSNumber numberWithDouble: 9.98989898989] });
    runTestNS({ [NSNumber numberWithBool: true] });
    runTestNS({ [NSNumber numberWithBool: false] });
    runTestNS({ [NSNumber numberWithInteger: NSIntegerMax] });
    runTestNS({ [NSNumber numberWithInteger: NSIntegerMin] });
    runTestNS({ [NSNumber numberWithUnsignedInteger: NSUIntegerMax] });

    // NSArray
    runTestNS({ @[ @"Array test", @1, @{ @"hello": @9 }, @[ @"Another", @3, @"array"], [NSURL URLWithString:@"https://webkit.org/"], [NSData dataWithBytes:"Data test" length:strlen("Data test")] ] });

    // NSDictionary
    runTestNS({ @{ @"Dictionary": @[ @"array value", @12 ] } });

    // CFDictionary with a non-toll-free bridged CFType, also run as NSDictionary
    auto cfDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 1, NULL, NULL));
    CFDictionaryAddValue(cfDictionary.get(), CFSTR("MyKey"), cgColor.get());
    runTestCF({ cfDictionary.get() });
    runTestNS({ bridge_cast(cfDictionary.get()) });

    // NSFont/UIFont
    runTestNS({ [WebCore::CocoaFont systemFontOfSize:[WebCore::CocoaFont systemFontSize]] });
}
