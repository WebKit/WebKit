/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "CoreIPCError.h"
#import "Encoder.h"
#import "MessageSenderInlines.h"
#import "test.h"
#import <CoreVideo/CoreVideo.h>
#import <Foundation/NSValue.h>
#import <Security/Security.h>
#import <WebCore/AttributedString.h>
#import <WebCore/CVUtilities.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/FontCocoa.h>
#import <WebCore/IOSurface.h>
#import <limits.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/ContactsSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/ContactsSoftLink.h>
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#import <pal/cocoa/PassKitSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>
#import <pal/mac/DataDetectorsSoftLink.h>

// This test makes it trivial to test round trip encoding and decoding of a particular object type.
// The primary focus here is Objective-C and similar types - Objects that exist on the platform or in
// runtime that the WebKit project doesn't have direct control of.
// To test a new ObjC type:
// 1 - Add that type to ObjCHolderForTesting's ValueType variant
// 2 - Run a test exercising that type

@interface NSURLProtectionSpace (WebKitNSURLProtectionSpace)
- (void)_setServerTrust:(SecTrustRef)serverTrust;
- (void)_setDistinguishedNames:(NSArray<NSData *> *)distinguishedNames;
@end

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
    auto decoder = IPC::Decoder::create(encoder->span(), encoder->releaseAttachments());
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
        WTF::switchOn(value, [&] (std::nullptr_t) {
            result = nullptr;
        }, [&](auto&& arg) {
            result = arg.get();
        });
        return result;
    }

    using ValueType = std::variant<
        std::nullptr_t,
        RetainPtr<CFArrayRef>,
        RetainPtr<CFBooleanRef>,
        RetainPtr<CFCharacterSetRef>,
        RetainPtr<CFDataRef>,
        RetainPtr<CFDateRef>,
        RetainPtr<CFDictionaryRef>,
        RetainPtr<CFNullRef>,
        RetainPtr<CFNumberRef>,
        RetainPtr<CFStringRef>,
        RetainPtr<CFURLRef>,
        RetainPtr<CVPixelBufferRef>,
        RetainPtr<CGColorRef>,
        RetainPtr<CGColorSpaceRef>,
        RetainPtr<SecCertificateRef>,
#if USE(SEC_ACCESS_CONTROL)
        RetainPtr<SecAccessControlRef>,
#endif
#if HAVE(SEC_KEYCHAIN)
        RetainPtr<SecKeychainItemRef>,
#endif
#if HAVE(SEC_ACCESS_CONTROL)
        RetainPtr<SecAccessControlRef>,
#endif
        RetainPtr<SecTrustRef>
    >;

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

namespace IPC {

template<> struct ArgumentCoder<CFHolderForTesting> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const CFHolderForTesting& holder)
    {
        holder.encode(encoder);
    }
    static std::optional<CFHolderForTesting> decode(Decoder& decoder)
    {
        return CFHolderForTesting::decode(decoder);
    }
};

}

static bool cvPixelBufferRefsEqual(CVPixelBufferRef pixelBuffer1, CVPixelBufferRef pixelBuffer2)
{
    CVPixelBufferLockBaseAddress(pixelBuffer1, 0);
    CVPixelBufferLockBaseAddress(pixelBuffer2, 0);
    size_t pixelBuffer1Size = CVPixelBufferGetDataSize(pixelBuffer1);
    size_t pixelBuffer2Size = CVPixelBufferGetDataSize(pixelBuffer2);
    if (pixelBuffer1Size != pixelBuffer2Size)
        return false;
    auto base1 = CVPixelBufferGetBaseAddress(pixelBuffer1);
    auto base2 = CVPixelBufferGetBaseAddress(pixelBuffer2);
    bool areEqual = !memcmp(base1, base2, pixelBuffer1Size);
    CVPixelBufferUnlockBaseAddress(pixelBuffer1, 0);
    CVPixelBufferUnlockBaseAddress(pixelBuffer2, 0);
    return areEqual;
}

static bool secTrustRefsEqual(SecTrustRef trust1, SecTrustRef trust2)
{
    // SecTrust doesn't compare equal after round-tripping through SecTrustSerialize/SecTrustDeserialize <rdar://122051396>
    // Therefore, we compare all the attributes we can access to verify equality.
    SecKeyRef pk1 = SecTrustCopyPublicKey(trust1);
    SecKeyRef pk2 = SecTrustCopyPublicKey(trust2);
    EXPECT_TRUE(pk1);
    EXPECT_TRUE(pk2);
    bool equal = CFEqual(pk1, pk2);
    CFRelease(pk1);
    CFRelease(pk2);
    EXPECT_TRUE(equal);
    if (!equal)
        return false;

    equal = (SecTrustGetCertificateCount(trust1) == SecTrustGetCertificateCount(trust2));
    EXPECT_TRUE(equal);
    if (!equal)
        return false;

    CFDataRef ex1 = SecTrustCopyExceptions(trust1);
    CFDataRef ex2 = SecTrustCopyExceptions(trust2);
    EXPECT_TRUE(ex1);
    EXPECT_TRUE(ex2);
    equal = CFEqual(ex1, ex2);
    CFRelease(ex1);
    CFRelease(ex2);
    EXPECT_TRUE(equal);
    if (!equal)
        return false;

    CFArrayRef array1, array2;
    EXPECT_TRUE(SecTrustCopyPolicies(trust1, &array1) == errSecSuccess);
    EXPECT_TRUE(SecTrustCopyPolicies(trust2, &array2) == errSecSuccess);
    equal = CFEqual(array1, array2);
    CFRelease(array1);
    CFRelease(array2);
    EXPECT_TRUE(equal);
    if (!equal)
        return false;

    EXPECT_TRUE(SecTrustCopyPolicies(trust1, &array1) == errSecSuccess);
    EXPECT_TRUE(SecTrustCopyPolicies(trust2, &array2) == errSecSuccess);
    equal = CFEqual(array1, array2);
    CFRelease(array1);
    CFRelease(array2);
    EXPECT_TRUE(equal);
    if (!equal)
        return false;

#if HAVE(SECTRUST_COPYPROPERTIES)
    array1 = SecTrustCopyProperties(trust1);
    array2 = SecTrustCopyProperties(trust2);
    EXPECT_TRUE(array1);
    EXPECT_TRUE(array2);
    equal = CFEqual(array1, array2);
    CFRelease(array1);
    CFRelease(array2);
    EXPECT_TRUE(equal);
    if (!equal)
        return false;
#endif

    Boolean bool1, bool2;
    EXPECT_TRUE(SecTrustGetNetworkFetchAllowed(trust1, &bool1) == errSecSuccess);
    EXPECT_TRUE(SecTrustGetNetworkFetchAllowed(trust2, &bool2) == errSecSuccess);
    equal = (bool1 == bool2);
    EXPECT_TRUE(equal);
    if (!equal)
        return false;

    SecTrustResultType result1, result2;
    EXPECT_TRUE(SecTrustGetTrustResult(trust1, &result1) == errSecSuccess);
    EXPECT_TRUE(SecTrustGetTrustResult(trust2, &result2) == errSecSuccess);
    equal = (result1 == result2);
    EXPECT_TRUE(equal);

    return equal;
}

