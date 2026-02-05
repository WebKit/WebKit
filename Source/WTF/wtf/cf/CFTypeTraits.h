/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if USE(CF)

#include <CoreFoundation/CoreFoundation.h>
#include <type_traits>

namespace WTF {

// CFTypeTrait primary template - specializations provide typeID() for specific CF types.
template <typename> struct CFTypeTrait;

} // namespace WTF

// Macro to declare CFTypeTrait specializations.
#define WTF_DECLARE_CF_TYPE_TRAIT(ClassName) \
template <> \
struct WTF::CFTypeTrait<ClassName##Ref> { \
    static inline CFTypeID typeID() { return ClassName##GetTypeID(); } \
};

#define WTF_DECLARE_CF_MUTABLE_TYPE_TRAIT(ClassName, MutableClassName) \
template <> \
struct WTF::CFTypeTrait<MutableClassName##Ref> { \
    static inline CFTypeID typeID() { return ClassName##GetTypeID(); } \
};

// Standard CF type specializations.
WTF_DECLARE_CF_TYPE_TRAIT(CFArray);
WTF_DECLARE_CF_TYPE_TRAIT(CFBoolean);
WTF_DECLARE_CF_TYPE_TRAIT(CFData);
WTF_DECLARE_CF_TYPE_TRAIT(CFDictionary);
WTF_DECLARE_CF_TYPE_TRAIT(CFNumber);
WTF_DECLARE_CF_TYPE_TRAIT(CFRunLoop);
WTF_DECLARE_CF_TYPE_TRAIT(CFRunLoopSource);
WTF_DECLARE_CF_TYPE_TRAIT(CFRunLoopTimer);
WTF_DECLARE_CF_TYPE_TRAIT(CFString);
WTF_DECLARE_CF_TYPE_TRAIT(CFURL);

// Mutable CF type specializations.
WTF_DECLARE_CF_MUTABLE_TYPE_TRAIT(CFArray, CFMutableArray);
WTF_DECLARE_CF_MUTABLE_TYPE_TRAIT(CFData, CFMutableData);
WTF_DECLARE_CF_MUTABLE_TYPE_TRAIT(CFDictionary, CFMutableDictionary);
WTF_DECLARE_CF_MUTABLE_TYPE_TRAIT(CFString, CFMutableString);

namespace WTF {

namespace detail {

template<typename T, typename = void>
inline constexpr bool HasCFTypeTraitHelper = false;

template<typename T>
inline constexpr bool HasCFTypeTraitHelper<T, std::void_t<decltype(CFTypeTrait<T>::typeID())>> = true;

} // namespace detail

template<typename T>
inline constexpr bool HasCFTypeTrait = detail::HasCFTypeTraitHelper<T>;

// IsCFType: true for CFTypeRef or any type with a CFTypeTrait specialization.
template<typename T>
concept IsCFType = std::is_pointer_v<T> && (
    std::is_same_v<std::remove_cv_t<T>, CFTypeRef> || HasCFTypeTrait<T>
);

} // namespace WTF

using WTF::HasCFTypeTrait;
using WTF::IsCFType;

#endif // USE(CF)
