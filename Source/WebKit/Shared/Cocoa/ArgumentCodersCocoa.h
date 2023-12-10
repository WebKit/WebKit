/*
 * Copyright (C) 2018, 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import "WKKeyedCoder.h"
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
OBJC_CLASS CNPhoneNumber;
OBJC_CLASS CNPostalAddress;
OBJC_CLASS PKContact;
#endif

namespace IPC {

#ifdef __OBJC__

template<typename T>
class CoreIPCRetainPtr : public RetainPtr<T> {
public:
    CoreIPCRetainPtr()
        : RetainPtr<T>()
    {
    }

    CoreIPCRetainPtr(T *object)
        : RetainPtr<T>(object)
    {
    }

    CoreIPCRetainPtr(RetainPtr<T>&& object)
        : RetainPtr<T>(WTFMove(object))
    {
    }
};

enum class NSType : uint8_t {
#if USE(AVFOUNDATION)
    AVOutputContext,
#endif
    Array,
#if USE(PASSKIT)
    CNPhoneNumber,
    CNPostalAddress,
    PKContact,
#endif
    Color,
#if ENABLE(DATA_DETECTION)
#if PLATFORM(MAC)
    DDActionContext,
#endif
    DDScannerResult,
#endif
    Data,
    Date,
    Error,
    Dictionary,
    Font,
    Locale,
    Number,
    PersonNameComponents,
    SecureCoding,
    String,
    URL,
    NSValue,
    CF,
    Unknown,
};
NSType typeFromObject(id);
bool isSerializableValue(id);

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
template<> Class getClass<CNPhoneNumber>();
template<> Class getClass<CNPostalAddress>();
template<> Class getClass<PKContact>();
#endif

void encodeObjectWithWrapper(Encoder&, id);
std::optional<RetainPtr<id>> decodeObjectFromWrapper(Decoder&, const HashSet<Class>& allowedClasses);

template<typename T> void encodeObjectDirectly(Encoder&, T *);
template<typename T> void encodeObjectDirectly(Encoder&, T);
template<typename T> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses(Decoder&);

template<typename T, typename = IsObjCObject<T>> void encode(Encoder&, T *);

#if ASSERT_ENABLED

static inline bool isObjectClassAllowed(id object, const HashSet<Class>& allowedClasses)
{
    for (Class allowedClass : allowedClasses) {
        if ([object isKindOfClass:allowedClass])
            return true;
    }
    return false;
}

#endif // ASSERT_ENABLED

template<typename T, typename>
std::optional<RetainPtr<T>> decodeRequiringAllowedClasses(Decoder& decoder)
{
    auto result = decodeObjectFromWrapper(decoder, decoder.allowedClasses());
    if (!result)
        return std::nullopt;
    ASSERT(!*result || isObjectClassAllowed((*result).get(), decoder.allowedClasses()));
    return { *result };
}

template<typename T, typename>
std::optional<T> decodeRequiringAllowedClasses(Decoder& decoder)
{
    auto result = decodeObjectFromWrapper(decoder, decoder.allowedClasses());
    if (!result)
        return std::nullopt;
    ASSERT(!*result || isObjectClassAllowed((*result).get(), decoder.allowedClasses()));
    return { *result };
}

template<typename T> struct ArgumentCoder<T *> {
    template<typename U = T, typename = IsObjCObject<U>>
    static void encode(Encoder& encoder, U *object)
    {
        encodeObjectWithWrapper(encoder, object);
    }
};

template<typename T> struct ArgumentCoder<CoreIPCRetainPtr<T>> {
    template<typename U = T>
    static void encode(Encoder& encoder, const CoreIPCRetainPtr<U>& object)
    {
        encodeObjectDirectly<U>(encoder, object.get());
    }

    template<typename U = T>
    static std::optional<RetainPtr<U>> decode(Decoder& decoder)
    {
        return decodeObjectDirectlyRequiringAllowedClasses<U>(decoder);
    }
};

template<typename T> struct ArgumentCoder<RetainPtr<T>> {
    template<typename U = T, typename = IsObjCObject<U>>
    static void encode(Encoder& encoder, const RetainPtr<U>& object)
    {
        ArgumentCoder<U *>::encode(encoder, object.get());
    }

    template<typename U = T, typename = IsObjCObject<U>>
    static std::optional<RetainPtr<U>> decode(Decoder& decoder)
    {
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
