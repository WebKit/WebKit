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

#include <gtest/gtest.h>

#include <wtf/ArrayBuffer.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/MemoryInstrumentation.h>
#include <wtf/MemoryInstrumentationArrayBufferView.h>
#include <wtf/MemoryInstrumentationHashCountedSet.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/MemoryInstrumentationListHashSet.h>
#include <wtf/MemoryInstrumentationString.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/MemoryObjectInfo.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

namespace {
enum TestEnum { ONE = 1, TWO, THREE, MY_ENUM_MAX };
}

namespace WTF {

template<> struct DefaultHash<TestEnum> {
    typedef IntHash<unsigned> Hash;
};

template<> struct HashTraits<TestEnum> : GenericHashTraits<TestEnum> {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;
    static void constructDeletedValue(TestEnum& slot) { slot = static_cast<TestEnum>(MY_ENUM_MAX + 1); }
    static bool isDeletedValue(TestEnum value) { return value == (MY_ENUM_MAX + 1); }
};

}

namespace {

using WTF::MemoryObjectInfo;
using WTF::MemoryClassInfo;
using WTF::MemoryObjectType;
using WTF::MemberType;

MemoryObjectType TestType = "TestType";

class MemoryInstrumentationTestClient : public WTF::MemoryInstrumentationClient {
public:
    MemoryInstrumentationTestClient() : m_links(WTF::LastMemberTypeEntry)
    {
        m_links[WTF::PointerMember] = 0;
        m_links[WTF::ReferenceMember] = 0;
        m_links[WTF::RefPtrMember] = 0;
        m_links[WTF::OwnPtrMember] = 0;
    }

    virtual void countObjectSize(const void*, MemoryObjectType objectType, size_t size)
    {
        TypeToSizeMap::AddResult result = m_totalSizes.add(objectType, size);
        if (!result.isNewEntry)
            result.iterator->value += size;
    }
    virtual bool visited(const void* object) { return !m_visitedObjects.add(object).isNewEntry; }
    virtual bool checkCountedObject(const void*) { return true; }
    virtual void reportNode(const MemoryObjectInfo&) OVERRIDE { }
    virtual void reportEdge(const void*, const char*, MemberType memberType) OVERRIDE
    {
        ++m_links[memberType];
    }
    virtual void reportLeaf(const MemoryObjectInfo&, const char*) OVERRIDE
    {
        ++m_links[WTF::OwnPtrMember];
    }
    virtual void reportBaseAddress(const void*, const void*) OVERRIDE { }
    virtual int registerString(const char*) OVERRIDE { return -1; }

    size_t visitedObjects() const { return m_visitedObjects.size(); }
    size_t totalSize(const MemoryObjectType objectType) const
    {
        TypeToSizeMap::const_iterator i = m_totalSizes.find(objectType);
        return i == m_totalSizes.end() ? 0 : i->value;
    }

    size_t linksCount(const WTF::MemberType memberType) const
    {
        return m_links[memberType];
    }

    size_t reportedSizeForAllTypes() const
    {
        size_t size = 0;
        for (TypeToSizeMap::const_iterator i = m_totalSizes.begin(); i != m_totalSizes.end(); ++i)
            size += i->value;
        return size;
    }

private:
    typedef HashMap<MemoryObjectType, size_t> TypeToSizeMap;
    TypeToSizeMap m_totalSizes;
    WTF::HashSet<const void*> m_visitedObjects;
    WTF::Vector<size_t, WTF::LastMemberTypeEntry> m_links;
};

class InstrumentationTestImpl : public WTF::MemoryInstrumentation {
public:
    explicit InstrumentationTestImpl(MemoryInstrumentationTestClient* client)
        : MemoryInstrumentation(client)
        , m_client(client) { }

    virtual void processDeferredObjects();
    virtual void deferObject(PassOwnPtr<WrapperBase>);

