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
#include <wtf/text/StringHash.h>

using WTF::MemoryObjectType;

namespace WebCore {

typedef HashSet<const void*> VisitedObjects;
typedef HashMap<String, size_t> TypeNameToSizeMap;

class MemoryInstrumentationClientImpl : public WTF::MemoryInstrumentationClient {
public:
    typedef HashMap<const void*, size_t> ObjectToSizeMap;

    MemoryInstrumentationClientImpl()
        : m_totalCountedObjects(0)
        , m_totalObjectsNotInAllocatedSet(0)
    { }

    size_t totalSize(MemoryObjectType objectType) const
    {
        TypeToSizeMap::const_iterator i = m_totalSizes.find(objectType);
        return i == m_totalSizes.end() ? 0 : i->value;
    }

    size_t reportedSizeForAllTypes() const
    {
        size_t size = 0;
        for (TypeToSizeMap::const_iterator i = m_totalSizes.begin(); i != m_totalSizes.end(); ++i)
            size += i->value;
        return size;
    }

    TypeNameToSizeMap sizesMap() const;
    VisitedObjects& allocatedObjects() { return m_allocatedObjects; }
    const ObjectToSizeMap& countedObjects() { return m_countedObjects; }

    bool checkInstrumentedObjects() const { return !m_allocatedObjects.isEmpty(); }
    size_t visitedObjects() const { return m_visitedObjects.size(); }
    size_t totalCountedObjects() const { return m_totalCountedObjects; }
    size_t totalObjectsNotInAllocatedSet() const { return m_totalObjectsNotInAllocatedSet; }

    virtual void countObjectSize(const void*, MemoryObjectType, size_t) OVERRIDE;
    virtual bool visited(const void*) OVERRIDE;
    virtual bool checkCountedObject(const void*) OVERRIDE;
    virtual void reportNode(const MemoryObjectInfo&) OVERRIDE { }
    virtual void reportEdge(const void*, const void*, const char*) OVERRIDE { }
    virtual void reportLeaf(const void*, const MemoryObjectInfo&, const char*) OVERRIDE { }
    virtual void reportBaseAddress(const void*, const void*) OVERRIDE { }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    typedef HashMap<MemoryObjectType, size_t> TypeToSizeMap;
    TypeToSizeMap m_totalSizes;
    VisitedObjects m_visitedObjects;
    VisitedObjects m_allocatedObjects;
    ObjectToSizeMap m_countedObjects;
    size_t m_totalCountedObjects;
    size_t m_totalObjectsNotInAllocatedSet;
};

class MemoryInstrumentationImpl : public WTF::MemoryInstrumentation {
public:
    explicit MemoryInstrumentationImpl(WTF::MemoryInstrumentationClient* client)
        : MemoryInstrumentation(client)
    {
    }

    size_t selfSize() const;

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    virtual void deferObject(PassOwnPtr<WrapperBase>) OVERRIDE;
    virtual void processDeferredObjects() OVERRIDE;

    Vector<OwnPtr<WrapperBase> > m_deferredObjects;
};

} // namespace WebCore

#endif // !defined(MemoryInstrumentationImpl_h)

