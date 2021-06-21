/*
 *  Copyright (C) 2005-2018 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <new>
#include <stdlib.h>
#include <wtf/DebugHeap.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// There are several malloc-related macros to annotate class / struct. If these annotations are attached,
// allocation is handled by bmalloc if bmalloc is available.
//
// TLDR; Here is a quick guidance.
//
//   1. If your class / struct is derived from a base class which uses WTF_MAKE_ISO_ALLOCATED / WTF_MAKE_ISO_ALLOCATED_EXPORT,
//      you must use WTF_MAKE_ISO_ALLOCATED / WTF_MAKE_ISO_ALLOCATED_EXPORT.
//   2. If your class / struct is a DOM object, use WTF_MAKE_ISO_ALLOCATED.
//   3. If your class / struct is particularly memory consuming and if you think tracking footprint of your class is helpful for memory-reduction work,
//      use WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER / WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER.
//   4. Otherwise, use WTF_MAKE_FAST_ALLOCATED / WTF_MAKE_STRUCT_FAST_ALLOCATED.
//
// Let's explain the differences in detail.
//
// - WTF_MAKE_FAST_ALLOCATED
// - WTF_MAKE_STRUCT_FAST_ALLOCATED
//     class / struct is allocated with FastMalloc (bmalloc if it is available). We encourage using FastMalloc for all the class / struct allocations if possible
//     to avoid using system malloc. If a class is not having WTF_MAKE_FAST_ALLOCATED, the class will be allocated with system malloc, which is less efficient
//     compared to FastMalloc in terms of performance and memory footprint. We would like to minimize the use of system malloc.
//     These annotations should be the default choice for allocations.
//     WTF_MAKE_FAST_ALLOCATED is for classes and WTF_MAKE_STRUCT_FAST_ALLOCATED is for structs. The difference between them is how we
//     use `public:` / `private:` access specifiers in the expanded macro.
//
// - WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ClassName)
// - WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ClassName)
//     Normally, these are identical to WTF_MAKE_FAST_ALLOCATED.
//     When the MallocHeapBreakdown debugging feature is enabled, these macros act differently. bmalloc is switched
//     to using system malloc internally, and bmalloc creates a named MallocZone per this annotation. MallocZone allows us to track heap usage
//     per this annotation, offering quick way of breaking down memory usage.
//     To enable MallocHeapBreakdown, set ENABLE_MALLOC_HEAP_BREAKDOWN in WTF and BENABLE_MALLOC_HEAP_BREAKDOWN in bmalloc.
//     For the details of MallocHeapBreakdown, please look at ChangeLog of https://trac.webkit.org/changeset/253987/webkit.
//
// - WTF_MAKE_ISO_ALLOCATED(ClassName)
// - WTF_MAKE_ISO_ALLOCATED_EXPORT(ClassName, exportMacro)
//     class / struct is allocated from bmalloc IsoHeap. IsoHeap assigns virtual address only for particular type,
//     so that this avoids use-after-free based type punning. We are adopting IsoHeap mainly for class / struct which is exposed to user JavaScript (e.g. DOM objects).
//     For example , all the derived classes of ScriptWrappable must be allocated in IsoHeap.
//     Unlike the other macros, you need to annotate each derived class with WTF_MAKE_ISO_ALLOCATED if your base class is annotated with WTF_MAKE_ISO_ALLOCATED.
//     When you annotate the class with WTF_MAKE_ISO_ALLOCATED(XXX), you need to add WTF_MAKE_ISO_ALLOCATED_IMPL(XXX) in cpp file side.
//     Because WTF_MAKE_ISO_ALLOCATED_IMPL defines functions in cpp side, you sometimes need to annotate these functions with export macros when your class is
//     used outside of the component defining your class (e.g. your class is in WebCore and it is also used in WebKit). In this case, you can use WTF_MAKE_ISO_ALLOCATED_EXPORT
//     to annotate these functions with appropriate export macros: e.g. WTF_MAKE_ISO_ALLOCATED_EXPORT(IDBTransaction, WEBCORE_EXPORT).

#if !defined(NDEBUG)
WTF_EXPORT_PRIVATE void fastSetMaxSingleAllocationSize(size_t);
#endif

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

WTF_EXPORT_PRIVATE bool isFastMallocEnabled();

// These functions call CRASH() if an allocation fails.
WTF_EXPORT_PRIVATE void* fastMalloc(size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* fastZeroedMalloc(size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* fastCalloc(size_t numElements, size_t elementSize) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* fastRealloc(void*, size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE char* fastStrDup(const char*) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* fastMemDup(const void*, size_t);

WTF_EXPORT_PRIVATE TryMallocReturnValue tryFastMalloc(size_t);
WTF_EXPORT_PRIVATE TryMallocReturnValue tryFastZeroedMalloc(size_t);
WTF_EXPORT_PRIVATE TryMallocReturnValue tryFastCalloc(size_t numElements, size_t elementSize);
WTF_EXPORT_PRIVATE TryMallocReturnValue tryFastRealloc(void*, size_t);

WTF_EXPORT_PRIVATE void fastFree(void*);

// Allocations from fastAlignedMalloc() must be freed using fastAlignedFree().
WTF_EXPORT_PRIVATE void* fastAlignedMalloc(size_t alignment, size_t) RETURNS_NONNULL;
WTF_EXPORT_PRIVATE void* tryFastAlignedMalloc(size_t alignment, size_t);
WTF_EXPORT_PRIVATE void fastAlignedFree(void*);

WTF_EXPORT_PRIVATE size_t fastMallocSize(const void*);

// FIXME: This is non-helpful; fastMallocGoodSize will be removed soon.
WTF_EXPORT_PRIVATE size_t fastMallocGoodSize(size_t);

WTF_EXPORT_PRIVATE void releaseFastMallocFreeMemory();
WTF_EXPORT_PRIVATE void releaseFastMallocFreeMemoryForThisThread();

WTF_EXPORT_PRIVATE void fastCommitAlignedMemory(void*, size_t);
WTF_EXPORT_PRIVATE void fastDecommitAlignedMemory(void*, size_t);

WTF_EXPORT_PRIVATE void fastEnableMiniMode();

WTF_EXPORT_PRIVATE void fastDisableScavenger();

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

struct FastMallocStatistics {
    size_t reservedVMBytes;
    size_t committedVMBytes;
    size_t freeListBytes;
};
WTF_EXPORT_PRIVATE FastMallocStatistics fastMallocStatistics();

WTF_EXPORT_PRIVATE void fastMallocDumpMallocStats();

// This defines a type which holds an unsigned integer and is the same
// size as the minimally aligned memory allocation.
typedef unsigned long long AllocAlignmentInteger;

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

// C++ STL allocator implementation. You can integrate fastMalloc into STL containers.
// e.g. std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, FastAllocator<std::pair<const Key, Value>>>.
template<typename T>
class FastAllocator {
public:
    using value_type = T;

    FastAllocator() = default;

    template<typename U> FastAllocator(const FastAllocator<U>&) { }

    T* allocate(size_t count)
    {
        return static_cast<T*>(fastMalloc(sizeof(T) * count));
    }

    void deallocate(T* pointer, size_t)
    {
        fastFree(pointer);
    }

#if defined(__GLIBCXX__) && (!defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE < 6)
    // This allocator also supports pre-C++11 STL allocator interface. This is a workaround for GCC < 6, which std::list
    // does not support C++11 allocator. Note that _GLIBCXX_RELEASE is only defined after GCC 7 release. So currently
    // this workaround is enabled in GCC 6 too.
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55409

    using pointer = value_type*;
    using const_pointer = typename std::pointer_traits<pointer>::template rebind<value_type const>;
    using void_pointer = typename std::pointer_traits<pointer>::template rebind<void>;
    using const_void_pointer = typename std::pointer_traits<pointer>::template rebind<const void>;

    using reference = T&;
    using const_reference = const T&;

    using difference_type = typename std::pointer_traits<pointer>::difference_type;
    using size_type = std::make_unsigned_t<difference_type>;

    template <class U> struct rebind {
        using other = FastAllocator<U>;
    };

    value_type* allocate(std::size_t count, const_void_pointer)
    {
        return allocate(count);
    }

    template <class U, class ...Args>
    void construct(U* p, Args&& ...args)
    {
        new (const_cast<void*>(static_cast<const void*>(p))) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U* p)
    {
        p->~U();
    }

    std::size_t max_size() const
    {
        return std::numeric_limits<size_type>::max();
    }

    FastAllocator<T> select_on_container_copy_construction() const
    {
        return *this;
    }

    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::is_empty<FastAllocator>;
#endif // defined(__GLIBCXX__) && (!defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE < 6)
};

template<typename T, typename U> inline bool operator==(const FastAllocator<T>&, const FastAllocator<U>&) { return true; }
template<typename T, typename U> inline bool operator!=(const FastAllocator<T>&, const FastAllocator<U>&) { return false; }

struct FastMalloc {
    static void* malloc(size_t size) { return fastMalloc(size); }
    
    static void* tryMalloc(size_t size)
    {
        auto result = tryFastMalloc(size);
        void* realResult;
        if (result.getValue(realResult))
            return realResult;
        return nullptr;
    }

    static void* zeroedMalloc(size_t size) { return fastZeroedMalloc(size); }

    static void* tryZeroedMalloc(size_t size)
    {
        auto result = tryFastZeroedMalloc(size);
        void* realResult;
        if (result.getValue(realResult))
            return realResult;
        return nullptr;
    }
    
    static void* realloc(void* p, size_t size) { return fastRealloc(p, size); }

    static void* tryRealloc(void* p, size_t size)
    {
        auto result = tryFastRealloc(p, size);
        void* realResult;
        if (result.getValue(realResult))
            return realResult;
        return nullptr;
    }
    
    static void free(void* p) { fastFree(p); }
};

template<typename T>
struct FastFree {
    static_assert(std::is_trivially_destructible<T>::value, "");

    void operator()(T* pointer) const
    {
        fastFree(const_cast<typename std::remove_cv<T>::type*>(pointer));
    }
};

template<typename T>
struct FastFree<T[]> {
    static_assert(std::is_trivially_destructible<T>::value, "");

    void operator()(T* pointer) const
    {
        fastFree(const_cast<typename std::remove_cv<T>::type*>(pointer));
    }
};

} // namespace WTF

#if !defined(NDEBUG)
using WTF::fastSetMaxSingleAllocationSize;
#endif

using WTF::DisableMallocRestrictionsForCurrentThreadScope;
using WTF::FastAllocator;
using WTF::FastMalloc;
using WTF::FastFree;
using WTF::ForbidMallocUseForCurrentThreadScope;
using WTF::isFastMallocEnabled;
using WTF::fastCalloc;
using WTF::fastFree;
using WTF::fastMalloc;
using WTF::fastMallocGoodSize;
using WTF::fastMallocSize;
using WTF::fastMemDup;
using WTF::fastRealloc;
using WTF::fastStrDup;
using WTF::fastZeroedMalloc;
using WTF::tryFastAlignedMalloc;
using WTF::tryFastCalloc;
using WTF::tryFastMalloc;
using WTF::tryFastZeroedMalloc;
using WTF::fastAlignedMalloc;
using WTF::fastAlignedFree;

#if COMPILER(GCC_COMPATIBLE) && OS(DARWIN)
#define WTF_PRIVATE_INLINE __private_extern__ inline __attribute__((always_inline))
#elif COMPILER(GCC_COMPATIBLE)
#define WTF_PRIVATE_INLINE inline __attribute__((always_inline))
#elif COMPILER(MSVC)
#define WTF_PRIVATE_INLINE __forceinline
#else
#define WTF_PRIVATE_INLINE inline
#endif

#define WTF_MAKE_FAST_ALLOCATED_IMPL \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return ::WTF::fastMalloc(size); \
    } \
    \
    void operator delete(void* p) \
    { \
        ::WTF::fastFree(p); \
    } \
    \
    void* operator new[](size_t size) \
    { \
        return ::WTF::fastMalloc(size); \
    } \
    \
    void operator delete[](void* p) \
    { \
        ::WTF::fastFree(p); \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    using webkitFastMalloced = int; \

// FIXME: WTF_MAKE_FAST_ALLOCATED should take class name so that we can create malloc_zone per this macro.
// https://bugs.webkit.org/show_bug.cgi?id=205702
#define WTF_MAKE_FAST_ALLOCATED \
public: \
    WTF_MAKE_FAST_ALLOCATED_IMPL \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_FAST_ALLOCATED \
    WTF_MAKE_FAST_ALLOCATED_IMPL \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#if ENABLE(MALLOC_HEAP_BREAKDOWN)

#define WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname) \
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
    using webkitFastMalloced = int; \

#define WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(classname) \
public: \
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname) \
private: \
    WTF_EXPORT_PRIVATE static WTF::DebugHeap& debugHeap(const char*); \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(className) \
private: \
    WTF_EXPORT_PRIVATE static WTF::DebugHeap& debugHeap(const char*); \
public: \
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(className) \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#else

#define WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname) \
    WTF_MAKE_FAST_ALLOCATED_IMPL

#define WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(classname) \
public: \
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(classname) \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(className) \
public: \
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER_IMPL(className) \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#endif
