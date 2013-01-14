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

#include "HeapGraphSerializer.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/Assertions.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/MemoryInstrumentationVector.h>
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

void MemoryInstrumentationClientImpl::countObjectSize(const void* object, MemoryObjectType objectType, size_t size)
{
    ASSERT(objectType);

    TypeToSizeMap::AddResult result = m_totalSizes.add(objectType, size);
    if (!result.isNewEntry)
        result.iterator->value += size;
    ++m_totalCountedObjects;

    if (!checkInstrumentedObjects())
        return;

    if (object)
        m_countedObjects.add(object, size);
}

bool MemoryInstrumentationClientImpl::visited(const void* object)
{
    return !m_visitedObjects.add(object).isNewEntry;
}

bool MemoryInstrumentationClientImpl::checkCountedObject(const void* object)
{
    if (!checkInstrumentedObjects())
        return true;
    if (!m_allocatedObjects.contains(object)) {
        ++m_totalObjectsNotInAllocatedSet;
        return false;
#if 0
        printf("Found unknown object referenced by pointer: %p\n", object);
        WTFReportBacktrace();
#endif
    }
    return true;
}

void MemoryInstrumentationClientImpl::reportNode(const MemoryObjectInfo& node)
{
    if (m_graphSerializer)
        m_graphSerializer->reportNode(node);
}

void MemoryInstrumentationClientImpl::reportEdge(const void* target, const char* name, MemberType memberType)
{
    if (m_graphSerializer)
        m_graphSerializer->reportEdge(target, name, memberType);
}

void MemoryInstrumentationClientImpl::reportLeaf(const MemoryObjectInfo& target, const char* edgeName)
{
    if (m_graphSerializer)
        m_graphSerializer->reportLeaf(target, edgeName);
}

void MemoryInstrumentationClientImpl::reportBaseAddress(const void* base, const void* real)
{
    if (m_graphSerializer)
        m_graphSerializer->reportBaseAddress(base, real);
}

void MemoryInstrumentationClientImpl::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::InspectorMemoryAgent);
    info.addMember(m_totalSizes);
    info.addMember(m_visitedObjects);
    info.addMember(m_allocatedObjects);
    info.addMember(m_countedObjects);
    info.addMember(m_graphSerializer);
}

void MemoryInstrumentationImpl::processDeferredObjects()
{
    while (!m_deferredObjects.isEmpty()) {
        OwnPtr<WrapperBase> pointer = m_deferredObjects.last().release();
        m_deferredObjects.removeLast();
        pointer->process(this);
    }
}

void MemoryInstrumentationImpl::deferObject(PassOwnPtr<WrapperBase> pointer)
{
    m_deferredObjects.append(pointer);
}

void MemoryInstrumentationImpl::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::InspectorMemoryAgent);
    info.addMember(m_deferredObjects);
}


} // namespace WebCore

#endif // ENABLE(INSPECTOR)
