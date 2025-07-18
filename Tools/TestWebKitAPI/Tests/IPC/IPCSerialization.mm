/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#import "CoreIPCCFDictionary.h"
#import "CoreIPCError.h"
#import "CoreIPCPlistDictionary.h"
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

    using ValueType = Variant<
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
#if HAVE(SEC_KEYCHAIN)
        RetainPtr<SecKeychainItemRef>,
#endif
        RetainPtr<SecAccessControlRef>,
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
    CFErrorRef error = NULL;
    bool equal;
#if HAVE(WK_SECURE_CODING_SECTRUST)
    CFPropertyListRef trust1Plist = SecTrustCopyPropertyListRepresentation(trust1, &error);
    EXPECT_FALSE(error);
    CFPropertyListRef trust2Plist = SecTrustCopyPropertyListRepresentation(trust2, &error);
    EXPECT_FALSE(error);
    EXPECT_TRUE(CFGetTypeID(trust1Plist) == CFDictionaryGetTypeID());
    EXPECT_TRUE(CFGetTypeID(trust2Plist) == CFDictionaryGetTypeID());
    equal = CFEqual(trust1Plist, trust2Plist);
    EXPECT_TRUE(equal);
    CFRelease(trust1Plist);
    CFRelease(trust2Plist);
#endif

    SecKeyRef pk1 = SecTrustCopyPublicKey(trust1);
    SecKeyRef pk2 = SecTrustCopyPublicKey(trust2);
    EXPECT_TRUE(pk1);
    EXPECT_TRUE(pk2);
    equal = CFEqual(pk1, pk2);
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

    CFPropertyListFormat format;
    CFPropertyListRef ex1plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault,
        ex1,
        kCFPropertyListImmutable,
        &format,
        &error
    );
    EXPECT_FALSE(error);
    CFPropertyListRef ex2plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault,
        ex2,
        kCFPropertyListImmutable,
        &format,
        &error
    );
    EXPECT_FALSE(error);
    equal = CFEqual(ex1plist, ex2plist);
    CFRelease(ex1);
    CFRelease(ex2);
    CFRelease(ex1plist);
    CFRelease(ex2plist);
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

#if HAVE(SECTRUST_COPYPROPERTIES)
    array1 = SecTrustCopyProperties(trust1);
    array2 = SecTrustCopyProperties(trust2);
    if (array1 && array2) {
        equal = CFEqual(array1, array2);
        CFRelease(array1);
        CFRelease(array2);
        EXPECT_TRUE(equal);
        if (!equal)
            return false;
    }
    bool onlyOneIsNil = (!array1 && array2) || (!array2 && array1);
    EXPECT_FALSE(onlyOneIsNil);
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
    if (typeID == SecAccessControlGetTypeID())
        return { (SecAccessControlRef)type };
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
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(Context);
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

    typedef Variant<
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
        : m_holder(holder)
    {
    }

    template<typename Encoder> void encode(Encoder& encoder)
    {
        encoder << m_holder;
    }

private:
    const ObjCHolderForTesting& m_holder;
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
        : m_holder(holder)
    {
    }

    template<typename Encoder> void encode(Encoder& encoder)
    {
        encoder << m_holder;
    }

private:
    const CFHolderForTesting& m_holder;
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
    RetainPtr certData = adoptNS([[NSData alloc] initWithBase64EncodedString:certDataBase64.createNSString().get() options:0]);
    auto cert = adoptCF(SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData.get()));
    EXPECT_NOT_NULL(cert);
    return cert;
}

static RetainPtr<SecKeyRef> createPrivateKey()
{
    RetainPtr keyData = adoptNS([[NSData alloc] initWithBase64EncodedString:privateKeyDataBase64.createNSString().get() options:0]);

    NSDictionary *keyAttrs = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
        (id)kSecAttrKeySizeInBits: @4096,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate
    };

    auto privateKey = adoptCF(SecKeyCreateWithData((CFDataRef)keyData.get(), (CFDictionaryRef)keyAttrs, NULL));
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

TEST(IPCSerialization, Plist)
{
    NSDictionary *plist = @{
        @"key1": @"A String",
        @"key2": @1.0,
        @"key3": @[
            @{
                @"date": NSDate.now,
                @"number": @2.0,
                @"string": @"A String"
            },
        ],
        @"key4": [NSData dataWithBytes:"Data test" length:strlen("Data test")]
    };

    auto p = WebKit::CoreIPCPlistDictionary(plist);
    auto d = p.toID();
    EXPECT_TRUE([d.get() isEqual:plist]);
}

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

    NSDictionary *protection = @{
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrSynchronizable : @(NO),
        (id)kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly : @(YES),
        (id)kSecAttrAccessibleWhenUnlocked: @(YES) };
    SecAccessControlCreateFlags flags = (kSecAccessControlDevicePasscode | kSecAccessControlBiometryAny | kSecAccessControlOr);
    auto accessControlRef = adoptCF(SecAccessControlCreateWithFlags(kCFAllocatorDefault, (CFTypeRef)protection, flags, NULL));
    EXPECT_NOT_NULL(accessControlRef);
    runTestCF({ accessControlRef.get() });

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
        (id)accessControlRef.get(),
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
#if HAVE(DICTIONARY_SERIALIZABLE_NSURLCREDENTIAL) && !HAVE(WK_SECURE_CODING_NSURLCREDENTIAL)
    runTestNS({ [NSURLCredential credentialWithIdentity:identity.get() certificates:@[(id)cert.get()] persistence:NSURLCredentialPersistencePermanent] });
    runTestNS({ [NSURLCredential credentialWithIdentity:identity.get() certificates:nil persistence:NSURLCredentialPersistenceForSession] });
#endif
    runTestNS({ [NSURLCredential credentialWithUser:@"user" password:@"password" persistence:NSURLCredentialPersistenceSynchronizable] });
}

