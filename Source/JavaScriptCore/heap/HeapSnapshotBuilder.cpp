/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HeapSnapshotBuilder.h"

#include "DeferGC.h"
#include "Heap.h"
#include "HeapProfiler.h"
#include "HeapSnapshot.h"
#include "JSCInlines.h"
#include "JSCast.h"
#include "PreventCollectionScope.h"
#include "VM.h"
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

static const char* rootTypeToString(SlotVisitor::RootMarkReason);

NodeIdentifier HeapSnapshotBuilder::nextAvailableObjectIdentifier = 1;
NodeIdentifier HeapSnapshotBuilder::getNextObjectIdentifier() { return nextAvailableObjectIdentifier++; }
void HeapSnapshotBuilder::resetNextAvailableObjectIdentifier() { HeapSnapshotBuilder::nextAvailableObjectIdentifier = 1; }

HeapSnapshotBuilder::HeapSnapshotBuilder(HeapProfiler& profiler, SnapshotType type)
    : m_profiler(profiler)
    , m_snapshotType(type)
{
}

HeapSnapshotBuilder::~HeapSnapshotBuilder()
{
    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot)
        m_profiler.clearSnapshots();
}

void HeapSnapshotBuilder::buildSnapshot()
{
    // GCDebuggingSnapshot are always full snapshots, so clear any existing snapshots.
    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot)
        m_profiler.clearSnapshots();

    PreventCollectionScope preventCollectionScope(m_profiler.vm().heap);

    m_snapshot = std::make_unique<HeapSnapshot>(m_profiler.mostRecentSnapshot());
    {
        m_profiler.setActiveSnapshotBuilder(this);
        m_profiler.vm().heap.collectNow(Sync, CollectionScope::Full);
        m_profiler.setActiveSnapshotBuilder(nullptr);
    }
    m_snapshot->finalize();

    m_profiler.appendSnapshot(WTFMove(m_snapshot));
}

void HeapSnapshotBuilder::appendNode(JSCell* cell)
{
    ASSERT(m_profiler.activeSnapshotBuilder() == this);

    ASSERT(Heap::isMarked(cell));

    NodeIdentifier identifier;
    if (previousSnapshotHasNodeForCell(cell, identifier))
        return;

    std::lock_guard<Lock> lock(m_buildingNodeMutex);
    m_snapshot->appendNode(HeapSnapshotNode(cell, getNextObjectIdentifier()));
}

void HeapSnapshotBuilder::appendEdge(JSCell* from, JSCell* to, SlotVisitor::RootMarkReason rootMarkReason)
{
    ASSERT(m_profiler.activeSnapshotBuilder() == this);
    ASSERT(to);

    // Avoid trivial edges.
    if (from == to)
        return;

    std::lock_guard<Lock> lock(m_buildingEdgeMutex);

    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot && !from) {
        if (rootMarkReason == SlotVisitor::RootMarkReason::None && m_snapshotType == SnapshotType::GCDebuggingSnapshot)
            WTFLogAlways("Cell %p is a root but no root marking reason was supplied", to);

        m_rootData.ensure(to, [] () -> RootData {
            return { };
        }).iterator->value.markReason = rootMarkReason;
    }

    m_edges.append(HeapSnapshotEdge(from, to));
}

void HeapSnapshotBuilder::appendPropertyNameEdge(JSCell* from, JSCell* to, UniquedStringImpl* propertyName)
{
    ASSERT(m_profiler.activeSnapshotBuilder() == this);
    ASSERT(to);

    std::lock_guard<Lock> lock(m_buildingEdgeMutex);

    m_edges.append(HeapSnapshotEdge(from, to, EdgeType::Property, propertyName));
}

void HeapSnapshotBuilder::appendVariableNameEdge(JSCell* from, JSCell* to, UniquedStringImpl* variableName)
{
    ASSERT(m_profiler.activeSnapshotBuilder() == this);
    ASSERT(to);

    std::lock_guard<Lock> lock(m_buildingEdgeMutex);

    m_edges.append(HeapSnapshotEdge(from, to, EdgeType::Variable, variableName));
}

void HeapSnapshotBuilder::appendIndexEdge(JSCell* from, JSCell* to, uint32_t index)
{
    ASSERT(m_profiler.activeSnapshotBuilder() == this);
    ASSERT(to);

    std::lock_guard<Lock> lock(m_buildingEdgeMutex);

    m_edges.append(HeapSnapshotEdge(from, to, index));
}

