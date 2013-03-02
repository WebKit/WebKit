/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "WTFStringUtilities.h"
#include <gtest/gtest.h>

#include "HeapGraphSerializer.h"
#include "MemoryInstrumentationImpl.h"
#include <wtf/MemoryInstrumentation.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/MemoryInstrumentationString.h>
#include <wtf/MemoryObjectInfo.h>

namespace TestWebKitAPI {

using namespace WebCore;

static WTF::MemoryObjectType g_defaultObjectType = "DefaultObjectType";

class HeapGraphReceiver : public HeapGraphSerializer::Client {
public:
    HeapGraphReceiver() : m_serializer(this) { }

    virtual void addNativeSnapshotChunk(PassRefPtr<TypeBuilder::Memory::HeapSnapshotChunk> heapSnapshotChunk) OVERRIDE
    {
        ASSERT(!m_heapSnapshotChunk);
        m_heapSnapshotChunk = heapSnapshotChunk;
        m_strings = chunkPart("strings");
        m_edges = chunkPart("edges");
        m_nodes = chunkPart("nodes");

        // Reset platform depended size field values.
        for (InspectorArray::iterator i = m_nodes->begin(); i != m_nodes->end(); i += s_nodeFieldCount)
            *(i + s_sizeOffset) = InspectorBasicValue::create(0);

        m_id2index.clear();

        for (unsigned index = 0; index < m_nodes->length(); index += s_nodeFieldCount)
            m_id2index.add(intValue(m_nodes.get(), index + s_idOffset), index);
    }

    void printGraph()
    {
        EXPECT_TRUE(m_heapSnapshotChunk);
        int processedEdgesCount = 0;
        for (unsigned index = 0; index < m_nodes->length(); index += s_nodeFieldCount)
            processedEdgesCount += printNode(index, processedEdgesCount);
    }

    String dumpNodes() { return dumpPart("nodes"); }
    String dumpEdges() { return dumpPart("edges"); }
    String dumpBaseToRealNodeId() { return dumpPart("baseToRealNodeId"); }
    String dumpStrings() { return dumpPart("strings"); }

    HeapGraphSerializer* serializer() { return &m_serializer; }

private:
    PassRefPtr<InspectorArray> chunkPart(String partName)
    {
        EXPECT_TRUE(m_heapSnapshotChunk);
        RefPtr<InspectorObject> chunk = *reinterpret_cast<RefPtr<InspectorObject>*>(&m_heapSnapshotChunk);
        RefPtr<InspectorValue> partValue = chunk->get(partName);
        RefPtr<InspectorArray> partArray;
        EXPECT_TRUE(partValue->asArray(&partArray));
        return partArray.release();
    }

    String dumpPart(String partName)
    {
        return chunkPart(partName)->toJSONString().replace("\"", "'");
    }

    String stringValue(InspectorArray* array, int index)
    {
        RefPtr<InspectorValue> inspectorValue = array->get(index);
        String value;
        EXPECT_TRUE(inspectorValue->asString(&value));
        return value;
    }

    int intValue(InspectorArray* array, int index)
    {
        RefPtr<InspectorValue> inspectorValue = array->get(index);
        int value;
        EXPECT_TRUE(inspectorValue->asNumber(&value));
        return value;
    }

    String nodeToString(unsigned nodeIndex)
    {
        StringBuilder builder;
        builder.append("node: ");
        builder.appendNumber(intValue(m_nodes.get(), nodeIndex + s_idOffset));
        builder.append(" with className:'");
        builder.append(stringValue(m_strings.get(), intValue(m_nodes.get(), nodeIndex + s_classNameOffset)));
        builder.append("' and name: '");
        builder.append(stringValue(m_strings.get(), intValue(m_nodes.get(), nodeIndex + s_nameOffset)));
        builder.append("'");
        return builder.toString();
    }

    String edgeToString(unsigned edgeOrdinal)
    {
        unsigned edgeIndex = edgeOrdinal * s_edgeFieldCount;
        StringBuilder builder;
        builder.append("'");
        builder.append(stringValue(m_strings.get(), intValue(m_edges.get(), edgeIndex + s_edgeTypeOffset)));
        builder.append("' edge '");
        builder.append(stringValue(m_strings.get(), intValue(m_edges.get(), edgeIndex + s_edgeNameOffset)));
        builder.append("' points to ");
        int nodeId = intValue(m_edges.get(), edgeIndex + s_toNodeIdOffset);
        builder.append(nodeToString(m_id2index.get(nodeId)));
        return builder.toString();
    }

    int printNode(unsigned nodeIndex, unsigned processedEdgesCount)
    {
        String nodeString = nodeToString(nodeIndex);
        unsigned edgeCount = intValue(m_nodes.get(), nodeIndex + s_edgeCountOffset);

        printf("%s\n", nodeString.utf8().data());
        for (unsigned i = 0; i < edgeCount; ++i) {
            String edgeText = edgeToString(i + processedEdgesCount);
            printf("\thas %s\n", edgeText.utf8().data());
        }
        return edgeCount;
    }

    HeapGraphSerializer m_serializer;
    RefPtr<TypeBuilder::Memory::HeapSnapshotChunk> m_heapSnapshotChunk;

