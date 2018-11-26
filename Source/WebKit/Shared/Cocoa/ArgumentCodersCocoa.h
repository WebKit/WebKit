/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

void encodeObject(Encoder&, id <NSSecureCoding>);
std::optional<RetainPtr<id <NSSecureCoding>>> decodeObject(Decoder&, NSArray<Class> *allowedClasses);

template<typename T> std::optional<RetainPtr<T>> decode(Decoder&, Class allowedClass);
template<typename T> std::optional<RetainPtr<T>> decode(Decoder&, NSArray<Class> *allowedClasses = @[ [T class] ]);

template<typename T>
std::optional<RetainPtr<T>> decode(Decoder& decoder, Class allowedClass)
{
    return decode<T>(decoder, @[ allowedClass ]);
}

template<typename T>
std::optional<RetainPtr<T>> decode(Decoder& decoder, NSArray<Class> *allowedClasses)
{
    auto result = decodeObject(decoder, allowedClasses);
    if (!result)
        return std::nullopt;

    if (!*result)
        return { nullptr };

    id object = result->leakRef();
    ASSERT([allowedClasses containsObject:[object class]]);
    return { adoptNS(static_cast<T *>(object)) };
}

template<typename T> using ConformsToSecureCoding = std::is_convertible<T *, id <NSSecureCoding>>;

template<typename T> struct ArgumentCoder<T *> {
    template<typename U = T, std::enable_if_t<ConformsToSecureCoding<U>::value>* = nullptr>
    static void encode(Encoder& encoder, U *object)
    {
        encodeObject(encoder, object);
    }
};

template<typename T> struct ArgumentCoder<RetainPtr<T>> {
    template <typename U = T, std::enable_if_t<ConformsToSecureCoding<U>::value>* = nullptr>
    static void encode(Encoder& encoder, const RetainPtr<U>& object)
    {
        ArgumentCoder<U *>::encode(encoder, object.get());
    }

    template <typename U = T, std::enable_if_t<ConformsToSecureCoding<U>::value>* = nullptr>
    static std::optional<RetainPtr<U>> decode(Decoder& decoder)
    {
        return IPC::decode<U>(decoder);
    }
};

} // namespace IPC

#endif // PLATFORM(COCOA)