#if HAVE(WK_SECURE_CODING_SECTRUST)
String cert1(""
    "MIIHezCCBmOgAwIBAgIQfrZYqgaHdOKdEb5ZVqa0LTANBgkqhkiG9w0BAQsFADBRMQswCQYDVQQGEw"
    "JVUzETMBEGA1UEChMKQXBwbGUgSW5jLjEtMCsGA1UEAxMkQXBwbGUgUHVibGljIEVWIFNlcnZlciBS"
    "U0EgQ0EgMiAtIEcxMB4XDTI0MTIwOTE3NTcwNFoXDTI1MDQwODE5NTY1NlowgccxHTAbBgNVBA8MFF"
    "ByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYBBAGCNzwCAQMTAlVTMRswGQYLKwYBBAGCNzwCAQIM"
    "CkNhbGlmb3JuaWExETAPBgNVBAUTCEMwODA2NTkyMQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaW"
    "Zvcm5pYTESMBAGA1UEBwwJQ3VwZXJ0aW5vMRMwEQYDVQQKDApBcHBsZSBJbmMuMRYwFAYDVQQDDA13"
    "d3cuYXBwbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxlSqT8ZVN6y8/a3TDd"
    "V9zwNyhjeCH7AEckmzN810eAKJ/079QHFI+5KisoP9Uuu0W/dWO/NVFSH6cf6zA6I1qvewAkkaocJE"
    "TuO+OKosPXWZpAGFUcoT1j098EQy2OCLD5DfYYGYZDIFqGzGw9jgxAfuMcIcPmHn0PbS6wX/ePTze5"
    "kFzNTlc5//w+UtIGHi3QP327Urzyi2rjIGOWBjjKDdvqdYQl+7Sv1QKKBZpWlinNqe5POugwkOYQLb"
    "E3MwjaVDN/iF7CE005avSxPo4G8vNKaq9VGdQuJb0qzI1M2gTD9G86oeFh5AZ0/erTAe+NshueHXK3"
    "tX13JLNjodlwIDAQABo4ID1jCCA9IwDAYDVR0TAQH/BAIwADAfBgNVHSMEGDAWgBRQVatDoa+pSCta"
    "waKHiQTkeg7K2jB6BggrBgEFBQcBAQRuMGwwMgYIKwYBBQUHMAKGJmh0dHA6Ly9jZXJ0cy5hcHBsZS"
    "5jb20vYXBldnNyc2EyZzEuZGVyMDYGCCsGAQUFBzABhipodHRwOi8vb2NzcC5hcHBsZS5jb20vb2Nz"
    "cDAzLWFwZXZzcnNhMmcxMDEwPAYDVR0RBDUwM4IQd3d3LmFwcGxlLmNvbS5jboINd3d3LmFwcGxlLm"
    "NvbYIQaW1hZ2VzLmFwcGxlLmNvbTBgBgNVHSAEWTBXMEgGBWeBDAEBMD8wPQYIKwYBBQUHAgEWMWh0"
    "dHBzOi8vd3d3LmFwcGxlLmNvbS9jZXJ0aWZpY2F0ZWF1dGhvcml0eS9wdWJsaWMwCwYJYIZIAYb9bA"
    "IBMBMGA1UdJQQMMAoGCCsGAQUFBwMBMDUGA1UdHwQuMCwwKqAooCaGJGh0dHA6Ly9jcmwuYXBwbGUu"
    "Y29tL2FwZXZzcnNhMmcxLmNybDAdBgNVHQ4EFgQURk32jbOK8tOTKUM09x18GFD6T6MwDgYDVR0PAQ"
    "H/BAQDAgWgMA8GCSqGSIb3Y2QGVgQCBQAwggH3BgorBgEEAdZ5AgQCBIIB5wSCAeMB4QB3AH1ZHhLh"
    "eCp7HGFnfF79+NCHXBSgTpWeuQMv2Q6MLnm4AAABk6yafEsAAAQDAEgwRgIhAOb4XXNkF7XCVLpkJY"
    "YXdIKiD+Tziy4e0v4d+0dqXSZiAiEA5pyC6BSpmzVasV2UZLgc5Efc/JrvEJsA8gGQyeDcKZQAdgDM"
    "+w9qhXEJZf6Vm1PO6bJ8IumFXA2XjbapflTA/kwNsAAAAZOsmnxzAAAEAwBHMEUCIQDgvdW/qcpks4"
    "kJ8s0p3L+Dbk2oFW8WoX6Y0QTbQZ3AqAIgQKsyTTgEX7+nBebYXO9P95TnNFfZzZ8j6O3yNNBTZh4A"
    "dgBOdaMnXJoQwzhbbNTfP1LrHfDgjhuNacCx+mSxYpo53wAAAZOsmnxNAAAEAwBHMEUCIG5b5kcJPY"
    "mr4IYZaC0YpHwUv+qbZz8D8+PPxsfu8nGaAiEA/gFI1QZrlKSxFWnzLVNJxbnSfUxK78RfvmzAUJTu"
    "9poAdgDgkrP8DB3I52g2H95huZZNClJ4GYpy1nLEsE2lbW9UBAAAAZOsmnxnAAAEAwBHMEUCIQCz0K"
    "gSqiTLe9nviiPOBcnKhvfyN33UpL6gx2Ot9NF6oAIgUgLXXf+hys90XZnVumvz5omAQ8zK1iSzkLtw"
    "GaJx6dIwDQYJKoZIhvcNAQELBQADggEBAGZWm3BMjUDhPBgvkopxXuQreRAzJmEWlD+cJdoHFYhsdf"
    "TSIbSvO3kEsKoBzxVZPeDopZTTfqH+XLDHu46HCXYLUjEmHZi3gwd93cu8WytJc0O3Vb9HbeSChgMi"
    "7q4zcNpzqYS9m6+z4sD6I3hCVV8iv7dqoZulTh1g9MD9bOql9eEW8LvryY6qu8JeDuha/eYU6lGJwl"
    "eS/MTE0gr+TxmY6N0bv7xYtSlEBLXSElz7VFcq7GqsrApEqoVkk24oOpfQ4xeEDhvulyDtEOW0DT8D"
    "l+UjuDVM264DL03VGGv1bidxXqf1ZtsLYiVfIFz+7GNj9xjKL+6JR7ir/XGGvyY="_s);

