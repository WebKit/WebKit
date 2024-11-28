/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import "CoreIPCNSCFObject.h"
#import "CoreIPCNSURLCredential.h"
#import "CoreIPCNSURLRequest.h"
#import "CoreIPCTypes.h"
#import "CoreTextHelpers.h"
#import "LegacyGlobalSettings.h"
#import "Logging.h"
#import "MessageNames.h"
#import "WebPreferencesKeys.h"
#import <CoreText/CTFont.h>
#import <CoreText/CTFontDescriptor.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/FontCocoa.h>
#import <pal/spi/cocoa/NSKeyedUnarchiverSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/HashSet.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/cf/CFURLExtras.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/StringHash.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIColor.h>
#import <UIKit/UIFont.h>
#import <UIKit/UIFontDescriptor.h>
#import <UIKit/UIKit.h>
#import <pal/ios/UIKitSoftLink.h>
#endif

#if ENABLE(DATA_DETECTION)
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif
#if ENABLE(APPLE_PAY)
#import <pal/cocoa/PassKitSoftLink.h>
#endif
#if ENABLE(REVEAL)
#import <pal/cocoa/RevealSoftLink.h>
#endif
#if HAVE(VK_IMAGE_ANALYSIS)
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#endif
#if ENABLE(DATA_DETECTION)
#import <pal/mac/DataDetectorsSoftLink.h>
#endif
#if USE(AVFOUNDATION)
#import <pal/cocoa/AVFoundationSoftLink.h>
#endif
#if USE(PASSKIT)
#import <pal/cocoa/ContactsSoftLink.h>
#endif

@interface WKSecureCodingArchivingDelegate : NSObject <NSKeyedArchiverDelegate, NSKeyedUnarchiverDelegate>
@property (nonatomic, assign) BOOL rewriteMutableArray;
@property (nonatomic, assign) BOOL rewriteMutableData;
@property (nonatomic, assign) BOOL rewriteMutableDictionary;
@property (nonatomic, assign) BOOL rewriteMutableString;
@property (nonatomic, assign) BOOL transformURLs;
@end

@interface WKSecureCodingURLWrapper : NSURL <NSSecureCoding>
- (instancetype)initWithURL:(NSURL *)wrappedURL;
@property (nonatomic, readonly) NSURL * wrappedURL;
@end

@interface WKSecureCodingCGColorWrapper : NSObject <NSSecureCoding>
- (instancetype)initWithCGColor:(CGColorRef)wrappedColor;
@property (nonatomic, readonly) CGColorRef wrappedColor;
@end

@implementation WKSecureCodingArchivingDelegate

@synthesize rewriteMutableArray;
@synthesize rewriteMutableData;
@synthesize rewriteMutableDictionary;
@synthesize rewriteMutableString;
@synthesize transformURLs;

- (id)archiver:(NSKeyedArchiver *)archiver willEncodeObject:(id)object
{
    if (auto unwrappedURL = dynamic_objc_cast<NSURL>(object); unwrappedURL && transformURLs)
        return adoptNS([[WKSecureCodingURLWrapper alloc] initWithURL:unwrappedURL]).autorelease();

    if (rewriteMutableArray) {
        if (auto mutableArray = dynamic_objc_cast<NSMutableArray>(object))
            return adoptNS([mutableArray copy]).autorelease();
    }

    if (rewriteMutableData) {
        if (auto mutableData = dynamic_objc_cast<NSMutableData>(object))
            return adoptNS([mutableData copy]).autorelease();
    }

    if (rewriteMutableDictionary) {
        if (auto mutableDict = dynamic_objc_cast<NSMutableDictionary>(object))
            return adoptNS([mutableDict copy]).autorelease();
    }

    if (rewriteMutableString) {
        if (auto mutableString = dynamic_objc_cast<NSMutableString>(object))
            return adoptNS([mutableString copy]).autorelease();
    }

    // We can't just return a WebCore::CocoaColor here, because the decoder would
    // have no way of distinguishing an authentic WebCore::CocoaColor vs a CGColor
    // masquerading as a WebCore::CocoaColor.
    if (CFGetTypeID(object) == CGColorGetTypeID())
        return adoptNS([[WKSecureCodingCGColorWrapper alloc] initWithCGColor:static_cast<CGColorRef>(object)]).autorelease();

    return object;
}

