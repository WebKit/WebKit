/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GCActivityCallback.h"

#include "Heap.h"
#include <BlackBerryPlatformMemory.h>

namespace JSC {

struct DefaultGCActivityCallbackPlatformData {
    explicit DefaultGCActivityCallbackPlatformData(Heap* heap) : m_heap(heap) { }
    Heap* m_heap;
};

DefaultGCActivityCallback::DefaultGCActivityCallback(Heap* heap)
    : d(adoptPtr(new DefaultGCActivityCallbackPlatformData(heap)))
{
}

DefaultGCActivityCallback::~DefaultGCActivityCallback()
{
}

void DefaultGCActivityCallback::didAllocate(size_t bytesAllocated)
{
    if (!BlackBerry::Platform::isMemoryLow())
        return;

    if (bytesAllocated < 1 * 1024 * 1024)
        return;

    if (d->m_heap->isBusy() || !d->m_heap->isSafeToCollect())
        return;

    d->m_heap->collect(Heap::DoNotSweep);
}

void DefaultGCActivityCallback::willCollect()
{
}

void DefaultGCActivityCallback::synchronize()
{
}

void DefaultGCActivityCallback::cancel()
{
}

}
