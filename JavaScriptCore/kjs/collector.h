// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _KJSCOLLECTOR_H_
#define _KJSCOLLECTOR_H_

// KJS_MEM_LIMIT and KJS_MEM_INCREMENT can be tweaked to adjust how the
// garbage collector allocates memory. KJS_MEM_LIMIT is the largest # of objects
// the collector will allow to be present in memory. Once this limit is reached,
// a running script will get an "out of memory" exception.
//
// KJS_MEM_INCREMENT specifies the amount by which the "soft limit" on memory is
// increased when the memory gets filled up. The soft limit is the amount after
// which the GC will run and delete unused objects.
//
// If you are debugging seemingly random crashes where an object has been deleted,
// try setting KJS_MEM_INCREMENT to something small, e.g. 300, to force garbage
// collection to happen more often, and disable the softLimit increase &
// out-of-memory testing code in Collector::allocate()

#define KJS_MEM_LIMIT 500000
#define KJS_MEM_INCREMENT 1000

#include <stdlib.h>

// for DEBUG_*
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"

namespace KJS {

  class CollectorBlock;

  /**
   * @short Garbage collector.
   */
  class Collector {
    // disallow direct construction/destruction
    Collector();
  public:
    /**
     * Register an object with the collector. The following assumptions are
     * made:
     * @li the operator new() of the object class is overloaded.
     * @li operator delete() has been overloaded as well and does not free
     * the memory on its own.
     *
     * @param s Size of the memory to be registered.
     * @return A pointer to the allocated memory.
     */
    static void* allocate(size_t s);
    /**
     * Run the garbage collection. This involves calling the delete operator
     * on each object and freeing the used memory.
     */
    static bool collect();
    static int size() { return filled; }
    static bool outOfMemory() { return memLimitReached; }

#ifdef KJS_DEBUG_MEM
    /**
     * Check that nothing is left when the last interpreter gets deleted
     */
    static void finalCheck();
    /**
     * @internal
     */
    static bool collecting;
#endif
#ifdef APPLE_CHANGES
    static int numInterpreters();
    static int numGCNotAllowedObjects();
    static int numReferencedObjects();
    static void lock();
    static void unlock();
#endif
  private:
    static CollectorBlock* root;
    static CollectorBlock* currentBlock;
    static unsigned long filled;
    static unsigned long softLimit;
    static unsigned long timesFilled;
    static unsigned long increaseLimitAt;
    static bool memLimitReached;
    enum { BlockSize = 100 };
  };

};

#endif