- (id)unarchiver:(NSKeyedUnarchiver *)unarchiver didDecodeObject:(id) NS_RELEASES_ARGUMENT object NS_RETURNS_RETAINED
{
    auto adoptedObject = adoptNS(object);
    if (auto wrapper = dynamic_objc_cast<WKSecureCodingURLWrapper>(adoptedObject.get()))
        return retainPtr(wrapper.wrappedURL).leakRef();

    if (auto wrapper = dynamic_objc_cast<WKSecureCodingCGColorWrapper>(adoptedObject.get()))
        return static_cast<id>(retainPtr(wrapper.wrappedColor).leakRef());

    return adoptedObject.leakRef();
}

- (instancetype)init
{
    if (self = [super init]) {
        rewriteMutableArray = NO;
        rewriteMutableData = NO;
        rewriteMutableDictionary = NO;
        rewriteMutableString = NO;
        transformURLs = YES;
    }

    return self;
}

@end

@implementation WKSecureCodingURLWrapper {
    RetainPtr<NSURL> m_wrappedURL;
}

- (NSURL *)wrappedURL
{
    return m_wrappedURL.get();
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    RELEASE_ASSERT(m_wrappedURL);
    auto bytes = bytesAsVector(bridge_cast([m_wrappedURL.get() absoluteURL]));
    [coder encodeBytes:bytes.data() length:bytes.size()];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithString:@""];
    if (!self)
        return nil;

    NSUInteger length;
    if (auto bytes = (UInt8 *)[coder decodeBytesWithReturnedLength:&length]) {
        m_wrappedURL = bridge_cast(adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, bytes, length, kCFStringEncodingUTF8, nullptr, true)));
        if (!m_wrappedURL)
            LOG_ERROR("Failed to decode NSURL due to invalid encoding of length %d. Substituting a blank URL", length);
    }

    if (!m_wrappedURL)
        m_wrappedURL = adoptNS([[NSURL alloc] initWithString:@""]);

    return self;
}

- (instancetype)initWithURL:(NSURL *)url
{
    if (self = [super initWithString:@""])
        m_wrappedURL = url;

    return self;
}

@end

@implementation WKSecureCodingCGColorWrapper {
    RetainPtr<CGColorRef> m_wrappedColor;
}

- (CGColorRef)wrappedColor
{
    return m_wrappedColor.get();
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

static constexpr NSString *innerColorKey = @"WK.CocoaColor";

- (void)encodeWithCoder:(NSCoder *)coder
{
    RELEASE_ASSERT(m_wrappedColor);
    [coder encodeObject:[WebCore::CocoaColor colorWithCGColor:m_wrappedColor.get()] forKey:innerColorKey];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super init];
    if (!self)
        return nil;

    RetainPtr<WebCore::CocoaColor> color = static_cast<WebCore::CocoaColor *>([coder decodeObjectOfClass:[WebCore::CocoaColor class] forKey:innerColorKey]);
    m_wrappedColor = color.get().CGColor;

    return self;
}

- (instancetype)initWithCGColor:(CGColorRef)color
{
    if (self = [super init])
        m_wrappedColor = color;

    return self;
}

@end

