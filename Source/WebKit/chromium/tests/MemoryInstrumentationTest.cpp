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

#include "DataRef.h"
#include "MemoryInstrumentationImpl.h"

#include <gtest/gtest.h>

#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace {

class NotInstrumented {
public:
    char m_data[42];
};

class Instrumented {
public:
    Instrumented() : m_notInstrumented(new NotInstrumented) { }
    virtual ~Instrumented() { delete m_notInstrumented; }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::DOM);
        info.addMember(m_notInstrumented);
    }
    NotInstrumented* m_notInstrumented;
};

TEST(MemoryInstrumentationTest, sizeOf)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    Instrumented instrumented;
    impl.addRootObject(instrumented);
    EXPECT_EQ(sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
    EXPECT_EQ(1, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, nullCheck)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    Instrumented* instrumented = 0;
    impl.addRootObject(instrumented);
    EXPECT_EQ(0u, impl.reportedSizeForAllTypes());
    EXPECT_EQ(0, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, ptrVsRef)
{
    {
        VisitedObjects visitedObjects;
        MemoryInstrumentationImpl impl(visitedObjects);
        Instrumented instrumented;
        impl.addRootObject(&instrumented);
        EXPECT_EQ(sizeof(Instrumented) + sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
        EXPECT_EQ(2, visitedObjects.size());
    }
    {
        VisitedObjects visitedObjects;
        MemoryInstrumentationImpl impl(visitedObjects);
        Instrumented instrumented;
        impl.addRootObject(instrumented);
        EXPECT_EQ(sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
        EXPECT_EQ(1, visitedObjects.size());
    }
}

TEST(MemoryInstrumentationTest, ownPtr)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    OwnPtr<Instrumented> instrumented(adoptPtr(new Instrumented));
    impl.addRootObject(instrumented);
    EXPECT_EQ(sizeof(Instrumented) + sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
    EXPECT_EQ(2, visitedObjects.size());
}

class InstrumentedRefPtr : public RefCounted<InstrumentedRefPtr> {
public:
    InstrumentedRefPtr() : m_notInstrumented(new NotInstrumented) { }
    virtual ~InstrumentedRefPtr() { delete m_notInstrumented; }
    static PassRefPtr<InstrumentedRefPtr> create() { return adoptRef(new InstrumentedRefPtr()); }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::DOM);
        info.addMember(m_notInstrumented);
    }
    NotInstrumented* m_notInstrumented;
};

TEST(MemoryInstrumentationTest, dataRef)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    DataRef<InstrumentedRefPtr> instrumentedRefPtr;
    instrumentedRefPtr.init();
    impl.addRootObject(instrumentedRefPtr);
    EXPECT_EQ(sizeof(InstrumentedRefPtr) + sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
    EXPECT_EQ(2, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, refPtr)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    RefPtr<InstrumentedRefPtr> instrumentedRefPtr(adoptRef(new InstrumentedRefPtr));
    impl.addRootObject(instrumentedRefPtr);
    EXPECT_EQ(sizeof(InstrumentedRefPtr) + sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
    EXPECT_EQ(2, visitedObjects.size());
}

class InstrumentedWithOwnPtr : public Instrumented {
public:
    InstrumentedWithOwnPtr() : m_notInstrumentedOwnPtr(adoptPtr(new NotInstrumented)) { }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
        Instrumented::reportMemoryUsage(memoryObjectInfo);
        info.addMember(m_notInstrumentedOwnPtr);
    }
    OwnPtr<NotInstrumented> m_notInstrumentedOwnPtr;
};

TEST(MemoryInstrumentationTest, ownPtrNotInstrumented)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    InstrumentedWithOwnPtr instrumentedWithOwnPtr;
    impl.addRootObject(instrumentedWithOwnPtr);
    EXPECT_EQ(2 * sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
    EXPECT_EQ(2, visitedObjects.size());
}

class InstrumentedOther {
public:
    InstrumentedOther() : m_data(0) { }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::Other);
    }
    int m_data;
};

class InstrumentedDOM {
public:
    InstrumentedDOM() : m_instrumentedOther(adoptPtr(new InstrumentedOther)) { }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::DOM);
        info.addInstrumentedMember(m_instrumentedOther);
    }
    OwnPtr<InstrumentedOther> m_instrumentedOther;
};

TEST(MemoryInstrumentationTest, ownerTypePropagation)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    OwnPtr<InstrumentedDOM> instrumentedDOM(adoptPtr(new InstrumentedDOM));
    impl.addRootObject(instrumentedDOM);
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedOther), impl.reportedSizeForAllTypes());
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedOther), impl.totalSize(MemoryInstrumentation::DOM));
    EXPECT_EQ(2, visitedObjects.size());
}

class NonVirtualInstrumented {
public:
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::DOM);
        info.addInstrumentedMember(m_instrumented);
    }

    Instrumented m_instrumented;
};

TEST(MemoryInstrumentationTest, visitFirstMemberInNonVirtualClass)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    NonVirtualInstrumented nonVirtualInstrumented;
    impl.addRootObject(&nonVirtualInstrumented);
    EXPECT_EQ(sizeof(NonVirtualInstrumented) + sizeof(NotInstrumented), impl.reportedSizeForAllTypes());
    EXPECT_EQ(2, visitedObjects.size());
}

} // namespace

