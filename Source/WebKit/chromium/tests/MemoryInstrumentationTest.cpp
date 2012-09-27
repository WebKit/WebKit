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

#include <wtf/ArrayBuffer.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryInstrumentationArrayBufferView.h>
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

class InstrumentationTestHelper {
public:
    InstrumentationTestHelper() : m_client(0), m_instrumentation(&m_client) { }

    template<typename T>
    void addRootObject(const T& t) { m_instrumentation.addRootObject(t); }
    size_t reportedSizeForAllTypes() const { return m_client.reportedSizeForAllTypes(); }
    int visitedObjects() const { return m_client.visitedObjects(); }
    size_t totalSize(MemoryObjectType objectType) const { return m_client.totalSize(objectType); }

private:
    MemoryInstrumentationClientImpl m_client;
    MemoryInstrumentationImpl m_instrumentation;
};

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
    InstrumentationTestHelper helper;
    Instrumented instrumented;
    helper.addRootObject(instrumented);
    EXPECT_EQ(sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, nullCheck)
{
    InstrumentationTestHelper helper;
    Instrumented* instrumented = 0;
    helper.addRootObject(instrumented);
    EXPECT_EQ(0u, helper.reportedSizeForAllTypes());
    EXPECT_EQ(0, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, ptrVsRef)
{
    {
        InstrumentationTestHelper helper;
        Instrumented instrumented;
        helper.addRootObject(&instrumented);
        EXPECT_EQ(sizeof(Instrumented) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
        EXPECT_EQ(2, helper.visitedObjects());
    }
    {
        InstrumentationTestHelper helper;
        Instrumented instrumented;
        helper.addRootObject(instrumented);
        EXPECT_EQ(sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1, helper.visitedObjects());
    }
}

TEST(MemoryInstrumentationTest, ownPtr)
{
    InstrumentationTestHelper helper;
    OwnPtr<Instrumented> instrumented(adoptPtr(new Instrumented));
    helper.addRootObject(instrumented);
    EXPECT_EQ(sizeof(Instrumented) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2, helper.visitedObjects());
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
    InstrumentationTestHelper helper;
    DataRef<InstrumentedRefPtr> instrumentedRefPtr;
    instrumentedRefPtr.init();
    helper.addRootObject(instrumentedRefPtr);
    EXPECT_EQ(sizeof(InstrumentedRefPtr) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, refPtr)
{
    InstrumentationTestHelper helper;
    RefPtr<InstrumentedRefPtr> instrumentedRefPtr(adoptRef(new InstrumentedRefPtr));
    helper.addRootObject(instrumentedRefPtr);
    EXPECT_EQ(sizeof(InstrumentedRefPtr) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2, helper.visitedObjects());
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
    InstrumentationTestHelper helper;
    InstrumentedWithOwnPtr instrumentedWithOwnPtr;
    helper.addRootObject(instrumentedWithOwnPtr);
    EXPECT_EQ(2 * sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2, helper.visitedObjects());
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
    InstrumentationTestHelper helper;
    OwnPtr<InstrumentedDOM> instrumentedDOM(adoptPtr(new InstrumentedDOM));
    helper.addRootObject(instrumentedDOM);
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedUndefined), helper.reportedSizeForAllTypes());
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedUndefined), helper.totalSize(WebCoreMemoryTypes::DOM));
    EXPECT_EQ(2, helper.visitedObjects());
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
    InstrumentationTestHelper helper;
    NonVirtualInstrumented nonVirtualInstrumented;
    helper.addRootObject(&nonVirtualInstrumented);
    EXPECT_EQ(sizeof(NonVirtualInstrumented) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2, helper.visitedObjects());
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
        InstrumentationTestHelper helper;
        InstrumentedOwner<String> stringInstrumentedOwner("String");
        stringInstrumentedOwner.m_value.characters(); // Force 16bit shadow creation.
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + stringInstrumentedOwner.m_value.length() * 2, helper.reportedSizeForAllTypes());
        EXPECT_EQ(2, helper.visitedObjects());
    }

    {
        InstrumentationTestHelper helper;
        InstrumentedOwner<AtomicString> atomicStringInstrumentedOwner("AtomicString");
        atomicStringInstrumentedOwner.m_value.string().characters(); // Force 16bit shadow creation.
        helper.addRootObject(atomicStringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + atomicStringInstrumentedOwner.m_value.length() * 2, helper.reportedSizeForAllTypes());
        EXPECT_EQ(2, helper.visitedObjects());
    }

    {
        InstrumentationTestHelper helper;
        InstrumentedOwner<CString> cStringInstrumentedOwner("CString");
        helper.addRootObject(cStringInstrumentedOwner);
        EXPECT_EQ(sizeof(WTF::CStringBuffer) + cStringInstrumentedOwner.m_value.length(), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1, helper.visitedObjects());
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
    InstrumentationTestHelper helper;
    RefPtr<StringImpl> refPtr;
    TwoPointersToRefPtr root(refPtr);
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(RefPtr<StringImpl>), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1, helper.visitedObjects());
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
    InstrumentationTestHelper helper;
    OwnPtr<NotInstrumented> ownPtr;
    TwoPointersToOwnPtr root(ownPtr);
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(OwnPtr<NotInstrumented>), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1, helper.visitedObjects());
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
        InstrumentationTestHelper helper;

        OwnPtr<InstrumentedTemplate<String> > value = adoptPtr(new InstrumentedTemplate<String>(""));
        InstrumentedOwner<InstrumentedTemplate<String>* > root(value.get());
        helper.addRootObject(root);
        EXPECT_EQ(sizeof(InstrumentedTemplate<String>) + sizeof(StringImpl), helper.reportedSizeForAllTypes());
        // FIXME: it is failing on Chromium Canary bots but works fine locally.
        // EXPECT_EQ(2, helper.visitedObjects());
    }
    {
        InstrumentationTestHelper helper;

        OwnPtr<InstrumentedTemplate<NotInstrumented> > value = adoptPtr(new InstrumentedTemplate<NotInstrumented>(""));
        InstrumentedOwner<InstrumentedTemplate<NotInstrumented>* > root(value.get());
        helper.addRootObject(root);
        EXPECT_EQ(sizeof(InstrumentedTemplate<NotInstrumented>), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1, helper.visitedObjects());
    }
}

