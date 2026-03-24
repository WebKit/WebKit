/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace WTF {

template<typename T>
constexpr bool requiresGCAwareContainer()
{
    if constexpr (requires { std::remove_pointer_t<T>::requiresGCAwareContainer; })
        return true;
    return false;
}

} // namespace WTF

#define WTF_FORBID_GC_ELEMENT_TYPE_USING_SFINAE(type__) \
    static_assert(!std::is_same_v<type__, type__>, #type__ " should not be stored in a heap allocated buffer")

#define WTF_FORBID_GC_ELEMENT_TYPE_IN_STD_VECTOR(type__) \
    namespace std { \
        template<typename Allocator> \
        class vector<type__, Allocator> { \
            WTF_FORBID_GC_ELEMENT_TYPE_USING_SFINAE(type__); \
        }; \
    }

#define WTF_FORBID_GC_ELEMENT_TYPE_IN_STD_UNORDERED_MAP_VALUES(type__) \
    namespace std { \
        template<class Key, class Hash, class KeyEqual> \
        class unordered_map<Key, type__, Hash, KeyEqual> { \
            WTF_FORBID_GC_ELEMENT_TYPE_USING_SFINAE(type__); \
        }; \
    }

#define WTF_FORBID_GC_ELEMENT_TYPE_IN_STD_CONTAINERS(type__) \
    WTF_FORBID_GC_ELEMENT_TYPE_IN_STD_VECTOR(type__) \
    WTF_FORBID_GC_ELEMENT_TYPE_IN_STD_UNORDERED_MAP_VALUES(type__) \

#define WTF_DECLARE_REQUIRES_GC_AWARE_CONTAINER(type__) \
    namespace WTF { \
        template<> \
        constexpr bool requiresGCAwareContainer<type__>() { return true; } \
    } \
    WTF_FORBID_GC_ELEMENT_TYPE_IN_STD_CONTAINERS(type__)

extern "C" {

// We'd normally expect to put these overloads where we declare the types but
// that's not possible here since these types are declared in public headers.
// In order to make sure folks don't accidentally end up failing to include
// some header with these checks, we put them here.
// All other overloads should go where the type is declared.
typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSContext* JSGlobalContextRef;
typedef const struct OpaqueJSValue* JSValueRef;
typedef struct OpaqueJSValue* JSObjectRef;

} // extern "C"

template<typename T>
static constexpr bool isJSCAPIValueType()
{
    if constexpr (std::is_same_v<T, JSContextRef>
        || std::is_same_v<T, JSGlobalContextRef>
        || std::is_same_v<T, JSValueRef>
        || std::is_same_v<T, JSObjectRef>)
        return true;
    return false;
}

WTF_DECLARE_REQUIRES_GC_AWARE_CONTAINER(JSContextRef)
WTF_DECLARE_REQUIRES_GC_AWARE_CONTAINER(JSGlobalContextRef)
WTF_DECLARE_REQUIRES_GC_AWARE_CONTAINER(JSValueRef)
WTF_DECLARE_REQUIRES_GC_AWARE_CONTAINER(JSObjectRef)
