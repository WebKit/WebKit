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
#import <WebCore/ColorCocoa.h>
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
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
#import <pal/cocoa/WebContentAnalysisSoftLink.h>
#endif

namespace IPC {
using namespace WebCore;

#pragma mark - Helpers

#if ENABLE(DATA_DETECTION)
template<> Class getClass<DDScannerResult>()
{
    return PAL::getDDScannerResultClassSingleton();
}

#if PLATFORM(MAC)
template<> Class getClass<WKDDActionContext>()
{
    return PAL::getWKDDActionContextClassSingleton();
}
#endif
#endif
#if USE(AVFOUNDATION)
template<> Class getClass<AVOutputContext>()
{
    return PAL::getAVOutputContextClassSingleton();
}
#endif
#if USE(PASSKIT)
template<> Class getClass<CNContact>()
{
    return PAL::getCNContactClassSingleton();
}
template<> Class getClass<CNPhoneNumber>()
{
    return PAL::getCNPhoneNumberClassSingleton();
}
template<> Class getClass<CNPostalAddress>()
{
    return PAL::getCNPostalAddressClassSingleton();
}
template<> Class getClass<PKContact>()
{
    return PAL::getPKContactClassSingleton();
}
template<> Class getClass<PKPaymentMerchantSession>()
{
    return PAL::getPKPaymentMerchantSessionClassSingleton();
}
template<> Class getClass<PKPaymentSetupFeature>()
{
    return PAL::getPKPaymentSetupFeatureClassSingleton();
}
template<> Class getClass<PKPayment>()
{
    return PAL::getPKPaymentClassSingleton();
}
template<> Class getClass<PKPaymentToken>()
{
    return PAL::getPKPaymentTokenClassSingleton();
}
template<> Class getClass<PKShippingMethod>()
{
    return PAL::getPKShippingMethodClassSingleton();
}
template<> Class getClass<PKDateComponentsRange>()
{
    return PAL::getPKDateComponentsRangeClassSingleton();
}
template<> Class getClass<PKPaymentMethod>()
{
    return PAL::getPKPaymentMethodClassSingleton();
}
template<> Class getClass<PKSecureElementPass>()
{
    return PAL::getPKSecureElementPassClassSingleton();
}
#endif

#if !HAVE(WEBCONTENTRESTRICTIONS) && HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
template<> Class getClass<WebFilterEvaluator>()
{
    return PAL::getWebFilterEvaluatorClassSingleton();
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
#if HAVE(WK_SECURE_CODING_PKPAYMENTSETUPFEATURE)
    if ([object isKindOfClass:[NSSet class]])
        return NSType::Set;
#endif
    if ([object isKindOfClass:[NSURL class]])
        return NSType::URL;
#if USE(PASSKIT)
    // No need to retain Class, it is immortal.
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKPaymentMethod>()])
        return NSType::PKPaymentMethod;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKPaymentMerchantSession>()])
        return NSType::PKPaymentMerchantSession;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKPaymentSetupFeature>()])
        return NSType::PKPaymentSetupFeature;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKContact>()])
        return NSType::PKContact;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKSecureElementPass>()])
        return NSType::PKSecureElementPass;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKPayment>()])
        return NSType::PKPayment;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKPaymentToken>()])
        return NSType::PKPaymentToken;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKShippingMethod>()])
        return NSType::PKShippingMethod;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<PKDateComponentsRange>()])
        return NSType::PKDateComponentsRange;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<CNContact>()])
        return NSType::CNContact;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<CNPhoneNumber>()])
        return NSType::CNPhoneNumber;
    SUPPRESS_UNRETAINED_ARG if ([object isKindOfClass:getClass<CNPostalAddress>()])
        return NSType::CNPostalAddress;
#endif
    if ([object isKindOfClass:[NSDateComponents class]])
        return NSType::NSDateComponents;
    // Not all CF types are toll-free-bridged to NS types.
    // Non-toll-free-bridged CF types do not conform to NSSecureCoding.
    if ([object isKindOfClass:NSClassFromString(@"__NSCFType")])
        return NSType::CF;

    RELEASE_LOG_FAULT(IPC, "WebKit::typeFromObject: Unknown NSType inferred from object of type '%s'", class_getName(object_getClass(object)));
    ASSERT_NOT_REACHED();
    return NSType::Unknown;
}

bool isSerializableValue(id value)
{
    return typeFromObject(value) != NSType::Unknown;
}

#pragma mark - CF

template<> void encodeObjectDirectly<CFTypeRef>(Encoder& encoder, CFTypeRef cf)
{
    ArgumentCoder<CFTypeRef>::encode(encoder, cf);
}

template<> void encodeObjectDirectly<CFTypeRef>(StreamConnectionEncoder& encoder, CFTypeRef cf)
{
    ArgumentCoder<CFTypeRef>::encode(encoder, cf);
}

template<> std::optional<RetainPtr<id>> decodeObjectDirectly<CFTypeRef>(Decoder& decoder)
{
    auto result = ArgumentCoder<RetainPtr<CFTypeRef>>::decode(decoder);
    if (!result)
        return std::nullopt;

    return static_cast<id>(result->get());
}

} // namespace IPC

#endif // PLATFORM(COCOA)