    size_t visitedObjects() const { return m_client->visitedObjects(); }
    size_t reportedSizeForAllTypes() const { return m_client->reportedSizeForAllTypes(); }
    size_t totalSize(const MemoryObjectType objectType) const { return m_client->totalSize(objectType); }
    size_t linksCount(const WTF::MemberType memberType) const {  return m_client->linksCount(memberType); }

private:
    MemoryInstrumentationTestClient* m_client;
    Vector<OwnPtr<WrapperBase> > m_deferredObjects;
};

class InstrumentationTestHelper : public InstrumentationTestImpl {
public:
    InstrumentationTestHelper() : InstrumentationTestImpl(&m_client) { }

private:
    MemoryInstrumentationTestClient m_client;
};

void InstrumentationTestImpl::processDeferredObjects()
{
    while (!m_deferredObjects.isEmpty()) {
        OwnPtr<WrapperBase> pointer = m_deferredObjects.last().release();
        m_deferredObjects.removeLast();
        pointer->process(this);
    }
}

void InstrumentationTestImpl::deferObject(PassOwnPtr<WrapperBase> pointer)
{
    m_deferredObjects.append(pointer);
}

class NotInstrumented {
public:
    NotInstrumented(const char* = 0) { }
    char m_data[42];
};

class Instrumented {
public:
    Instrumented() : m_notInstrumented(new NotInstrumented) { }
    virtual ~Instrumented() { disposeOwnedObject(); }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
        memoryObjectInfo->setClassName("Instrumented");
        info.addMember(m_notInstrumented, "m_notInstrumented");
    }

    void disposeOwnedObject()
    {
        delete m_notInstrumented;
        m_notInstrumented = 0;
    }

    NotInstrumented* m_notInstrumented;
};

TEST(MemoryInstrumentationTest, sizeOf)
{
    InstrumentationTestHelper helper;
    Instrumented instrumented;
    helper.addRootObject(instrumented);
    EXPECT_EQ(sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1u, helper.visitedObjects());
    EXPECT_EQ(1u, helper.linksCount(WTF::PointerMember));
}

TEST(MemoryInstrumentationTest, nullCheck)
{
    InstrumentationTestHelper helper;
    Instrumented* instrumented = 0;
    helper.addRootObject(instrumented);
    EXPECT_EQ(0u, helper.reportedSizeForAllTypes());
    EXPECT_EQ(0u, helper.visitedObjects());
    EXPECT_EQ(0u, helper.linksCount(WTF::PointerMember));
}

TEST(MemoryInstrumentationTest, ptrVsRef)
{
    {
        InstrumentationTestHelper helper;
        Instrumented instrumented;
        helper.addRootObject(&instrumented);
        EXPECT_EQ(sizeof(Instrumented) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
        EXPECT_EQ(2u, helper.visitedObjects());
        EXPECT_EQ(1u, helper.linksCount(WTF::PointerMember));
    }
    {
        InstrumentationTestHelper helper;
        Instrumented instrumented;
        helper.addRootObject(instrumented);
        EXPECT_EQ(sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1u, helper.visitedObjects());
        EXPECT_EQ(1u, helper.linksCount(WTF::PointerMember));
    }
}

class InstrumentedWithOwnPtr : public Instrumented {
public:
    InstrumentedWithOwnPtr() : m_notInstrumentedOwnPtr(adoptPtr(new NotInstrumented)) { }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
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
    EXPECT_EQ(2u * sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2u, helper.visitedObjects());
    EXPECT_EQ(1u, helper.linksCount(WTF::OwnPtrMember));
    EXPECT_EQ(1u, helper.linksCount(WTF::PointerMember));
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
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
        info.addMember(m_instrumentedUndefined);
    }
    OwnPtr<InstrumentedUndefined> m_instrumentedUndefined;
};

TEST(MemoryInstrumentationTest, ownerTypePropagation)
{
    InstrumentationTestHelper helper;
    OwnPtr<InstrumentedDOM> instrumentedDOM(adoptPtr(new InstrumentedDOM));
    helper.addRootObject(instrumentedDOM.get());
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedUndefined), helper.reportedSizeForAllTypes());
    EXPECT_EQ(sizeof(InstrumentedDOM) + sizeof(InstrumentedUndefined), helper.totalSize(TestType));
    EXPECT_EQ(2u, helper.visitedObjects());
}

class NonVirtualInstrumented {
public:
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
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
    EXPECT_EQ(2u, helper.visitedObjects());
}