CFHolderForTesting cfHolder(CFTypeRef type)
{
    if (!type)
        return { nullptr };
    CFTypeID typeID = CFGetTypeID(type);
    if (typeID == CFArrayGetTypeID())
        return { (CFArrayRef)type };
    if (typeID == CFBooleanGetTypeID())
        return { (CFBooleanRef)type };
    if (typeID == CFCharacterSetGetTypeID())
        return { (CFCharacterSetRef)type };
    if (typeID == CFDataGetTypeID())
        return { (CFDataRef)type };
    if (typeID == CFDateGetTypeID())
        return { (CFDateRef)type };
    if (typeID == CFDictionaryGetTypeID())
        return { (CFDictionaryRef)type };
    if (typeID == CFNullGetTypeID())
        return { }; // FIXME: Test CFNullRef.
    if (typeID == CFNumberGetTypeID())
        return { (CFNumberRef)type };
    if (typeID == CFStringGetTypeID())
        return { (CFStringRef)type };
    if (typeID == CFURLGetTypeID())
        return { (CFURLRef)type };
    if (typeID == CVPixelBufferGetTypeID())
        return { (CVPixelBufferRef)type };
    if (typeID == CGColorSpaceGetTypeID())
        return { (CGColorSpaceRef)type };
    if (typeID == CGColorGetTypeID())
        return { (CGColorRef)type };
    if (typeID == SecCertificateGetTypeID())
        return { (SecCertificateRef)type };
#if HAVE(SEC_KEYCHAIN)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (typeID == SecKeychainItemGetTypeID())
        return { (SecKeychainItemRef)type };
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    if (typeID == SecAccessControlGetTypeID())
        return { (SecAccessControlRef)type };
#endif
    if (typeID == SecTrustGetTypeID())
        return { (SecTrustRef)type };
    ASSERT_NOT_REACHED();
    return { };
}

bool arraysEqual(CFArrayRef, CFArrayRef);
bool dictionariesEqual(CFDictionaryRef, CFDictionaryRef);

inline bool operator==(const CFHolderForTesting& a, const CFHolderForTesting& b)
{
    auto aObject = a.valueAsCFType();
    auto bObject = b.valueAsCFType();

    if (!aObject && !bObject)
        return true;

    EXPECT_TRUE(aObject);
    EXPECT_TRUE(bObject);

    if (CFEqual(aObject, bObject))
        return true;

    // Sometimes the CF input and CF output fail the CFEqual call above (Such as CFDictionaries containing certain things)
    // In these cases, give the Obj-C equivalent equality check a chance.
    if ([(NSObject *)aObject isEqual: (NSObject *)bObject])
        return true;

    auto aTypeID = CFGetTypeID(aObject);
    auto bTypeID = CFGetTypeID(bObject);

    if (aTypeID == CVPixelBufferGetTypeID() && bTypeID == CVPixelBufferGetTypeID())
        return cvPixelBufferRefsEqual((CVPixelBufferRef)aObject, (CVPixelBufferRef)bObject);

    if (aTypeID == SecTrustGetTypeID() && bTypeID == SecTrustGetTypeID())
        return secTrustRefsEqual((SecTrustRef)aObject, (SecTrustRef)bObject);

    if (aTypeID == CFArrayGetTypeID() && bTypeID == CFArrayGetTypeID())
        return arraysEqual((CFArrayRef)aObject, (CFArrayRef)bObject);

    if (aTypeID == CFDictionaryGetTypeID() && bTypeID == CFDictionaryGetTypeID())
        return dictionariesEqual((CFDictionaryRef)aObject, (CFDictionaryRef)bObject);

    return false;
}

inline bool operator!=(const CFHolderForTesting& a, const CFHolderForTesting& b)
{
    return !(a == b);
}

bool arraysEqual(CFArrayRef a, CFArrayRef b)
{
    auto aCount = CFArrayGetCount(a);
    auto bCount = CFArrayGetCount(b);
    if (aCount != bCount)
        return false;
    for (CFIndex i = 0; i < aCount; i++) {
        if (cfHolder(CFArrayGetValueAtIndex(a, i)) != cfHolder(CFArrayGetValueAtIndex(b, i)))
            return false;
    }
    return true;
}

