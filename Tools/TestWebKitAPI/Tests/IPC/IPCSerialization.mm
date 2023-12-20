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
#import "CoreIPCError.h"
#import "Encoder.h"
#import "MessageSenderInlines.h"
#import "test.h"
#import <Foundation/NSValue.h>
#import <WebCore/FontCocoa.h>
#import <limits.h>
#import <pal/spi/cocoa/ContactsSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/ContactsSoftLink.h>
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#import <pal/cocoa/PassKitSoftLink.h>
#import <pal/mac/DataDetectorsSoftLink.h>

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
        RetainPtr<WebCore::CocoaFont>,
        RetainPtr<NSError>,
        RetainPtr<NSLocale>,
#if ENABLE(DATA_DETECTION)
#if PLATFORM(MAC)
        RetainPtr<WKDDActionContext>,
#endif
        RetainPtr<DDScannerResult>,
#endif
#if USE(AVFOUNDATION)
        RetainPtr<AVOutputContext>,
#endif
        RetainPtr<NSPersonNameComponents>,
#if USE(PASSKIT) && !PLATFORM(WATCHOS)
        RetainPtr<CNPhoneNumber>,
        RetainPtr<CNPostalAddress>,
        RetainPtr<PKContact>,
#endif
        RetainPtr<NSValue>
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

#if PLATFORM(MAC)
inline bool isEqual(WKDDActionContext *a, WKDDActionContext* b)
{
    if (![a.authorNameComponents isEqual:b.authorNameComponents])
        return false;
    if (!CFEqual(a.mainResult, b.mainResult))
        return false;
    if (a.allResults.count != b.allResults.count)
        return false;
    for (size_t i = 0; i < a.allResults.count; ++i) {
        if (!CFEqual((__bridge DDResultRef)a.allResults[i], (__bridge DDResultRef)b.allResults[i]))
            return false;
    }

    if (!NSEqualRects(a.highlightFrame, b.highlightFrame))
        return false;

    return true;
}
#endif

