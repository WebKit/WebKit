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

#include "HeapGraphSerializer.h"

#include "WebCoreMemoryInstrumentation.h"
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/MemoryObjectInfo.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

HeapGraphSerializer::HeapGraphSerializer(Client* client)
    : m_client(client)
    , m_strings(Strings::create())
    , m_edges(Edges::create())
    , m_nodeEdgesCount(0)
    , m_nodes(Nodes::create())
    , m_baseToRealNodeIdMap(BaseToRealNodeIdMap::create())
    , m_typeStrings(InspectorObject::create())
    , m_leafCount(0)
{
    ASSERT(m_client);
    m_strings->addItem(String()); // An empty string with 0 index.

    memset(m_edgeTypes, 0, sizeof(m_edgeTypes));

    m_edgeTypes[WTF::PointerMember] = registerTypeString("weak");
    m_edgeTypes[WTF::RetainingPointer] = registerTypeString("property");

    // FIXME: It is used as a magic constant for 'object' node type.
    registerTypeString("object");

    m_unknownClassNameId = registerString("unknown");
}

HeapGraphSerializer::~HeapGraphSerializer()
{
}

void HeapGraphSerializer::pushUpdateIfNeeded()
{
    static const size_t chunkSize = 10000;
    static const size_t averageEdgesPerNode = 5;

    if (m_strings->length() <= chunkSize
        && m_nodes->length() <= chunkSize * s_nodeFieldsCount
        && m_edges->length() <= chunkSize * averageEdgesPerNode * s_edgeFieldsCount
        && m_baseToRealNodeIdMap->length() <= chunkSize * s_idMapEntryFieldCount)
        return;

    pushUpdate();
}

void HeapGraphSerializer::pushUpdate()
{
    typedef TypeBuilder::Memory::HeapSnapshotChunk HeapSnapshotChunk;

    RefPtr<HeapSnapshotChunk> chunk = HeapSnapshotChunk::create()
        .setStrings(m_strings.release())
        .setNodes(m_nodes.release())
        .setEdges(m_edges.release())
        .setBaseToRealNodeId(m_baseToRealNodeIdMap.release());

    m_client->addNativeSnapshotChunk(chunk.release());

    m_strings = Strings::create();
    m_edges = Edges::create();
    m_nodes = Nodes::create();
    m_baseToRealNodeIdMap = BaseToRealNodeIdMap::create();
}

void HeapGraphSerializer::reportNode(const WTF::MemoryObjectInfo& info)
{
    ASSERT(info.reportedPointer());
    reportNodeImpl(info, m_nodeEdgesCount);
    m_nodeEdgesCount = 0;
    if (info.isRoot())
        m_roots.append(info.reportedPointer());
    pushUpdateIfNeeded();
}

int HeapGraphSerializer::reportNodeImpl(const WTF::MemoryObjectInfo& info, int edgesCount)
{
    int nodeId = toNodeId(info.reportedPointer());
    int classNameId = info.classNameId();
    m_nodes->addItem(classNameId ? classNameId : m_unknownClassNameId);
    m_nodes->addItem(info.nameId());
    m_nodes->addItem(nodeId);
    m_nodes->addItem(info.objectSize());
    m_nodes->addItem(edgesCount);

    return nodeId;
}

void HeapGraphSerializer::reportEdge(const void* to, const char* name, WTF::MemberType memberType)
{
    ASSERT(to);
    reportEdgeImpl(toNodeId(to), name, m_edgeTypes[memberType]);
    pushUpdateIfNeeded();
}

void HeapGraphSerializer::reportEdgeImpl(const int toNodeId, const char* name, int memberType)
{
    ASSERT(memberType >= 0);
    ASSERT(memberType < WTF::LastMemberTypeEntry);

    m_edges->addItem(memberType);
    m_edges->addItem(registerString(name));
    m_edges->addItem(toNodeId);

    ++m_nodeEdgesCount;
}

void HeapGraphSerializer::reportLeaf(const WTF::MemoryObjectInfo& info, const char* edgeName)
{
    int nodeId = reportNodeImpl(info, 0);
    reportEdgeImpl(nodeId, edgeName, m_edgeTypes[WTF::RetainingPointer]);
    pushUpdateIfNeeded();
}

void HeapGraphSerializer::reportBaseAddress(const void* base, const void* real)
{
    m_baseToRealNodeIdMap->addItem(toNodeId(base));
    m_baseToRealNodeIdMap->addItem(toNodeId(real));
}

PassRefPtr<InspectorObject> HeapGraphSerializer::finish()
{
    addRootNode();
    pushUpdate();
    String metaString =
        "{"
            "\"node_fields\":["
                "\"type\","
                "\"name\","
                "\"id\","
                "\"self_size\","
                "\"edge_count\""
            "],"
            "\"node_types\":["
                "[]," // FIXME: It is a fallback for Heap Snapshot parser. In case of Native Heap Snapshot it is a plain string id.
                "\"string\","
                "\"number\","
                "\"number\","
                "\"number\""
            "],"
            "\"edge_fields\":["
                "\"type\","
                "\"name_or_index\","
                "\"to_node\""
            "],"
            "\"edge_types\":["
                "[],"
                "\"string_or_number\","
                "\"node\""
            "]"
        "}";

    RefPtr<InspectorValue> metaValue = InspectorValue::parseJSON(metaString);
    RefPtr<InspectorObject> meta;
    metaValue->asObject(&meta);
    ASSERT(meta);
    meta->setObject("type_strings", m_typeStrings);
    return meta.release();
}

void HeapGraphSerializer::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Inspector);
    info.ignoreMember(m_stringToIndex);
    info.ignoreMember(m_strings);
    info.ignoreMember(m_edges);
    info.ignoreMember(m_nodes);
    info.ignoreMember(m_baseToRealNodeIdMap);
    info.ignoreMember(m_roots);
}

int HeapGraphSerializer::registerString(const char* string)
{
    if (!string)
        return 0;
    int length = strlen(string);
    if (length > 256)
        length = 256;
    StringMap::AddResult result = m_stringToIndex.add(String(string, length), m_stringToIndex.size() + 1);
    if (result.isNewEntry)
        m_strings->addItem(string);
    return result.iterator->value;
}

int HeapGraphSerializer::registerTypeString(const char* string)
{
    int stringId = registerString(string);
    m_typeStrings->setNumber(string, stringId);
    return stringId;
}

int HeapGraphSerializer::toNodeId(const void* to)
{
    if (!to)
        return s_firstNodeId + m_address2NodeIdMap.size() + m_leafCount++;

    Address2NodeId::AddResult result = m_address2NodeIdMap.add(to, s_firstNodeId + m_leafCount + m_address2NodeIdMap.size());
    return result.iterator->value;
}

void HeapGraphSerializer::addRootNode()
{
    for (size_t i = 0; i < m_roots.size(); i++)
        reportEdgeImpl(toNodeId(m_roots[i]), 0, m_edgeTypes[WTF::PointerMember]);

    m_nodes->addItem(registerString("Root"));
    m_nodes->addItem(0);
    m_nodes->addItem(s_firstNodeId + m_address2NodeIdMap.size() + m_leafCount);
    m_nodes->addItem(0);
    m_nodes->addItem(m_roots.size());
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