template<typename T>
class InstrumentedOwner {
public:
    template<typename V>
    InstrumentedOwner(const V& value) : m_value(value) { }
    InstrumentedOwner() { }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
        info.addMember(m_value);
    }

    T m_value;
};

TEST(MemoryInstrumentationTest, visitStrings)
{
    { // 8-bit string.
        InstrumentationTestHelper helper;
        InstrumentedOwner<String> stringInstrumentedOwner("String");
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + stringInstrumentedOwner.m_value.length(), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1u, helper.visitedObjects());
    }

    { // 8-bit string with 16bit shadow.
        InstrumentationTestHelper helper;
        InstrumentedOwner<String> stringInstrumentedOwner("String");
        stringInstrumentedOwner.m_value.characters();
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + stringInstrumentedOwner.m_value.length() * (sizeof(LChar) + sizeof(UChar)), helper.reportedSizeForAllTypes());
        EXPECT_EQ(2u, helper.visitedObjects());
    }

    { // 16 bit string.
        InstrumentationTestHelper helper;
        String string("String");
        InstrumentedOwner<String> stringInstrumentedOwner(String(string.characters(), string.length()));
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + stringInstrumentedOwner.m_value.length() * sizeof(UChar), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1u, helper.visitedObjects());
    }

    { // ASCIILiteral
        InstrumentationTestHelper helper;
        ASCIILiteral literal("String");
        InstrumentedOwner<String> stringInstrumentedOwner(literal);
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1u, helper.visitedObjects());
    }

    { // Zero terminated internal buffer.
        InstrumentationTestHelper helper;
        InstrumentedOwner<String> stringInstrumentedOwner("string");
        stringInstrumentedOwner.m_value.charactersWithNullTermination();
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + (stringInstrumentedOwner.m_value.length() + 1) * (sizeof(LChar) + sizeof(UChar)), helper.reportedSizeForAllTypes());
        EXPECT_EQ(2u, helper.visitedObjects());
    }

    { // Substring
        InstrumentationTestHelper helper;
        String baseString("String");
        baseString.characters(); // Force 16 shadow creation.
        InstrumentedOwner<String> stringInstrumentedOwner(baseString.substringSharingImpl(1, 4));
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) * 2 + baseString.length() * (sizeof(LChar) + sizeof(UChar)), helper.reportedSizeForAllTypes());
        EXPECT_EQ(3u, helper.visitedObjects());
    }

    { // Owned buffer.
        InstrumentationTestHelper helper;
        StringBuffer<LChar> buffer(6);
        InstrumentedOwner<String> stringInstrumentedOwner(String::adopt(buffer));
        helper.addRootObject(stringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + stringInstrumentedOwner.m_value.length(), helper.reportedSizeForAllTypes());
        EXPECT_EQ(2u, helper.visitedObjects());
    }

    {
        InstrumentationTestHelper helper;
        InstrumentedOwner<AtomicString> atomicStringInstrumentedOwner("AtomicString");
        atomicStringInstrumentedOwner.m_value.string().characters(); // Force 16bit shadow creation.
        helper.addRootObject(atomicStringInstrumentedOwner);
        EXPECT_EQ(sizeof(StringImpl) + atomicStringInstrumentedOwner.m_value.length() * (sizeof(LChar) + sizeof(UChar)), helper.reportedSizeForAllTypes());
        EXPECT_EQ(2u, helper.visitedObjects());
    }

    {
        InstrumentationTestHelper helper;
        InstrumentedOwner<CString> cStringInstrumentedOwner("CString");
        helper.addRootObject(cStringInstrumentedOwner);
        EXPECT_EQ(sizeof(WTF::CStringBuffer) + cStringInstrumentedOwner.m_value.length(), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1u, helper.visitedObjects());
    }
}

class TwoPointersToRefPtr {
public:
    TwoPointersToRefPtr(const RefPtr<StringImpl>& value) : m_ptr1(&value), m_ptr2(&value)  { }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
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
    EXPECT_EQ(1u, helper.visitedObjects());
}

class TwoPointersToOwnPtr {
public:
    TwoPointersToOwnPtr(const OwnPtr<NotInstrumented>& value) : m_ptr1(&value), m_ptr2(&value)  { }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
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
    EXPECT_EQ(1u, helper.visitedObjects());
}