inline bool operator==(const ObjCHolderForTesting& a, const ObjCHolderForTesting& b)
{
    id aObject = a.valueAsID();
    id bObject = b.valueAsID();

    EXPECT_TRUE(aObject != nil);
    EXPECT_TRUE(bObject != nil);

#if PLATFORM(MAC)
    // DDActionContext doesn't have an isEqual: reliable for our unit testing, so do it ourselves.
    if ([aObject isKindOfClass:PAL::getWKDDActionContextClass()] && [bObject isKindOfClass:PAL::getWKDDActionContextClass()])
        return isEqual(aObject, bObject);
#endif

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

/*
    Certificate and private key were generated by running this command:
    openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
    and entering this information:

    Country Name (2 letter code) []:US
    State or Province Name (full name) []:New Mexico
    Locality Name (eg, city) []:Santa Fe
    Organization Name (eg, company) []:Self
    Organizational Unit Name (eg, section) []:Myself
    Common Name (eg, fully qualified host name) []:Me
    Email Address []:me@example.com

    Getting encoded data:
    $ openssl rsa -in key.pem -outform DER | base64
    $ openssl x509 -in cert.pem -outform DER | base64
 */

String privateKeyDataBase64(""
    "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"
    "6I0QC5zps4Kj7g=="_s);

String certDataBase64(""
    "MIIFgDCCA2gCCQC9pt3ltu0TzTANBgkqhkiG9w0BAQsFADCBgTELMAkGA1UEBhMCVVMxEzARBgNVBA"
    "gMCk5ldyBNZXhpY28xETAPBgNVBAcMCFNhbnRhIEZlMQ0wCwYDVQQKDARTZWxmMQ8wDQYDVQQLDAZN"
    "eXNlbGYxCzAJBgNVBAMMAk1lMR0wGwYJKoZIhvcNAQkBFg5tZUBleGFtcGxlLmNvbTAeFw0yMzExMT"
    "AxOTE5NTdaFw0yNDExMDkxOTE5NTdaMIGBMQswCQYDVQQGEwJVUzETMBEGA1UECAwKTmV3IE1leGlj"
    "bzERMA8GA1UEBwwIU2FudGEgRmUxDTALBgNVBAoMBFNlbGYxDzANBgNVBAsMBk15c2VsZjELMAkGA1"
    "UEAwwCTWUxHTAbBgkqhkiG9w0BCQEWDm1lQGV4YW1wbGUuY29tMIICIjANBgkqhkiG9w0BAQEFAAOC"
    "Ag8AMIICCgKCAgEAu4RuNu7ekW19odDzXwgJBtHoQ9p4cy1iuzYiuGaXrvPUP5uR2MpyY9ptp8iHVn"
    "1tt9zMl1suwuAGXZtI5AZhgnL3prlD8eZIEbcbvN6JwdkzMt4aWwvSooNJTSucMXwpy/vEd8Q7AD/R"
    "bSNVuFtsl0ACuOpwdGCX1Pg+hak2WrPpZ+mUMnTH3AyQH0vnjsiiGo40L7vtpG3fFHwXpY4dI8lEgC"
    "Q3ikxgOFO8qZIfvcZjOt2O0sBZ8i5mgHWKrVJ9iJ+QkbUmEvsz7VsDc0RHC7kzeF0VWGLIkJQ5tIWV"
    "KTNCbWSbqI4Gt3qge80u9bK/37ohdnWOuR3V25lOfvAZeGgLg/Rm6BZiYAl/C6n0Neh1TDhUPLUDXA"
    "LEf1kTP8FUk1477H2dVcg8ry2F5pjYRsJcdhPLChOJEv4UL5acxjYf8iukeHEVslnULXPBHCRcRq63"
    "u10QcEO+MFMnUc+yvPXZCI+P+cJZqo7OzxVxLRNhKvnx21/bqdSVNLymtTazDRVplsaWGH1s+Ol9QB"
    "0Cb2us+liosO43CouytSaGJludU/cEO43DUGOvTdQanaLg8t8AM+FxO7MSo3EO9gDUU/v6HV9GyeWG"
    "39Q7PyQ6zrDQ618CzFzgu0LvHYT6LGxgQun2YXmXQGyC709fSxxDcWh+sUBj6ttkpRUdsntg5tcCAw"
    "EAATANBgkqhkiG9w0BAQsFAAOCAgEAOiLuwjX/Ian4+LNAz984J4F/9w9PiuIPS+GAF9+MPRs0v/VK"
    "xKu4jQJHbDWDugVL/s3WrcmBl/acIpOW1uvuv6anRASiNEgtg9wWdLCgHIh0fGrk3syWFybcGOvj/5"
    "34cewjUsoEbU1Zuh4rf6W4uhHlmdQoWAFYuH6FZI3HubMV9asna63QHwvh+RERGGMqCxspaMSAIhh7"
    "CoIWRQubqgraoRJOGGIITDGk+lr/4aZ24wlUSqVoYR0axNwL2P2E8SN1yn67rOSYBCB6USag9PXK7E"
    "F9wRm02TP+nuqdM/dYLoxCmOgLbsCnu+DrM1HqMrh6mPkpF6dmDazdX/aKVb6Da7E4J0NmG3TQmZeg"
    "JI3IBlwik6OrRprkb/jNmWZEfoCIuvckTGAmnEYmcIRx7nxzpT6KXNdyKaWTEcRkujFqv+mzEpFbaZ"
    "N2Skkd5zvkCB7rSVdVXdAVVF+qHNf5C47RtTt21Fr7qG/miTeOhCakhuKGNGkV6XwQCCPERC3YSF0K"
    "Gz9lE+7wW1g0PBpQBaxC5EIyXlxUQwBo0O/L9ouyKjobVCQB5PPxyKIQbn/NdBXRWnoV6bzxOz04yd"
    "KB60HHk4YvR2oWqd2iJChiDA9k4nb2CvxtfV8MaskXZ+6oKR2LEowlc+KefHOdeTroXmIAN+5u2eBl"
    "vcUc7R4j6cg="_s);

static RetainPtr<SecCertificateRef> createCertificate()
{
    NSData *certData = [[NSData alloc] initWithBase64EncodedString:certDataBase64 options:0];
    auto cert = adoptCF(SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData));
    EXPECT_NOT_NULL(cert);
    return cert;
}

