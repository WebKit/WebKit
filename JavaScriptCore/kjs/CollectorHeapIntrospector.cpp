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

#include "config.h"
#include "CollectorHeapIntrospector.h"

#include "collector.h"
#include "MallocZoneSupport.h"

namespace KJS {

extern "C" {
malloc_introspection_t jscore_collector_introspection = { &CollectorHeapIntrospector::enumerate, &CollectorHeapIntrospector::goodSize, &CollectorHeapIntrospector::check, &CollectorHeapIntrospector::print,
    &CollectorHeapIntrospector::log, &CollectorHeapIntrospector::forceLock, &CollectorHeapIntrospector::forceUnlock, &CollectorHeapIntrospector::statistics };
}

void CollectorHeapIntrospector::init(CollectorHeap* primaryHeap, CollectorHeap* numberHeap)
{
    static CollectorHeapIntrospector zone(primaryHeap, numberHeap);
}

CollectorHeapIntrospector::CollectorHeapIntrospector(CollectorHeap* primaryHeap, CollectorHeap* numberHeap)
    : m_primaryHeap(primaryHeap)
    , m_numberHeap(numberHeap)
{
    memset(&m_zone, 0, sizeof(m_zone));
    m_zone.zone_name = "JavaScriptCore Collector";
    m_zone.size = &CollectorHeapIntrospector::size;
    m_zone.malloc = &CollectorHeapIntrospector::zoneMalloc;
    m_zone.calloc = &CollectorHeapIntrospector::zoneCalloc;
    m_zone.realloc = &CollectorHeapIntrospector::zoneRealloc;
    m_zone.free = &CollectorHeapIntrospector::zoneFree;
    m_zone.valloc = &CollectorHeapIntrospector::zoneValloc;
    m_zone.destroy = &CollectorHeapIntrospector::zoneDestroy;
    m_zone.introspect = &jscore_collector_introspection;
    malloc_zone_register(&m_zone);
}

kern_return_t CollectorHeapIntrospector::enumerate(task_t task, void* context, unsigned typeMask, vm_address_t zoneAddress, memory_reader_t reader, vm_range_recorder_t recorder)
{
    RemoteMemoryReader memoryReader(task, reader);
    CollectorHeapIntrospector* zone = memoryReader(reinterpret_cast<CollectorHeapIntrospector*>(zoneAddress));
    CollectorHeap* heaps[2] = {memoryReader(zone->m_primaryHeap), memoryReader(zone->m_numberHeap)};

    if (!heaps[0]->blocks && !heaps[1]->blocks)
        return 0;

    for (int currentHeap = 0; currentHeap < 2; currentHeap++) {
        CollectorHeap* heap = heaps[currentHeap];
        CollectorBlock** blocks = memoryReader(heap->blocks);
        for (unsigned i = 0; i < heap->usedBlocks; i++) {
            vm_address_t remoteBlockAddress = reinterpret_cast<vm_address_t>(blocks[i]);
            vm_range_t ptrRange = { remoteBlockAddress, sizeof(CollectorBlock) };
            
            if (typeMask & (MALLOC_PTR_REGION_RANGE_TYPE | MALLOC_ADMIN_REGION_RANGE_TYPE))
                (*recorder)(task, context, MALLOC_PTR_REGION_RANGE_TYPE, &ptrRange, 1);
            
            // Recording individual cells causes frequent false-positives.  Any garbage cells
            // which have yet to be collected are labeled as leaks.  Recording on a per-block
            // basis provides less detail but avoids these false-positives.
            if (memoryReader(blocks[i])->usedCells && (typeMask & MALLOC_PTR_IN_USE_RANGE_TYPE))
                (*recorder)(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, &ptrRange, 1);
        }
    }

    return 0;
}

} // namespace KJS
