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

#include "InspectorValues.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/MemoryObjectInfo.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class HeapGraphNode {
public:
    HeapGraphNode()
        : m_type(0)
        , m_size(0)
        , m_className(0)
        , m_name(0)
        , m_edgeCount(0)
    {
    }

    int m_type;
    unsigned m_size;
    int m_className;
    int m_name;
    int m_edgeCount;
};

class HeapGraphEdge {
public:
    HeapGraphEdge()
        : m_type(0)
        , m_targetIndexIsKnown(false)
        , m_toIndex(0)
        , m_name(0)
    {
    }

    int m_type;
    bool m_targetIndexIsKnown;
    union {
        const void* m_toObject;
        int m_toIndex;
    };
    int m_name;
};

HeapGraphSerializer::HeapGraphSerializer()
    : m_lastReportedEdgeIndex(0)
{
    m_strings.append(String());

    memset(m_edgeTypes, 0, sizeof(m_edgeTypes));

    ASSERT(m_strings.size());
    m_edgeTypes[WTF::PointerMember] = m_strings.size();
    m_strings.append("weakRef");

    m_edgeTypes[WTF::OwnPtrMember] = m_strings.size();
    m_strings.append("ownRef");

    m_edgeTypes[WTF::RefPtrMember] = m_strings.size();
    m_strings.append("countRef");
}

HeapGraphSerializer::~HeapGraphSerializer()
{
}

void HeapGraphSerializer::reportNode(const WTF::MemoryObjectInfo& info)
{
    HeapGraphNode node;
    node.m_type = addString(info.objectType());
    node.m_size = info.objectSize();
    node.m_className = addString(info.className());
    node.m_name = addString(info.name());
    // Node is always reported after its outgoing edges and leaves.
    node.m_edgeCount = m_edges.size() - m_lastReportedEdgeIndex;
    m_lastReportedEdgeIndex = m_edges.size();

    m_objectToNodeIndex.set(info.reportedPointer(), m_nodes.size());
    if (info.isRoot())
        m_roots.append(info.reportedPointer());
    m_nodes.append(node);
}

void HeapGraphSerializer::reportEdge(const void* to, const char* name, WTF::MemberType memberType)
{
    HeapGraphEdge edge;
    ASSERT(to);
    ASSERT(memberType >= 0);
    ASSERT(memberType < WTF::LastMemberTypeEntry);
    edge.m_type = m_edgeTypes[memberType];
    edge.m_toObject = to;
    edge.m_name = addString(name);
    m_edges.append(edge);
}

void HeapGraphSerializer::reportLeaf(const WTF::MemoryObjectInfo& info, const char* edgeName)
{
    HeapGraphNode node;
    node.m_type = addString(info.objectType());
    node.m_size = info.objectSize();
    node.m_className = addString(info.className());
    node.m_name = addString(info.name());

    int nodeIndex = m_nodes.size();
    m_nodes.append(node);

    HeapGraphEdge edge;
    edge.m_type = m_edgeTypes[WTF::OwnPtrMember];
    edge.m_toIndex = nodeIndex;
    edge.m_targetIndexIsKnown = true;
    edge.m_name = addString(edgeName);
    m_edges.append(edge);
}

void HeapGraphSerializer::reportBaseAddress(const void* base, const void* real)
{
    m_baseToRealAddress.set(base, real);
}

PassRefPtr<InspectorObject> HeapGraphSerializer::serialize()
{
    adjutEdgeTargets();
    RefPtr<InspectorArray> nodes = InspectorArray::create();
    for (size_t i = 0; i < m_nodes.size(); i++) {
        HeapGraphNode& node = m_nodes[i];
        nodes->pushInt(node.m_type);
        nodes->pushInt(node.m_size);
        nodes->pushInt(node.m_className);
        nodes->pushInt(node.m_name);
        nodes->pushInt(node.m_edgeCount);
    }
    RefPtr<InspectorArray> edges = InspectorArray::create();
    for (size_t i = 0; i < m_edges.size(); i++) {
        HeapGraphEdge& edge = m_edges[i];
        edges->pushInt(edge.m_type);
        edges->pushInt(edge.m_toIndex);
        edges->pushInt(edge.m_name);
    }
    RefPtr<InspectorArray> strings = InspectorArray::create();
    for (size_t i = 0; i < m_strings.size(); i++)
        strings->pushString(m_strings[i]);

    RefPtr<InspectorObject> graph = InspectorObject::create();
    graph->setArray("nodes", nodes);
    graph->setArray("edges", edges);
    graph->setArray("strings", strings);
    return graph.release();
}

void HeapGraphSerializer::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Inspector);
    info.addMember(m_stringToIndex);
    info.addMember(m_strings);
    info.addMember(m_objectToNodeIndex);
    info.addMember(m_baseToRealAddress);
    info.addMember(m_nodes);
    info.addMember(m_edges);
}

int HeapGraphSerializer::addString(const String& string)
{
    if (string.isEmpty())
        return 0;
    StringMap::AddResult result = m_stringToIndex.add(string, m_strings.size());
    if (result.isNewEntry)
        m_strings.append(string);
    return result.iterator->value;
}

void HeapGraphSerializer::addRootNode()
{
    for (size_t i = 0; i < m_roots.size(); i++)
        reportEdge(m_roots[i], 0, WTF::PointerMember);
    HeapGraphNode node;
    node.m_name = addString("Root");
    node.m_edgeCount = m_edges.size() - m_lastReportedEdgeIndex;
    m_lastReportedEdgeIndex = m_edges.size();
    m_nodes.append(node);
}

void HeapGraphSerializer::adjutEdgeTargets()
{
    for (size_t i = 0; i < m_edges.size(); i++) {
        HeapGraphEdge& edge = m_edges[i];
        if (edge.m_targetIndexIsKnown)
            continue;
        const void* realTarget = m_baseToRealAddress.get(edge.m_toObject);
        if (!realTarget)
            realTarget = edge.m_toObject;
        edge.m_toIndex = m_objectToNodeIndex.get(realTarget);
    }
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