static RetainPtr<SecKeyRef> createPrivateKey()
{
    NSData * keyData = [[NSData alloc] initWithBase64EncodedString:privateKeyDataBase64 options:0];

    NSDictionary *keyAttrs = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
        (id)kSecAttrKeySizeInBits: @4096,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate
    };

    auto privateKey = adoptCF(SecKeyCreateWithData((CFDataRef)keyData, (CFDictionaryRef)keyAttrs, NULL));
    EXPECT_NOT_NULL(privateKey);
    return privateKey;
}

static RetainPtr<NSPersonNameComponents> personNameComponentsForTesting()
{
    auto phoneticComponents = adoptNS([[NSPersonNameComponents alloc] init]);
    phoneticComponents.get().familyName = @"Family";
    phoneticComponents.get().middleName = @"Middle";
    phoneticComponents.get().namePrefix = @"Doctor";
    phoneticComponents.get().givenName = @"Given";
    phoneticComponents.get().nickname = @"Buddy";

    auto components = adoptNS([[NSPersonNameComponents alloc] init]);
    components.get().familyName = @"Familia";
    components.get().middleName = @"Median";
    components.get().namePrefix = @"Physician";
    components.get().givenName = @"Gifted";
    components.get().nickname = @"Pal";
    components.get().phoneticRepresentation = phoneticComponents.get();

    return components;
}

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

    auto runNumberTest = [&](NSNumber *number) {
        ObjCHolderForTesting::ValueType numberVariant;
        numberVariant.emplace<RetainPtr<NSNumber>>(number);
        runTestNS({ numberVariant });
    };

    // NSNumber
    runNumberTest([NSNumber numberWithChar: CHAR_MIN]);
    runNumberTest([NSNumber numberWithUnsignedChar: CHAR_MAX]);
    runNumberTest([NSNumber numberWithShort: SHRT_MIN]);
    runNumberTest([NSNumber numberWithUnsignedShort: SHRT_MAX]);
    runNumberTest([NSNumber numberWithInt: INT_MIN]);
    runNumberTest([NSNumber numberWithUnsignedInt: UINT_MAX]);
    runNumberTest([NSNumber numberWithLong: LONG_MIN]);
    runNumberTest([NSNumber numberWithUnsignedLong: ULONG_MAX]);
    runNumberTest([NSNumber numberWithLongLong: LLONG_MIN]);
    runNumberTest([NSNumber numberWithUnsignedLongLong: ULLONG_MAX]);
    runNumberTest([NSNumber numberWithFloat: 3.14159]);
    runNumberTest([NSNumber numberWithDouble: 9.98989898989]);
    runNumberTest([NSNumber numberWithBool: true]);
    runNumberTest([NSNumber numberWithBool: false]);
    runNumberTest([NSNumber numberWithInteger: NSIntegerMax]);
    runNumberTest([NSNumber numberWithInteger: NSIntegerMin]);
    runNumberTest([NSNumber numberWithUnsignedInteger: NSUIntegerMax]);

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

    // NSError
    RetainPtr<SecCertificateRef> cert = createCertificate();
    RetainPtr<SecKeyRef> key = createPrivateKey();
    RetainPtr<SecIdentityRef> identity = adoptCF(SecIdentityCreate(kCFAllocatorDefault, cert.get(), key.get()));

    NSError *error1 = [NSError errorWithDomain:@"TestWTFDomain" code:1 userInfo:@{ NSLocalizedDescriptionKey : @"TestWTF Error" }];
    runTestNS({ error1 });

    NSError *error2 = [NSError errorWithDomain:@"TestWTFDomain" code:2 userInfo:@{
        NSLocalizedDescriptionKey: @"Localized Description",
        NSLocalizedFailureReasonErrorKey: @"Localized Failure Reason Error",
        NSUnderlyingErrorKey: error1,
    }];
    runTestNS({ error2 });

    NSError *error3 = [NSError errorWithDomain:@"TestUserInfo" code:3 userInfo:@{
        NSLocalizedDescriptionKey: @"Localized Description",
        NSLocalizedFailureReasonErrorKey: @"Localized Failure Reason Error",
        NSUnderlyingErrorKey: error1,
        @"Key1" : @"String value",
        @"Key2" : [NSURL URLWithString:@"https://webkit.org/"],
        @"Key3" : [NSNumber numberWithFloat: 3.14159],
        @"Key4" : @[ @"String in Array"],
        @"Key5" : @{ @"InnerKey1" : [NSNumber numberWithBool: true] },
        @"NSErrorClientCertificateChainKey" : @[ (__bridge id) identity.get(), @10 ]
    }];

    // Test converting SecIdentity objects to SecCertificate objects
    WebKit::CoreIPCError filteredError3 = WebKit::CoreIPCError(error3);
    RetainPtr<id> error4 = filteredError3.toID();
    auto error3Size = [[[error3 userInfo] objectForKey:@"NSErrorClientCertificateChainKey"] count];
    auto error4Size = [[[error4.get() userInfo] objectForKey:@"NSErrorClientCertificateChainKey"] count];
    EXPECT_TRUE(error3Size == (error4Size + 1)); // The @10 in error3 will be filtered out.
    runTestNS({ (NSError *)error4.get() });

    // Test filtering out unexpected types in NSErrorClientCertificateChainKey array
    NSError *error5 = [NSError errorWithDomain:@"TestUserInfoFilter" code:5 userInfo:@{
        @"NSErrorClientCertificateChainKey" : @[ @(65) ]
    }];
    WebKit::CoreIPCError filteredError5 = WebKit::CoreIPCError(error5);
    RetainPtr<id> error6 = filteredError5.toID();
    EXPECT_EQ([[[error6.get() userInfo] objectForKey:@"NSErrorClientCertificateChainKey"] count], (NSUInteger)0);

    // Test that a bad key value type is filtered out
    NSError *error7 = [NSError errorWithDomain:@"TestBadKeyValue" code:7 userInfo:@{
        @"NSErrorClientCertificateChainKey" : @(66)
    }];
    WebKit::CoreIPCError filteredError7 = WebKit::CoreIPCError(error7);
    RetainPtr<id> error8 = filteredError7.toID();
    EXPECT_EQ([[error8.get() userInfo] objectForKey:@"NSErrorClientCertificateChainKey"], nil);

    // Test type checking for NSURLErrorFailingURLPeerTrustErrorKey
    NSError *error9 = [NSError errorWithDomain:@"TestPeerCertificates" code:9 userInfo:@{
        NSURLErrorFailingURLPeerTrustErrorKey : @[ (__bridge id) cert.get() ]
    }];
    WebKit::CoreIPCError filteredError9 = WebKit::CoreIPCError(error9);
    RetainPtr<id> error10 = filteredError9.toID();
    EXPECT_EQ([[error10.get() userInfo] objectForKey:@"NSErrorPeerCertificateChainKey"], nil);
    EXPECT_EQ([[error10.get() userInfo] objectForKey:@"NSURLErrorFailingURLPeerTrustErrorKey" ], nil);

    // Test value for NSErrorPeerCertificateChainKey is expected type (SecCertificate)
    NSError *error11 = [NSError errorWithDomain:@"TestPeerCertificates" code:11 userInfo:@{
        @"NSErrorPeerCertificateChainKey" : @[ (__bridge id) cert.get() ]
    }];
    WebKit::CoreIPCError filteredError11 = WebKit::CoreIPCError(error11);
    RetainPtr<id> error12 = filteredError11.toID();
    EXPECT_EQ([[[error12.get() userInfo] objectForKey:@"NSErrorPeerCertificateChainKey"] count], (NSUInteger)1);
    runTestNS({ (NSError *)error12.get() });

    // NSLocale
    for (NSString* identifier : [NSLocale availableLocaleIdentifiers])
        runTestNS({ [NSLocale localeWithLocaleIdentifier: identifier] });

    // NSPersonNameComponents
    auto components = personNameComponentsForTesting();
    runTestNS({ components.get().phoneticRepresentation });
    runTestNS({ components.get() });

