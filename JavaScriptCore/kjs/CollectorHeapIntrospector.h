/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CollectorHeapIntrospector_h
#define CollectorHeapIntrospector_h

#include <malloc/malloc.h>
#include "Assertions.h"

namespace KJS {

struct CollectorHeap;
    
class CollectorHeapIntrospector {
public:
    static void init(CollectorHeap*, CollectorHeap*);
    static kern_return_t enumerate(task_t, void* context, unsigned typeMask, vm_address_t zoneAddress, memory_reader_t, vm_range_recorder_t);
    static size_t goodSize(malloc_zone_t*, size_t size) { return size; }
    static boolean_t check(malloc_zone_t*) { return true; }
    static void  print(malloc_zone_t*, boolean_t) { }
    static void log(malloc_zone_t*, void*) { }
    static void forceLock(malloc_zone_t*) { }
    static void forceUnlock(malloc_zone_t*) { }
    static void statistics(malloc_zone_t*, malloc_statistics_t*) { }

private:
    CollectorHeapIntrospector(CollectorHeap*, CollectorHeap*);
    static size_t size(malloc_zone_t*, const void*) { return 0; }
    static void* zoneMalloc(malloc_zone_t*, size_t) { LOG_ERROR("malloc is not supported"); return 0; }
    static void* zoneCalloc(malloc_zone_t*, size_t, size_t) { LOG_ERROR("calloc is not supported"); return 0; }
    static void* zoneRealloc(malloc_zone_t*, void*, size_t) { LOG_ERROR("realloc is not supported"); return 0; }
    static void* zoneValloc(malloc_zone_t*, size_t) { LOG_ERROR("valloc is not supported"); return 0; }
    static void zoneDestroy(malloc_zone_t*) { }

    static void zoneFree(malloc_zone_t*, void* ptr)
    {
        // Due to <rdar://problem/5671357> zoneFree may be called by the system free even if the pointer
        // is not in this zone.  When this happens, the pointer being freed was not allocated by any
        // zone so we need to print a useful error for the application developer.
        malloc_printf("*** error for object %p: pointer being freed was not allocated\n", ptr);
    }

    malloc_zone_t m_zone;
    CollectorHeap* m_primaryHeap;
    CollectorHeap* m_numberHeap;
};

}

#endif // CollectorHeapIntrospector_h
