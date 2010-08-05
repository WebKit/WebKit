/*
 *  Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef AlignedMemoryAllocator_h
#define AlignedMemoryAllocator_h

#include <wtf/Bitmap.h>
#include <wtf/PageReservation.h>

namespace JSC {

struct AlignedMemoryAllocatorConstants {
// Set sane defaults if -D<flagname=value> wasn't provided via compiler args
#if defined(JSCCOLLECTOR_VIRTUALMEM_RESERVATION)
    // Keep backwards compatibility with symbian build system
    static const size_t virtualMemoryReservation = JSCCOLLECTOR_VIRTUALMEM_RESERVATION;
#elif defined(__WINS__)
    // Emulator has limited virtual address space
    static const size_t virtualMemoryReservation = 0x400000;
#else
    // HW has plenty of virtual addresses
    static const size_t virtualMemoryReservation = 0x8000000;
#endif
};

template<size_t blockSize> class AlignedMemory;
template<size_t blockSize> class AlignedMemoryAllocator;

#if HAVE(PAGE_ALLOCATE_ALIGNED)

template<size_t blockSize>
class AlignedMemoryAllocator;

template<size_t blockSize>
class AlignedMemory {
public:
    void deallocate();
    void* base();

private:
    friend class AlignedMemoryAllocator<blockSize>;

    AlignedMemory(PageAllocation);

    PageAllocation m_allocation;
};

template<size_t blockSize>
class AlignedMemoryAllocator {
public:
    void destroy();
    AlignedMemory<blockSize> allocate();
};

template<size_t blockSize>
inline void AlignedMemoryAllocator<blockSize>::destroy()
{
}

template<size_t blockSize>
inline AlignedMemory<blockSize> AlignedMemoryAllocator<blockSize>::allocate()
{
    return AlignedMemory<blockSize>(PageAllocation::allocateAligned(blockSize, PageAllocation::JSGCHeapPages));
}

template<size_t blockSize>
inline void AlignedMemory<blockSize>::deallocate()
{
    m_allocation.deallocate();
}

template<size_t blockSize>
inline void* AlignedMemory<blockSize>::base()
{
    return m_allocation.base();
}

template<size_t blockSize>
inline AlignedMemory<blockSize>::AlignedMemory(PageAllocation allocation)
    : m_allocation(allocation)
{
}

#else

template<size_t blockSize>
class AlignedMemory {
public:
    void deallocate();
    void* base();

private:
    friend class AlignedMemoryAllocator<blockSize>;

    AlignedMemory(void* base, AlignedMemoryAllocator<blockSize>* allocator);

    void* m_base;
    AlignedMemoryAllocator<blockSize>* m_allocator;
};

template<size_t blockSize>
class AlignedMemoryAllocator {
public:
    AlignedMemoryAllocator();
    ~AlignedMemoryAllocator();

    void destroy();
    AlignedMemory<blockSize> allocate();
    void free(AlignedMemory<blockSize>);

private:
    static const size_t reservationSize = AlignedMemoryAllocatorConstants::virtualMemoryReservation;
    static const size_t bitmapSize = reservationSize / blockSize;

    PageReservation m_reservation;
    size_t m_nextFree;
    uintptr_t m_reservationBase;
    WTF::Bitmap<bitmapSize> m_bitmap;
};

template<size_t blockSize>
AlignedMemoryAllocator<blockSize>::AlignedMemoryAllocator()
    : m_reservation(PageReservation::reserve(reservationSize + blockSize, PageAllocation::JSGCHeapPages))
    , m_nextFree(0)
{
    // check that blockSize and reservationSize are powers of two
    ASSERT(!(blockSize & (blockSize - 1)));
    ASSERT(!(reservationSize & (reservationSize - 1)));

    // check that blockSize is a multiple of pageSize and that
    // reservationSize is a multiple of blockSize
    ASSERT(!(blockSize & (PageAllocation::pageSize() - 1)));
    ASSERT(!(reservationSize & (blockSize - 1)));

    ASSERT(m_reservation);

    m_reservationBase = reinterpret_cast<uintptr_t>(m_reservation.base());
    m_reservationBase = (m_reservationBase + blockSize) & ~(blockSize - 1);
}

template<size_t blockSize>
AlignedMemoryAllocator<blockSize>::~AlignedMemoryAllocator()
{
    destroy();
    m_reservation.deallocate();
}

template<size_t blockSize>
inline void AlignedMemoryAllocator<blockSize>::destroy()
{
    for (unsigned i = 0; i < bitmapSize; ++i) {
        if (m_bitmap.get(i)) {
            void* blockAddress = reinterpret_cast<void*>(m_reservationBase + m_nextFree * blockSize);
            m_reservation.decommit(blockAddress, blockSize);

            m_bitmap.clear(i);
        }
    }
}

template<size_t blockSize>
AlignedMemory<blockSize> AlignedMemoryAllocator<blockSize>::allocate()
{
    while (m_nextFree < bitmapSize) {
        if (!m_bitmap.get(m_nextFree)) {
            void* blockAddress = reinterpret_cast<void*>(m_reservationBase + m_nextFree * blockSize);
            m_reservation.commit(blockAddress, blockSize);

            m_bitmap.set(m_nextFree);
            ++m_nextFree;

            return AlignedMemory<blockSize>(blockAddress, this);
        }
        m_bitmap.advanceToNextFreeBit(m_nextFree);
    }

    if (m_bitmap.isFull())
        return AlignedMemory<blockSize>(0, this);

    m_nextFree = 0;

    return allocate();
}

template<size_t blockSize>
void AlignedMemoryAllocator<blockSize>::free(AlignedMemory<blockSize> allocation)
{
    ASSERT(allocation.base());
    m_reservation.decommit(allocation.base(), blockSize);

    size_t diff = (reinterpret_cast<uintptr_t>(allocation.base()) - m_reservationBase);
    ASSERT(!(diff & (blockSize - 1)));

    size_t i = diff / blockSize;
    ASSERT(m_bitmap.get(i));

    m_bitmap.clear(i);
}

template<size_t blockSize>
inline void AlignedMemory<blockSize>::deallocate()
{
    m_allocator->free(*this);
}

template<size_t blockSize>
inline void* AlignedMemory<blockSize>::base()
{
    return m_base;
}

template<size_t blockSize>
AlignedMemory<blockSize>::AlignedMemory(void* base, AlignedMemoryAllocator<blockSize>* allocator)
    : m_base(base)
    , m_allocator(allocator)
{
}

#endif

}

#endif