#if USE(PASSKIT) && !PLATFORM(WATCHOS)
    // CNPhoneNumber
    RetainPtr<CNPhoneNumber> phoneNumber = [PAL::getCNPhoneNumberClass() phoneNumberWithDigits:@"4085551234" countryCode:@"us"];
    runTestNS({ phoneNumber.get() });

    // CNPostalAddress
    RetainPtr<CNMutablePostalAddress> address = adoptNS([PAL::getCNMutablePostalAddressClass() new]);
    address.get().street = @"1 Apple Park Way";
    address.get().subLocality = @"Birdland";
    address.get().city = @"Cupertino";
    address.get().subAdministrativeArea = @"Santa Clara County";
    address.get().state = @"California";
    address.get().postalCode = @"95014";
    address.get().country = @"United States of America";
    address.get().ISOCountryCode = @"US";
    address.get().formattedAddress = @"Hello world";
    runTestNS({ address.get() });

    // PKContact
    RetainPtr<PKContact> contact = adoptNS([PAL::getPKContactClass() new]);
    contact.get().name = components.get();
    contact.get().emailAddress = @"admin@webkit.org";
    contact.get().phoneNumber = phoneNumber.get();
    contact.get().postalAddress = address.get();
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    contact.get().supplementarySubLocality = @"City 17";
ALLOW_DEPRECATED_DECLARATIONS_END
    runTestNS({ contact.get() });
