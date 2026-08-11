/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#pragma once

#import "ArgumentCoders.h"
#import "CoreIPCRetainPtr.h"

#if PLATFORM(COCOA)

#import "WKKeyedCoder.h"
#import <WebCore/AttributedString.h>
#import <wtf/RetainPtr.h>

#if ENABLE(DATA_DETECTION)
OBJC_CLASS DDScannerResult;
#if PLATFORM(MAC)
#if HAVE(SECURE_ACTION_CONTEXT)
OBJC_CLASS DDSecureActionContext;
using WKDDActionContext = DDSecureActionContext;
#else
OBJC_CLASS DDActionContext;
using WKDDActionContext = DDActionContext;
#endif // #if HAVE(SECURE_ACTION_CONTEXT)
#endif // #if PLATFORM(MAC)
#endif // #if ENABLE(DATA_DETECTION)

#if USE(AVFOUNDATION)
OBJC_CLASS AVOutputContext;
#endif

#if USE(PASSKIT)
OBJC_CLASS CNContact;
OBJC_CLASS CNPhoneNumber;
OBJC_CLASS CNPostalAddress;
OBJC_CLASS PKContact;
OBJC_CLASS PKDateComponentsRange;
OBJC_CLASS PKPayment;
OBJC_CLASS PKPaymentMerchantSession;
OBJC_CLASS PKPaymentSetupFeature;
OBJC_CLASS PKPaymentMethod;
OBJC_CLASS PKPaymentToken;
OBJC_CLASS PKShippingMethod;
#endif

#if !HAVE(WEBCONTENTRESTRICTIONS) && HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
OBJC_CLASS WebFilterEvaluator;
#endif

OBJC_CLASS PlatformColor;
OBJC_CLASS NSShadow;