    RefPtr<InspectorArray> m_strings;
    RefPtr<InspectorArray> m_nodes;
    RefPtr<InspectorArray> m_edges;
    HashMap<int, int> m_id2index;

    static const int s_nodeFieldCount = 5;
    static const int s_classNameOffset = 0;
    static const int s_nameOffset = 1;
    static const int s_idOffset = 2;
    static const int s_sizeOffset = 3;
    static const int s_edgeCountOffset = 4;

    static const int s_edgeFieldCount = 3;
    static const int s_edgeTypeOffset = 0;
    static const int s_edgeNameOffset = 1;
    static const int s_toNodeIdOffset = 2;
};

class Helper {
public:
    Helper(HeapGraphSerializer* serializer)
        : m_serializer(serializer)
        , m_memoryInstrumentationClient(serializer)
        , m_memoryInstrumentation(&m_memoryInstrumentationClient)
        , m_currentPointer(0)
    { }

    void* addNode(const char* className, const char* name, bool isRoot)
    {
        WTF::MemoryObjectInfo info(&m_memoryInstrumentation, g_defaultObjectType, ++m_currentPointer);
        info.setClassName(className);
        info.setName(name);
        if (isRoot)
            info.markAsRoot();
        m_serializer->reportNode(info);
        return m_currentPointer;
    }

    void addEdge(void* to, const char* edgeName, WTF::MemberType memberType)
    {
        m_serializer->reportEdge(to, edgeName, memberType);
    }

    void done()
    {
        m_serializer->finish();
    }

private:
    HeapGraphSerializer* m_serializer;
    MemoryInstrumentationClientImpl m_memoryInstrumentationClient;
    MemoryInstrumentationImpl m_memoryInstrumentation;

    class Object {
    public:
        Object() { m_data[0] = 0; }
        char m_data[sizeof(void*)];
    };
    Object* m_currentPointer;
};

TEST(HeapGraphSerializerTest, snapshotWithoutUserObjects)
{
    HeapGraphReceiver receiver;
    Helper helper(receiver.serializer());
    helper.done();
    receiver.printGraph();
    EXPECT_EQ(String("['','weak','property','object','unknown','Root']"), receiver.dumpStrings());
    EXPECT_EQ(String("[5,0,1,0,0]"), receiver.dumpNodes()); // Only Root object.
    EXPECT_EQ(String("[]"), receiver.dumpEdges()); // No edges.
    EXPECT_EQ(String("[]"), receiver.dumpBaseToRealNodeId()); // No id maps.
}

TEST(HeapGraphSerializerTest, oneRootUserObject)
{
    HeapGraphReceiver receiver;
    Helper helper(receiver.serializer());
    helper.addNode("ClassName", "objectName", true);
    helper.done();
    receiver.printGraph();
    EXPECT_EQ(String("['','weak','property','object','unknown','ClassName','objectName','Root']"), receiver.dumpStrings());
    EXPECT_EQ(String("[5,6,1,0,0,7,0,2,0,1]"), receiver.dumpNodes());
    EXPECT_EQ(String("[1,0,1]"), receiver.dumpEdges());
    EXPECT_EQ(String("[]"), receiver.dumpBaseToRealNodeId());
}

TEST(HeapGraphSerializerTest, twoUserObjectsWithEdge)
{
    HeapGraphReceiver receiver;
    Helper helper(receiver.serializer());
    void* childObject = helper.addNode("Child", "child", false);
    helper.addEdge(childObject, "pointerToChild", WTF::RetainingPointer);
    helper.addNode("Parent", "parent", true);
    helper.done();
    receiver.printGraph();
    EXPECT_EQ(String("['','weak','property','object','unknown','Child','child','pointerToChild','Parent','parent','Root']"), receiver.dumpStrings());
    EXPECT_EQ(String("[5,6,1,0,0,8,9,2,0,1,10,0,3,0,1]"), receiver.dumpNodes());
    EXPECT_EQ(String("[2,7,1,1,0,2]"), receiver.dumpEdges());
    EXPECT_EQ(String("[]"), receiver.dumpBaseToRealNodeId());
}

class Owner {
public:
    Owner()
    {
        m_strings.add("first element");
        m_strings.add("second element");
    }
    void reportMemoryUsage(WTF::MemoryObjectInfo* memoryObjectInfo) const
    {
        WTF::MemoryClassInfo info(memoryObjectInfo, this, g_defaultObjectType);
        info.addMember(m_strings, "strings");
    }
private:
    HashSet<String> m_strings;
};

TEST(HeapGraphSerializerTest, hashSetWithTwoStrings)
{
    HeapGraphReceiver receiver;
    MemoryInstrumentationClientImpl memoryInstrumentationClient(receiver.serializer());
    MemoryInstrumentationImpl memoryInstrumentation(&memoryInstrumentationClient);

    Owner owner;
    memoryInstrumentation.addRootObject(&owner);
    receiver.serializer()->finish();
    receiver.printGraph();
    EXPECT_EQ(String("[6,0,1,0,0,5,0,4,0,3,9,0,3,0,0,9,0,2,0,0,10,0,5,0,1]"), receiver.dumpNodes());
    EXPECT_EQ(String("[2,7,1,2,8,2,2,8,3,1,0,4]"), receiver.dumpEdges());
    EXPECT_EQ(String("[]"), receiver.dumpBaseToRealNodeId());
}

} // namespace
