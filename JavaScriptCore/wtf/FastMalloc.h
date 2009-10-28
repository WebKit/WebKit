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

#include "Platform.h"
#include "PossiblyNull.h"
#include <stdlib.h>
#include <new>

#if COMPILER(GCC)
#define WTF_FAST_MALLOC_EXPORT __attribute__((visibility("default")))
#else
#define WTF_FAST_MALLOC_EXPORT
#endif

namespace WTF {

    // These functions call CRASH() if an allocation fails.
    void* fastMalloc(size_t) WTF_FAST_MALLOC_EXPORT;
    void* fastZeroedMalloc(size_t);
    void* fastCalloc(size_t numElements, size_t elementSize) WTF_FAST_MALLOC_EXPORT;
    void* fastRealloc(void*, size_t) WTF_FAST_MALLOC_EXPORT;

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

    TryMallocReturnValue tryFastMalloc(size_t n);
    TryMallocReturnValue tryFastZeroedMalloc(size_t n);
    TryMallocReturnValue tryFastCalloc(size_t n_elements, size_t element_size);
    TryMallocReturnValue tryFastRealloc(void* p, size_t n);

    void fastFree(void*) WTF_FAST_MALLOC_EXPORT;

#ifndef NDEBUG    
    void fastMallocForbid();
    void fastMallocAllow();
#endif

    void releaseFastMallocFreeMemory();
    
    struct FastMallocStatistics {
        size_t heapSize;
        size_t freeSizeInHeap;
        size_t freeSizeInCaches;
        size_t returnedSize;
    };
    FastMallocStatistics fastMallocStatistics();

    // This defines a type which holds an unsigned integer and is the same
    // size as the minimally aligned memory allocation.
    typedef unsigned long long AllocAlignmentInteger;

    namespace Internal {
        enum AllocType {                    // Start with an unusual number instead of zero, because zero is common.
            AllocTypeMalloc = 0x375d6750,   // Encompasses fastMalloc, fastZeroedMalloc, fastCalloc, fastRealloc.
            AllocTypeClassNew,              // Encompasses class operator new from FastAllocBase.
            AllocTypeClassNewArray,         // Encompasses class operator new[] from FastAllocBase.
            AllocTypeFastNew,               // Encompasses fastNew.
            AllocTypeFastNewArray,          // Encompasses fastNewArray.
            AllocTypeNew,                   // Encompasses global operator new.
            AllocTypeNewArray               // Encompasses global operator new[].
        };
    }

#if ENABLE(FAST_MALLOC_MATCH_VALIDATION)

    // Malloc validation is a scheme whereby a tag is attached to an
    // allocation which identifies how it was originally allocated.
    // This allows us to verify that the freeing operation matches the
    // allocation operation. If memory is allocated with operator new[]
    // but freed with free or delete, this system would detect that.
    // In the implementation here, the tag is an integer prepended to
    // the allocation memory which is assigned one of the AllocType
    // enumeration values. An alternative implementation of this
    // scheme could store the tag somewhere else or ignore it.
    // Users of FastMalloc don't need to know or care how this tagging
    // is implemented.

    namespace Internal {

        // Return the AllocType tag associated with the allocated block p.
        inline AllocType fastMallocMatchValidationType(const void* p)
        {
            const AllocAlignmentInteger* type = static_cast<const AllocAlignmentInteger*>(p) - 1;
            return static_cast<AllocType>(*type);
        }

        // Return the address of the AllocType tag associated with the allocated block p.
        inline AllocAlignmentInteger* fastMallocMatchValidationValue(void* p)
        {
            return reinterpret_cast<AllocAlignmentInteger*>(static_cast<char*>(p) - sizeof(AllocAlignmentInteger));
        }

        // Set the AllocType tag to be associaged with the allocated block p.
        inline void setFastMallocMatchValidationType(void* p, AllocType allocType)
        {
            AllocAlignmentInteger* type = static_cast<AllocAlignmentInteger*>(p) - 1;
            *type = static_cast<AllocAlignmentInteger>(allocType);
        }

        // Handle a detected alloc/free mismatch. By default this calls CRASH().
        void fastMallocMatchFailed(void* p);

    } // namespace Internal

    // This is a higher level function which is used by FastMalloc-using code.
    inline void fastMallocMatchValidateMalloc(void* p, Internal::AllocType allocType)
    {
        if (!p)
            return;

        Internal::setFastMallocMatchValidationType(p, allocType);
    }

    // This is a higher level function which is used by FastMalloc-using code.
    inline void fastMallocMatchValidateFree(void* p, Internal::AllocType allocType)
    {
        if (!p)
            return;

        if (Internal::fastMallocMatchValidationType(p) != allocType)
            Internal::fastMallocMatchFailed(p);
        Internal::setFastMallocMatchValidationType(p, Internal::AllocTypeMalloc);  // Set it to this so that fastFree thinks it's OK.
    }

#else

    inline void fastMallocMatchValidateMalloc(void*, Internal::AllocType)
    {
    }

    inline void fastMallocMatchValidateFree(void*, Internal::AllocType)
    {
    }

#endif

} // namespace WTF

using WTF::fastMalloc;
using WTF::fastZeroedMalloc;
using WTF::fastCalloc;
using WTF::fastRealloc;
using WTF::tryFastMalloc;
using WTF::tryFastZeroedMalloc;
using WTF::tryFastCalloc;
using WTF::tryFastRealloc;
using WTF::fastFree;

#ifndef NDEBUG    
using WTF::fastMallocForbid;
using WTF::fastMallocAllow;
#endif

#if COMPILER(GCC) && PLATFORM(DARWIN)
#define WTF_PRIVATE_INLINE __private_extern__ inline __attribute__((always_inline))
#elif COMPILER(GCC)
#define WTF_PRIVATE_INLINE inline __attribute__((always_inline))
#elif COMPILER(MSVC)
#define WTF_PRIVATE_INLINE __forceinline
#else
#define WTF_PRIVATE_INLINE inline
#endif

#if !defined(_CRTDBG_MAP_ALLOC) && !(defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC)

// The nothrow functions here are actually not all that helpful, because fastMalloc will
// call CRASH() rather than returning 0, and returning 0 is what nothrow is all about.
// But since WebKit code never uses exceptions or nothrow at all, this is probably OK.
// Long term we will adopt FastAllocBase.h everywhere, and and replace this with
// debug-only code to make sure we don't use the system malloc via the default operator
// new by accident.

// We musn't customize the global operator new and delete for the Qt port.
#if !PLATFORM(QT)

WTF_PRIVATE_INLINE void* operator new(size_t size) { return fastMalloc(size); }
WTF_PRIVATE_INLINE void* operator new(size_t size, const std::nothrow_t&) throw() { return fastMalloc(size); }
WTF_PRIVATE_INLINE void operator delete(void* p) { fastFree(p); }
WTF_PRIVATE_INLINE void operator delete(void* p, const std::nothrow_t&) throw() { fastFree(p); }
WTF_PRIVATE_INLINE void* operator new[](size_t size) { return fastMalloc(size); }
WTF_PRIVATE_INLINE void* operator new[](size_t size, const std::nothrow_t&) throw() { return fastMalloc(size); }
WTF_PRIVATE_INLINE void operator delete[](void* p) { fastFree(p); }
WTF_PRIVATE_INLINE void operator delete[](void* p, const std::nothrow_t&) throw() { fastFree(p); }

#endif

#endif

#endif /* WTF_FastMalloc_h */