void HeapSnapshotBuilder::setOpaqueRootReachabilityReasonForCell(JSCell* cell, const char* reason)
{
    if (!reason || !*reason || m_snapshotType != SnapshotType::GCDebuggingSnapshot)
        return;

    m_rootData.ensure(cell, [] () -> RootData {
        return { };
    }).iterator->value.reachabilityFromOpaqueRootReasons = reason;
}

void HeapSnapshotBuilder::setWrappedObjectForCell(JSCell* cell, void* wrappedPtr)
{
    m_wrappedObjectPointers.set(cell, wrappedPtr);
}

bool HeapSnapshotBuilder::previousSnapshotHasNodeForCell(JSCell* cell, NodeIdentifier& identifier)
{
    if (!m_snapshot->previous())
        return false;

    auto existingNode = m_snapshot->previous()->nodeForCell(cell);
    if (existingNode) {
        identifier = existingNode.value().identifier;
        return true;
    }

    return false;
}

// Heap Snapshot JSON Format:
//
//  Inspector snapshots:
//
//   {
//      "version": 2,
//      "type": "Inspector",
//      // [<address>, <labelIndex>, <wrappedAddress>] only present in GCDebuggingSnapshot-type snapshots
//      "nodes": [
//          <nodeId>, <sizeInBytes>, <nodeClassNameIndex>, <flags>
//          <nodeId>, <sizeInBytes>, <nodeClassNameIndex>, <flags>
//          ...
//      ],
//      "nodeClassNames": [
//          "string", "Structure", "Object", ...
//      ],
//      "edges": [
//          <fromNodeId>, <toNodeId>, <edgeTypeIndex>, <edgeExtraData>,
//          <fromNodeId>, <toNodeId>, <edgeTypeIndex>, <edgeExtraData>,
//          ...
//      ],
//      "edgeTypes": [
//          "Internal", "Property", "Index", "Variable"
//      ],
//      "edgeNames": [
//          "propertyName", "variableName", ...
//      ]
//   }
//
//  GC heap debugger snapshots:
//
//   {
//      "version": 2,
//      "type": "GCDebugging",
//      "nodes": [
//          <nodeId>, <sizeInBytes>, <nodeClassNameIndex>, <flags>, <labelIndex>, <cellEddress>, <wrappedAddress>,
//          <nodeId>, <sizeInBytes>, <nodeClassNameIndex>, <flags>, <labelIndex>, <cellEddress>, <wrappedAddress>,
//          ...
//      ],
//      "nodeClassNames": [
//          "string", "Structure", "Object", ...
//      ],
//      "edges": [
//          <fromNodeId>, <toNodeId>, <edgeTypeIndex>, <edgeExtraData>,
//          <fromNodeId>, <toNodeId>, <edgeTypeIndex>, <edgeExtraData>,
//          ...
//      ],
//      "edgeTypes": [
//          "Internal", "Property", "Index", "Variable"
//      ],
//      "edgeNames": [
//          "propertyName", "variableName", ...
//      ],
//      "roots" : [
//          <nodeId>, <rootReasonIndex>, <reachabilityReasonIndex>,
//          <nodeId>, <rootReasonIndex>, <reachabilityReasonIndex>,
//          ... // <nodeId> may be repeated
//      ],
//      "labels" : [
//          "foo", "bar", ...
//      ]
//   }
//
// Notes:
//
//     <nodeClassNameIndex>
//       - index into the "nodeClassNames" list.
//
//     <flags>
//       - 0b0000 - no flags
//       - 0b0001 - internal instance
//       - 0b0010 - Object subclassification
//
//     <edgeTypeIndex>
//       - index into the "edgeTypes" list.
//
//     <edgeExtraData>
//       - for Internal edges this should be ignored (0).
//       - for Index edges this is the index value.
//       - for Property or Variable edges this is an index into the "edgeNames" list.
//
//      <rootReasonIndex>
//       - index into the "labels" list.

enum class NodeFlags {
    Internal      = 1 << 0,
    ObjectSubtype = 1 << 1,
};

static uint8_t edgeTypeToNumber(EdgeType type)
{
    return static_cast<uint8_t>(type);
}

static const char* edgeTypeToString(EdgeType type)
{
    switch (type) {
    case EdgeType::Internal:
        return "Internal";
    case EdgeType::Property:
        return "Property";
    case EdgeType::Index:
        return "Index";
    case EdgeType::Variable:
        return "Variable";
    }
    ASSERT_NOT_REACHED();
    return "Internal";
}

static const char* snapshotTypeToString(HeapSnapshotBuilder::SnapshotType type)
{
    switch (type) {
    case HeapSnapshotBuilder::SnapshotType::InspectorSnapshot:
        return "Inspector";
    case HeapSnapshotBuilder::SnapshotType::GCDebuggingSnapshot:
        return "GCDebugging";
    }
    ASSERT_NOT_REACHED();
    return "Inspector";
}

