/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GigacageAlignedMemoryAllocator.h"

namespace JSC {

GigacageAlignedMemoryAllocator::GigacageAlignedMemoryAllocator(Gigacage::Kind kind)
    : m_kind(kind)
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    , m_heap(makeString("WebKit GigacageAlignedMemoryAllocator ", Gigacage::name(m_kind)).utf8().data())
#endif
{
}

GigacageAlignedMemoryAllocator::~GigacageAlignedMemoryAllocator()
{
}

void* GigacageAlignedMemoryAllocator::tryAllocateAlignedMemory(size_t alignment, size_t size)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.memalign(alignment, size, true);
#else
    return Gigacage::tryAlignedMalloc(m_kind, alignment, size);
#endif
}

void GigacageAlignedMemoryAllocator::freeAlignedMemory(void* basePtr)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.free(basePtr);
#else
    Gigacage::alignedFree(m_kind, basePtr);
#endif
}

void GigacageAlignedMemoryAllocator::dump(PrintStream& out) const
{
    out.print(Gigacage::name(m_kind), "Gigacage");
}

void* GigacageAlignedMemoryAllocator::tryAllocateMemory(size_t size)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.malloc(size);
#else
    return Gigacage::tryMalloc(m_kind, size);
#endif
}

void GigacageAlignedMemoryAllocator::freeMemory(void* pointer)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    m_heap.free(pointer);
#else
    Gigacage::free(m_kind, pointer);
#endif
}

void* GigacageAlignedMemoryAllocator::tryReallocateMemory(void* pointer, size_t size)
{
#if ENABLE(MALLOC_HEAP_BREAKDOWN)
    return m_heap.realloc(pointer, size);
#else
    return Gigacage::tryRealloc(m_kind, pointer, size);
#endif
}

} // namespace JSC