bool dictionariesEqual(CFDictionaryRef a, CFDictionaryRef b)
{
    auto aCount = CFDictionaryGetCount(a);
    auto bCount = CFDictionaryGetCount(b);
    if (aCount != bCount)
        return false;

    struct Context {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        Context(bool& result, CFDictionaryRef b)
            : result(result)
            , b(b) { }
        bool& result;
        CFDictionaryRef b;
        CFIndex keyCount { 0 };
    };
    bool result { true };
    auto context = makeUnique<Context>(result, b);
    CFDictionaryApplyFunction(a, [](CFTypeRef key, CFTypeRef value, void* voidContext) {
        auto& context = *static_cast<Context*>(voidContext);
        if (cfHolder(CFDictionaryGetValue(context.b, key)) != cfHolder(value))
            context.result = false;
        context.keyCount++;
    }, context.get());
    return context->keyCount == aCount;
}

struct ObjCHolderForTesting {
    void encode(IPC::Encoder&) const;
    static std::optional<ObjCHolderForTesting> decode(IPC::Decoder&);

    id valueAsID() const
    {
        id result;
        WTF::switchOn(value, [&] (std::nullptr_t) {
            result = nil;
        }, [&](auto&& arg) {
            result = arg.get();
        });
        return result;
    }

    typedef std::variant<
        std::nullptr_t,
        RetainPtr<NSDate>,
        RetainPtr<NSString>,
        RetainPtr<NSURL>,
        RetainPtr<NSData>,
        RetainPtr<NSNumber>,
        RetainPtr<NSArray>,
        RetainPtr<NSDictionary>,
        RetainPtr<WebCore::CocoaFont>,
        RetainPtr<NSError>,
        RetainPtr<NSNull>,
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
        RetainPtr<NSPresentationIntent>,
        RetainPtr<NSURLProtectionSpace>,
        RetainPtr<NSURLRequest>,
        RetainPtr<NSURLCredential>,
#if USE(PASSKIT)
        RetainPtr<CNContact>,
        RetainPtr<CNPhoneNumber>,
        RetainPtr<CNPostalAddress>,
        RetainPtr<NSDateComponents>,
        RetainPtr<PKContact>,
        RetainPtr<PKDateComponentsRange>,
        RetainPtr<PKPaymentMerchantSession>,
        RetainPtr<PKPaymentMethod>,
        RetainPtr<PKPaymentToken>,
        RetainPtr<PKShippingMethod>,
        RetainPtr<PKPayment>,
#endif
        RetainPtr<NSShadow>,
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

namespace IPC {

template<> struct ArgumentCoder<ObjCHolderForTesting> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const ObjCHolderForTesting& holder)
    {
        holder.encode(encoder);
    }
    static std::optional<ObjCHolderForTesting> decode(Decoder& decoder)
    {
        return ObjCHolderForTesting::decode(decoder);
    }
};

}

@interface NSDateComponents ()
-(BOOL)oldIsEqual:(id)other;
@end

static BOOL nsDateComponentsTesting_isEqual(NSDateComponents *a, SEL, NSDateComponents *b)
{
    // Override the equality check of NSDateComponents objects to only look at the identifier string
    // values for the NSCalendar and NSTimeZone objects.
    // Instances of those objects can be configured "weirdly" at runtime and have undesirable isEqual: behaviors,
    // but in practice for WebKit CoreIPC only the default values for each are important.

    NSCalendar *aCalendar = a.calendar;
    NSCalendar *bCalendar = b.calendar;
    NSTimeZone *aTimeZone = a.timeZone;
    NSTimeZone *bTimeZone = b.timeZone;

    a.calendar = nil;
    a.timeZone = nil;
    b.calendar = nil;
    b.timeZone = nil;

    BOOL result = [a oldIsEqual:b];

    if (aCalendar && result)
        result = [aCalendar.calendarIdentifier isEqual:bCalendar.calendarIdentifier];

    if (aTimeZone && result)
        result = [aTimeZone.name isEqual:bTimeZone.name];

    a.calendar = aCalendar;
    a.timeZone = aTimeZone;
    b.calendar = bCalendar;
    b.timeZone = bTimeZone;

    return result;
}

#if PLATFORM(MAC)
static BOOL wkDDActionContext_isEqual(WKDDActionContext *a, SEL, WKDDActionContext *b)
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

static bool wkNSURLProtectionSpace_isEqual(NSURLProtectionSpace *a, SEL, NSURLProtectionSpace* b)
{
    if (![a.host isEqual: b.host]) {
        if (!(a.host == nil && b.host == nil))
            return false;
    }
    if (!(a.port == b.port))
        return false;
    if (![a.protocol isEqual:b.protocol])
        return false;
    if (!([a.realm isEqual:b.realm] || (!a.realm && a.realm == b.realm)))
        return false;
    if (![a.authenticationMethod isEqual:b.authenticationMethod])
        return false;
    if (!([a.distinguishedNames isEqual:b.distinguishedNames] || (!a.distinguishedNames && a.distinguishedNames == b.distinguishedNames)))
        return false;
    if (!((!a.serverTrust && a.serverTrust == b.serverTrust) || secTrustRefsEqual(a.serverTrust, a.serverTrust)))
        return false;

    return true;
}

#if USE(PASSKIT)
static bool CNPostalAddressTesting_isEqual(CNPostalAddress *a, CNPostalAddress *b)
{
    // CNPostalAddress treats a nil formattedAddress and empty formattedAddress the same for equality.
    // But, there's other behavior with CNPostalAddress where nil-vs-empty makes a critical difference.
    // To regression test our IPC of the object, we explicitly make sure that "nil goes in, nil comes out"
    if (a.formattedAddress && !b.formattedAddress)
        return false;
    if (!a.formattedAddress && b.formattedAddress)
        return false;

    return [a isEqual:b];
}
#endif