namespace IPC {

#ifdef __OBJC__

enum class NSType : uint8_t {
    Array,
    Color,
#if USE(PASSKIT)
    PKPaymentMethod,
    PKPaymentMerchantSession,
    PKPaymentSetupFeature,
    PKContact,
    PKSecureElementPass,
    PKPayment,
    PKPaymentToken,
    PKShippingMethod,
    PKDateComponentsRange,
    CNContact,
    CNPhoneNumber,
    CNPostalAddress,
#endif
    NSDateComponents,
    Data,
    Date,
    Error,
    Dictionary,
    Locale,
    Number,
    Null,
#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    SecureCoding,
#endif
#if HAVE(WK_SECURE_CODING_PKPAYMENTSETUPFEATURE)
    Set,
#endif
    String,
    URL,
    NSValue,
    CF,
    Unknown,
};
NSType typeFromObject(id);
bool isSerializableValue(id);

enum class CFType : uint8_t {
    CFArray,
    CFBoolean,
    CFCharacterSet,
    CFData,
    CFDate,
    CFDictionary,
    CFNull,
    CFNumber,
    CFString,
    CFURL,
    SecCertificate,
    SecAccessControl,
    SecTrust,
    CGColorSpace,
    CGColor,
    Nullptr,
    Unknown,
};
CFType typeFromCFTypeRef(CFTypeRef);

#if ENABLE(DATA_DETECTION)
template<> Class getClass<DDScannerResult>();
#if PLATFORM(MAC)
template<> Class getClass<WKDDActionContext>();
#endif
#endif
#if USE(AVFOUNDATION)
template<> Class getClass<AVOutputContext>();
#endif
#if USE(PASSKIT)
template<> Class getClass<CNContact>();
template<> Class getClass<CNPhoneNumber>();
template<> Class getClass<CNPostalAddress>();
template<> Class getClass<PKContact>();
template<> Class getClass<PKPaymentMerchantSession>();
template<> Class getClass<PKPaymentSetupFeature>();
template<> Class getClass<PKPayment>();
template<> Class getClass<PKPaymentToken>();
template<> Class getClass<PKShippingMethod>();
template<> Class getClass<PKDateComponentsRange>();
template<> Class getClass<PKPaymentMethod>();
template<> Class getClass<PKSecureElementPass>();
#endif
#if !HAVE(WEBCONTENTRESTRICTIONS) && HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
template<> Class getClass<WebFilterEvaluator>();
#endif

template<> Class getClass<PlatformColor>();
template<> Class getClass<NSShadow>();

template<typename T> void encodeObjectDirectly(Encoder&, T *);
template<typename T> void encodeObjectDirectly(Encoder&, T);
template<typename T> void encodeObjectDirectly(StreamConnectionEncoder&, T *);
template<typename T> void encodeObjectDirectly(StreamConnectionEncoder&, T);
template<typename T> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses(Decoder&);

template<typename T, typename = IsObjCObject<T>> void encode(Encoder&, T *);

#if ASSERT_ENABLED

static inline bool isObjectClassAllowed(id object, const AllowedClassHashSet& allowedClasses)
{
    for (auto& allowedClass : allowedClasses) {
        if ([object isKindOfClass:allowedClass.get()])
            return true;
    }
    return false;
}

#endif // ASSERT_ENABLED

template<typename T, typename>
std::optional<RetainPtr<T>> decodeRequiringAllowedClasses(Decoder& decoder)
{
#if ASSERT_ENABLED && !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    auto allowedClasses = decoder.allowedClasses();
#endif
    auto result = decodeObjectDirectlyRequiringAllowedClasses<T>(decoder);
    if (!result)
        return std::nullopt;
#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    ASSERT(!*result || isObjectClassAllowed((*result).get(), allowedClasses));
#endif
    return { *result };
}

template<typename T, typename>
std::optional<T> decodeRequiringAllowedClasses(Decoder& decoder)
{
    auto result = decodeObjectDirectlyRequiringAllowedClasses<T>(decoder);
    if (!result)
        return std::nullopt;
#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    ASSERT(!*result || isObjectClassAllowed((*result).get(), decoder.allowedClasses()));
#endif
    return { *result };
}

template<typename T> struct ArgumentCoder<T *> {
    template<typename U = T, typename = IsObjCObject<U>>
    static void encode(Encoder& encoder, U *object)
    {
        encodeObjectDirectly<U>(encoder, object);
    }
};

#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
template<typename T> struct ArgumentCoder<CoreIPCRetainPtr<T>> {
    template<typename U = T>
    static void encode(Encoder& encoder, const CoreIPCRetainPtr<U>& object)
    {
        encodeObjectDirectly<U>(encoder, object.get());
    }

    template<typename U = T>
    static void encode(StreamConnectionEncoder& encoder, const CoreIPCRetainPtr<U>& object)
    {
        encodeObjectDirectly<U>(encoder, object.get());
    }

    template<typename U = T>
    static std::optional<RetainPtr<U>> decode(Decoder& decoder)
    {
        return decodeObjectDirectlyRequiringAllowedClasses<U>(decoder);
    }
};
#endif // !HAVE(WK_SECURE_CODING_NSURLREQUEST)

template<typename T> struct ArgumentCoder<RetainPtr<T>> {
    template<typename U = T, typename = IsObjCObject<U>>
    static void encode(Encoder& encoder, const RetainPtr<U>& object)
    {
        if (!object) {
            encoder << false;
            return;
        }

        encoder << true;
        ArgumentCoder<U *>::encode(encoder, object.get());
    }

    template<typename U = T, typename = IsObjCObject<U>>
    static std::optional<RetainPtr<U>> decode(Decoder& decoder)
    {
        auto isEngaged = decoder.template decode<bool>();
        if (!isEngaged)
            return std::nullopt;
        if (!*isEngaged)
            return { nullptr };
        return decoder.decodeWithAllowedClasses<U>();
    }
};

template<typename T, typename>
void encode(Encoder& encoder, T *object)
{
    ArgumentCoder<T *>::encode(encoder, object);
}

#endif // __OBJC__

} // namespace IPC

#endif // PLATFORM(COCOA)