namespace IPC {
using namespace WebCore;

#pragma mark - Helpers

#if ENABLE(DATA_DETECTION)
template<> Class getClass<DDScannerResult>()
{
    return PAL::getDDScannerResultClass();
}

#if PLATFORM(MAC)
template<> Class getClass<WKDDActionContext>()
{
    return PAL::getWKDDActionContextClass();
}
#endif
#endif
#if USE(AVFOUNDATION)
template<> Class getClass<AVOutputContext>()
{
    return PAL::getAVOutputContextClass();
}
#endif
#if USE(PASSKIT)
template<> Class getClass<CNContact>()
{
    return PAL::getCNContactClass();
}
template<> Class getClass<CNPhoneNumber>()
{
    return PAL::getCNPhoneNumberClass();
}
template<> Class getClass<CNPostalAddress>()
{
    return PAL::getCNPostalAddressClass();
}
template<> Class getClass<PKContact>()
{
    return PAL::getPKContactClass();
}
template<> Class getClass<PKPaymentMerchantSession>()
{
    return PAL::getPKPaymentMerchantSessionClass();
}
template<> Class getClass<PKPayment>()
{
    return PAL::getPKPaymentClass();
}
template<> Class getClass<PKPaymentToken>()
{
    return PAL::getPKPaymentTokenClass();
}
template<> Class getClass<PKShippingMethod>()
{
    return PAL::getPKShippingMethodClass();
}
template<> Class getClass<PKDateComponentsRange>()
{
    return PAL::getPKDateComponentsRangeClass();
}
template<> Class getClass<PKPaymentMethod>()
{
    return PAL::getPKPaymentMethodClass();
}
template<> Class getClass<PKSecureElementPass>()
{
    return PAL::getPKSecureElementPassClass();
}
#endif

template<> Class getClass<PlatformColor>()
{
    return PlatformColorClass;
}

template<> Class getClass<NSShadow>()
{
    return PlatformNSShadow;
}

NSType typeFromObject(id object)
{
    ASSERT(object);

    // Specific classes handled.
    if ([object isKindOfClass:[NSArray class]])
        return NSType::Array;
    if ([object isKindOfClass:[NSData class]])
        return NSType::Data;
    if ([object isKindOfClass:[NSDate class]])
        return NSType::Date;
    if ([object isKindOfClass:[NSError class]])
        return NSType::Error;
    if ([object isKindOfClass:[NSDictionary class]])
        return NSType::Dictionary;
    if ([object isKindOfClass:[CocoaFont class]])
        return NSType::Font;
    if ([object isKindOfClass:[NSLocale class]])
        return NSType::Locale;
    if ([object isKindOfClass:[NSNumber class]])
        return NSType::Number;
    if ([object isKindOfClass:[NSNull class]])
        return NSType::Null;
    if ([object isKindOfClass:[NSValue class]])
        return NSType::NSValue;
    if ([object isKindOfClass:[NSString class]])
        return NSType::String;
    if ([object isKindOfClass:[NSURL class]])
        return NSType::URL;
    // Not all CF types are toll-free-bridged to NS types.
    // Non-toll-free-bridged CF types do not conform to NSSecureCoding.
    if ([object isKindOfClass:NSClassFromString(@"__NSCFType")])
        return NSType::CF;

    // Check NSSecureCoding after the specific cases since
    // most of the classes above conform to NSSecureCoding,
    // and we want our special case coders for them instead.
    if ([object conformsToProtocol:@protocol(NSSecureCoding)])
        return NSType::SecureCoding;

    ASSERT_NOT_REACHED();
    return NSType::Unknown;
}

static inline bool isSerializableFont(CTFontRef font)
{
    return adoptCF(CTFontCopyAttribute(font, kCTFontURLAttribute));
}

bool isSerializableValue(id value)
{
    if ([value isKindOfClass:[CocoaFont class]])
        return isSerializableFont((__bridge CTFontRef)value);
    return typeFromObject(value) != NSType::Unknown;
}

#pragma mark - id <NSSecureCoding>

template<> void encodeObjectDirectly<NSObject<NSSecureCoding>>(Encoder& encoder, NSObject<NSSecureCoding> *object)
{
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);