static const char* rootTypeToString(SlotVisitor::RootMarkReason type)
{
    switch (type) {
    case SlotVisitor::RootMarkReason::None:
        return "None";
    case SlotVisitor::RootMarkReason::ConservativeScan:
        return "Conservative scan";
    case SlotVisitor::RootMarkReason::StrongReferences:
        return "Strong references";
    case SlotVisitor::RootMarkReason::ProtectedValues:
        return "Protected values";
    case SlotVisitor::RootMarkReason::MarkListSet:
        return "Mark list set";
    case SlotVisitor::RootMarkReason::VMExceptions:
        return "VM exceptions";
    case SlotVisitor::RootMarkReason::StrongHandles:
        return "Strong handles";
    case SlotVisitor::RootMarkReason::Debugger:
        return "Debugger";
    case SlotVisitor::RootMarkReason::JITStubRoutines:
        return "JIT stub routines";
    case SlotVisitor::RootMarkReason::WeakSets:
        return "Weak sets";
    case SlotVisitor::RootMarkReason::Output:
        return "Output";
    case SlotVisitor::RootMarkReason::DFGWorkLists:
        return "DFG work lists";
    case SlotVisitor::RootMarkReason::CodeBlocks:
        return "Code blocks";
    case SlotVisitor::RootMarkReason::DOMGCOutput:
        return "DOM GC output";
    }
    ASSERT_NOT_REACHED();
    return "None";
}

String HeapSnapshotBuilder::json()
{
    return json([] (const HeapSnapshotNode&) { return true; });
}

void HeapSnapshotBuilder::setLabelForCell(JSCell* cell, const String& label)
{
    m_cellLabels.set(cell, label);
}

String HeapSnapshotBuilder::descriptionForCell(JSCell *cell) const
{
    if (cell->isString())
        return emptyString(); // FIXME: get part of string.

    VM& vm = m_profiler.vm();
    Structure* structure = cell->structure(vm);

    if (structure->classInfo()->isSubClassOf(Structure::info())) {
        Structure* cellAsStructure = jsCast<Structure*>(cell);
        return cellAsStructure->classInfo()->className;
    }

    return emptyString();
}

