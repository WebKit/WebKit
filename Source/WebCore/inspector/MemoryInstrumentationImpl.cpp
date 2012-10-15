/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR)

#include "MemoryInstrumentationImpl.h"
#include <wtf/Assertions.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

TypeNameToSizeMap MemoryInstrumentationClientImpl::sizesMap() const
{
    // TypeToSizeMap uses const char* as the key.
    // Thus it could happen that we have two different keys with equal string.
    TypeNameToSizeMap sizesMap;
    for (TypeToSizeMap::const_iterator i = m_totalSizes.begin(); i != m_totalSizes.end(); ++i) {
        String objectType(i->key);
        TypeNameToSizeMap::AddResult result = sizesMap.add(objectType, i->value);
        if (!result.isNewEntry)
            result.iterator->value += i->value;
    }

    return sizesMap;
}

void MemoryInstrumentationClientImpl::countObjectSize(MemoryObjectType objectType, size_t size)
{
    ASSERT(objectType);
    TypeToSizeMap::AddResult result = m_totalSizes.add(objectType, size);
    if (!result.isNewEntry)
        result.iterator->value += size;
    ++m_totalCountedObjects;
}

bool MemoryInstrumentationClientImpl::visited(const void* object)
{
    return !m_visitedObjects.add(object).isNewEntry;
}

void MemoryInstrumentationClientImpl::checkCountedObject(const void* object)
{
    if (!checkInstrumentedObjects())
        return;
    if (!m_allocatedObjects->contains(object)) {
        ++m_totalObjectsNotInAllocatedSet;
#if 0
        printf("Found unknown object referenced by pointer: %p\n", object);
        WTFReportBacktrace();
#endif
    }
}

void MemoryInstrumentationImpl::processDeferredInstrumentedPointers()
{
    while (!m_deferredInstrumentedPointers.isEmpty()) {
        OwnPtr<InstrumentedPointerBase> pointer = m_deferredInstrumentedPointers.last().release();
        m_deferredInstrumentedPointers.removeLast();
        pointer->process(this);
    }
}

void MemoryInstrumentationImpl::deferInstrumentedPointer(PassOwnPtr<InstrumentedPointerBase> pointer)
{
    m_deferredInstrumentedPointers.append(pointer);
}

size_t MemoryInstrumentationImpl::selfSize() const
{
    return calculateContainerSize(m_deferredInstrumentedPointers);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
