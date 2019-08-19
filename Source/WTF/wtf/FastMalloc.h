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
#include <wtf/StdLibExtras.h>

namespace WTF {

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

struct FastMallocStatistics {
    size_t reservedVMBytes;
    size_t committedVMBytes;
    size_t freeListBytes;
};
WTF_EXPORT_PRIVATE FastMallocStatistics fastMallocStatistics();

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
        return reinterpret_cast<T*>(fastMalloc(sizeof(T) * count));
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

using WTF::FastAllocator;
using WTF::FastMalloc;
using WTF::FastFree;
using WTF::isFastMallocEnabled;
using WTF::fastCalloc;
using WTF::fastFree;
using WTF::fastMalloc;
using WTF::fastMallocGoodSize;
using WTF::fastMallocSize;
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

#define WTF_MAKE_FAST_ALLOCATED \
public: \
    WTF_MAKE_FAST_ALLOCATED_IMPL \
private: \
using __thisIsHereToForceASemicolonAfterThisMacro = int

#define WTF_MAKE_STRUCT_FAST_ALLOCATED \
    WTF_MAKE_FAST_ALLOCATED_IMPL \
using __thisIsHereToForceASemicolonAfterThisMacro = int