String HeapSnapshotBuilder::json(Function<bool (const HeapSnapshotNode&)> allowNodeCallback)
{
    VM& vm = m_profiler.vm();
    DeferGCForAWhile deferGC(vm.heap);

    // Build a node to identifier map of allowed nodes to use when serializing edges.
    HashMap<JSCell*, NodeIdentifier> allowedNodeIdentifiers;

    // Build a list of used class names.
    HashMap<String, unsigned> classNameIndexes;
    classNameIndexes.set("<root>"_s, 0);
    unsigned nextClassNameIndex = 1;

    // Build a list of labels (this is just a string table).
    HashMap<String, unsigned> labelIndexes;
    labelIndexes.set(emptyString(), 0);
    unsigned nextLabelIndex = 1;

    // Build a list of used edge names.
    HashMap<UniquedStringImpl*, unsigned> edgeNameIndexes;
    unsigned nextEdgeNameIndex = 0;

    StringBuilder json;

    auto appendNodeJSON = [&] (const HeapSnapshotNode& node) {
        // Let the client decide if they want to allow or disallow certain nodes.
        if (!allowNodeCallback(node))
            return;

        unsigned flags = 0;

        allowedNodeIdentifiers.set(node.cell, node.identifier);

        String className = node.cell->classInfo(vm)->className;
        if (node.cell->isObject() && className == JSObject::info()->className) {
            flags |= NodeFlags::ObjectSubtype;

            // Skip calculating a class name if this object has a `constructor` own property.
            // These cases are typically F.prototype objects and we want to treat these as
            // "Object" in snapshots and not get the name of the prototype's parent.
            JSObject* object = asObject(node.cell);
            if (JSGlobalObject* globalObject = object->globalObject(vm)) {
                ExecState* exec = globalObject->globalExec();
                PropertySlot slot(object, PropertySlot::InternalMethodType::VMInquiry);
                if (!object->getOwnPropertySlot(object, exec, vm.propertyNames->constructor, slot))
                    className = JSObject::calculatedClassName(object);
            }
        }

        auto result = classNameIndexes.add(className, nextClassNameIndex);
        if (result.isNewEntry)
            nextClassNameIndex++;
        unsigned classNameIndex = result.iterator->value;

        void* wrappedAddress = 0;
        unsigned labelIndex = 0;
        if (!node.cell->isString()) {
            Structure* structure = node.cell->structure(vm);
            if (!structure || !structure->globalObject())
                flags |= NodeFlags::Internal;

            if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
                String nodeLabel;
                auto it = m_cellLabels.find(node.cell);
                if (it != m_cellLabels.end())
                    nodeLabel = it->value;

                if (nodeLabel.isEmpty()) {
                    if (auto* object = jsDynamicCast<JSObject*>(vm, node.cell)) {
                        if (auto* function = jsDynamicCast<JSFunction*>(vm, object))
                            nodeLabel = function->calculatedDisplayName(vm);
                    }
                }

                String description = descriptionForCell(node.cell);
                if (description.length()) {
                    if (nodeLabel.length())
                        nodeLabel.append(' ');
                    nodeLabel.append(description);
                }

                if (!nodeLabel.isEmpty() && m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
                    auto result = labelIndexes.add(nodeLabel, nextLabelIndex);
                    if (result.isNewEntry)
                        nextLabelIndex++;
                    labelIndex = result.iterator->value;
                }

                wrappedAddress = m_wrappedObjectPointers.get(node.cell);
            }
        }

        // <nodeId>, <sizeInBytes>, <nodeClassNameIndex>, <flags>, [<labelIndex>, <cellEddress>, <wrappedAddress>]
        json.append(',');
        json.appendNumber(node.identifier);
        json.append(',');
        json.appendNumber(node.cell->estimatedSizeInBytes(vm));
        json.append(',');
        json.appendNumber(classNameIndex);
        json.append(',');
        json.appendNumber(flags);
        if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
            json.append(',');
            json.appendNumber(labelIndex);
            json.appendLiteral(",\"0x");
            appendUnsignedAsHex(reinterpret_cast<uintptr_t>(node.cell), json, Lowercase);
            json.appendLiteral("\",\"0x");
            appendUnsignedAsHex(reinterpret_cast<uintptr_t>(wrappedAddress), json, Lowercase);
            json.append('"');
        }
    };

    bool firstEdge = true;
    auto appendEdgeJSON = [&] (const HeapSnapshotEdge& edge) {
        if (!firstEdge)
            json.append(',');
        firstEdge = false;

        // <fromNodeId>, <toNodeId>, <edgeTypeIndex>, <edgeExtraData>
        json.appendNumber(edge.from.identifier);
        json.append(',');
        json.appendNumber(edge.to.identifier);
        json.append(',');
        json.appendNumber(edgeTypeToNumber(edge.type));
        json.append(',');
        switch (edge.type) {
        case EdgeType::Property:
        case EdgeType::Variable: {
            auto result = edgeNameIndexes.add(edge.u.name, nextEdgeNameIndex);
            if (result.isNewEntry)
                nextEdgeNameIndex++;
            unsigned edgeNameIndex = result.iterator->value;
            json.appendNumber(edgeNameIndex);
            break;
        }
        case EdgeType::Index:
            json.appendNumber(edge.u.index);
            break;
        default:
            // No data for this edge type.
            json.append('0');
            break;
        }
    };

    json.append('{');

    // version
    json.appendLiteral("\"version\":2");

    // type
    json.append(',');
    json.appendLiteral("\"type\":");
    json.appendQuotedJSONString(snapshotTypeToString(m_snapshotType));

    // nodes
    json.append(',');
    json.appendLiteral("\"nodes\":");
    json.append('[');
    // <root>
    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot)
        json.appendLiteral("0,0,0,0,0,\"0x0\",\"0x0\"");
    else
        json.appendLiteral("0,0,0,0");

    for (HeapSnapshot* snapshot = m_profiler.mostRecentSnapshot(); snapshot; snapshot = snapshot->previous()) {
        for (auto& node : snapshot->m_nodes)
            appendNodeJSON(node);
    }
    json.append(']');

    // node class names
    json.append(',');
    json.appendLiteral("\"nodeClassNames\":");
    json.append('[');
    Vector<String> orderedClassNames(classNameIndexes.size());
    for (auto& entry : classNameIndexes)
        orderedClassNames[entry.value] = entry.key;
    classNameIndexes.clear();
    bool firstClassName = true;
    for (auto& className : orderedClassNames) {
        if (!firstClassName)
            json.append(',');
        firstClassName = false;
        json.appendQuotedJSONString(className);
    }
    orderedClassNames.clear();
    json.append(']');

    // Process edges.
    // Replace pointers with identifiers.
    // Remove any edges that we won't need.
    m_edges.removeAllMatching([&] (HeapSnapshotEdge& edge) {
        // If the from cell is null, this means a <root> edge.
        if (!edge.from.cell)
            edge.from.identifier = 0;
        else {
            auto fromLookup = allowedNodeIdentifiers.find(edge.from.cell);
            if (fromLookup == allowedNodeIdentifiers.end()) {
                if (m_snapshotType == SnapshotType::GCDebuggingSnapshot)
                    WTFLogAlways("Failed to find node for from-edge cell %p", edge.from.cell);
                return true;
            }
            edge.from.identifier = fromLookup->value;
        }

        if (!edge.to.cell)
            edge.to.identifier = 0;
        else {
            auto toLookup = allowedNodeIdentifiers.find(edge.to.cell);
            if (toLookup == allowedNodeIdentifiers.end()) {
                if (m_snapshotType == SnapshotType::GCDebuggingSnapshot)
                    WTFLogAlways("Failed to find node for to-edge cell %p", edge.to.cell);
                return true;
            }
            edge.to.identifier = toLookup->value;
        }

        return false;
    });

    allowedNodeIdentifiers.clear();
    m_edges.shrinkToFit();

    // Sort edges based on from identifier.
    std::sort(m_edges.begin(), m_edges.end(), [&] (const HeapSnapshotEdge& a, const HeapSnapshotEdge& b) {
        return a.from.identifier < b.from.identifier;
    });

    // edges
    json.append(',');
    json.appendLiteral("\"edges\":");
    json.append('[');
    for (auto& edge : m_edges)
        appendEdgeJSON(edge);
    json.append(']');

    // edge types
    json.append(',');
    json.appendLiteral("\"edgeTypes\":");
    json.append('[');
    json.appendQuotedJSONString(edgeTypeToString(EdgeType::Internal));
    json.append(',');
    json.appendQuotedJSONString(edgeTypeToString(EdgeType::Property));
    json.append(',');
    json.appendQuotedJSONString(edgeTypeToString(EdgeType::Index));
    json.append(',');
    json.appendQuotedJSONString(edgeTypeToString(EdgeType::Variable));
    json.append(']');

    // edge names
    json.append(',');
    json.appendLiteral("\"edgeNames\":");
    json.append('[');
    Vector<UniquedStringImpl*> orderedEdgeNames(edgeNameIndexes.size());
    for (auto& entry : edgeNameIndexes)
        orderedEdgeNames[entry.value] = entry.key;
    edgeNameIndexes.clear();
    bool firstEdgeName = true;
    for (auto& edgeName : orderedEdgeNames) {
        if (!firstEdgeName)
            json.append(',');
        firstEdgeName = false;
        json.appendQuotedJSONString(edgeName);
    }
    orderedEdgeNames.clear();
    json.append(']');

    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
        json.append(',');
        json.appendLiteral("\"roots\":");
        json.append('[');

        HeapSnapshot* snapshot = m_profiler.mostRecentSnapshot();

        bool firstNode = true;
        for (auto it : m_rootData) {
            auto snapshotNode = snapshot->nodeForCell(it.key);
            if (!snapshotNode) {
                WTFLogAlways("Failed to find snapshot node for cell %p", it.key);
                continue;
            }

            if (!firstNode)
                json.append(',');

            firstNode = false;
            json.appendNumber(snapshotNode.value().identifier);

            // Maybe we should just always encode the root names.
            const char* rootName = rootTypeToString(it.value.markReason);
            auto result = labelIndexes.add(rootName, nextLabelIndex);
            if (result.isNewEntry)
                nextLabelIndex++;
            unsigned labelIndex = result.iterator->value;
            json.append(',');
            json.appendNumber(labelIndex);

            unsigned reachabilityReasonIndex = 0;
            if (it.value.reachabilityFromOpaqueRootReasons) {
                auto result = labelIndexes.add(it.value.reachabilityFromOpaqueRootReasons, nextLabelIndex);
                if (result.isNewEntry)
                    nextLabelIndex++;
                reachabilityReasonIndex = result.iterator->value;
            }
            json.append(',');
            json.appendNumber(reachabilityReasonIndex);
        }

        json.append(']');
    }

    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
        // internal node descriptions
        json.append(',');
        json.appendLiteral("\"labels\":");
        json.append('[');

        Vector<String> orderedLabels(labelIndexes.size());
        for (auto& entry : labelIndexes)
            orderedLabels[entry.value] = entry.key;
        labelIndexes.clear();
        bool firstLabel = true;
        for (auto& label : orderedLabels) {
            if (!firstLabel)
                json.append(',');

            firstLabel = false;
            json.appendQuotedJSONString(label);
        }
        orderedLabels.clear();

        json.append(']');
    }

    json.append('}');
    return json.toString();
}

} // namespace JSC