String cert2(""
    "MIIFMjCCBBqgAwIBAgIQBxd5EQBdImf2iJL2j4tQWDANBgkqhkiG9w0BAQsFADBsMQswCQYDVQQGEw"
    "JVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSswKQYD"
    "VQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5jZSBFViBSb290IENBMB4XDTIwMDQyOTEyNTQ1MFoXDT"
    "MwMDQxMDIzNTk1OVowUTELMAkGA1UEBhMCVVMxEzARBgNVBAoTCkFwcGxlIEluYy4xLTArBgNVBAMT"
    "JEFwcGxlIFB1YmxpYyBFViBTZXJ2ZXIgUlNBIENBIDIgLSBHMTCCASIwDQYJKoZIhvcNAQEBBQADgg"
    "EPADCCAQoCggEBAOIA/aXfX7k4cUnrupPYw00z3FwU6nAvwepTO8ueUcBUsmQGppcx5BeEyngvZ8PS"
    "ieH0GHHK7RnHbgLChyon2H9EpgYo7NQ1yrcC5THvo3VrlAP6U746ORSDxUbbv4z15kAsyvABUCFi8S"
    "7IXkzDIjhOICNrA8fXUpUKbIccI2JvMz7Rvw5GeG7caa2u+vSI3TmBnwMcjVqlsScqY6tbE/ji7C/X"
    "Dw7wUpMHyaQMVGPO7mJfi0/QbiUPWwnCJPYAqPpvBVjeBh0avUCGaP2ZtZc2Jns1C8h9ebJG+Z3awd"
    "gBqQPYD2I+fy/aBtnTOkhnBJti8jxh1ThNV65S9SucZecCAwEAAaOCAekwggHlMB0GA1UdDgQWBBRQ"
    "VatDoa+pSCtawaKHiQTkeg7K2jAfBgNVHSMEGDAWgBSxPsNpA/i/RwHUmCYaCALvY2QrwzAOBgNVHQ"
    "8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8C"
    "AQAwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wSw"
    "YDVR0fBEQwQjBAoD6gPIY6aHR0cDovL2NybDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0SGlnaEFzc3Vy"
    "YW5jZUVWUm9vdENBLmNybDCB3AYDVR0gBIHUMIHRMIHFBglghkgBhv1sAgEwgbcwKAYIKwYBBQUHAg"
    "EWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwgYoGCCsGAQUFBwICMH4MfEFueSB1c2Ugb2Yg"
    "dGhpcyBDZXJ0aWZpY2F0ZSBjb25zdGl0dXRlcyBhY2NlcHRhbmNlIG9mIHRoZSBSZWx5aW5nIFBhcn"
    "R5IEFncmVlbWVudCBsb2NhdGVkIGF0IGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9ycGEtdWEwBwYF"
    "Z4EMAQEwDQYJKoZIhvcNAQELBQADggEBAKZebFC2ZVwrTj+u6nDo3O03e0/g/hN+6U5iA7X9dBGmQx"
    "3C7NkPNAV0mUoaklsceIBIQ/bC7utdgwnSKTnm5HdVipASyLloU7TP2jAtDQdAxBavmLnFwcwXBp6n"
    "17uLp+uPU4DZgubM96LyUQilUlYERbgu66rCK18jRmobDvFT8E71oU13o1Oe/1WUHFbTynRkKW73JD"
    "d2rZ21Pim7LEJVY3OcRmtYNHaM/lunYx1ZQ+0fw7Hc5J/xR7vlRiuyP+fJ9ucuDYupLg333Di5R7JZ"
    "IfnX42ecX0Dd0wIeuFj0HBjH6c25FUov/Fa5Zjr0VPjmmgN6PnoMArUZXDkQe3M="_s);

