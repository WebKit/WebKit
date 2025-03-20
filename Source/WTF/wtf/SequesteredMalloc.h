/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstdlib>
#include <new>
#include <wtf/DebugHeap.h>
#include <wtf/FastMalloc.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>

#if USE(PROTECTED_JIT)

namespace WTF {

// Arena Allocator

#if ASSERT_ENABLED
WTF_EXPORT_PRIVATE void sequesteredArenaSetMaxSingleAllocationSize(size_t);
#endif

WTF_EXPORT_PRIVATE bool isSequesteredArenaMallocEnabled();

WTF_EXPORT_PRIVATE void* sequesteredArenaMalloc(size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* sequesteredArenaZeroedMalloc(size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* sequesteredArenaCalloc(size_t numElements, size_t elementSize) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* sequesteredArenaRealloc(void*, size_t) RETURNS_NONNULL;

WTF_EXPORT_PRIVATE TryMallocReturnValue trySequesteredArenaMalloc(size_t);
WTF_EXPORT_PRIVATE TryMallocReturnValue trySequesteredArenaZeroedMalloc(size_t);
WTF_EXPORT_PRIVATE TryMallocReturnValue trySequesteredArenaCalloc(size_t numElements, size_t elementSize);
WTF_EXPORT_PRIVATE TryMallocReturnValue trySequesteredArenaRealloc(void*, size_t);

WTF_EXPORT_PRIVATE void sequesteredArenaFree(void*);

// Allocations from sequesteredArenaAlignedMalloc() must be freed using sequesteredArenaAlignedFree().
WTF_EXPORT_PRIVATE void* sequesteredArenaAlignedMalloc(size_t alignment, size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* trySequesteredArenaAlignedMalloc(size_t alignment, size_t);
WTF_EXPORT_PRIVATE void sequesteredArenaAlignedFree(void*);

struct SequesteredArenaMallocStatistics { /* FIXME: add statistics */ };
WTF_EXPORT_PRIVATE SequesteredArenaMallocStatistics sequesteredArenaMallocStatistics();

WTF_EXPORT_PRIVATE void sequesteredArenaMallocDumpMallocStats();

template<typename T>
class SaAllocator {
public:
    using value_type = T;

    SaAllocator() = default;

    template<typename U> SaAllocator(const SaAllocator<U>&) { }

    T* allocate(size_t count)
    {
        return static_cast<T*>(sequesteredArenaMalloc(sizeof(T) * count));
    }

    void deallocate(T* pointer, size_t)
    {
        sequesteredArenaFree(pointer);
    }

    template <typename U>
    struct rebind {
        using other = SaAllocator<U>;
    };
};

template<typename T, typename U> inline bool operator==(const SaAllocator<T>&, const SaAllocator<U>&) { return true; }

struct SequesteredArenaMalloc {
    static void* malloc(size_t size) { return sequesteredArenaMalloc(size); }

    static void* tryMalloc(size_t size)
    {
        auto result = trySequesteredArenaMalloc(size);
        void* realResult;
        if (result.getValue(realResult))
            return realResult;
        return nullptr;
    }

    static void* zeroedMalloc(size_t size) { return sequesteredArenaZeroedMalloc(size); }

    static void* tryZeroedMalloc(size_t size)
    {
        auto result = trySequesteredArenaZeroedMalloc(size);
        void* realResult;
        if (result.getValue(realResult))
            return realResult;
        return nullptr;
    }

    static void* realloc(void* p, size_t size) { return sequesteredArenaRealloc(p, size); }

    static void* tryRealloc(void* p, size_t size)
    {
        auto result = trySequesteredArenaRealloc(p, size);
        void* realResult;
        if (result.getValue(realResult))
            return realResult;
        return nullptr;
    }

    static void free(void* p) { sequesteredArenaFree(p); }

    static constexpr ALWAYS_INLINE size_t nextCapacity(size_t capacity)
    {
        return capacity + capacity / 4 + 1;
    }
};

template<typename T>
struct SaFree {
    static_assert(std::is_trivially_destructible<T>::value);

    void operator()(T* pointer) const
    {
        sequesteredArenaFree(const_cast<typename std::remove_cv<T>::type*>(pointer));
    }
};

}

using WTF::SaAllocator;
using WTF::SequesteredArenaMalloc;
using WTF::SaFree;
using WTF::ForbidMallocUseForCurrentThreadScope;
using WTF::isSequesteredArenaMallocEnabled;
using WTF::sequesteredArenaCalloc;
using WTF::sequesteredArenaFree;
using WTF::sequesteredArenaMalloc;
using WTF::sequesteredArenaRealloc;
using WTF::sequesteredArenaZeroedMalloc;
using WTF::trySequesteredArenaAlignedMalloc;
using WTF::trySequesteredArenaCalloc;
using WTF::trySequesteredArenaMalloc;
using WTF::trySequesteredArenaZeroedMalloc;
using WTF::sequesteredArenaAlignedMalloc;
using WTF::sequesteredArenaAlignedFree;

#include <wtf/SequesteredAllocator.h>
#include <wtf/SequesteredImmortalHeap.h>

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_COMMON \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return ::WTF::sequesteredArenaMalloc(size); \
    } \
    \
    void operator delete(void* p) \
    { \
        ::WTF::sequesteredArenaFree(p); \
    } \
    \
    void* operator new[](size_t size) \
    { \
        return ::WTF::sequesteredArenaMalloc(size); \
    } \
    \
    void operator delete[](void* p) \
    { \
        ::WTF::sequesteredArenaFree(p); \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    static void freeAfterDestruction(void* p) \
    { \
        ::WTF::sequesteredArenaFree(p); \
    } \
    using WTFIsSaAllocated = int; \

#define WTF_MAKE_SEQUESTERED_ARENA_NONALLOCATABLE(name) WTF_FORBID_HEAP_ALLOCATION

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED(name) \
public: \
    WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_COMMON \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_FALLBACK_FAST_ALLOCATED(name) \
    WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED(name)

#define WTF_MAKE_STRUCT_SEQUESTERED_ARENA_ALLOCATED(name) \
    WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_COMMON \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_SEQUESTERED_ARENA_ALLOCATED_FALLBACK_FAST_ALLOCATED(name) \
    WTF_MAKE_STRUCT_SEQUESTERED_ARENA_ALLOCATED(name)

#define WTF_MAKE_COMPACT_SEQUESTERED_ARENA_ALLOCATED(name) \
public: \
    WTF_MAKE_COMPACT_SEQUESTERED_ARENA_ALLOCATED_COMMON \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_COMPACT_SEQUESTERED_ARENA_ALLOCATED_EXPORT(name, exportMacro) \
    WTF_MAKE_COMPACT_SEQUESTERED_ARENA_ALLOCATED(name)

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_EXPORT(name, exportMacro) \
    WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED(name)

#define WTF_MAKE_STRUCT_COMPACT_SEQUESTERED_ARENA_ALLOCATED(name) \
    WTF_MAKE_COMPACT_SEQUESTERED_ARENA_ALLOCATED_COMMON \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE(name) WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED(name)
#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type)

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE_IMPL_WITH_MULTIPLE_OR_SPECIALIZED_PARAMETERS()

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_IMPL(type)

// Immortal Allocator

// This is not a proper malloc implementation as it effectively leaks all
// allocations, and does not support many of the variants, so much of the
// machinery used in the other mallocs is intentionally left out

namespace WTF {
WTF_EXPORT_PRIVATE void* sequesteredImmortalMalloc(size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* sequesteredImmortalAlignedMalloc(size_t alignment, size_t) RETURNS_NONNULL;
}
using WTF::sequesteredImmortalMalloc;
using WTF::sequesteredImmortalAlignedMalloc;

#define WTF_MAKE_SEQUESTERED_IMMORTAL_ALLOCATED_COMMON \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return ::WTF::sequesteredImmortalMalloc(size); \
    } \
    \
    void operator delete(void*) { } \
    \
    void* operator new[](size_t size) \
    { \
        return ::WTF::sequesteredImmortalMalloc(size); \
    } \
    \
    void operator delete[](void*) \
    { \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    static void freeAfterDestruction(void*) { } \
    using WTFIsSiAllocated = int; \

#define WTF_MAKE_SEQUESTERED_IMMORTAL_ALLOCATED(type) \
public: \
    WTF_MAKE_SEQUESTERED_IMMORTAL_ALLOCATED_COMMON \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_SEQUESTERED_IMMORTAL_ALLOCATED(type) \
    WTF_MAKE_SEQUESTERED_IMMORTAL_ALLOCATED_COMMON \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#else // !USE(PROTECTED_JIT)

// Arena Allocator

namespace WTF {
using SequesteredArenaMalloc = FastMalloc;
}

using WTF::SequesteredArenaMalloc;

#define WTF_MAKE_SEQUESTERED_ARENA_NONALLOCATABLE(name) WTF_FORBID_HEAP_ALLOCATION

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED(name) WTF_MAKE_TZONE_ALLOCATED(name)
#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_FALLBACK_FAST_ALLOCATED(name) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_EXPORT(name, exportMacro) WTF_MAKE_TZONE_ALLOCATED_EXPORT(name, exportMacro)

#define WTF_MAKE_COMPACT_SEQUESTERED_ARENA_ALLOCATED(name) WTF_MAKE_COMPACT_TZONE_ALLOCATED(name)
#define WTF_MAKE_COMPACT_SEQUESTERED_ARENA_ALLOCATED_EXPORT(name, exportMacro) WTF_MAKE_COMPACT_TZONE_ALLOCATED_EXPORT(name, exportMacro)

#define WTF_MAKE_STRUCT_SEQUESTERED_ARENA_ALLOCATED(name) WTF_MAKE_STRUCT_TZONE_ALLOCATED(name)
#define WTF_MAKE_STRUCT_SEQUESTERED_ARENA_ALLOCATED_FALLBACK_FAST_ALLOCATED(name) WTF_MAKE_FAST_ALLOCATED

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_IMPL(type) WTF_MAKE_TZONE_ALLOCATED_IMPL(type)

#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE(name) WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(name)
#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type) WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type)
#define WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE_IMPL_WITH_MULTIPLE_OR_SPECIALIZED_PARAMETERS() \
    WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL_WITH_MULTIPLE_OR_SPECIALIZED_PARAMETERS()

// Immortal Allocator

#define WTF_MAKE_SEQUESTERED_IMMORTAL_ALLOCATED(type) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_STRUCT_SEQUESTERED_IMMORTAL_ALLOCATED(type) WTF_MAKE_FAST_ALLOCATED

#endif // !USE(PROTECTED_JIT)
