/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef WTF_FastMalloc_h
#define WTF_FastMalloc_h

#include <new>
#include <stdlib.h>
#include <wtf/PossiblyNull.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

    // These functions call CRASH() if an allocation fails.
    WTF_EXPORT_PRIVATE void* fastMalloc(size_t);
    WTF_EXPORT_PRIVATE void* fastZeroedMalloc(size_t);
    WTF_EXPORT_PRIVATE void* fastCalloc(size_t numElements, size_t elementSize);
    WTF_EXPORT_PRIVATE void* fastRealloc(void*, size_t);
    WTF_EXPORT_PRIVATE char* fastStrDup(const char*);
    WTF_EXPORT_PRIVATE size_t fastMallocSize(const void*);
    WTF_EXPORT_PRIVATE size_t fastMallocGoodSize(size_t);

    // Allocations from fastAlignedMalloc() must be freed using fastAlignedFree().
    WTF_EXPORT_PRIVATE void* fastAlignedMalloc(size_t alignment, size_t);
    WTF_EXPORT_PRIVATE void fastAlignedFree(void*);

    struct TryMallocReturnValue {
        TryMallocReturnValue(void* data)
            : m_data(data)
        {
        }
        TryMallocReturnValue(const TryMallocReturnValue& source)
            : m_data(source.m_data)
        {
            source.m_data = 0;
        }
        ~TryMallocReturnValue() { ASSERT(!m_data); }
        template <typename T> bool getValue(T& data) WARN_UNUSED_RETURN;
        template <typename T> operator PossiblyNull<T>()
        { 
            T value; 
            getValue(value); 
            return PossiblyNull<T>(value);
        } 
    private:
        mutable void* m_data;
    };
    
    template <typename T> bool TryMallocReturnValue::getValue(T& data)
    {
        union u { void* data; T target; } res;
        res.data = m_data;
        data = res.target;
        bool returnValue = !!m_data;
        m_data = 0;
        return returnValue;
    }

    WTF_EXPORT_PRIVATE TryMallocReturnValue tryFastMalloc(size_t n);
    TryMallocReturnValue tryFastZeroedMalloc(size_t n);
    WTF_EXPORT_PRIVATE TryMallocReturnValue tryFastCalloc(size_t n_elements, size_t element_size);
    WTF_EXPORT_PRIVATE TryMallocReturnValue tryFastRealloc(void* p, size_t n);

    WTF_EXPORT_PRIVATE void fastFree(void*);

    WTF_EXPORT_PRIVATE void releaseFastMallocFreeMemory();
    WTF_EXPORT_PRIVATE void releaseFastMallocFreeMemoryForThisThread();
    
    struct FastMallocStatistics {
        size_t reservedVMBytes;
        size_t committedVMBytes;
        size_t freeListBytes;
    };
    WTF_EXPORT_PRIVATE FastMallocStatistics fastMallocStatistics();

    // This defines a type which holds an unsigned integer and is the same
    // size as the minimally aligned memory allocation.
    typedef unsigned long long AllocAlignmentInteger;

} // namespace WTF

using WTF::fastCalloc;
using WTF::fastFree;
using WTF::fastMalloc;
using WTF::fastMallocGoodSize;
using WTF::fastMallocSize;
using WTF::fastRealloc;
using WTF::fastStrDup;
using WTF::fastZeroedMalloc;
using WTF::tryFastCalloc;
using WTF::tryFastMalloc;
using WTF::tryFastRealloc;
using WTF::tryFastZeroedMalloc;
using WTF::fastAlignedMalloc;
using WTF::fastAlignedFree;

#if COMPILER(GCC) && OS(DARWIN)
#define WTF_PRIVATE_INLINE __private_extern__ inline __attribute__((always_inline))
#elif COMPILER(GCC)
#define WTF_PRIVATE_INLINE inline __attribute__((always_inline))
#elif COMPILER(MSVC)
#define WTF_PRIVATE_INLINE __forceinline
#else
#define WTF_PRIVATE_INLINE inline
#endif

#define WTF_MAKE_FAST_ALLOCATED \
public: \
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
private: \
typedef int __thisIsHereToForceASemicolonAfterThisMacro

#endif /* WTF_FastMalloc_h */