String chain1(""
    "MIIHezCCBmOgAwIBAgIQfrZYqgaHdOKdEb5ZVqa0LTANBgkqhkiG9w0BAQsFADBRMQswCQYDVQQGEw"
    "JVUzETMBEGA1UEChMKQXBwbGUgSW5jLjEtMCsGA1UEAxMkQXBwbGUgUHVibGljIEVWIFNlcnZlciBS"
    "U0EgQ0EgMiAtIEcxMB4XDTI0MTIwOTE3NTcwNFoXDTI1MDQwODE5NTY1NlowgccxHTAbBgNVBA8MFF"
    "ByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYBBAGCNzwCAQMTAlVTMRswGQYLKwYBBAGCNzwCAQIM"
    "CkNhbGlmb3JuaWExETAPBgNVBAUTCEMwODA2NTkyMQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaW"
    "Zvcm5pYTESMBAGA1UEBwwJQ3VwZXJ0aW5vMRMwEQYDVQQKDApBcHBsZSBJbmMuMRYwFAYDVQQDDA13"
    "d3cuYXBwbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxlSqT8ZVN6y8/a3TDd"
    "V9zwNyhjeCH7AEckmzN810eAKJ/079QHFI+5KisoP9Uuu0W/dWO/NVFSH6cf6zA6I1qvewAkkaocJE"
    "TuO+OKosPXWZpAGFUcoT1j098EQy2OCLD5DfYYGYZDIFqGzGw9jgxAfuMcIcPmHn0PbS6wX/ePTze5"
    "kFzNTlc5//w+UtIGHi3QP327Urzyi2rjIGOWBjjKDdvqdYQl+7Sv1QKKBZpWlinNqe5POugwkOYQLb"
    "E3MwjaVDN/iF7CE005avSxPo4G8vNKaq9VGdQuJb0qzI1M2gTD9G86oeFh5AZ0/erTAe+NshueHXK3"
    "tX13JLNjodlwIDAQABo4ID1jCCA9IwDAYDVR0TAQH/BAIwADAfBgNVHSMEGDAWgBRQVatDoa+pSCta"
    "waKHiQTkeg7K2jB6BggrBgEFBQcBAQRuMGwwMgYIKwYBBQUHMAKGJmh0dHA6Ly9jZXJ0cy5hcHBsZS"
    "5jb20vYXBldnNyc2EyZzEuZGVyMDYGCCsGAQUFBzABhipodHRwOi8vb2NzcC5hcHBsZS5jb20vb2Nz"
    "cDAzLWFwZXZzcnNhMmcxMDEwPAYDVR0RBDUwM4IQd3d3LmFwcGxlLmNvbS5jboINd3d3LmFwcGxlLm"
    "NvbYIQaW1hZ2VzLmFwcGxlLmNvbTBgBgNVHSAEWTBXMEgGBWeBDAEBMD8wPQYIKwYBBQUHAgEWMWh0"
    "dHBzOi8vd3d3LmFwcGxlLmNvbS9jZXJ0aWZpY2F0ZWF1dGhvcml0eS9wdWJsaWMwCwYJYIZIAYb9bA"
    "IBMBMGA1UdJQQMMAoGCCsGAQUFBwMBMDUGA1UdHwQuMCwwKqAooCaGJGh0dHA6Ly9jcmwuYXBwbGUu"
    "Y29tL2FwZXZzcnNhMmcxLmNybDAdBgNVHQ4EFgQURk32jbOK8tOTKUM09x18GFD6T6MwDgYDVR0PAQ"
    "H/BAQDAgWgMA8GCSqGSIb3Y2QGVgQCBQAwggH3BgorBgEEAdZ5AgQCBIIB5wSCAeMB4QB3AH1ZHhLh"
    "eCp7HGFnfF79+NCHXBSgTpWeuQMv2Q6MLnm4AAABk6yafEsAAAQDAEgwRgIhAOb4XXNkF7XCVLpkJY"
    "YXdIKiD+Tziy4e0v4d+0dqXSZiAiEA5pyC6BSpmzVasV2UZLgc5Efc/JrvEJsA8gGQyeDcKZQAdgDM"
    "+w9qhXEJZf6Vm1PO6bJ8IumFXA2XjbapflTA/kwNsAAAAZOsmnxzAAAEAwBHMEUCIQDgvdW/qcpks4"
    "kJ8s0p3L+Dbk2oFW8WoX6Y0QTbQZ3AqAIgQKsyTTgEX7+nBebYXO9P95TnNFfZzZ8j6O3yNNBTZh4A"
    "dgBOdaMnXJoQwzhbbNTfP1LrHfDgjhuNacCx+mSxYpo53wAAAZOsmnxNAAAEAwBHMEUCIG5b5kcJPY"
    "mr4IYZaC0YpHwUv+qbZz8D8+PPxsfu8nGaAiEA/gFI1QZrlKSxFWnzLVNJxbnSfUxK78RfvmzAUJTu"
    "9poAdgDgkrP8DB3I52g2H95huZZNClJ4GYpy1nLEsE2lbW9UBAAAAZOsmnxnAAAEAwBHMEUCIQCz0K"
    "gSqiTLe9nviiPOBcnKhvfyN33UpL6gx2Ot9NF6oAIgUgLXXf+hys90XZnVumvz5omAQ8zK1iSzkLtw"
    "GaJx6dIwDQYJKoZIhvcNAQELBQADggEBAGZWm3BMjUDhPBgvkopxXuQreRAzJmEWlD+cJdoHFYhsdf"
    "TSIbSvO3kEsKoBzxVZPeDopZTTfqH+XLDHu46HCXYLUjEmHZi3gwd93cu8WytJc0O3Vb9HbeSChgMi"
    "7q4zcNpzqYS9m6+z4sD6I3hCVV8iv7dqoZulTh1g9MD9bOql9eEW8LvryY6qu8JeDuha/eYU6lGJwl"
    "eS/MTE0gr+TxmY6N0bv7xYtSlEBLXSElz7VFcq7GqsrApEqoVkk24oOpfQ4xeEDhvulyDtEOW0DT8D"
    "l+UjuDVM264DL03VGGv1bidxXqf1ZtsLYiVfIFz+7GNj9xjKL+6JR7ir/XGGvyY="_s);

String chain2(""
    "MIIFMjCCBBqgAwIBAgIQBxd5EQBdImf2iJL2j4tQWDANBgkqhkiG9w0BAQsFADBsMQswCQYDVQQGEw"
    "JVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSswKQYD"
    "VQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5jZSBFViBSb290IENBMB4XDTIwMDQyOTEyNTQ1MFoXDT"
    "MwMDQxMDIzNTk1OVowUTELMAkGA1UEBhMCVVMxEzARBgNVBAoTCkFwcGxlIEluYy4xLTArBgNVBAMT"
    "JEFwcGxlIFB1YmxpYyBFViBTZXJ2ZXIgUlNBIENBIDIgLSBHMTCCASIwDQYJKoZIhvcNAQEBBQADgg"
    "EPADCCAQoCggEBAOIA/aXfX7k4cUnrupPYw00z3FwU6nAvwepTO8ueUcBUsmQGppcx5BeEyngvZ8PS"
    "ieH0GHHK7RnHbgLChyon2H9EpgYo7NQ1yrcC5THvo3VrlAP6U746ORSDxUbbv4z15kAsyvABUCFi8S"
    "7IXkzDIjhOICNrA8fXUpUKbIccI2JvMz7Rvw5GeG7caa2u+vSI3TmBnwMcjVqlsScqY6tbE/ji7C/X"
    "Dw7wUpMHyaQMVGPO7mJfi0/QbiUPWwnCJPYAqPpvBVjeBh0avUCGaP2ZtZc2Jns1C8h9ebJG+Z3awd"
    "gBqQPYD2I+fy/aBtnTOkhnBJti8jxh1ThNV65S9SucZecCAwEAAaOCAekwggHlMB0GA1UdDgQWBBRQ"
    "VatDoa+pSCtawaKHiQTkeg7K2jAfBgNVHSMEGDAWgBSxPsNpA/i/RwHUmCYaCALvY2QrwzAOBgNVHQ"
    "8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8C"
    "AQAwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wSw"
    "YDVR0fBEQwQjBAoD6gPIY6aHR0cDovL2NybDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0SGlnaEFzc3Vy"
    "YW5jZUVWUm9vdENBLmNybDCB3AYDVR0gBIHUMIHRMIHFBglghkgBhv1sAgEwgbcwKAYIKwYBBQUHAg"
    "EWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwgYoGCCsGAQUFBwICMH4MfEFueSB1c2Ugb2Yg"
    "dGhpcyBDZXJ0aWZpY2F0ZSBjb25zdGl0dXRlcyBhY2NlcHRhbmNlIG9mIHRoZSBSZWx5aW5nIFBhcn"
    "R5IEFncmVlbWVudCBsb2NhdGVkIGF0IGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9ycGEtdWEwBwYF"
    "Z4EMAQEwDQYJKoZIhvcNAQELBQADggEBAKZebFC2ZVwrTj+u6nDo3O03e0/g/hN+6U5iA7X9dBGmQx"
    "3C7NkPNAV0mUoaklsceIBIQ/bC7utdgwnSKTnm5HdVipASyLloU7TP2jAtDQdAxBavmLnFwcwXBp6n"
    "17uLp+uPU4DZgubM96LyUQilUlYERbgu66rCK18jRmobDvFT8E71oU13o1Oe/1WUHFbTynRkKW73JD"
    "d2rZ21Pim7LEJVY3OcRmtYNHaM/lunYx1ZQ+0fw7Hc5J/xR7vlRiuyP+fJ9ucuDYupLg333Di5R7JZ"
    "IfnX42ecX0Dd0wIeuFj0HBjH6c25FUov/Fa5Zjr0VPjmmgN6PnoMArUZXDkQe3M="_s);

