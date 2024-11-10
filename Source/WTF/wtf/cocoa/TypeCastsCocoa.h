/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TollFreeBridging.h>

namespace WTF {

// Use bridge_cast() to convert between CF <-> NS types without ref churn.

#if __has_feature(objc_arc)
#define WTF_CF_TO_NS_BRIDGE_TRANSFER(type, value) ((__bridge_transfer type)value)
#define WTF_NS_TO_CF_BRIDGE_TRANSFER(type, value) ((type)reinterpret_cast<uintptr_t>(value))
#else
#define WTF_CF_TO_NS_BRIDGE_TRANSFER(type, value) ((__bridge type)value)
#define WTF_NS_TO_CF_BRIDGE_TRANSFER(type, value) ((__bridge type)value)
#endif

template<typename T> inline typename NSTollFreeBridgingTraits<std::remove_pointer_t<T>>::BridgedType bridge_cast(T object)
{
    return (__bridge typename NSTollFreeBridgingTraits<std::remove_pointer_t<T>>::BridgedType)object;
}

template<typename T> inline RetainPtr<typename NSTollFreeBridgingTraits<std::remove_pointer_t<T>>::BridgedType> bridge_cast(RetainPtr<T>&& object)
{
    return adoptCF(WTF_NS_TO_CF_BRIDGE_TRANSFER(typename NSTollFreeBridgingTraits<std::remove_pointer_t<T>>::BridgedType, object.leakRef()));
}

template<typename T> inline typename CFTollFreeBridgingTraits<T>::BridgedType bridge_cast(T object)
{
    return (__bridge typename CFTollFreeBridgingTraits<T>::BridgedType)object;
}

template<typename T> inline RetainPtr<std::remove_pointer_t<typename CFTollFreeBridgingTraits<T>::BridgedType>> bridge_cast(RetainPtr<T>&& object)
{
    return adoptNS(WTF_CF_TO_NS_BRIDGE_TRANSFER(typename CFTollFreeBridgingTraits<T>::BridgedType, object.leakRef()));
}

// Use bridge_id_cast to convert from CF -> id without ref churn.

inline id bridge_id_cast(CFTypeRef object)
{
    return (__bridge id)object;
}

inline RetainPtr<id> bridge_id_cast(RetainPtr<CFTypeRef>&& object)
{
    return adoptNS(WTF_CF_TO_NS_BRIDGE_TRANSFER(id, object.leakRef()));
}

#undef WTF_NS_TO_CF_BRIDGE_TRANSFER
#undef WTF_CF_TO_NS_BRIDGE_TRANSFER

// Use checked_objc_cast<> instead of dynamic_objc_cast<> when a specific NS type is required.

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define checked_objc_cast checked_objc_castARC
#endif

template <typename ExpectedType>
struct ObjCTypeCastTraits {
public:
    static bool isType(id object) { return [object isKindOfClass:[ExpectedType class]]; }

    template <typename ArgType>
    static bool isType(const ArgType *object) { return [object isKindOfClass:[ExpectedType class]]; }
};

#define SPECIALIZE_OBJC_TYPE_TRAITS(ClassName, ClassExpresssion) \
namespace WTF { \
template <> \
class ObjCTypeCastTraits<ClassName> { \
public: \
    static bool isType(id object) { return [object isKindOfClass:(ClassExpresssion)]; } \
    static bool isType(const NSObject *object) { return [object isKindOfClass:(ClassExpresssion)]; } \
}; \
}

template <typename ExpectedType, typename ArgType>
inline bool is_objc(ArgType * source)
{
    return source && ObjCTypeCastTraits<ExpectedType>::isType(source);
}

template<typename T> inline T *checked_objc_cast(id object)
{
    if (!object)
        return nullptr;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(is_objc<T>(object));

    return reinterpret_cast<T*>(object);
}

template<typename T, typename U> inline T *checked_objc_cast(U *object)
{
    if (!object)
        return nullptr;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(is_objc<T>(object));

    return reinterpret_cast<T*>(object);
}

// Use dynamic_objc_cast<> instead of checked_objc_cast<> when actively checking NS types,
// similar to dynamic_cast<> in C++.

// See RetainPtr.h for: template<typename T> T* dynamic_objc_cast(id object).

template<typename T, typename U> RetainPtr<T> dynamic_objc_cast(RetainPtr<U>&& object)
{
    if (!is_objc<T>(object.get()))
        return nullptr;
    return WTFMove(object);
}

template<typename T, typename U> RetainPtr<T> dynamic_objc_cast(const RetainPtr<U>& object)
{
    if (!is_objc<T>(object.get()))
        return nullptr;
    return reinterpret_cast<T*>(object.get());
}

template<typename T> T *dynamic_objc_cast(NSObject *object)
{
    if (!is_objc<T>(object))
        return nullptr;
    return reinterpret_cast<T*>(object);
}

template<typename T> T *dynamic_objc_cast(id object)
{
    if (!is_objc<T>(object))
        return nullptr;
    return reinterpret_cast<T*>(object);
}

} // namespace WTF

using WTF::bridge_cast;
using WTF::bridge_id_cast;
using WTF::checked_objc_cast;
using WTF::dynamic_objc_cast;
using WTF::is_objc;
