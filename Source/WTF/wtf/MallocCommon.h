/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

class TryMallocReturnValue {
public:
    TryMallocReturnValue(void*);
    TryMallocReturnValue(TryMallocReturnValue&&);
    ~TryMallocReturnValue();
    template<typename T> bool getValue(T*&) WARN_UNUSED_RETURN;
private:
    void operator=(TryMallocReturnValue&&) = delete;
    mutable void* m_data;
};

inline TryMallocReturnValue::TryMallocReturnValue(void* data)
    : m_data(data)
{
}

inline TryMallocReturnValue::TryMallocReturnValue(TryMallocReturnValue&& source)
    : m_data(source.m_data)
{
    source.m_data = nullptr;
}

inline TryMallocReturnValue::~TryMallocReturnValue()
{
    ASSERT(!m_data);
}

template<typename T> inline bool TryMallocReturnValue::getValue(T*& data)
{
    data = static_cast<T*>(m_data);
    m_data = nullptr;
    return data;
}

class ForbidMallocUseForCurrentThreadScope {
public:
#if ASSERT_ENABLED
    WTF_EXPORT_PRIVATE ForbidMallocUseForCurrentThreadScope();
    WTF_EXPORT_PRIVATE ~ForbidMallocUseForCurrentThreadScope();
#else
    ForbidMallocUseForCurrentThreadScope() = default;
    ~ForbidMallocUseForCurrentThreadScope() { }
#endif

    ForbidMallocUseForCurrentThreadScope(const ForbidMallocUseForCurrentThreadScope&) = delete;
    ForbidMallocUseForCurrentThreadScope(ForbidMallocUseForCurrentThreadScope&&) = delete;
    ForbidMallocUseForCurrentThreadScope& operator=(const ForbidMallocUseForCurrentThreadScope&) = delete;
    ForbidMallocUseForCurrentThreadScope& operator=(ForbidMallocUseForCurrentThreadScope&&) = delete;
};

class DisableMallocRestrictionsForCurrentThreadScope {
public:
#if ASSERT_ENABLED
    WTF_EXPORT_PRIVATE DisableMallocRestrictionsForCurrentThreadScope();
    WTF_EXPORT_PRIVATE ~DisableMallocRestrictionsForCurrentThreadScope();
#else
    DisableMallocRestrictionsForCurrentThreadScope() = default;
    ~DisableMallocRestrictionsForCurrentThreadScope() { }
#endif

    DisableMallocRestrictionsForCurrentThreadScope(const DisableMallocRestrictionsForCurrentThreadScope&) = delete;
    DisableMallocRestrictionsForCurrentThreadScope(DisableMallocRestrictionsForCurrentThreadScope&&) = delete;
    DisableMallocRestrictionsForCurrentThreadScope& operator=(const DisableMallocRestrictionsForCurrentThreadScope&) = delete;
    DisableMallocRestrictionsForCurrentThreadScope& operator=(DisableMallocRestrictionsForCurrentThreadScope&&) = delete;
};

#if ASSERT_ENABLED
WTF_EXPORT_PRIVATE void assertMallocRestrictionForCurrentThreadScope();
#else
inline void assertMallocRestrictionForCurrentThreadScope() { }
#endif

} // namespace WTF

using WTF::TryMallocReturnValue;
using WTF::DisableMallocRestrictionsForCurrentThreadScope;
using WTF::ForbidMallocUseForCurrentThreadScope;

// FIXME: de-duplicate with WTF_DEPRECATED_MAKE_FAST_ALLOCATED_IMPL
#define WTF_MAKE_CONFIGURABLE_ALLOCATED_IMPL(alloc, family) \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return alloc::malloc(size); \
    } \
    \
    void operator delete(void* p) \
    { \
        alloc::free(p); \
    } \
    \
    void* operator new[](size_t size) \
    { \
        return alloc::malloc(size); \
    } \
    \
    void operator delete[](void* p) \
    { \
        return alloc::free(p); \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    static void freeAfterDestruction(void* p) \
    { \
        alloc::fastFree(p); \
    } \
    using WTFIs ## family ## Allocated  = int; \

#if ENABLE(MALLOC_HEAP_BREAKDOWN)
// FIXME: de-duplicate with WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL
#define WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc, family) \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return classname##Malloc::malloc(size); \
    } \
    \
    void operator delete(void* p) \
    { \
        classname##Malloc::free(p); \
    } \
    \
    void* operator new[](size_t size) \
    { \
        return classname##Malloc::malloc(size); \
    } \
    \
    void operator delete[](void* p) \
    { \
        classname##Malloc::free(p); \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    static void freeAfterDestruction(void* p) \
    { \
        classname##Malloc::free(p); \
    } \
    using WTFIs ## family ## Allocated = int; \

#define WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(classname, alloc) \
public: \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc, alloc) \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(classname, alloc) \
private: \
public: \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc, alloc) \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_CONFIGURABLE_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(classname) \
    WTF_ALLOW_COMPACT_POINTERS; \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(classname)

#define WTF_MAKE_STRUCT_CONFIGURABLE_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(classname) \
    WTF_ALLOW_STRUCT_COMPACT_POINTERS; \
    WTF_MAKE_STRUCT_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(classname)

#else

#define WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc) \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_IMPL(alloc, alloc)

#define WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(classname, alloc) \
public: \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc) \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(classname) \
public: \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname) \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_CONFIGURABLE_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc) \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_IMPL(alloc::CompactMalloc, alloc)

#define WTF_MAKE_CONFIGURABLE_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(classname, alloc) \
public: \
    WTF_MAKE_CONFIGURABLE_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc) \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_CONFIGURABLE_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(classname, alloc) \
public: \
    WTF_MAKE_CONFIGURABLE_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname, alloc) \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#endif // ENABLE(MALLOC_HEAP_BREAKDOWN)

#define WTF_MAKE_CONFIGURABLE_ALLOCATED(alloc) \
public: \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_IMPL(alloc, alloc) \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_CONFIGURABLE_ALLOCATED(alloc) \
    WTF_MAKE_CONFIGURABLE_ALLOCATED_IMPL(alloc, alloc) \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#if OS(DARWIN)
#define WTF_PRIVATE_INLINE __private_extern__ inline __attribute__((always_inline))
#else
#define WTF_PRIVATE_INLINE inline __attribute__((always_inline))
#endif