String chain3(""
    "MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBsMQswCQYDVQQGEw"
    "JVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSswKQYD"
    "VQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5jZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDT"
    "MxMTExMDAwMDAwMFowbDELMAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UE"
    "CxMQd3d3LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2UgRVYgUm"
    "9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm+9S75S0tMqbf5YE/yc0l"
    "SbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTWPNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtq"
    "umX8OkhPhPYlG++MXs2ziS4wblCJEMxChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0A"
    "FFoEt7oT61EKmEFBIk5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZO"
    "E3hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsgEsxBu24LUTi4"
    "S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFLE+w2"
    "kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaAFLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3"
    "DQEBBQUAA4IBAQAcGgaX3NecnzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMU"
    "u4kehDLI6zeM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jFhS9O"
    "MPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2Yzi9RKR/5CYrCsSXaQ"
    "3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCevEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8"
    "nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep+OkuE6N36B9K"_s);

String extendedKeyUsage1("VR0lAA=="_s);
String extendedKeyUsage2("KwYBBQUHAwE="_s);
String extendedKeyUsage3("KwYBBAGCNwoDAw=="_s);
String extendedKeyUsage4("YIZIAYb4QgQB"_s);

TEST(IPCSerialization, SecTrustRef)
{
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss Z"];
    [dateFormatter setTimeZone:[NSTimeZone timeZoneWithAbbreviation:@"UTC"]];

    NSDictionary *appleCertificatePlist = @{
        @"anchorsOnly" : @(NO),
        @"certificates" : @[
            [[NSData alloc] initWithBase64EncodedString:cert1.createNSString().get() options:0],
            [[NSData alloc] initWithBase64EncodedString:cert2.createNSString().get() options:0]
        ],
        @"chain" : @[
            [[NSData alloc] initWithBase64EncodedString:chain1.createNSString().get() options:0],
            [[NSData alloc] initWithBase64EncodedString:chain2.createNSString().get() options:0],
            [[NSData alloc] initWithBase64EncodedString:chain3.createNSString().get() options:0],
        ],
        @"details" : @[
            @{ },
            @{ },
            @{ },
        ],
        @"info" : @{
            @"CertificateTransparency" : @(YES),
            @"CompanyName" : @"Apple Inc.",
            @"ExtendedValidation" : @(YES),
            @"Organization" : @"Apple Inc.",
            @"Revocation" : @(YES),
            @"RevocationValidUntil" : [dateFormatter dateFromString:@"2024-12-20 15:15:45 +0000"],
            @"TrustExpirationDate" : [dateFormatter dateFromString:@"2024-12-20 15:15:45 +0000"],
            @"TrustExtendedValidation" : @(YES),
            @"TrustResultNotAfter" : [dateFormatter dateFromString:@"2024-12-20 06:12:43 +0000"],
            @"TrustResultNotBefore" : [dateFormatter dateFromString:@"2024-12-20 03:42:43 +0000"],
            @"TrustRevocationChecked" : @(YES),
        },
        @"keychainsAllowed" : @(YES),
        @"policies" : @[
            @{
                @"SecPolicyOid" : @"1.2.840.113635.100.1.3",
                @"SecPolicyPolicyName" : @"sslServer",
                @"policyOptions" : @{
                    @"BasicConstraints" : @(YES),
                    @"CriticalExtensions" : @(YES),
                    @"DuplicateExtension" : @(YES),
                    @"ExtendedKeyUsage" : @[
                        [NSData new],
                        [[NSData alloc] initWithBase64EncodedString:extendedKeyUsage1.createNSString().get() options:0],
                        [[NSData alloc] initWithBase64EncodedString:extendedKeyUsage2.createNSString().get() options:0],
                        [[NSData alloc] initWithBase64EncodedString:extendedKeyUsage3.createNSString().get() options:0],
                        [[NSData alloc] initWithBase64EncodedString:extendedKeyUsage4.createNSString().get() options:0],
                    ],
                    @"GrayListedLeaf" : @(YES),
                    @"IdLinkage" : @(YES),
                    @"KeySize" : @{
                        @"42" : @(2048),
                        @"73" : @(256)
                    },
                    @"KeyUsage" : @[
                        @(1),
                        @(0)
                    ],
                    @"NonEmptySubject" : @(YES),
                    @"OtherTrustValidityPeriod": @[
                        @[
                            [dateFormatter dateFromString:@"2019-06-30 23:00:00 +0000"],
                            @(71283600)
                        ]
                    ],
                    @"SSLHostname" : @"www.apple.com",
                    @"ServerAuthEKU" : @(YES),
                    @"SignatureHashAlgorithms" : @[
                        @"SignatureDigestMD2",
                        @"SignatureDigestMD4",
                        @"SignatureDigestMD5",
                        @"SignatureDigestSHA1"
                    ],
                    @"SystemTrustValidityPeriod" : @[
                        @[
                            [dateFormatter dateFromString:@"2018-03-01 00:00:00 +0000"],
                            @(71283600)
                        ],
                        @[
                            [dateFormatter dateFromString:@"2020-09-01 00:00:00 +0000"],
                            @(34387200)
                        ]
                    ],
                    @"SystemTrustedCTRequired" : @(YES),
                    @"TemporalValidity" : @(YES),
                    @"UnparseableExtension" : @(YES),
                    @"WeakKeySize" : @(YES),
                    @"WeakSignature" : @(YES)
                }
            }
        ],
        @"result" : @(4),
        @"verifyDate" : [dateFormatter dateFromString:@"2024-12-20 04:57:08 +0000"],
        @"responses" : @[
            [NSData dataWithBytes:"AAAA" length:strlen("AAAA")]
        ],
        @"scts" : @[
            [NSData dataWithBytes:"BBBB" length:strlen("BBBB")]
        ],
        @"anchors" : @[
            [[NSData alloc] initWithBase64EncodedString:cert1.createNSString().get() options:0]
        ],
        @"trustedLogs" : @[
            [NSData dataWithBytes:"CCCC" length:strlen("CCCC")]
        ],
        @"exceptions" : @[
            @{
                @"Exception1" : @(YES),
                @"Exception2" : @(NO),
                @"Exception3" : [NSData dataWithBytes:"DDDD" length:strlen("DDDD")],
                @"Exception4" : @(3)
            }
        ]
    };

    // The inline dictionary above does not compare equal. Do a serialization loop to get types which will compare equal.
    CFErrorRef error = NULL;
    RetainPtr<CFDataRef> dataBlob = adoptCF(CFPropertyListCreateData(
        kCFAllocatorDefault,
        appleCertificatePlist,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &error
    ));
    EXPECT_FALSE(error);

    CFPropertyListFormat format;
    RetainPtr<CFPropertyListRef> recreatedAppleCertificatePlist = adoptCF(CFPropertyListCreateWithData(
        kCFAllocatorDefault,
        dataBlob.get(),
        kCFPropertyListImmutable,
        &format,
        &error
    ));
    EXPECT_FALSE(error);

    RetainPtr<SecTrustRef> trust = adoptCF(SecTrustCreateFromPropertyListRepresentation(recreatedAppleCertificatePlist.get(), &error));
    EXPECT_FALSE(error);

    RetainPtr<CFPropertyListRef> plistFromGeneratedObject = adoptCF(SecTrustCopyPropertyListRepresentation(trust.get(), &error));

    bool loopedPlistEqual = CFEqual(recreatedAppleCertificatePlist.get(), plistFromGeneratedObject.get());
    EXPECT_TRUE(loopedPlistEqual);

    runTestCF({ trust.get() });
}
#endif

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

