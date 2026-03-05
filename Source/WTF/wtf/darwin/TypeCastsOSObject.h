/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import <CoreFoundation/CFBase.h>
#import <objc/runtime.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/cf/TypeCastsCF.h>

// See DispatchOSObject.h or NetworkOSObject.h for how to add support
// for a new OSObject type.

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define dynamicOSObjectCast dynamicOSObjectCastARC
#define osObjectCast osObjectCastARC
#endif

namespace WTF {

template<typename> struct OSObjectTypeCastTraits;

// Common macro for declaring OSObjectTypeCastTraits.
#define WTF_DECLARE_OS_OBJECT_TYPE_CAST_TRAITS_INTERNAL(TypeName, Suffix) \
template<> struct OSObjectTypeCastTraits<TypeName##_t> { \
    using BaseType = struct TypeName##Suffix*; \
}; \

// Must define this for isOSObject<T>() when not building with Objective-C.
#ifndef OS_OBJECT_CLASS
#define OS_OBJECT_CLASS(name) OS_##name
#endif

template<typename T> bool isOSObject(CFTypeRef);

// Common macro for implementing isOSObject functions.
#define WTF_IMPLEMENT_IS_OS_OBJECT_FUNCTIONS_INTERNAL(TypeName, ProtocolString) \
template<> inline bool isOSObject<OSObjectTypeCastTraits<TypeName>::BaseType>(CFTypeRef object) \
{ \
    RetainPtr cls = object_getClass(bridge_id_cast(object)); \
    return class_conformsToProtocol(cls.get(), objc_getProtocol(ProtocolString)); \
} \

#ifdef __OBJC__

template<typename T> inline bool isOSObject(id object)
{
    return isOSObject<T>(bridgeCFCast(object));
}

template<typename T> inline T osObjectCast(id object)
{
    if (!object)
        return nullptr;

    RELEASE_ASSERT(isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object));

    return static_cast<T>(object);
}

// The dynamicOSObjectCast<T>() methods that have OSObjectPtr<U> arguments use different template types
// in Objective-C++ vs. C++, so these Objective-C method definitions do not create an ODR violation
// with the C++ method definitions below.

template<typename T, typename U> requires (!std::is_same_v<U, T>)
inline OSObjectPtr<T> dynamicOSObjectCast(OSObjectPtr<U>&& object)
{
    if (!object)
        return nullptr;

    if (!isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object.get()))
        return nullptr;

    return adoptOSObject(static_cast<T>(object.leakRef()));
}

template<typename T, typename U> requires (!std::is_same_v<U, T>)
inline OSObjectPtr<T> dynamicOSObjectCast(const OSObjectPtr<U>& object)
{
    if (!object)
        return nullptr;

    if (!isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object.get()))
        return nullptr;

    return static_cast<T*>(object.get());
}

template<typename T> inline T dynamicOSObjectCast(id object)
{
    if (!object)
        return nullptr;

    if (!isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object))
        return nullptr;

    return reinterpret_cast<T>(object);
}

#endif // defined(__OBJC__)

template<typename T> inline T osObjectCast(CFTypeRef object)
{
    if (!object)
        return nullptr;

    RELEASE_ASSERT(isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object));

    return static_cast<T>(const_cast<CF_BRIDGED_TYPE(id) void*>(object));
}

template<typename T> inline T dynamicOSObjectCast(CFTypeRef object)
{
    if (!object)
        return nullptr;

    if (!isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object))
        return nullptr;

    return static_cast<T>(const_cast<CF_BRIDGED_TYPE(id) void*>(object));
}

#ifndef __OBJC__

template<typename T, typename U> requires (!std::is_same_v<U, T>)
inline OSObjectPtr<T> dynamicOSObjectCast(OSObjectPtr<U>&& object)
{
    if (!object)
        return nullptr;

    if (!isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object.get()))
        return nullptr;

    return adoptOSObject(static_cast<T>(object.leakRef()));
}

template<typename T, typename U> requires (!std::is_same_v<U, T>)
inline OSObjectPtr<T> dynamicOSObjectCast(const OSObjectPtr<U>& object)
{
    if (!object)
        return nullptr;

    if (!isOSObject<typename OSObjectTypeCastTraits<T>::BaseType>(object.get()))
        return nullptr;

    return static_cast<T*>(object.get());
}

#endif // !defined(__OBJC__)

} // namespace WTF

using WTF::dynamicOSObjectCast;
using WTF::osObjectCast;
