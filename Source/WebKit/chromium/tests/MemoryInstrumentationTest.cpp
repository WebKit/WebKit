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
#include "WebCoreMemoryInstrumentation.h"

#include <gtest/gtest.h>

#include <wtf/HashSet.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace {

class NotInstrumented {
public:
    NotInstrumented(const char* = 0) { }
    char m_data[42];
};

class Instrumented {
public:
    Instrumented() : m_notInstrumented(new NotInstrumented) { }
    virtual ~Instrumented() { delete m_notInstrumented; }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
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
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
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
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
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

class InstrumentedUndefined {
public:
    InstrumentedUndefined() : m_data(0) { }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this);
    }
    int m_data;
};

class InstrumentedDOM {
public:
    InstrumentedDOM() : m_instrumentedUndefined(adoptPtr(new InstrumentedUndefined)) { }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.addMember(m_instrumentedUndefined);
    }
    OwnPtr<InstrumentedUndefined> m_instrumentedUndefined;
};

TEST(MemoryInstrumentationTest, ownerTypePropagation)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    OwnPtr<InstrumentedDOM> instrumentedDOM(adoptPtr(new InstrumentedDOM));
    impl.addRootObject(instrumentedDOM);
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedUndefined), impl.reportedSizeForAllTypes());
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedUndefined), impl.totalSize(WebCoreMemoryTypes::DOM));
    EXPECT_EQ(2, visitedObjects.size());
}

class NonVirtualInstrumented {
public:
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.addMember(m_instrumented);
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

template<typename T>
class InstrumentedOwner {
public:
    template<typename V>
    InstrumentedOwner(const V& value) : m_value(value) { }
    InstrumentedOwner() { }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.addMember(m_value);
    }

    T m_value;
};

TEST(MemoryInstrumentationTest, visitStrings)
{
    {
        VisitedObjects visitedObjects;
        MemoryInstrumentationImpl impl(visitedObjects);
        InstrumentedOwner<String> stringInstrumentedOwner("String");
        stringInstrumentedOwner.m_value.characters(); // Force 16bit shadow creation.
        impl.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + stringInstrumentedOwner.m_value.length() * 2, impl.reportedSizeForAllTypes());
        EXPECT_EQ(2, visitedObjects.size());
    }

    {
        VisitedObjects visitedObjects;
        MemoryInstrumentationImpl impl(visitedObjects);
        InstrumentedOwner<AtomicString> atomicStringInstrumentedOwner("AtomicString");
        atomicStringInstrumentedOwner.m_value.string().characters(); // Force 16bit shadow creation.
        impl.addRootObject(atomicStringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + atomicStringInstrumentedOwner.m_value.length() * 2, impl.reportedSizeForAllTypes());
        EXPECT_EQ(2, visitedObjects.size());
    }

    {
        VisitedObjects visitedObjects;
        MemoryInstrumentationImpl impl(visitedObjects);
        InstrumentedOwner<CString> cStringInstrumentedOwner("CString");
        impl.addRootObject(cStringInstrumentedOwner);
        EXPECT_EQ(sizeof(WTF::CStringBuffer) + cStringInstrumentedOwner.m_value.length(), impl.reportedSizeForAllTypes());
        EXPECT_EQ(1, visitedObjects.size());
    }
}

class TwoPointersToRefPtr {
public:
    TwoPointersToRefPtr(const RefPtr<StringImpl>& value) : m_ptr1(&value), m_ptr2(&value)  { }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.addMember(m_ptr1);
        info.addMember(m_ptr2);
    }

    const RefPtr<StringImpl>* m_ptr1;
    const RefPtr<StringImpl>* m_ptr2;
};

TEST(MemoryInstrumentationTest, refPtrPtr)
{
    RefPtr<StringImpl> refPtr;
    TwoPointersToRefPtr root(refPtr);
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    impl.addRootObject(root);
    EXPECT_EQ(sizeof(RefPtr<StringImpl>), impl.reportedSizeForAllTypes());
    EXPECT_EQ(1, visitedObjects.size());
}

class TwoPointersToOwnPtr {
public:
    TwoPointersToOwnPtr(const OwnPtr<NotInstrumented>& value) : m_ptr1(&value), m_ptr2(&value)  { }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.addMember(m_ptr1);
        info.addMember(m_ptr2);
    }

    const OwnPtr<NotInstrumented>* m_ptr1;
    const OwnPtr<NotInstrumented>* m_ptr2;
};

TEST(MemoryInstrumentationTest, ownPtrPtr)
{
    OwnPtr<NotInstrumented> ownPtr;
    TwoPointersToOwnPtr root(ownPtr);
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    impl.addRootObject(root);
    EXPECT_EQ(sizeof(OwnPtr<NotInstrumented>), impl.reportedSizeForAllTypes());
    EXPECT_EQ(1, visitedObjects.size());
}