template<typename T>
class InstrumentedTemplate {
public:
    template<typename V>
    InstrumentedTemplate(const V& value) : m_value(value) { }

    template<typename MemoryObjectInfo>
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        typename MemoryObjectInfo::ClassInfo info(memoryObjectInfo, this, TestType);
        info.addMember(m_value);
    }

    T m_value;
};

TEST(MemoryInstrumentationTest, detectReportMemoryUsageMethod)
{
    {
        InstrumentationTestHelper helper;

        OwnPtr<InstrumentedTemplate<String> > value(adoptPtr(new InstrumentedTemplate<String>("")));
        InstrumentedOwner<InstrumentedTemplate<String>* > root(value.get());
        helper.addRootObject(root);
        EXPECT_EQ(sizeof(InstrumentedTemplate<String>) + sizeof(StringImpl), helper.reportedSizeForAllTypes());
        // FIXME: it is failing on Chromium Canary bots but works fine locally.
        // EXPECT_EQ(2, helper.visitedObjects());
    }
    {
        InstrumentationTestHelper helper;

        OwnPtr<InstrumentedTemplate<NotInstrumented> > value(adoptPtr(new InstrumentedTemplate<NotInstrumented>("")));
        InstrumentedOwner<InstrumentedTemplate<NotInstrumented>* > root(value.get());
        helper.addRootObject(root);
        EXPECT_EQ(sizeof(InstrumentedTemplate<NotInstrumented>), helper.reportedSizeForAllTypes());
        EXPECT_EQ(1u, helper.visitedObjects());
    }
}