static bool NSURLCredentialTesting_isEqual(NSURLCredential *a, NSURLCredential *b)
{
    if (a.persistence != b.persistence)
        return false;
    if (a.user && ![a.user isEqualToString:b.user])
        return false;
    if (a.password && ![a.password isEqualToString:b.password])
        return false;
    if (a.hasPassword != b.hasPassword)
        return false;
    if (a.certificates.count != b.certificates.count)
        return false;
    return true;
}

#if USE(PASSKIT)
static BOOL wkSecureCoding_isEqual(id a, SEL, id b)
{
    RetainPtr<WKKeyedCoder> aCoder = adoptNS([WKKeyedCoder new]);
    RetainPtr<WKKeyedCoder> bCoder = adoptNS([WKKeyedCoder new]);

    [a encodeWithCoder:aCoder.get()];
    [b encodeWithCoder:bCoder.get()];

    return [[aCoder accumulatedDictionary] isEqual:[bCoder accumulatedDictionary]];
}
#endif // USE(PASSKIT)

inline bool operator==(const ObjCHolderForTesting& a, const ObjCHolderForTesting& b)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // These classes do not have isEqual: methods useful for our unit testing, so we'll swap it in ourselves.

        auto oldIsEqual = class_getMethodImplementation([NSDateComponents class], @selector(isEqual:));
        class_replaceMethod([NSDateComponents class], @selector(isEqual:), (IMP)nsDateComponentsTesting_isEqual, "v@:@");
        class_addMethod([NSDateComponents class], @selector(oldIsEqual:), oldIsEqual, "v@:@");

        auto oldIsEqual2 = class_getMethodImplementation([NSURLProtectionSpace class], @selector(isEqual:));
        class_replaceMethod([NSURLProtectionSpace class], @selector(isEqual:), (IMP)wkNSURLProtectionSpace_isEqual, "v@:@");
        class_addMethod([NSURLProtectionSpace class], @selector(oldIsEqual:), oldIsEqual2, "v@:@");

#if USE(PASSKIT)
        class_addMethod(PAL::getPKPaymentMethodClass(), @selector(isEqual:), (IMP)wkSecureCoding_isEqual, "v@:@");
        class_addMethod(PAL::getPKPaymentTokenClass(), @selector(isEqual:), (IMP)wkSecureCoding_isEqual, "v@:@");
        class_addMethod(PAL::getPKDateComponentsRangeClass(), @selector(isEqual:), (IMP)wkSecureCoding_isEqual, "v@:@");
        class_addMethod(PAL::getPKShippingMethodClass(), @selector(isEqual:), (IMP)wkSecureCoding_isEqual, "v@:@");
        class_addMethod(PAL::getPKPaymentClass(), @selector(isEqual:), (IMP)wkSecureCoding_isEqual, "v@:@");
#endif
#if ENABLE(DATA_DETECTION) && PLATFORM(MAC)
        class_addMethod(PAL::getWKDDActionContextClass(), @selector(isEqual:), (IMP)wkDDActionContext_isEqual, "v@:@");
#endif
    });

    id aObject = a.valueAsID();
    id bObject = b.valueAsID();

    if (!aObject && !bObject)
        return true;

    EXPECT_TRUE(aObject != nil);
    EXPECT_TRUE(bObject != nil);

#if USE(PASSKIT)
    if ([aObject isKindOfClass:PAL::getCNPostalAddressClass()])
        return CNPostalAddressTesting_isEqual(aObject, bObject);
#endif

    if ([aObject isKindOfClass:NSURLCredential.class])
        return NSURLCredentialTesting_isEqual(aObject, bObject);

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
    "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"
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

#if HAVE(SEC_KEYCHAIN)
static SecKeychainRef getTempKeychain()
{
    SecKeychainRef keychainRef = NULL;
    uuid_t uu;
    char pass[PATH_MAX];

    uuid_generate_random(uu);
    uuid_unparse_upper(uu, pass);
    UInt32 passLen = (UInt32)strlen(pass);
    NSString* path = [NSString stringWithFormat:@"%@/%s", NSTemporaryDirectory(), pass];

    [NSFileManager.defaultManager removeItemAtPath:path error:NULL];
    EXPECT_TRUE(SecKeychainCreate(path.fileSystemRepresentation, passLen, pass, false, NULL, &keychainRef) == errSecSuccess);
    EXPECT_NOT_NULL(keychainRef);
    EXPECT_TRUE(SecKeychainUnlock(keychainRef, passLen, pass, TRUE) == errSecSuccess);

    return keychainRef;
}

static void destroyTempKeychain(SecKeychainRef keychainRef)
{
    if (keychainRef) {
        SecKeychainDelete(keychainRef);
        CFRelease(keychainRef);
    }
}
#endif // HAVE(SEC_KEYCHAIN)

#if USE(PASSKIT)
static RetainPtr<CNMutablePostalAddress> postalAddressForTesting()
{
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
    return address;
}

static RetainPtr<PKContact> pkContactForTesting()
{
    RetainPtr<PKContact> contact = adoptNS([PAL::getPKContactClass() new]);
    contact.get().name = personNameComponentsForTesting().get();
    contact.get().emailAddress = @"admin@webkit.org";
    contact.get().phoneNumber = [PAL::getCNPhoneNumberClass() phoneNumberWithDigits:@"4085551234" countryCode:@"us"];
    contact.get().postalAddress = postalAddressForTesting().get();
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    contact.get().supplementarySubLocality = @"City 17";
ALLOW_DEPRECATED_DECLARATIONS_END

    return contact;
}
#endif // USE(PASSKIT)