#endif // USE(PASSKIT) && !PLATFORM(WATCHOS)


    // CFURL
    // The following URL described is quite nonsensical.
    const UInt8 baseBytes[10] = { 'h', 't', 't', 'p', ':', '/', '/', 0xE2, 0x80, 0x80 };
    auto baseURL = adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, baseBytes, 10, kCFStringEncodingUTF8, nullptr, true));
    const UInt8 compoundBytes[10] = { 'p', 'a', 't', 'h' };
    auto compoundURL = adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, compoundBytes, 4, kCFStringEncodingUTF8, baseURL.get(), true));
    runTestCF({ baseURL.get() });
    runTestCF({ compoundURL.get() });


    auto runValueTest = [&](NSValue *value) {
        ObjCHolderForTesting::ValueType valueVariant;
        valueVariant.emplace<RetainPtr<NSValue>>(value);
        runTestNS({ valueVariant });
    };

    // NSValue, wrapping any of the following classes:
    //   - NSRange
    //   - NSRect
    runValueTest([NSValue valueWithRange:NSMakeRange(1, 2)]);
    runValueTest([NSValue valueWithRect:NSMakeRect(1, 2, 79, 80)]);
}

#if PLATFORM(MAC)

static RetainPtr<DDScannerResult> fakeDataDetectorResultForTesting()
{
    auto scanner = adoptCF(PAL::softLink_DataDetectorsCore_DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    auto stringToScan = CFSTR("webkit.org");
    auto query = adoptCF(PAL::softLink_DataDetectorsCore_DDScanQueryCreateFromString(kCFAllocatorDefault, stringToScan, CFRangeMake(0, CFStringGetLength(stringToScan))));
    if (!PAL::softLink_DataDetectorsCore_DDScannerScanQuery(scanner.get(), query.get()))
        return nil;

    auto results = adoptCF(PAL::softLink_DataDetectorsCore_DDScannerCopyResultsWithOptions(scanner.get(), DDScannerCopyResultsOptionsNoOverlap));
    if (!CFArrayGetCount(results.get()))
        return nil;

    return [[PAL::getDDScannerResultClass() resultsFromCoreResults:results.get()] firstObject];
}

TEST(IPCSerialization, SecureCoding)
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

    // DDScannerResult
    //   - Note: For now, there's no reasonable way to create anything but an empty DDScannerResult object
    auto scannerResult = fakeDataDetectorResultForTesting();
    runTestNS({ scannerResult.get() });

    // DDActionContext/DDSecureActionContext
    auto actionContext = adoptNS([PAL::allocWKDDActionContextInstance() init]);
    [actionContext setAllResults:@[ (__bridge id)scannerResult.get().coreResult ]];
    [actionContext setMainResult:scannerResult.get().coreResult];
    auto components = personNameComponentsForTesting();
    [actionContext setAuthorNameComponents:components.get()];
    [actionContext setHighlightFrame:NSMakeRect(1, 2, 3, 4)];

    runTestNS({ actionContext.get() });

    // AVOutputContext
#if USE(AVFOUNDATION)
    RetainPtr<AVOutputContext> outputContext = adoptNS([[PAL::getAVOutputContextClass() alloc] init]);
    runTestNS({ outputContext.get() });
#endif // USE(AVFOUNDATION)
}

#endif // PLATFORM(MAC)

