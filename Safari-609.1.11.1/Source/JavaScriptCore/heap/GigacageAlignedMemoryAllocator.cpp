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
{
}

GigacageAlignedMemoryAllocator::~GigacageAlignedMemoryAllocator()
{
}

void* GigacageAlignedMemoryAllocator::tryAllocateAlignedMemory(size_t alignment, size_t size)
{
    return Gigacage::tryAlignedMalloc(m_kind, alignment, size);
}

void GigacageAlignedMemoryAllocator::freeAlignedMemory(void* basePtr)
{
    Gigacage::alignedFree(m_kind, basePtr);
}

void GigacageAlignedMemoryAllocator::dump(PrintStream& out) const
{
    out.print(Gigacage::name(m_kind), "Gigacage");
}

void* GigacageAlignedMemoryAllocator::tryAllocateMemory(size_t size)
{
    return Gigacage::tryMalloc(m_kind, size);
}

void GigacageAlignedMemoryAllocator::freeMemory(void* pointer)
{
    Gigacage::free(m_kind, pointer);
}

void* GigacageAlignedMemoryAllocator::tryReallocateMemory(void* pointer, size_t size)
{
    return Gigacage::tryRealloc(m_kind, pointer, size);
}

} // namespace JSC

