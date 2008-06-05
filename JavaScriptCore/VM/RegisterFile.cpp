/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "RegisterFile.h"

#include "RegisterFileStack.h"
#include "Register.h"

using namespace std;

namespace KJS {

size_t RegisterFile::newBuffer(size_t size, size_t capacity, size_t minCapacity, size_t maxSize, size_t offset)
{
    capacity = (max(minCapacity, min(maxSize, max<size_t>(16, capacity + capacity / 4 + 1))));
    Register* newBuffer = static_cast<Register*>(fastCalloc(capacity, sizeof(Register))); // zero-filled memory

    if (m_buffer)
        memcpy(newBuffer + offset, m_buffer, size * sizeof(Register));

    setBuffer(newBuffer);
    return capacity;
}

bool RegisterFile::growBuffer(size_t minCapacity, size_t maxSize)
{
    if (minCapacity > m_maxSize)
        return false;

    size_t numGlobalSlots = this->numGlobalSlots();
    size_t size = m_size + numGlobalSlots;
    size_t capacity = m_capacity + numGlobalSlots;
    minCapacity += numGlobalSlots;

    capacity = newBuffer(size, capacity, minCapacity, maxSize, 0);

    setBase(m_buffer + numGlobalSlots);
    m_capacity = capacity - numGlobalSlots;
    return true;
}

void RegisterFile::addGlobalSlots(size_t count)
{
    if (!count)
        return;
    ASSERT(safeForReentry());
    size_t numGlobalSlots = this->numGlobalSlots();
    size_t size = m_size + numGlobalSlots;
    size_t capacity = m_capacity + numGlobalSlots;
    size_t minCapacity = size + count;

    if (minCapacity < capacity)
        memmove(m_buffer + count, m_buffer, size * sizeof(Register));
    else
        capacity = newBuffer(size, capacity, minCapacity, m_maxSize, count);

    numGlobalSlots += count;

    setBase(m_buffer + numGlobalSlots);
    m_capacity = capacity - numGlobalSlots;
}

void RegisterFile::copyGlobals(RegisterFile* src)
{
    ASSERT(src->numGlobalSlots() > 0); // Global code should always allocate at least a "this" slot.
    size_t numSlotsToCopy = src->numGlobalSlots() - 1; // Don't propogate the nested "this" value back to the parent register file.
    if (!numSlotsToCopy)
        return;
    memcpy(m_buffer, src->m_buffer, numSlotsToCopy * sizeof(Register));
}

void RegisterFile::setBase(Register* base)
{
    m_base = base;
    if (m_baseObserver)
        m_baseObserver->baseChanged(this);
}

void RegisterFile::clear()
{
    setBase(m_buffer);
    m_size = 0;
}

} // namespace KJS
