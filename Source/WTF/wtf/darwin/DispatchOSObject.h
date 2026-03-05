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

#include <dispatch/dispatch.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/darwin/TypeCastsOSObject.h>

#define WTF_OS_OBJECT_DISPATCH_TYPES(M) \
    M(dispatch_data) \
    M(dispatch_group) \
    M(dispatch_io) \
    M(dispatch_object) \
    M(dispatch_queue) \
    M(dispatch_queue_global) \
    M(dispatch_semaphore) \
    M(dispatch_source)

// Forward declarations for dispatch base struct types.
WTF_EXTERN_C_BEGIN
#define WTF_DECLARE_OS_OBJECT_DISPATCH_BASE_STRUCT(TypeName) \
struct TypeName##_s;
WTF_OS_OBJECT_DISPATCH_TYPES(WTF_DECLARE_OS_OBJECT_DISPATCH_BASE_STRUCT)
#undef WTF_DECLARE_OS_OBJECT_DISPATCH_BASE_STRUCT
WTF_EXTERN_C_END

namespace WTF {

// Dispatch OSObject type cast traits.
#define WTF_DECLARE_OS_OBJECT_DISPATCH_TYPE_CAST_TRAITS(TypeName) \
WTF_DECLARE_OS_OBJECT_TYPE_CAST_TRAITS_INTERNAL(TypeName, _s)
WTF_OS_OBJECT_DISPATCH_TYPES(WTF_DECLARE_OS_OBJECT_DISPATCH_TYPE_CAST_TRAITS)
#undef WTF_DECLARE_OS_OBJECT_DISPATCH_TYPE_CAST_TRAITS

// Dispatch isOSObject functions.
#define WTF_IMPLEMENT_IS_OS_OBJECT_FUNCTIONS_DISPATCH(TypeName) WTF_IMPLEMENT_IS_OS_OBJECT_FUNCTIONS_INTERNAL(TypeName##_t, STRINGIZE_VALUE_OF(OS_OBJECT_CLASS(TypeName)))
WTF_OS_OBJECT_DISPATCH_TYPES(WTF_IMPLEMENT_IS_OS_OBJECT_FUNCTIONS_DISPATCH)
#undef WTF_IMPLEMENT_IS_OS_OBJECT_FUNCTIONS_DISPATCH

// Dispatch protect() functions.
#define WTF_DECLARE_DISPATCH_PROTECT(TypeName) \
ALWAYS_INLINE CLANG_POINTER_CONVERSION OSObjectPtr<TypeName##_t> protect(TypeName##_t ptr) \
{ \
    return ptr; \
}
WTF_OS_OBJECT_DISPATCH_TYPES(WTF_DECLARE_DISPATCH_PROTECT)
#undef WTF_DECLARE_DISPATCH_PROTECT

#if !__has_feature(objc_arc)
// Template specializations for dispatch retain/release traits (non-ARC only).
#define WTF_DECLARE_DISPATCH_OSOBJECT_RETAIN_TRAITS(TypeName) \
template<> \
struct DefaultOSObjectRetainTraits<TypeName##_t, std::false_type> { \
    static ALWAYS_INLINE void retain(TypeName##_t ptr) \
    { \
        dispatch_retain(ptr); \
    } \
    static ALWAYS_INLINE void release(TypeName##_t ptr) \
    { \
        dispatch_release(ptr); \
    } \
}; \

WTF_OS_OBJECT_DISPATCH_TYPES(WTF_DECLARE_DISPATCH_OSOBJECT_RETAIN_TRAITS)
#undef WTF_DECLARE_DISPATCH_OSOBJECT_RETAIN_TRAITS
#endif // !__has_feature(objc_arc)

} // namespace WTF

using WTF::protect;
