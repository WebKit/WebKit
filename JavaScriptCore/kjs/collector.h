// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _KJSCOLLECTOR_H_
#define _KJSCOLLECTOR_H_

#include "value.h"

#define KJS_MEM_LIMIT 500000

namespace KJS {

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
    static int size();
    static bool outOfMemory() { return memoryFull; }

#ifdef KJS_DEBUG_MEM
    /**
     * Check that nothing is left when the last interpreter gets deleted
     */
    static void finalCheck();
#endif

    static int numInterpreters();
    static int numGCNotAllowedObjects();
    static int numReferencedObjects();
#if APPLE_CHANGES
    static const void *rootObjectClasses(); // actually returns CFSetRef
#endif

    class Thread;
    static void registerThread();

  private:

    static void markProtectedObjects();
    static void markCurrentThreadConservatively();
    static void markOtherThreadConservatively(Thread *thread);
    static void markStackObjectsConservatively();
    static void markStackObjectsConservatively(void *start, void *end);

    static bool memoryFull;
  };

};

#endif /* _KJSCOLLECTOR_H_ */