    auto delegate = adoptNS([[WKSecureCodingArchivingDelegate alloc] init]);

#if ENABLE(DATA_DETECTION)
    if (PAL::isDataDetectorsCoreFrameworkAvailable() && [object isKindOfClass:PAL::getDDScannerResultClass()])
        [delegate setRewriteMutableString:YES];
#if PLATFORM(MAC)
    if (PAL::isDataDetectorsFrameworkAvailable() && [object isKindOfClass:PAL::getWKDDActionContextClass()])
        [delegate setRewriteMutableString:YES];
#endif // PLATFORM(MAC)
#endif // ENABLE(DATA_DETECTION)
#if ENABLE(REVEAL)
    if (PAL::isRevealCoreFrameworkAvailable() && [object isKindOfClass:PAL::getRVItemClass()])
        [delegate setRewriteMutableString:YES];
#endif // ENABLE(REVEAL)

    if ([object isKindOfClass:NSTextAttachment.class]) {
        [delegate setRewriteMutableData:YES];
        [delegate setRewriteMutableArray:YES];
    }

#if ENABLE(REVEAL)
    // FIXME: This can be removed for RVItem on operating systems that have rdar://109237983.
    if (PAL::isRevealCoreFrameworkAvailable() && [object isKindOfClass:PAL::getRVItemClass()])
        [delegate setTransformURLs:NO];
#endif
#if ENABLE(DATA_DETECTION)
    if (PAL::isDataDetectorsCoreFrameworkAvailable() && [object isKindOfClass:PAL::getDDScannerResultClass()])
        [delegate setTransformURLs:NO];
#if PLATFORM(MAC)
    if (PAL::isDataDetectorsFrameworkAvailable() && [object isKindOfClass:PAL::getWKDDActionContextClass()])
        [delegate setTransformURLs:NO];
#endif
#endif // ENABLE(DATA_DETECTION)

#if USE(PASSKIT)
    if (PAL::isPassKitCoreFrameworkAvailable() && [object isKindOfClass:PAL::getPKSecureElementPassClass()]) {
        [delegate setTransformURLs:NO];
        [delegate setRewriteMutableArray:YES];
    }
#endif

    [archiver setDelegate:delegate.get()];

    [archiver encodeObject:object forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];
    [archiver setDelegate:nil];

    encoder << (__bridge CFDataRef)[archiver encodedData];
}

