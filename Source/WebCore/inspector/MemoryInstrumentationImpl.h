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

#ifndef MemoryInstrumentationImpl_h
#define MemoryInstrumentationImpl_h


#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryInstrumentation.h>
#include <wtf/Vector.h>

using WTF::MemoryObjectType;

namespace WebCore {

typedef HashSet<const void*> VisitedObjects;

class MemoryInstrumentationImpl : public WTF::MemoryInstrumentation {
public:
    MemoryInstrumentationImpl(VisitedObjects&, const VisitedObjects* allocatedObjects = 0);

    size_t selfSize() const;
    size_t totalSize(MemoryObjectType objectType) const
    {
        TypeToSizeMap::const_iterator i = m_totalSizes.find(objectType);
        return i == m_totalSizes.end() ? 0 : i->second;
    }

    size_t reportedSizeForAllTypes() const
    {
        size_t size = 0;
        for (TypeToSizeMap::const_iterator i = m_totalSizes.begin(); i != m_totalSizes.end(); ++i)
            size += i->second;
        return size;
    }

    bool checkInstrumentedObjects() const { return m_allocatedObjects; }
    size_t totalCountedObjects() const { return m_totalCountedObjects; }
    size_t totalObjectsNotInAllocatedSet() const { return m_totalObjectsNotInAllocatedSet; }

private:
    virtual void countObjectSize(MemoryObjectType, size_t) OVERRIDE;
    virtual void deferInstrumentedPointer(PassOwnPtr<InstrumentedPointerBase>) OVERRIDE;
    virtual bool visited(const void*) OVERRIDE;
    virtual void processDeferredInstrumentedPointers() OVERRIDE;
    virtual void checkCountedObject(const void*) OVERRIDE;

    typedef HashMap<MemoryObjectType, size_t> TypeToSizeMap;
    TypeToSizeMap m_totalSizes;
    VisitedObjects& m_visitedObjects;
    Vector<OwnPtr<InstrumentedPointerBase> > m_deferredInstrumentedPointers;
    const VisitedObjects* m_allocatedObjects;
    size_t m_totalCountedObjects;
    size_t m_totalObjectsNotInAllocatedSet;
};

} // namespace WebCore

#endif // !defined(MemoryInstrumentationImpl_h)