#if USE(AVFOUNDATION) && PLATFORM(MAC)
TEST(IPCSerialization, AVOutputContext)
{
    RetainPtr<AVOutputContext> outputContext = adoptNS([[PAL::getAVOutputContextClass() alloc] init]);
    runTestNS({ outputContext.get() });
}
#endif // USE(AVFOUNDATION) && PLATFORM(MAC)

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

TEST(CoreIPCCFDictionary, InsertDifferentValueTypes)
{
    // Test that a CoreIPCCFDictionary can be created, encoded, and decoded with valid key-value pairs.
    // Keys will be string type (CFStringRef).
    // Values will be all possible variants from IPC::CFType.
    auto cfDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // CFArrayRef
    auto arrayKey = adoptCF(CFSTR("arrayKey"));
    auto arrayValue = adoptCF(CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks));
    CFDictionaryAddValue(cfDictionary.get(), arrayKey.get(), arrayValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 1);

    // CFBooleanRef
    auto booleanKey = adoptCF(CFSTR("booleanKey"));
    auto booleanValue = adoptCF(kCFBooleanFalse);
    CFDictionaryAddValue(cfDictionary.get(), booleanKey.get(), booleanValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 2);

    // CFCharacterSetRef
    auto charSetKey = adoptCF(CFSTR("charSetKey"));
    auto charSetValue = adoptCF(CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, CFSTR("ABC")));
    CFDictionaryAddValue(cfDictionary.get(), charSetKey.get(), charSetValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 3);

    // CFDataRef
    auto dataKey = adoptCF(CFSTR("dataKey"));
    auto dataValue = adoptCF(CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"Data test", strlen("Data test")));
    CFDictionaryAddValue(cfDictionary.get(), dataKey.get(), dataValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 4);

    // CFDateRef
    auto dateKey = adoptCF(CFSTR("dateKey"));
    auto dateValue = adoptCF(CFDateCreate(kCFAllocatorDefault, 1.23));
    CFDictionaryAddValue(cfDictionary.get(), dateKey.get(), dateValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 5);

    // CFDictionaryRef
    auto dictKey = adoptCF(CFSTR("dictKey"));
    auto dictValue = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto key1 = adoptCF(CFSTR("key1"));
    auto value1 = adoptCF(CFSTR("value1"));
    CFDictionaryAddValue(dictValue.get(), key1.get(), value1.get());
    CFDictionaryAddValue(cfDictionary.get(), dictKey.get(), dictValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 6);

    // CFNullRef
    auto nullKey = adoptCF(CFSTR("nullKey"));
    auto nullValue = adoptCF(kCFNull);
    CFDictionaryAddValue(cfDictionary.get(), nullKey.get(), nullValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 7);

    // CFNumberRef
    auto numberKey = adoptCF(CFSTR("numberKey"));
    int32_t num = 123;
    auto numberValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (const void*)&num));
    CFDictionaryAddValue(cfDictionary.get(), numberKey.get(), numberValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 8);

    // CFStringRef
    auto stringKey = adoptCF(CFSTR("stringKey"));
    auto stringValue = adoptCF(CFSTR("stringValue"));
    CFDictionaryAddValue(cfDictionary.get(), stringKey.get(), stringValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 9);

    // CFURLRef
    auto urlKey = adoptCF(CFSTR("urlKey"));
    auto url = adoptCF(CFSTR("localhost.com"));
    auto urlValue = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, url.get(), NULL));
    CFDictionaryAddValue(cfDictionary.get(), urlKey.get(), urlValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 10);

    // SecCertificateRef
    auto secCertificateKey = adoptCF(CFSTR("secCertificateKey"));
    auto secCertificateValue = createCertificate();
    CFDictionaryAddValue(cfDictionary.get(), secCertificateKey.get(), secCertificateValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 11);

    // SecTrustRef
    auto certificate = createCertificate();
    NSArray* certArray = @[(__bridge id) certificate.get()];
    auto policy = adoptCF(SecPolicyCreateBasicX509());
    SecTrustRef trust;
    SecTrustCreateWithCertificates((__bridge CFTypeRef) certArray, policy.get(), &trust);

    auto secTrustKey = adoptCF(CFSTR("secTrustKey"));
    auto secTrustValue = adoptCF(trust);
    CFDictionaryAddValue(cfDictionary.get(), secTrustKey.get(), secTrustValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 12);

    // CGColorSpaceRef
    auto colorSpaceKey = adoptCF(CFSTR("colorSpaceKey"));
    auto colorSpaceValue = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    CFDictionaryAddValue(cfDictionary.get(), colorSpaceKey.get(), colorSpaceValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 13);

    // CGColorRef
    auto colorKey = adoptCF(CFSTR("colorKey"));
    auto colorValue = adoptCF(CGColorCreateSRGB(1.0, 0.0, 0.0, 1.0));
    CFDictionaryAddValue(cfDictionary.get(), colorKey.get(), colorValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 14);

    // SecAccessControlRef
    auto secAccessControlKey = adoptCF(CFSTR("secAccessControlKey"));
    SecAccessControlCreateFlags flags = (kSecAccessControlDevicePasscode | kSecAccessControlBiometryAny | kSecAccessControlOr);
    NSDictionary *protection = @{
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrSynchronizable : @(NO),
        (id)kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly : @(YES),
        (id)kSecAttrAccessibleWhenUnlocked: @(YES) };
    auto secAccessControlValue = adoptCF(SecAccessControlCreateWithFlags(kCFAllocatorDefault, (CFTypeRef)protection, flags, NULL));
    CFDictionaryAddValue(cfDictionary.get(), secAccessControlKey.get(), secAccessControlValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 15);

    // CFSocketRef (should not be accepted by CoreIPCCFDictionary)
    auto socketKey = adoptCF(CFSTR("socketKey"));
    auto socketValue = adoptCF(CFSocketCreate(kCFAllocatorDefault, 0, 0, 0, 0, NULL, NULL));
    CFDictionaryAddValue(cfDictionary.get(), socketKey.get(), socketValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 16);

    WebKit::CoreIPCCFDictionary coreIpcCfDict1(cfDictionary.get());
    auto cfDictionary2 = coreIpcCfDict1.createCFDictionary();
    EXPECT_NE(CFDictionaryGetCount(cfDictionary.get()), CFDictionaryGetCount(cfDictionary2.get()));
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary2.get()), 15);

    runTestCFWithExpectedResult({ cfDictionary.get() }, { cfDictionary2.get() });

    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), arrayKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), arrayKey.get())), CFArrayGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), booleanKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), booleanKey.get())), CFBooleanGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), charSetKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), charSetKey.get())), CFCharacterSetGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), dataKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), dataKey.get())), CFDataGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), dateKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), dateKey.get())), CFDateGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), dictKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), dictKey.get())), CFDictionaryGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), nullKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), nullKey.get())), CFNullGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), numberKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), numberKey.get())), CFNumberGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), stringKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), stringKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), urlKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), urlKey.get())), CFURLGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), secCertificateKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), secCertificateKey.get())), SecCertificateGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), secTrustKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), secTrustKey.get())), SecTrustGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), colorSpaceKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), colorSpaceKey.get())), CGColorSpaceGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), colorKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), colorKey.get())), CGColorGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), secAccessControlKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), secAccessControlKey.get())), SecAccessControlGetTypeID());

    EXPECT_FALSE(CFDictionaryContainsKey(cfDictionary2.get(), socketKey.get()));
}