static bool shouldEnableStrictMode(Decoder& decoder, const AllowedClassHashSet& allowedClasses)
{
#if HAVE(STRICT_DECODABLE_PKCONTACT) \
    && HAVE(STRICT_DECODABLE_CNCONTACT) \
    && (HAVE(STRICT_DECODABLE_PKPAYMENTPASS) || !HAVE(PKPAYMENTPASS))
    // Shortcut the following unnecessary Class checks on newer OSes to fix rdar://111926152.
    return true;
#else

#if HAVE(SECURE_ACTION_CONTEXT)
static constexpr bool haveSecureActionContext = true;
#else
static constexpr bool haveSecureActionContext = false;
#endif

#if ENABLE(DATA_DETECTION)
    // rdar://107553330 - don't re-introduce rdar://107676726
    if (PAL::isDataDetectorsCoreFrameworkAvailable()
        && PAL::getDDScannerResultClass()
        && allowedClasses.contains(PAL::getDDScannerResultClass()))
        return haveSecureActionContext;
#if PLATFORM(MAC)
    // rdar://107553348 - don't re-introduce rdar://107676726
    if (PAL::isDataDetectorsFrameworkAvailable()
        && PAL::getWKDDActionContextClass()
        && allowedClasses.contains(PAL::getWKDDActionContextClass()))
        return haveSecureActionContext;
#endif // PLATFORM(MAC)
#endif // ENABLE(DATA_DETECTION)
#if ENABLE(REVEAL)
    // rdar://107553310 - don't re-introduce rdar://107673064
    if (PAL::isRevealCoreFrameworkAvailable()
        && PAL::getRVItemClass()
        && allowedClasses.contains(PAL::getRVItemClass()))
        return haveSecureActionContext;
#endif // ENABLE(REVEAL)

#if ENABLE(APPLE_PAY)
    // rdar://107553480 Don't reintroduce rdar://108235706
#if HAVE(STRICT_DECODABLE_CNCONTACT)
    static constexpr bool haveStrictDecodableCNContact = true;
#else
    static constexpr bool haveStrictDecodableCNContact = false;
#endif

    // rdar://107553480 Don't reintroduce rdar://120005200
#if HAVE(STRICT_DECODABLE_PKPAYMENTPASS)
    static constexpr bool haveStrictDecodablePKPaymentPass = true;
#else
    static constexpr bool haveStrictDecodablePKPaymentPass = false;
#endif

    // FIXME: Remove these checks for CNContact, and PKSecureElementPass
    // once we directly serialize them ourselves.
    auto isDecodingPKPaymentRelatedType = [&] () {
        if (!PAL::isPassKitCoreFrameworkAvailable())
            return false;
        if (PAL::getPKPaymentMethodClass() && allowedClasses.contains(PAL::getPKPaymentMethodClass()))
            return true;
        if (PAL::getPKSecureElementPassClass() && allowedClasses.contains(PAL::getPKSecureElementPassClass()))
            return true;
        if (PAL::isContactsFrameworkAvailable() && PAL::getCNContactClass() && allowedClasses.contains(PAL::getCNContactClass()))
            return true;
        return false;
    };

    if (isDecodingPKPaymentRelatedType())
        return haveStrictDecodableCNContact && haveStrictDecodablePKPaymentPass;

    // Don't reintroduce rdar://108660074
#if HAVE(STRICT_DECODABLE_PKCONTACT)
    static constexpr bool haveStrictDecodablePKContact = true;
#else
    static constexpr bool haveStrictDecodablePKContact = false;
#endif
    if (PAL::isPassKitCoreFrameworkAvailable()
        && PAL::getPKContactClass()
        && allowedClasses.contains(PAL::getPKContactClass()))
        return haveStrictDecodablePKContact;
#endif // ENABLE(APPLE_PAY)

    // rdar://107553194, Don't reintroduce rdar://108339450
    if (allowedClasses.contains(NSMutableURLRequest.class))
        return true;

    if (allowedClasses.contains(NSTextAttachment.class) // rdar://107553273
#if ENABLE(APPLE_PAY)
        || (PAL::isPassKitCoreFrameworkAvailable() && PAL::getPKPaymentSetupFeatureClass() && allowedClasses.contains(PAL::getPKPaymentSetupFeatureClass())) // rdar://107553409
        || (PAL::isPassKitCoreFrameworkAvailable() && PAL::getPKPaymentMerchantSessionClass() && allowedClasses.contains(PAL::getPKPaymentMerchantSessionClass())) // rdar://107553452
        || (PAL::isPassKitCoreFrameworkAvailable() && PAL::getPKPaymentClass() && allowedClasses.contains(PAL::getPKPaymentClass()) && isInWebProcess())
#endif // ENABLE(APPLE_PAY)
        )
        return true;

    // Note: Do not add more classes to the list of strict decoded classes.
    // If you want to serialize something new, extract its contents into a
    // struct and use a *.serialization.in file to serialize its contents.
    RetainPtr<NSMutableArray> nsAllowedClasses = adoptNS([[NSMutableArray alloc] initWithCapacity:allowedClasses.size()]);
    for (auto& classPtr : allowedClasses)
        [nsAllowedClasses addObject:classPtr.get()];
    RELEASE_LOG_FAULT(SecureCoding, "Strict mode check found unknown classes %@", nsAllowedClasses.get());
    ASSERT_NOT_REACHED();
    return true;
#endif
}

