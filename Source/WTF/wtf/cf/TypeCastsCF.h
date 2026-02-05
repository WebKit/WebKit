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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CTFontDescriptor.h>
#include <objc/objc.h>
#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>
#include <wtf/cf/CFTypeTraits.h>

#ifndef CF_BRIDGED_TYPE
#define CF_BRIDGED_TYPE(T)
#endif

namespace WTF {

// Use dynamic_cf_cast<> instead of checked_cf_cast<> when actively checking CF types,
// similar to dynamic_cast<> in C++. Be sure to include a nullptr check.

template<typename T> T dynamic_cf_cast(CFTypeRef object)
{
    if (!object)
        return nullptr;

    if (CFGetTypeID(object) != CFTypeTrait<T>::typeID())
        return nullptr;

    return static_cast<T>(const_cast<CF_BRIDGED_TYPE(id) void*>(object));
}

template<typename T, typename U> RetainPtr<T> dynamic_cf_cast(RetainPtr<U>&& object)
{
    if (!object)
        return nullptr;

    if (CFGetTypeID(object.get()) != CFTypeTrait<T>::typeID())
        return nullptr;

    return adoptCF(static_cast<T>(const_cast<CF_BRIDGED_TYPE(id) void*>(object.leakRef())));
}

// Use checked_cf_cast<> instead of dynamic_cf_cast<> when a specific CF type is required.

template<typename T> T checked_cf_cast(CFTypeRef object)
{
    if (!object)
        return nullptr;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(CFGetTypeID(object) == CFTypeTrait<T>::typeID());

    return static_cast<T>(const_cast<CF_BRIDGED_TYPE(id) void*>(object));
}

// Use bridgeCFCast to convert from id -> CF without ref churn.

inline CFTypeRef bridgeCFCast(id object)
{
#ifdef __OBJC__
    return (__bridge CFTypeRef)object;
#else
    return reinterpret_cast<CFTypeRef>(object);
#endif
}

// Use bridge_id_cast to convert from CF -> id without ref churn.

inline id bridge_id_cast(CFTypeRef object)
{
#ifdef __OBJC__
    return (__bridge id)object;
#else
    return reinterpret_cast<id>(const_cast<void*>(object));
#endif
}

} // namespace WTF

// CTFontDescriptor requires CoreText, so we declare its trait here rather than in CFTypeTraits.h
WTF_DECLARE_CF_TYPE_TRAIT(CTFontDescriptor);

using WTF::bridgeCFCast;
using WTF::bridge_id_cast;
using WTF::checked_cf_cast;
using WTF::dynamic_cf_cast;