template<typename T>
class InstrumentedTemplate {
public:
    template<typename V>
    InstrumentedTemplate(const V& value) : m_value(value) { }

    template<typename MemoryObjectInfo>
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        typename MemoryObjectInfo::ClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.addMember(m_value);
    }

    T m_value;
};

TEST(MemoryInstrumentationTest, detectReportMemoryUsageMethod)
{
    {
        VisitedObjects visitedObjects;
        MemoryInstrumentationImpl impl(visitedObjects);

        OwnPtr<InstrumentedTemplate<String> > value = adoptPtr(new InstrumentedTemplate<String>(""));
        InstrumentedOwner<InstrumentedTemplate<String>* > root(value.get());
        impl.addRootObject(root);
        EXPECT_EQ(sizeof(InstrumentedTemplate<String>) + sizeof(StringImpl), impl.reportedSizeForAllTypes());
        // FIXME: it is failing on Chromium Canary bots but works fine locally.
        // EXPECT_EQ(2, visitedObjects.size());
    }
    {
        VisitedObjects visitedObjects;
        MemoryInstrumentationImpl impl(visitedObjects);

        OwnPtr<InstrumentedTemplate<NotInstrumented> > value = adoptPtr(new InstrumentedTemplate<NotInstrumented>(""));
        InstrumentedOwner<InstrumentedTemplate<NotInstrumented>* > root(value.get());
        impl.addRootObject(root);
        EXPECT_EQ(sizeof(InstrumentedTemplate<NotInstrumented>), impl.reportedSizeForAllTypes());
        EXPECT_EQ(1, visitedObjects.size());
    }
}

TEST(MemoryInstrumentationTest, vectorZeroInlineCapacity)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    InstrumentedOwner<Vector<int> > vectorOwner(16);
    impl.addRootObject(vectorOwner);
    EXPECT_EQ(16 * sizeof(int), impl.reportedSizeForAllTypes());
    EXPECT_EQ(1, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, vectorFieldWithInlineCapacity)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    InstrumentedOwner<Vector<int, 4> > vectorOwner;
    impl.addRootObject(vectorOwner);
    EXPECT_EQ(static_cast<size_t>(0), impl.reportedSizeForAllTypes());
    EXPECT_EQ(0, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, vectorFieldWithInlineCapacityResized)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    InstrumentedOwner<Vector<int, 4> > vectorOwner;
    vectorOwner.m_value.reserveCapacity(8);
    impl.addRootObject(vectorOwner);
    EXPECT_EQ(8 * sizeof(int), impl.reportedSizeForAllTypes());
    EXPECT_EQ(1, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, heapAllocatedVectorWithInlineCapacity)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    InstrumentedOwner<OwnPtr<Vector<int, 4> > > vectorOwner;
    vectorOwner.m_value = adoptPtr(new Vector<int, 4>());
    impl.addRootObject(vectorOwner);
    EXPECT_EQ(sizeof(Vector<int, 4>), impl.reportedSizeForAllTypes());
    EXPECT_EQ(1, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, heapAllocatedVectorWithInlineCapacityResized)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);
    InstrumentedOwner<OwnPtr<Vector<int, 4> > > vectorOwner;
    vectorOwner.m_value = adoptPtr(new Vector<int, 4>());
    vectorOwner.m_value->reserveCapacity(8);
    impl.addRootObject(vectorOwner);
    EXPECT_EQ(8 * sizeof(int) + sizeof(Vector<int, 4>), impl.reportedSizeForAllTypes());
    EXPECT_EQ(2, visitedObjects.size());
}

TEST(MemoryInstrumentationTest, vectorWithInstrumentedType)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);

    typedef Vector<String> StringVector;
    OwnPtr<StringVector> value = adoptPtr(new StringVector());
    size_t count = 10;
    for (size_t i = 0; i < count; ++i)
        value->append("string");
    InstrumentedOwner<StringVector* > root(value.get());
    impl.addRootObject(root);
    EXPECT_EQ(sizeof(StringVector) + sizeof(String) * value->capacity() + sizeof(StringImpl) * value->size(), impl.reportedSizeForAllTypes());
    EXPECT_EQ(count + 2, (size_t)visitedObjects.size());
}

TEST(MemoryInstrumentationTest, hashSetWithInstrumentedType)
{
    VisitedObjects visitedObjects;
    MemoryInstrumentationImpl impl(visitedObjects);

    typedef HashSet<String> ValueType;
    OwnPtr<ValueType> value = adoptPtr(new ValueType());
    size_t count = 10;
    for (size_t i = 0; i < count; ++i)
        value->add(String::number(i));
    InstrumentedOwner<ValueType* > root(value.get());
    impl.addRootObject(root);
    EXPECT_EQ(sizeof(ValueType) + sizeof(String) * value->capacity() + sizeof(StringImpl) * value->size(), impl.reportedSizeForAllTypes());
    EXPECT_EQ(count + 2, (size_t)visitedObjects.size());
}

} // namespace