template<> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses<NSObject<NSSecureCoding>>(Decoder& decoder)
{
    auto& allowedClasses = decoder.allowedClasses();
    RELEASE_ASSERT(allowedClasses.size());

    auto data = decoder.decode<RetainPtr<CFDataRef>>();
    if (!data)
        return std::nullopt;

    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:bridge_cast(data->get()) error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;

    auto delegate = adoptNS([[WKSecureCodingArchivingDelegate alloc] init]);
    unarchiver.get().delegate = delegate.get();

    if (allowedClasses.contains(NSMutableURLRequest.class)
        || allowedClasses.contains(NSURLRequest.class)) {
        allowedClasses.add(WKSecureCodingURLWrapper.class);
        allowedClasses.add(NSMutableString.class);
        allowedClasses.add(NSMutableArray.class);
        allowedClasses.add(NSMutableDictionary.class);
        allowedClasses.add(NSMutableData.class);
        allowedClasses.add(NSMutableURLRequest.class);
    }

#if USE(PASSKIT)
    // FIXME: Remove these exceptions for PKSecureElementPass
    // once we directly serialize them ourselves.
    if (PAL::isContactsFrameworkAvailable()) {
        if (allowedClasses.contains(PAL::getPKPaymentClass()) || allowedClasses.contains(PAL::getPKPaymentMethodClass()) || allowedClasses.contains(PAL::getPKPaymentTokenClass())) {
            allowedClasses.add(PAL::getPKSecureElementPassClass());
        }
    }

    if (PAL::isPassKitCoreFrameworkAvailable()) {
        if (PAL::getPKSecureElementPassClass() && allowedClasses.contains(PAL::getPKSecureElementPassClass()))
            allowedClasses.add(PAL::getPKPaymentPassClass());
    }
#endif

    auto allowedClassSet = adoptNS([[NSMutableSet alloc] initWithCapacity:allowedClasses.size()]);
    for (auto& allowedClass : allowedClasses)
        [allowedClassSet addObject:allowedClass.get()];

    if (shouldEnableStrictMode(decoder, allowedClasses))
        [unarchiver _enableStrictSecureDecodingMode];

    @try {
        id result = [unarchiver decodeObjectOfClasses:allowedClassSet.get() forKey:NSKeyedArchiveRootObjectKey];
        ASSERT(!result || [result conformsToProtocol:@protocol(NSSecureCoding)]);
        return { result };
    } @catch (NSException *exception) {
        RELEASE_LOG_FAULT(SecureCoding, "NSKU decode failed for object of classes %@: %@", allowedClassSet.get(), exception);
        ASSERT_NOT_REACHED();
        return std::nullopt;
    } @finally {
        [unarchiver finishDecoding];
        unarchiver.get().delegate = nil;
    }
}

#define ENCODE_AS_SECURE_CODING(c) \
template<> void encodeObjectDirectly<c>(IPC::Encoder& encoder, c *instance) \
{ \
    encoder << (instance ? std::optional(WebKit::CoreIPCSecureCoding(instance)) : std::nullopt); \
} \
template<> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses<c>(IPC::Decoder& decoder) \
{ \
    auto result = decoder.decode<std::optional<WebKit::CoreIPCSecureCoding>>(); \
    if (!result) \
        return std::nullopt; \
    return *result ? (*result)->toID() : nullptr; \
}

#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
ENCODE_AS_SECURE_CODING(NSURLRequest);
#endif

#if USE(PASSKIT)
ENCODE_AS_SECURE_CODING(PKSecureElementPass);
#endif

#if ENABLE(DATA_DETECTION) && !HAVE(WK_SECURE_CODING_DATA_DETECTORS)
ENCODE_AS_SECURE_CODING(DDScannerResult);
#if PLATFORM(MAC)
ENCODE_AS_SECURE_CODING(WKDDActionContext);
#endif
#endif

#undef ENCODE_AS_SECURE_CODING

#pragma mark - CF

template<> void encodeObjectDirectly<CFTypeRef>(Encoder& encoder, CFTypeRef cf)
{
    ArgumentCoder<CFTypeRef>::encode(encoder, cf);
}

template<> void encodeObjectDirectly<CFTypeRef>(StreamConnectionEncoder& encoder, CFTypeRef cf)
{
    ArgumentCoder<CFTypeRef>::encode(encoder, cf);
}

template<> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses<CFTypeRef>(Decoder& decoder)
{
    auto result = ArgumentCoder<RetainPtr<CFTypeRef>>::decode(decoder);
    if (!result)
        return std::nullopt;

    return static_cast<id>(result->get());
}

} // namespace IPC

#endif // PLATFORM(COCOA)