static void runTestNS(ObjCHolderForTesting&& holderArg)
{
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

static void runTestCFWithExpectedResult(const CFHolderForTesting& holderArg, const CFHolderForTesting& expectedResult)
{
    __block bool done = false;
    __block CFHolderForTesting holder = expectedResult;
    auto sender = SerializationTestSender { };
    sender.sendWithAsyncReplyWithoutUsingIPCConnection(CFPingBackMessage(holderArg), ^(CFHolderForTesting&& result) {
        EXPECT_TRUE(holder == result);
        done = true;
    });

    // The completion handler should be called synchronously, so this should be true already.
    EXPECT_TRUE(done);
};

static void runTestCF(const CFHolderForTesting& holderArg)
{
    runTestCFWithExpectedResult(holderArg, holderArg);
};

TEST(IPCSerialization, Basic)
{
    // CVPixelBuffer
    auto s1 = WebCore::IOSurface::create(nullptr, { 5, 5 }, WebCore::DestinationColorSpace::SRGB());
    auto pixelBuffer = WebCore::createCVPixelBuffer(s1->surface());

    runTestCF({ pixelBuffer->get() });

    // NSString/CFString
    runTestNS({ @"Hello world" });
    runTestCF({ CFSTR("Hello world") });

    // NSURL/CFURL
    NSURL *url = [NSURL URLWithString:@"https://webkit.org/"];
    runTestNS({ url });
    runTestCF({ (__bridge CFURLRef)url });

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

    // CGColorSpace
    runTestCF({ sRGBColorSpace.get() });
    auto grayColorSpace = adoptCF(CGColorSpaceCreateDeviceGray());
    runTestCF({ grayColorSpace.get() });

    auto runNumberTest = [&](NSNumber *number) {
        ObjCHolderForTesting::ValueType numberVariant;
        numberVariant.emplace<RetainPtr<NSNumber>>(number);
        runTestNS({ numberVariant });
    };

    // CFCharacterSet
    RetainPtr characterSet = CFCharacterSetGetPredefined(kCFCharacterSetWhitespaceAndNewline);
    runTestCF({ characterSet.get() });

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
    runTestNS({ @[ @"Array test", @1, @{ @"hello": @9 }, @[ @"Another", @3, @"array"], url, [NSData dataWithBytes:"Data test" length:strlen("Data test")] ] });

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
        @"Key2" : url,
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
    components.get().namePrefix = nil;
    runTestNS({ components.get() });
    components.get().givenName = nil;
    runTestNS({ components.get() });
    components.get().middleName = nil;
    runTestNS({ components.get() });
    components.get().familyName = nil;
    runTestNS({ components.get() });
    components.get().nickname = nil;
    runTestNS({ components.get() });

#if USE(PASSKIT)
    // CNPhoneNumber
    // Digits must be non-null at init-time, but countryCode can be null.
    // However, Contacts will calculate a default country code if you pass in a null one,
    // so testing encode/decode of such an instance is pointless.
    RetainPtr<CNPhoneNumber> phoneNumber = [PAL::getCNPhoneNumberClass() phoneNumberWithDigits:@"4085551234" countryCode:@"us"];
    runTestNS({ phoneNumber.get() });

    // CNPostalAddress
    RetainPtr<CNMutablePostalAddress> address = postalAddressForTesting();
    runTestNS({ address.get() });
    address.get().formattedAddress = nil;
    runTestNS({ address.get() });

    // PKContact
    auto pkContact = pkContactForTesting();
    runTestNS({ pkContact.get() });
    pkContact.get().name = nil;
    runTestNS({ pkContact.get() });
    pkContact.get().postalAddress = nil;
    runTestNS({ pkContact.get() });
    pkContact.get().phoneNumber = nil;
    runTestNS({ pkContact.get() });
    pkContact.get().emailAddress = nil;
    runTestNS({ pkContact.get() });
    pkContact.get().supplementarySubLocality = nil;
    runTestNS({ pkContact.get() });
#endif // USE(PASSKIT)


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


    // SecTrust -- evaluate the trust of the cert created above
    SecTrustRef trustRef = NULL;
    auto policy = adoptCF(SecPolicyCreateBasicX509());
    EXPECT_TRUE(SecTrustCreateWithCertificates(cert.get(), policy.get(), &trustRef) == errSecSuccess);
    EXPECT_NOT_NULL(trustRef);
    auto trust = adoptCF(trustRef);
    runTestCF({ trust.get() });

    EXPECT_TRUE(SecTrustEvaluateWithError(trust.get(), NULL) == errSecSuccess);
    runTestCF({ trust.get() });

#if HAVE(SEC_ACCESS_CONTROL)
    NSDictionary *protection = @{
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrSynchronizable : @(NO),
        (id)kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly : @(YES),
        (id)kSecAttrAccessibleWhenUnlocked: @(YES) };
    SecAccessControlCreateFlags flags = (kSecAccessControlDevicePasscode | kSecAccessControlBiometryAny | kSecAccessControlOr);
    auto accessControlRef = adoptCF(SecAccessControlCreateWithFlags(kCFAllocatorDefault, (CFTypeRef)protection, flags, NULL));
    EXPECT_NOT_NULL(accessControlRef);
    runTestCF({ accessControlRef.get() });
#endif // HAVE(SEC_ACCESS_CONTROL)

    // SecKeychainItem
#if HAVE(SEC_KEYCHAIN)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Store the certificate created above into the temp keychain
    SecKeychainRef tempKeychain = getTempKeychain();
    CFDataRef certData = NULL;
    EXPECT_TRUE(SecItemExport(cert.get(), kSecFormatX509Cert, kSecItemPemArmour, nil, &certData) == errSecSuccess);
    CFArrayRef itemsPtr = NULL;
    EXPECT_TRUE(SecKeychainItemImport(certData, CFSTR(".pem"), NULL, NULL, 0, NULL, tempKeychain, &itemsPtr) == errSecSuccess);
    EXPECT_NOT_NULL(itemsPtr);
    auto items = adoptCF(itemsPtr);
    EXPECT_GT(CFArrayGetCount(items.get()), 0);

    SecKeychainItemRef keychainItemRef = (SecKeychainItemRef)CFArrayGetValueAtIndex(items.get(), 0);
    EXPECT_NOT_NULL(keychainItemRef);
    runTestCF({ keychainItemRef });

    CFRelease(certData);

    destroyTempKeychain(tempKeychain);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif // HAVE(SEC_KEYCHAIN)

    NSArray *nestedArray = @[
        @YES,
        @(5.4),
        NSData.data,
        NSDate.now,
        NSNull.null,
        url,
        (id)sRGBColorSpace.get(),
        (id)cgColor.get(),
        (id)trust.get(),
        (id)cert.get(),
        (id)characterSet.get(),
#if HAVE(SEC_KEYCHAIN)
        (id)keychainItemRef,
#endif
#if HAVE(SEC_ACCESS_CONTROL)
        (id)accessControlRef.get(),
#endif
        @{ @"key": NSNull.null }
    ];

    runTestCFWithExpectedResult({ (__bridge CFArrayRef)@[
        nestedArray,
        (id)trust.get(),
        NSUUID.UUID, // Removed when encoding because CFUUIDRef is not a recognized type in CFArrayRef or CFDictionaryRef
        @{
            @"should be removed before encoding" : NSUUID.UUID,
            NSUUID.UUID : @"should also be removed before encoding"
        }
    ] }, { (__bridge CFArrayRef)@[
        nestedArray,
        (id)trust.get(),
        @{ }
    ] });

    runTestCFWithExpectedResult({ (__bridge CFDictionaryRef) @{
        @"key" : nestedArray,
        @"key2" : nestedArray,
        NSNull.null : NSData.data,
        url : (id)trust.get(),
        @"should be removed before encoding" : NSUUID.UUID,
        NSUUID.UUID : @"should also be removed before encoding",
    } }, { (__bridge CFDictionaryRef) @{
        @"key" : nestedArray,
        @"key2" : nestedArray,
        NSNull.null : NSData.data,
        url : (id)trust.get()
    } });

    // NSPresentationIntent
    NSInteger intentID = 1;
    NSPresentationIntent *paragraphIntent = [NSPresentationIntent paragraphIntentWithIdentity:intentID++ nestedInsideIntent:nil];
    runTestNS({ paragraphIntent });

    NSPresentationIntent *headingIntent = [NSPresentationIntent headerIntentWithIdentity:intentID++ level:1 nestedInsideIntent:nil];
    runTestNS({ headingIntent });

    NSPresentationIntent *codeBlockIntent = [NSPresentationIntent codeBlockIntentWithIdentity:intentID++ languageHint:@"Swift" nestedInsideIntent:paragraphIntent];
    runTestNS({ codeBlockIntent });

    NSPresentationIntent *thematicBreakIntent = [NSPresentationIntent thematicBreakIntentWithIdentity:intentID++ nestedInsideIntent:nil];
    runTestNS({ thematicBreakIntent });

    NSPresentationIntent *orderedListIntent = [NSPresentationIntent orderedListIntentWithIdentity:intentID++ nestedInsideIntent:paragraphIntent];
    runTestNS({ orderedListIntent });

    NSPresentationIntent *unorderedListIntent = [NSPresentationIntent unorderedListIntentWithIdentity:intentID++ nestedInsideIntent:paragraphIntent];
    runTestNS({ unorderedListIntent });

    NSPresentationIntent *listItemIntent = [NSPresentationIntent listItemIntentWithIdentity:intentID++ ordinal:1 nestedInsideIntent:orderedListIntent];
    runTestNS({ listItemIntent });

    NSPresentationIntent *blockQuoteIntent = [NSPresentationIntent blockQuoteIntentWithIdentity:intentID++ nestedInsideIntent:paragraphIntent];
    runTestNS({ blockQuoteIntent });

    NSPresentationIntent *tableIntent = [NSPresentationIntent tableIntentWithIdentity:intentID++ columnCount:2 alignments:@[@(NSPresentationIntentTableColumnAlignmentLeft), @(NSPresentationIntentTableColumnAlignmentRight)] nestedInsideIntent:unorderedListIntent];
    runTestNS({ tableIntent });

    NSPresentationIntent *tableHeaderRowIntent = [NSPresentationIntent tableHeaderRowIntentWithIdentity:intentID++ nestedInsideIntent:tableIntent];
    runTestNS({ tableHeaderRowIntent });

    NSPresentationIntent *tableRowIntent = [NSPresentationIntent tableRowIntentWithIdentity:intentID++ row:1 nestedInsideIntent:tableIntent];
    runTestNS({ tableRowIntent });

    NSPresentationIntent *tableCellIntent = [NSPresentationIntent tableCellIntentWithIdentity:intentID++ column:1 nestedInsideIntent:tableRowIntent];
    runTestNS({ tableCellIntent });

    runTestNS({ NSNull.null });
    runTestCF({ kCFNull });
    runTestNS({ nil });
    runTestCF({ nullptr });

    // NSURLProtectionSpace
    RetainPtr<NSURLProtectionSpace> protectionSpace = adoptNS([[NSURLProtectionSpace alloc] initWithHost:@"127.0.0.1" port:8000 protocol:NSURLProtectionSpaceHTTP realm:@"testrealm" authenticationMethod:NSURLAuthenticationMethodHTTPBasic]);
    runTestNS({ protectionSpace.get() });

    RetainPtr<NSURLProtectionSpace> protectionSpace2 = adoptNS([[NSURLProtectionSpace alloc] initWithHost:@"127.0.0.1" port:443 protocol:NSURLProtectionSpaceHTTPS realm:nil authenticationMethod:NSURLAuthenticationMethodServerTrust]);
    NSData *distinguishedNamesData = [NSData dataWithBytes:"AAAA" length:4];
    NSArray *distinguishedNames = @[distinguishedNamesData];
    [protectionSpace2.get() _setServerTrust:trustRef];
    [protectionSpace2.get() _setDistinguishedNames:distinguishedNames];
    runTestNS({ protectionSpace2.get() });

    NSString* nilString = nil;
    RetainPtr<NSURLProtectionSpace> protectionSpace3 = adoptNS([[NSURLProtectionSpace alloc] initWithHost:nilString port:443 protocol:NSURLProtectionSpaceHTTPS realm:nil authenticationMethod:NSURLAuthenticationMethodServerTrust]);
    [protectionSpace3.get() _setServerTrust:nil];
    [protectionSpace3.get() _setDistinguishedNames:nil];
    runTestNS({ protectionSpace3.get() });

    runTestNS({ [NSURLCredential credentialForTrust:trust.get()] });
#if HAVE(DICTIONARY_SERIALIZABLE_NSURLCREDENTIAL)
    runTestNS({ [NSURLCredential credentialWithIdentity:identity.get() certificates:@[(id)cert.get()] persistence:NSURLCredentialPersistencePermanent] });
    runTestNS({ [NSURLCredential credentialWithIdentity:identity.get() certificates:nil persistence:NSURLCredentialPersistenceForSession] });
#endif
    runTestNS({ [NSURLCredential credentialWithUser:@"user" password:@"password" persistence:NSURLCredentialPersistenceSynchronizable] });
}

TEST(IPCSerialization, NSShadow)
{
    auto runTestNSShadow = [&](CGSize shadowOffset, CGFloat shadowBlurRadius, PlatformColor *shadowColor) {
        RetainPtr<NSShadow> shadow = adoptNS([[PlatformNSShadow alloc] init]);
        [shadow setShadowOffset:shadowOffset];
        [shadow setShadowBlurRadius:shadowBlurRadius];
        [shadow setShadowColor:shadowColor];
        runTestNS({ shadow.get() });
    };

    runTestNSShadow({ 5.7, 10.5 }, 0.49, nil);

    RetainPtr<PlatformColor> platformColor = cocoaColor(WebCore::Color::blue);
    runTestNSShadow({ 10.5, 5.7 }, 0.79, platformColor.get());
}

TEST(IPCSerialization, NSURLRequest)
{
    RetainPtr url = [NSURL URLWithString:@"https://webkit.org/"];
    RetainPtr urlRequest = [NSURLRequest requestWithURL:url.get()];
    runTestNS({ urlRequest });

    NSDictionary *jsonDict = @{
        @"a" : @1,
        @"b" : @2
    };
    RetainPtr postData = [NSJSONSerialization dataWithJSONObject:jsonDict options:0 error:Nil];
    RetainPtr postRequest = [[NSMutableURLRequest alloc] initWithURL:url.get()];

    [postRequest setValue:@"application/json" forHTTPHeaderField:@"Accept"];
    [postRequest setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [postRequest setHTTPMethod:@"POST"];
    [postRequest setHTTPBody:postData.get()];
    [postRequest _setPrivacyProxyFailClosed:YES];

    runTestNS({ postRequest.get() });

    url = nil;
    urlRequest = [NSURLRequest requestWithURL:url.get()];
    runTestNS({ urlRequest });
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

@interface PKPaymentMerchantSession ()
- (instancetype)initWithMerchantIdentifier:(NSString *)merchantIdentifier
                 merchantSessionIdentifier:(NSString *)merchantSessionIdentifier
                                     nonce:(NSString *)nonce
                            epochTimestamp:(NSUInteger)epochTimestamp
                                 expiresAt:(NSUInteger)expiresAt
                               displayName:(NSString *)displayName
                         initiativeContext:(NSString *)initiativeContext
                                initiative:(NSString *)initiative
                      ampEnrollmentPinning:(NSData *)ampEnrollmentPinning
            operationalAnalyticsIdentifier:(NSString *)operationalAnalyticsIdentifier
                              signedFields:(NSArray<NSString *> *)signedFields
                                 signature:(NSData *)signature;

- (instancetype)initWithMerchantIdentifier:(NSString *)merchantIdentifier
                 merchantSessionIdentifier:(NSString *)merchantSessionIdentifier
                                     nonce:(NSString *)nonce
                            epochTimestamp:(NSUInteger)epochTimestamp
                                 expiresAt:(NSUInteger)expiresAt
                                    domain:(NSString *)domainName
                               displayName:(NSString *)displayName
            operationalAnalyticsIdentifier:(NSString *)operationalAnalyticsIdentifier
                                 signature:(NSData *)signature;
@end

TEST(IPCSerialization, SecureCoding)
{
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

    // PKPaymentMerchantSession
    // This initializer doesn't exercise retryNonce or domain
    RetainPtr<PKPaymentMerchantSession> session = adoptNS([[PAL::getPKPaymentMerchantSessionClass() alloc]
        initWithMerchantIdentifier:@"WebKit Open Source Project"
        merchantSessionIdentifier:@"WebKitMerchantSession"
        nonce:@"WebKitNonce"
        epochTimestamp:1000000000
        expiresAt:2000000000
        displayName:@"WebKit"
        initiativeContext:@"WebKit IPC Testing"
        initiative:@"WebKit Regression Test Suite"
        ampEnrollmentPinning:nil
        operationalAnalyticsIdentifier:@"WebKitOperations42"
        signedFields:@[ @"FirstField", @"AndTheSecond" ]
        signature:[NSData new]]);
    runTestNS({ session.get() });

    // This initializer adds in domain, but retryNonce is still unexercised
    session = adoptNS([[PAL::getPKPaymentMerchantSessionClass() alloc]
        initWithMerchantIdentifier:@"WebKit Open Source Project"
        merchantSessionIdentifier:@"WebKitMerchantSession"
        nonce:@"WebKitNonce"
        epochTimestamp:1000000000
        expiresAt:2000000000
        domain:@"webkit.org"
        displayName:@"WebKit"
        operationalAnalyticsIdentifier:@"WebKitOperations42"
        signature:[NSData new]]);
    runTestNS({ session.get() });

    RetainPtr<CNPostalAddress> address = postalAddressForTesting();
    RetainPtr<CNLabeledValue> labeledPostalAddress = adoptNS([[PAL::getCNLabeledValueClass() alloc] initWithLabel:@"Work" value:address.get()]);

    RetainPtr<CNLabeledValue> labeledEmailAddress = adoptNS([[PAL::getCNLabeledValueClass() alloc] initWithLabel:@"WorkSPAM" value:@"spam@webkit.org"]);

    RetainPtr<CNMutableContact> billingContact = adoptNS([PAL::getCNMutableContactClass() new]);
    billingContact.get().contactType = CNContactTypePerson;
    billingContact.get().namePrefix = @"Mrs";
    billingContact.get().givenName = @"WebKit";
    billingContact.get().middleName = @"von";
    billingContact.get().familyName = @"WebKittington";
    billingContact.get().nameSuffix = @"The Third";
    billingContact.get().organizationName = @"WebKit";
    billingContact.get().jobTitle = @"Web Kitten";
    billingContact.get().note = @"The Coolest Kitten out there";
    billingContact.get().postalAddresses = @[ labeledPostalAddress.get() ];
    billingContact.get().emailAddresses = @[ labeledEmailAddress.get() ];
    runTestNS({ billingContact.get() });

    RetainPtr<PKPaymentMethod> paymentMethod = adoptNS([PAL::getPKPaymentMethodClass() new]);
    paymentMethod.get().displayName = @"WebKitPay";
    paymentMethod.get().network = @"WebKitCard";
    paymentMethod.get().type = PKPaymentMethodTypeCredit;
    paymentMethod.get().billingAddress = billingContact.get();

    runTestNS({ paymentMethod.get() });

    RetainPtr<PKPaymentToken> paymentToken = adoptNS([PAL::getPKPaymentTokenClass() new]);
    paymentToken.get().paymentMethod = paymentMethod.get();
    paymentToken.get().transactionIdentifier = @"WebKitTXIdentifier";
    paymentToken.get().paymentData = [NSData new];
    paymentToken.get().redeemURL = [NSURL URLWithString:@"https://webkit.org/"];
    paymentToken.get().retryNonce = @"ANonce";

    runTestNS({ paymentToken.get() });

    RetainPtr<NSDateComponents> startComponents = adoptNS([NSDateComponents new]);
    runTestNS({ startComponents.get() });

    [startComponents setValue:1 forComponent:NSCalendarUnitEra];
    [startComponents setValue:2 forComponent:NSCalendarUnitYear];
    [startComponents setValue:3 forComponent:NSCalendarUnitYearForWeekOfYear];
    [startComponents setValue:4 forComponent:NSCalendarUnitQuarter];
    [startComponents setValue:5 forComponent:NSCalendarUnitMonth];
    [startComponents setValue:6 forComponent:NSCalendarUnitHour];
    [startComponents setValue:7 forComponent:NSCalendarUnitMinute];
    [startComponents setValue:8 forComponent:NSCalendarUnitSecond];
    [startComponents setValue:9 forComponent:NSCalendarUnitNanosecond];
    [startComponents setValue:10 forComponent:NSCalendarUnitWeekOfYear];
    [startComponents setValue:11 forComponent:NSCalendarUnitWeekOfMonth];
    [startComponents setValue:12 forComponent:NSCalendarUnitWeekday];
    [startComponents setValue:13 forComponent:NSCalendarUnitWeekdayOrdinal];
    [startComponents setValue:14 forComponent:NSCalendarUnitDay];
    startComponents.get().calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierBuddhist];
    startComponents.get().timeZone = [NSTimeZone timeZoneWithName:@"Asia/Calcutta"];
    runTestNS({ startComponents.get() });

    startComponents = adoptNS([NSDateComponents new]);
    startComponents.get().day = 1;
    startComponents.get().month = 4;
    startComponents.get().year = 1976;
    startComponents.get().calendar = NSCalendar.currentCalendar;

    RetainPtr<NSDateComponents> endComponents = adoptNS([NSDateComponents new]);
    endComponents.get().day = 9;
    endComponents.get().month = 1;
    endComponents.get().year = 2007;
    endComponents.get().calendar = NSCalendar.currentCalendar;

    runTestNS({ endComponents.get() });

    RetainPtr<PKDateComponentsRange> dateRange = adoptNS([[PAL::getPKDateComponentsRangeClass() alloc] initWithStartDateComponents:startComponents.get() endDateComponents:endComponents.get()]);

    runTestNS({ dateRange.get() });

    RetainPtr<PKShippingMethod> shippingMethod = adoptNS([PAL::getPKShippingMethodClass() new]);
    shippingMethod.get().identifier = @"WebKitPostalService";
    shippingMethod.get().detail = @"Ships in 1 to 2 bugzillas";
    shippingMethod.get().dateComponentsRange = dateRange.get();

    runTestNS({ shippingMethod.get() });

    RetainPtr<PKPayment> payment = adoptNS([PAL::getPKPaymentClass() new]);
    payment.get().token = paymentToken.get();
    payment.get().billingContact = pkContactForTesting().get();
    payment.get().shippingContact = pkContactForTesting().get();
    payment.get().shippingMethod = shippingMethod.get();

    runTestNS({ payment.get() });
}

#endif // PLATFORM(MAC)