TEST(MemoryInstrumentationTest, vectorZeroInlineCapacity)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<Vector<int> > vectorOwner(16);
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(16 * sizeof(int), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, vectorFieldWithInlineCapacity)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<Vector<int, 4> > vectorOwner;
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(static_cast<size_t>(0), helper.reportedSizeForAllTypes());
    EXPECT_EQ(0, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, vectorFieldWithInlineCapacityResized)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<Vector<int, 4> > vectorOwner;
    vectorOwner.m_value.reserveCapacity(8);
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(8 * sizeof(int), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, heapAllocatedVectorWithInlineCapacity)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<OwnPtr<Vector<int, 4> > > vectorOwner;
    vectorOwner.m_value = adoptPtr(new Vector<int, 4>());
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(sizeof(Vector<int, 4>), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, heapAllocatedVectorWithInlineCapacityResized)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<OwnPtr<Vector<int, 4> > > vectorOwner;
    vectorOwner.m_value = adoptPtr(new Vector<int, 4>());
    vectorOwner.m_value->reserveCapacity(8);
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(8 * sizeof(int) + sizeof(Vector<int, 4>), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, vectorWithInstrumentedType)
{
    InstrumentationTestHelper helper;

    typedef Vector<String> StringVector;
    OwnPtr<StringVector> value = adoptPtr(new StringVector());
    size_t count = 10;
    for (size_t i = 0; i < count; ++i)
        value->append("string");
    InstrumentedOwner<StringVector* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(StringVector) + sizeof(String) * value->capacity() + sizeof(StringImpl) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(count + 2, (size_t)helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashSetWithInstrumentedType)
{
    InstrumentationTestHelper helper;

    typedef HashSet<String> ValueType;
    OwnPtr<ValueType> value = adoptPtr(new ValueType());
    size_t count = 10;
    for (size_t i = 0; i < count; ++i)
        value->add(String::number(i));
    InstrumentedOwner<ValueType* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(ValueType) + sizeof(String) * value->capacity() + sizeof(StringImpl) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(count + 2, (size_t)helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, arrayBuffer)
{
    InstrumentationTestHelper helper;

    typedef InstrumentedTemplate<RefPtr<ArrayBuffer> > ValueType;
    ValueType value(ArrayBuffer::create(1000, sizeof(int)));
    helper.addRootObject(value);
    EXPECT_EQ(sizeof(int) * 1000 + sizeof(ArrayBuffer), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2, helper.visitedObjects());
}

} // namespace

