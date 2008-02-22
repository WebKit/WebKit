// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef KJSCOLLECTOR_H_
#define KJSCOLLECTOR_H_

#include <string.h>
#include <wtf/HashCountedSet.h>

namespace KJS {

  class JSCell;
  class JSValue;
  class CollectorBlock;

  class Collector {
  public:
    static void* allocate(size_t s);
    static void* allocateNumber(size_t s);
    static bool collect();
    static bool isBusy(); // true if an allocation or collection is in progress

    static const size_t minExtraCostSize = 256;

    static void reportExtraMemoryCost(size_t cost);

    static size_t size();

    static void protect(JSValue*);
    static void unprotect(JSValue*);
    
    static void collectOnMainThreadOnly(JSValue*);

    static size_t globalObjectCount();
    static size_t protectedObjectCount();
    static size_t protectedGlobalObjectCount();
    static HashCountedSet<const char*>* protectedObjectTypeCounts();

    class Thread;
    static void registerThread();
    
    static void registerAsMainThread();

    static bool isCellMarked(const JSCell*);
    static void markCell(JSCell*);

    enum HeapType { PrimaryHeap, NumberHeap };

  private:
    template <Collector::HeapType heapType> static void* heapAllocate(size_t s);
    template <Collector::HeapType heapType> static size_t sweep(bool);
    static const CollectorBlock* cellBlock(const JSCell*);
    static CollectorBlock* cellBlock(JSCell*);
    static size_t cellOffset(const JSCell*);

    Collector();

    static void recordExtraCost(size_t);
    static void markProtectedObjects();
    static void markMainThreadOnlyObjects();
    static void markCurrentThreadConservatively();
    static void markOtherThreadConservatively(Thread*);
    static void markStackObjectsConservatively();
    static void markStackObjectsConservatively(void* start, void* end);

    static size_t mainThreadOnlyObjectCount;
    static bool memoryFull;
    static void reportOutOfMemoryToAllExecStates();
  };

  // tunable parameters
  template<size_t bytesPerWord> struct CellSize;

  // cell size needs to be a power of two for certain optimizations in collector.cpp
  template<> struct CellSize<sizeof(uint32_t)> { static const size_t m_value = 32; }; // 32-bit
  template<> struct CellSize<sizeof(uint64_t)> { static const size_t m_value = 64; }; // 64-bit
  const size_t BLOCK_SIZE = 16 * 4096; // 64k
  
  // derived constants
  const size_t BLOCK_OFFSET_MASK = BLOCK_SIZE - 1;
  const size_t BLOCK_MASK = ~BLOCK_OFFSET_MASK;
  const size_t MINIMUM_CELL_SIZE = CellSize<sizeof(void*)>::m_value;
  const size_t CELL_ARRAY_LENGTH = (MINIMUM_CELL_SIZE / sizeof(double)) + (MINIMUM_CELL_SIZE % sizeof(double) != 0 ? sizeof(double) : 0);
  const size_t CELL_SIZE = CELL_ARRAY_LENGTH * sizeof(double);
  const size_t SMALL_CELL_SIZE = CELL_SIZE / 2;
  const size_t CELL_MASK = CELL_SIZE - 1;
  const size_t CELL_ALIGN_MASK = ~CELL_MASK;
  const size_t CELLS_PER_BLOCK = (BLOCK_SIZE * 8 - sizeof(uint32_t) * 8 - sizeof(void *) * 8 - 2 * (7 + 3 * 8)) / (CELL_SIZE * 8 + 2);
  const size_t SMALL_CELLS_PER_BLOCK = 2 * CELLS_PER_BLOCK;
  const size_t BITMAP_SIZE = (CELLS_PER_BLOCK + 7) / 8;
  const size_t BITMAP_WORDS = (BITMAP_SIZE + 3) / sizeof(uint32_t);
  
  struct CollectorBitmap {
    uint32_t bits[BITMAP_WORDS];
    bool get(size_t n) const { return !!(bits[n >> 5] & (1 << (n & 0x1F))); } 
    void set(size_t n) { bits[n >> 5] |= (1 << (n & 0x1F)); } 
    void clear(size_t n) { bits[n >> 5] &= ~(1 << (n & 0x1F)); } 
    void clearAll() { memset(bits, 0, sizeof(bits)); }
  };
  
  struct CollectorCell {
    union {
      double memory[CELL_ARRAY_LENGTH];
      struct {
        void* zeroIfFree;
        ptrdiff_t next;
      } freeCell;
    } u;
  };

  struct SmallCollectorCell {
    union {
      double memory[CELL_ARRAY_LENGTH / 2];
      struct {
        void* zeroIfFree;
        ptrdiff_t next;
      } freeCell;
    } u;
  };

  class CollectorBlock {
  public:
    CollectorCell cells[CELLS_PER_BLOCK];
    uint32_t usedCells;
    CollectorCell* freeList;
    CollectorBitmap marked;
    CollectorBitmap collectOnMainThreadOnly;
  };

  class SmallCellCollectorBlock {
  public:
    SmallCollectorCell cells[SMALL_CELLS_PER_BLOCK];
    uint32_t usedCells;
    SmallCollectorCell* freeList;
    CollectorBitmap marked;
    CollectorBitmap collectOnMainThreadOnly;
  };

  inline const CollectorBlock* Collector::cellBlock(const JSCell* cell)
  {
    return reinterpret_cast<const CollectorBlock*>(reinterpret_cast<uintptr_t>(cell) & BLOCK_MASK);
  }

  inline CollectorBlock* Collector::cellBlock(JSCell* cell)
  {
    return const_cast<CollectorBlock*>(cellBlock(const_cast<const JSCell*>(cell)));
  }

  inline size_t Collector::cellOffset(const JSCell* cell)
  {
    return (reinterpret_cast<uintptr_t>(cell) & BLOCK_OFFSET_MASK) / CELL_SIZE;
  }

  inline bool Collector::isCellMarked(const JSCell* cell)
  {
    return cellBlock(cell)->marked.get(cellOffset(cell));
  }

  inline void Collector::markCell(JSCell* cell)
  {
    cellBlock(cell)->marked.set(cellOffset(cell));
  }

  inline void Collector::reportExtraMemoryCost(size_t cost)
  { 
    if (cost > minExtraCostSize) 
      recordExtraCost(cost / (CELL_SIZE * 2)); 
  }

} // namespace KJS

#endif /* KJSCOLLECTOR_H_ */