TEST(CoreIPCCFDictionary, InsertDifferentKeyTypes)
{
    // Test that a CoreIPCCFDictionary can be created, encoded, and decoded with valid key-value pairs.
    // Keys will be all possible variants from CoreIPCCFDictionary::KeyType
    // Values will be string type (CFStringRef).
    auto cfDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // CFArrayRef
    auto arrayKey = adoptCF(CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks));
    auto arrayValue = adoptCF(CFSTR("arrayValue"));
    CFDictionaryAddValue(cfDictionary.get(), arrayKey.get(), arrayValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 1);

    // CFBooleanRef
    auto booleanKey = adoptCF(kCFBooleanFalse);
    auto booleanValue = adoptCF(CFSTR("booleanValue"));
    CFDictionaryAddValue(cfDictionary.get(), booleanKey.get(), booleanValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 2);

    // CFCharacterSetRef
    auto charSetKey = adoptCF(CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, CFSTR("ABC")));
    auto charSetValue = adoptCF(CFSTR("charSetValue"));
    CFDictionaryAddValue(cfDictionary.get(), charSetKey.get(), charSetValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 3);

    // CFDataRef
    auto dataKey = adoptCF(CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"Data test", strlen("Data test")));
    auto dataValue = adoptCF(CFSTR("dataValue"));
    CFDictionaryAddValue(cfDictionary.get(), dataKey.get(), dataValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 4);

    // CFDateRef
    auto dateKey = adoptCF(CFDateCreate(kCFAllocatorDefault, 1.23));
    auto dateValue = adoptCF(CFSTR("dateValue"));
    CFDictionaryAddValue(cfDictionary.get(), dateKey.get(), dateValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 5);

    // CFDictionaryRef
    auto dictKey = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto dictValue = adoptCF(CFSTR("dictValue"));
    auto key1 = adoptCF(CFSTR("key1"));
    auto value1 = adoptCF(CFSTR("value1"));
    CFDictionaryAddValue(dictKey.get(), key1.get(), value1.get());
    CFDictionaryAddValue(cfDictionary.get(), dictKey.get(), dictValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 6);

    // CFNullRef
    auto nullKey = adoptCF(kCFNull);
    auto nullValue = adoptCF(CFSTR("nullValue"));
    CFDictionaryAddValue(cfDictionary.get(), nullKey.get(), nullValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 7);

    // CFNumberRef
    int32_t num = 123;
    auto numberKey = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (const void*)&num));
    auto numberValue = adoptCF(CFSTR("numberValue"));
    CFDictionaryAddValue(cfDictionary.get(), numberKey.get(), numberValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 8);

    // CFStringRef
    auto stringKey = adoptCF(CFSTR("stringKey"));
    auto stringValue = adoptCF(CFSTR("stringValue"));
    CFDictionaryAddValue(cfDictionary.get(), stringKey.get(), stringValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 9);

    // CFURLRef
    auto url = adoptCF(CFSTR("localhost.com"));
    auto urlKey = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, url.get(), NULL));
    auto urlValue = adoptCF(CFSTR("urlValue"));
    CFDictionaryAddValue(cfDictionary.get(), urlKey.get(), urlValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 10);

    // SecCertificateRef
    auto secCertificateKey = createCertificate();
    auto secCertificateValue = adoptCF(CFSTR("secCertificateValue"));
    CFDictionaryAddValue(cfDictionary.get(), secCertificateKey.get(), secCertificateValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 11);

    // SecTrustRef
    auto certificate = createCertificate();
    NSArray* certArray = @[(__bridge id) certificate.get()];
    auto policy = adoptCF(SecPolicyCreateBasicX509());
    SecTrustRef trust;
    SecTrustCreateWithCertificates((__bridge CFTypeRef) certArray, policy.get(), &trust);

    auto secTrustKey = adoptCF(trust);
    auto secTrustValue = adoptCF(CFSTR("secTrustValue"));
    CFDictionaryAddValue(cfDictionary.get(), secTrustKey.get(), secTrustValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 12);

    // CGColorSpaceRef
    auto colorSpaceKey = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto colorSpaceValue = adoptCF(CFSTR("colorSpaceValue"));
    CFDictionaryAddValue(cfDictionary.get(), colorSpaceKey.get(), colorSpaceValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 13);

    // CGColorRef
    auto colorKey = adoptCF(CGColorCreateSRGB(1.0, 0.0, 0.0, 1.0));
    auto colorValue = adoptCF(CFSTR("colorValue"));
    CFDictionaryAddValue(cfDictionary.get(), colorKey.get(), colorValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 14);

    // SecAccessControlRef
    SecAccessControlCreateFlags flags = (kSecAccessControlDevicePasscode | kSecAccessControlBiometryAny | kSecAccessControlOr);
    NSDictionary *protection = @{
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrSynchronizable : @(NO),
        (id)kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly : @(YES),
        (id)kSecAttrAccessibleWhenUnlocked: @(YES) };
    auto secAccessControlKey = adoptCF(SecAccessControlCreateWithFlags(kCFAllocatorDefault, (CFTypeRef)protection, flags, NULL));
    auto secAccessControlValue = adoptCF(CFSTR("secAccessControlValue"));
    CFDictionaryAddValue(cfDictionary.get(), secAccessControlKey.get(), secAccessControlValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 15);

    // CFSocketRef (should not be accepted by CoreIPCCFDictionary)
    auto socketKey = adoptCF(CFSocketCreate(kCFAllocatorDefault, 0, 0, 0, 0, NULL, NULL));
    auto socketValue = adoptCF(CFSTR("socketValue"));
    CFDictionaryAddValue(cfDictionary.get(), socketKey.get(), socketValue.get());
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary.get()), 16);

    WebKit::CoreIPCCFDictionary coreIpcCfDict1(cfDictionary.get());
    auto cfDictionary2 = coreIpcCfDict1.createCFDictionary();
    EXPECT_NE(CFDictionaryGetCount(cfDictionary.get()), CFDictionaryGetCount(cfDictionary2.get()));
    EXPECT_EQ(CFDictionaryGetCount(cfDictionary2.get()), 10);

    runTestCFWithExpectedResult({ cfDictionary.get() }, { cfDictionary2.get() });

    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), arrayKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), arrayKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), booleanKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), booleanKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), charSetKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), charSetKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), dataKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), dataKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), dateKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), dateKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), dictKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), dictKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), nullKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), nullKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), numberKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), numberKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), stringKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), stringKey.get())), CFStringGetTypeID());
    EXPECT_TRUE(CFDictionaryContainsKey(cfDictionary2.get(), urlKey.get()));
    EXPECT_EQ(CFGetTypeID(CFDictionaryGetValue(cfDictionary2.get(), urlKey.get())), CFStringGetTypeID());

    EXPECT_FALSE(CFDictionaryContainsKey(cfDictionary2.get(), secCertificateKey.get()));
    EXPECT_FALSE(CFDictionaryContainsKey(cfDictionary2.get(), secTrustKey.get()));
    EXPECT_FALSE(CFDictionaryContainsKey(cfDictionary2.get(), colorSpaceKey.get()));
    EXPECT_FALSE(CFDictionaryContainsKey(cfDictionary2.get(), colorKey.get()));
    EXPECT_FALSE(CFDictionaryContainsKey(cfDictionary2.get(), secAccessControlKey.get()));
    EXPECT_FALSE(CFDictionaryContainsKey(cfDictionary2.get(), socketKey.get()));
}


