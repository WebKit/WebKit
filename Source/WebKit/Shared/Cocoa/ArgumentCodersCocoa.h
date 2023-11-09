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

#import "ArgumentCoders.h"

#if PLATFORM(COCOA)

#import <wtf/RetainPtr.h>

namespace IPC {

#ifdef __OBJC__
void encodeObject(Encoder&, id);
std::optional<RetainPtr<id>> decodeObject(Decoder&, NSArray<Class> *allowedClasses);

template<typename T, typename = IsObjCObject<T>> void encode(Encoder&, T *);

#if ASSERT_ENABLED

static inline bool isObjectClassAllowed(id object, NSArray<Class> *allowedClasses)
{
    for (Class allowedClass in allowedClasses) {
        if ([object isKindOfClass:allowedClass])
            return true;
    }
    return false;
}

#endif // ASSERT_ENABLED

template<typename T, typename>
std::optional<RetainPtr<T>> decodeWithAllowedClasses(Decoder& decoder, NSArray<Class> *allowedClasses)
{
    auto result = decodeObject(decoder, allowedClasses);
    if (!result)
        return std::nullopt;
    ASSERT(!*result || isObjectClassAllowed((*result).get(), allowedClasses));
    return { *result };
}

template<typename T> struct ArgumentCoder<T *> {
    template<typename U = T, typename = IsObjCObject<U>>
    static void encode(Encoder& encoder, U *object)
    {
        encodeObject(encoder, object);
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