TEST(MemoryInstrumentationTest, vectorZeroInlineCapacity)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<Vector<int> > vectorOwner(16);
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(16 * sizeof(int), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1u, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, vectorFieldWithInlineCapacity)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<Vector<int, 4> > vectorOwner;
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(static_cast<size_t>(0), helper.reportedSizeForAllTypes());
    EXPECT_EQ(0u, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, vectorFieldWithInlineCapacityResized)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<Vector<int, 4> > vectorOwner;
    vectorOwner.m_value.reserveCapacity(8);
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(8u * sizeof(int), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1u, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, heapAllocatedVectorWithInlineCapacity)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<OwnPtr<Vector<int, 4> > > vectorOwner;
    vectorOwner.m_value = adoptPtr(new Vector<int, 4>());
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(sizeof(Vector<int, 4>), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1u, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, heapAllocatedVectorWithInlineCapacityResized)
{
    InstrumentationTestHelper helper;
    InstrumentedOwner<OwnPtr<Vector<int, 4> > > vectorOwner;
    vectorOwner.m_value = adoptPtr(new Vector<int, 4>());
    vectorOwner.m_value->reserveCapacity(8);
    helper.addRootObject(vectorOwner);
    EXPECT_EQ(8u * sizeof(int) + sizeof(Vector<int, 4>), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2u, helper.visitedObjects());
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
    EXPECT_EQ(sizeof(StringVector) + sizeof(String) * value->capacity() + (sizeof(StringImpl) + 6) * value->size(), helper.reportedSizeForAllTypes());
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
    EXPECT_EQ(sizeof(ValueType) + sizeof(String) * value->capacity() + (sizeof(StringImpl) + 1) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(count + 1, (size_t)helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashMapWithNotInstrumentedKeysAndValues)
{
    InstrumentationTestHelper helper;

    typedef HashMap<int, int> IntToIntMap;
    OwnPtr<IntToIntMap> value = adoptPtr(new IntToIntMap());
    size_t count = 10;
    for (size_t i = 1; i <= count; ++i)
        value->set(i, i);
    InstrumentedOwner<IntToIntMap* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(IntToIntMap) + sizeof(IntToIntMap::ValueType) * value->capacity(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(1u, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashMapWithInstrumentedKeys)
{
    InstrumentationTestHelper helper;

    typedef HashMap<String, int> StringToIntMap;
    OwnPtr<StringToIntMap> value = adoptPtr(new StringToIntMap());
    size_t count = 10;
    for (size_t i = 10; i < 10 + count; ++i)
        value->set(String::number(i), i);
    InstrumentedOwner<StringToIntMap* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(StringToIntMap) + sizeof(StringToIntMap::ValueType) * value->capacity() + (sizeof(StringImpl) + 2) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(count + 1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashMapWithInstrumentedValues)
{
    InstrumentationTestHelper helper;

    typedef HashMap<int, String> IntToStringMap;
    OwnPtr<IntToStringMap> value = adoptPtr(new IntToStringMap());
    size_t count = 10;
    for (size_t i = 10; i < 10 + count; ++i)
        value->set(i, String::number(i));
    InstrumentedOwner<IntToStringMap* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(IntToStringMap) + sizeof(IntToStringMap::ValueType) * value->capacity() + (sizeof(StringImpl) + 2) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(count + 1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashMapWithInstrumentedKeysAndValues)
{
    InstrumentationTestHelper helper;

    typedef HashMap<String, String> StringToStringMap;
    OwnPtr<StringToStringMap> value = adoptPtr(new StringToStringMap());
    size_t count = 10;
    for (size_t i = 10; i < 10 + count; ++i)
        value->set(String::number(count + i), String::number(i));
    InstrumentedOwner<StringToStringMap* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(StringToStringMap) + sizeof(StringToStringMap::ValueType) * value->capacity() + 2 * (sizeof(StringImpl) + 2) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2u * count + 1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashMapWithInstrumentedPointerKeysAndPointerValues)
{
    InstrumentationTestHelper helper;

    typedef HashMap<Instrumented*, Instrumented*> InstrumentedToInstrumentedMap;
    OwnPtr<InstrumentedToInstrumentedMap> value(adoptPtr(new InstrumentedToInstrumentedMap()));
    Vector<OwnPtr<Instrumented> > valuesVector;
    size_t count = 10;
    for (size_t i = 0; i < count; ++i) {
        valuesVector.append(adoptPtr(new Instrumented()));
        valuesVector.append(adoptPtr(new Instrumented()));
        value->set(valuesVector[2 * i].get(), valuesVector[2 * i + 1].get());
    }
    InstrumentedOwner<InstrumentedToInstrumentedMap* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(InstrumentedToInstrumentedMap) + sizeof(InstrumentedToInstrumentedMap::ValueType) * value->capacity() + 2 * (sizeof(Instrumented) + sizeof(NotInstrumented)) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2u * 2u * count + 1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, listHashSetWithInstrumentedType)
{
    InstrumentationTestHelper helper;

    typedef ListHashSet<String, 8> TestSet;
    OwnPtr<TestSet> value = adoptPtr(new TestSet());
    size_t count = 10;
    for (size_t i = 0; i < count; ++i)
        value->add(String::number(i));
    InstrumentedOwner<TestSet* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(TestSet) + sizeof(String) * value->capacity() + (sizeof(StringImpl) + 1 * sizeof(LChar)) * count +
        sizeof(WTF::ListHashSetNodeAllocator<String, 8>) + sizeof(WTF::ListHashSetNode<String, 8>) * (count - 8),
        helper.reportedSizeForAllTypes());
    EXPECT_EQ(1 + count, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, listHashSetWithInstrumentedTypeAfterValuesRemoval)
{
    InstrumentationTestHelper helper;

    typedef ListHashSet<String, 8> TestSet;
    OwnPtr<TestSet> value = adoptPtr(new TestSet());
    size_t count = 20;
    for (size_t i = 0; i < count; ++i)
        value->add(String::number(i));
    // Remove 10 values, 8 of which were allocated in the internal buffer.
    for (size_t i = 0; i < 10; ++i)
        value->remove(String::number(i));
    InstrumentedOwner<TestSet* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(TestSet) + sizeof(String) * value->capacity() + (sizeof(StringImpl) + 2 * sizeof(LChar)) * (count - 10) +
        sizeof(WTF::ListHashSetNodeAllocator<String, 8>) + sizeof(WTF::ListHashSetNode<String, 8>) * (count - 10),
        helper.reportedSizeForAllTypes());
    EXPECT_EQ(1 + (count - 10), helper.visitedObjects());
}

class InstrumentedConvertibleToInt {
public:
    InstrumentedConvertibleToInt() : m_notInstrumented(0) { }
    virtual ~InstrumentedConvertibleToInt() { }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
        info.addMember(m_notInstrumented);
    }

    operator int() const { return 2012; }

    NotInstrumented* m_notInstrumented;
};

// This test checks if reportMemoryUsage method will be called on a class
// that can be implicitly cast to int. Currently objects of such classes are
// treated as integers when they are stored in a HashMap by value and
// reportMemoryUsage will not be called on them. We may fix that later.
TEST(MemoryInstrumentationTest, hashMapWithValuesConvertibleToInt)
{
    InstrumentationTestHelper helper;

    typedef HashMap<InstrumentedConvertibleToInt*, InstrumentedConvertibleToInt> TestMap;
    OwnPtr<TestMap> value(adoptPtr(new TestMap()));
    Vector<OwnPtr<InstrumentedConvertibleToInt> > keysVector;
    Vector<OwnPtr<NotInstrumented> > valuesVector;
    size_t count = 10;
    for (size_t i = 0; i < count; ++i) {
        keysVector.append(adoptPtr(new InstrumentedConvertibleToInt()));
        valuesVector.append(adoptPtr(new NotInstrumented()));
        value->set(keysVector[i].get(), InstrumentedConvertibleToInt()).iterator->value.m_notInstrumented = valuesVector[i].get();
    }
    InstrumentedOwner<TestMap* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(TestMap) + sizeof(TestMap::ValueType) * value->capacity() +
        sizeof(InstrumentedConvertibleToInt) * count /* + sizeof(NotInstrumented) * count */, helper.reportedSizeForAllTypes());
    EXPECT_EQ(count + 1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashMapWithEnumKeysAndInstrumentedValues)
{
    InstrumentationTestHelper helper;

    typedef HashMap<TestEnum, String> EnumToStringMap;
    OwnPtr<EnumToStringMap> value(adoptPtr(new EnumToStringMap()));
    size_t count = MY_ENUM_MAX;
    for (size_t i = ONE; i <= count; ++i)
        value->set(static_cast<TestEnum>(i), String::number(i));
    InstrumentedOwner<EnumToStringMap* > root(value.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(EnumToStringMap) + sizeof(EnumToStringMap::ValueType) * value->capacity() + (sizeof(StringImpl) + 1) * value->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(count + 1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, hashCountedSetWithInstrumentedValues)
{
    InstrumentationTestHelper helper;

    typedef HashCountedSet<Instrumented*> TestSet;
    OwnPtr<TestSet> set(adoptPtr(new TestSet()));
    Vector<OwnPtr<Instrumented> > keysVector;
    size_t count = 10;
    for (size_t i = 0; i < count; ++i) {
        keysVector.append(adoptPtr(new Instrumented()));
        for (size_t j = 0; j <= i; j++)
            set->add(keysVector.last().get());
    }
    InstrumentedOwner<TestSet* > root(set.get());
    helper.addRootObject(root);
    EXPECT_EQ(sizeof(TestSet) + sizeof(HashMap<Instrumented*, unsigned>::ValueType) * set->capacity() + (sizeof(Instrumented) + sizeof(NotInstrumented))  * set->size(), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2u * count + 1, helper.visitedObjects());
}

TEST(MemoryInstrumentationTest, arrayBuffer)
{
    InstrumentationTestHelper helper;

    typedef InstrumentedTemplate<RefPtr<ArrayBuffer> > ValueType;
    ValueType value(ArrayBuffer::create(1000, sizeof(int)));
    helper.addRootObject(value);
    EXPECT_EQ(sizeof(int) * 1000 + sizeof(ArrayBuffer), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2u, helper.visitedObjects());
    EXPECT_EQ(1u, helper.linksCount(WTF::RefPtrMember));
}

class AncestorWithVirtualMethod {
public:
    virtual char* data() { return m_data; }

private:
    char m_data[10];
};

class ClassWithTwoAncestors : public AncestorWithVirtualMethod, public Instrumented {
public:
    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
        Instrumented::reportMemoryUsage(memoryObjectInfo);
    }
};

TEST(MemoryInstrumentationTest, instrumentedWithMultipleAncestors)
{
    InstrumentationTestHelper helper;
    OwnPtr<ClassWithTwoAncestors> instance = adoptPtr(new ClassWithTwoAncestors());
    ClassWithTwoAncestors* descendantPointer = instance.get();
    InstrumentedOwner<ClassWithTwoAncestors*> descendantPointerOwner(descendantPointer);
    Instrumented* ancestorPointer = descendantPointer;
    InstrumentedOwner<Instrumented*> ancestorPointerOwner(ancestorPointer);
    EXPECT_NE(static_cast<void*>(ancestorPointer), static_cast<void*>(descendantPointer));

    helper.addRootObject(descendantPointerOwner);
    helper.addRootObject(ancestorPointerOwner);
    EXPECT_EQ(sizeof(ClassWithTwoAncestors) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(3u, helper.visitedObjects());
}

class CheckCountedObjectsClient : public MemoryInstrumentationTestClient {
public:
    CheckCountedObjectsClient(const void* expectedPointer) : m_expectedPointer(expectedPointer), m_expectedPointerFound(false) { }
    virtual bool checkCountedObject(const void* pointer)
    {
        EXPECT_EQ(pointer, m_expectedPointer);
        m_expectedPointerFound = true;
        return true;
    }
    bool expectedPointerFound() { return m_expectedPointerFound; }

private:
    const void* m_expectedPointer;
    bool m_expectedPointerFound;
};

TEST(MemoryInstrumentationTest, checkCountedObjectWithMultipleAncestors)
{
    OwnPtr<ClassWithTwoAncestors> instance = adoptPtr(new ClassWithTwoAncestors());
    instance->disposeOwnedObject();
    ClassWithTwoAncestors* descendantPointer = instance.get();
    InstrumentedOwner<ClassWithTwoAncestors*> descendantPointerOwner(descendantPointer);
    Instrumented* ancestorPointer = descendantPointer;
    InstrumentedOwner<Instrumented*> ancestorPointerOwner(ancestorPointer);
    EXPECT_NE(static_cast<void*>(ancestorPointer), static_cast<void*>(descendantPointer));

    CheckCountedObjectsClient client(instance.get());
    InstrumentationTestImpl instrumentation(&client);
    instrumentation.addRootObject(descendantPointerOwner);
    instrumentation.addRootObject(ancestorPointerOwner);
    EXPECT_TRUE(client.expectedPointerFound());
}

class TwoPointersToSameInsrumented {
public:
    TwoPointersToSameInsrumented()
        : m_ownPtr(adoptPtr(new ClassWithTwoAncestors()))
        , m_baseClassPtr(m_ownPtr.get())
    {
        EXPECT_NE(static_cast<void*>(m_ownPtr.get()), static_cast<void*>(m_baseClassPtr));
    }
    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, TestType);
        info.addMember(m_ownPtr);
        info.addMember(m_baseClassPtr);
    }

private:
    OwnPtr<ClassWithTwoAncestors> m_ownPtr;
    Instrumented* m_baseClassPtr;
};

class CountLinksFromInstrumentedObject : public MemoryInstrumentationTestClient {
public:
    CountLinksFromInstrumentedObject() : m_linkCount(0) { }
    virtual void reportEdge(const void*, const char* name, MemberType) OVERRIDE
    {
        if (name && !strcmp("m_notInstrumented", name))
            m_linkCount++;
    }
    int linkCount() const { return m_linkCount; }

private:
    int m_linkCount;
};


TEST(MemoryInstrumentationTest, doNotReportEdgeTwice)
{
    OwnPtr<TwoPointersToSameInsrumented> instance = adoptPtr(new TwoPointersToSameInsrumented());

    CountLinksFromInstrumentedObject client;
    InstrumentationTestImpl instrumentation(&client);
    instrumentation.addRootObject(instance.get());
    EXPECT_EQ(1, client.linkCount());
}

class DerivedClass : public Instrumented {
public:
    size_t m_member;
};

TEST(MemoryInstrumentationTest, detectBaseClassInstrumentation)
{
    OwnPtr<DerivedClass> instance = adoptPtr(new DerivedClass());

    InstrumentationTestHelper helper;
    helper.addRootObject(instance.get(), TestType);
    EXPECT_EQ(sizeof(Instrumented) + sizeof(NotInstrumented), helper.reportedSizeForAllTypes());
    EXPECT_EQ(2u, helper.visitedObjects());
}

} // namespace

