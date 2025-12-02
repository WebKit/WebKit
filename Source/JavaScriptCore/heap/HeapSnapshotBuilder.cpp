/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include "DeferGCInlines.h"
#include "Heap.h"
#include "HeapProfiler.h"
#include "HeapSnapshot.h"
#include "JSCInlines.h"
#include "JSCast.h"
#include "PreventCollectionScope.h"
#include "VM.h"
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HeapSnapshotBuilder);

NodeIdentifier HeapSnapshotBuilder::nextAvailableObjectIdentifier = 1;
NodeIdentifier HeapSnapshotBuilder::getNextObjectIdentifier() { return nextAvailableObjectIdentifier++; }
void HeapSnapshotBuilder::resetNextAvailableObjectIdentifier() { HeapSnapshotBuilder::nextAvailableObjectIdentifier = 1; }

HeapSnapshotBuilder::HeapSnapshotBuilder(HeapProfiler& profiler, SnapshotType type)
    : HeapAnalyzer()
    , m_profiler(profiler)
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

    m_snapshot = makeUnique<HeapSnapshot>(m_profiler.mostRecentSnapshot());
    {
        ASSERT(!m_profiler.activeHeapAnalyzer());
        m_profiler.setActiveHeapAnalyzer(this);
        m_profiler.vm().heap.collectNow(Sync, CollectionScope::Full);
        m_profiler.setActiveHeapAnalyzer(nullptr);
    }

    {
        Locker locker { m_buildingNodeMutex };
        m_appendedCells.clear();
        m_snapshot->finalize();
    }
    m_profiler.appendSnapshot(WTFMove(m_snapshot));
}

void HeapSnapshotBuilder::analyzeNode(JSCell* cell)
{
    ASSERT(m_profiler.activeHeapAnalyzer() == this);

    ASSERT(m_profiler.vm().heap.isMarked(cell));

    NodeIdentifier identifier;
    if (previousSnapshotHasNodeForCell(cell, identifier))
        return;

    Locker locker { m_buildingNodeMutex };
    auto addResult = m_appendedCells.add(cell);
    if (!addResult.isNewEntry)
        return;
    m_snapshot->appendNode(HeapSnapshotNode(cell, getNextObjectIdentifier()));
}

void HeapSnapshotBuilder::analyzeEdge(JSCell* from, JSCell* to, RootMarkReason rootMarkReason)
{
    ASSERT(m_profiler.activeHeapAnalyzer() == this);
    ASSERT(to);

    // Avoid trivial edges.
    if (from == to)
        return;

    Locker locker { m_buildingEdgeMutex };

    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot && !from) {
        if (rootMarkReason == RootMarkReason::None && m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
            if (Options::verboseHeapSnapshotLogging())
                WTFLogAlways("Cell %p is a root but no root marking reason was supplied", to);
        }

        m_rootData.ensure(to, [] () -> RootData {
            return { };
        }).iterator->value.markReason = rootMarkReason;
    }

    m_edges.append(HeapSnapshotEdge(from, to));
}

void HeapSnapshotBuilder::analyzePropertyNameEdge(JSCell* from, JSCell* to, UniquedStringImpl* propertyName)
{
    ASSERT(m_profiler.activeHeapAnalyzer() == this);
    ASSERT(to);

    Locker locker { m_buildingEdgeMutex };

    m_edges.append(HeapSnapshotEdge(from, to, EdgeType::Property, propertyName));
}

void HeapSnapshotBuilder::analyzeVariableNameEdge(JSCell* from, JSCell* to, UniquedStringImpl* variableName)
{
    ASSERT(m_profiler.activeHeapAnalyzer() == this);
    ASSERT(to);

    Locker locker { m_buildingEdgeMutex };

    m_edges.append(HeapSnapshotEdge(from, to, EdgeType::Variable, variableName));
}

void HeapSnapshotBuilder::analyzeIndexEdge(JSCell* from, JSCell* to, uint32_t index)
{
    ASSERT(m_profiler.activeHeapAnalyzer() == this);
    ASSERT(to);

    Locker locker { m_buildingEdgeMutex };

    m_edges.append(HeapSnapshotEdge(from, to, index));
}

void HeapSnapshotBuilder::setOpaqueRootReachabilityReasonForCell(JSCell* cell, ASCIILiteral reason)
{
    if (reason.isEmpty() || m_snapshotType != SnapshotType::GCDebuggingSnapshot)
        return;

    Locker locker { m_buildingEdgeMutex };

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
//      "version": 3,
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
//      "version": 3,
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
//       - 0b0100 - Element subclassification
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
    ElementSubtype = 1 << 2,
};

static ASCIILiteral edgeTypeToString(EdgeType type)
{
    switch (type) {
    case EdgeType::Internal:
        return "Internal"_s;
    case EdgeType::Property:
        return "Property"_s;
    case EdgeType::Index:
        return "Index"_s;
    case EdgeType::Variable:
        return "Variable"_s;
    }
    ASSERT_NOT_REACHED();
    return "Internal"_s;
}

static ASCIILiteral snapshotTypeToString(HeapSnapshotBuilder::SnapshotType type)
{
    switch (type) {
    case HeapSnapshotBuilder::SnapshotType::InspectorSnapshot:
        return "Inspector"_s;
    case HeapSnapshotBuilder::SnapshotType::GCDebuggingSnapshot:
        return "GCDebugging"_s;
    }
    ASSERT_NOT_REACHED();
    return "Inspector"_s;
}

void HeapSnapshotBuilder::setLabelForCell(JSCell* cell, const String& label)
{
    m_cellLabels.set(cell, label);
}

String HeapSnapshotBuilder::descriptionForNode(const HeapSnapshotNode& node)
{
    JSCell* cell = node.cell;

    if (cell->isString())
        return emptyString(); // FIXME: get part of string.

    Structure* structure = cell->structure();

    if (structure->classInfoForCells()->isSubClassOf(Structure::info())) {
        Structure* cellAsStructure = jsCast<Structure*>(cell);
        String className = cellAsStructure->classInfoForCells()->className;
        if (m_client)
            className = m_client->heapSnapshotBuilderOverrideClassName(*this, cell, className);
        return className;
    }

    return emptyString();
}

String HeapSnapshotBuilder::json()
{
    StringPrintStream string;
    dumpToStream(string);
    if (m_hasOverflowed)
        return { };
    return string.tryToString().value_or(String());
}

void HeapSnapshotBuilder::dumpToStream(PrintStream& out)
{
    VM& vm = m_profiler.vm();
    DeferGCForAWhile deferGC(vm);

    // Build a node to identifier map of allowed nodes to use when serializing edges.
    UncheckedKeyHashMap<JSCell*, NodeIdentifier> allowedNodeIdentifiers;

    // Build a list of used class names.
    UncheckedKeyHashMap<String, unsigned> classNameIndexes;
    classNameIndexes.set("<root>"_s, 0);
    unsigned nextClassNameIndex = 1;

    // Build a list of labels (this is just a string table).
    UncheckedKeyHashMap<String, unsigned> labelIndexes;
    labelIndexes.set(emptyString(), 0);
    unsigned nextLabelIndex = 1;

    // Build a list of used edge names.
    UncheckedKeyHashMap<UniquedStringImpl*, unsigned> edgeNameIndexes;
    unsigned nextEdgeNameIndex = 0;

    auto printJSONString = [&](const auto& value) {
        // FIXME: We should have a better way to escape a JSON string.
        StringBuilder json(OverflowPolicy::RecordOverflow);
        json.appendQuotedJSONString(value);
        if (json.hasOverflowed()) [[unlikely]]
            m_hasOverflowed = true;
        else
            out.print(json.toString().utf8());
    };

    auto appendNodeJSON = [&] (const HeapSnapshotNode& node) {
        if (m_client && m_client->heapSnapshotBuilderIgnoreNode(*this, node.cell))
            return;

        unsigned flags = 0;

        allowedNodeIdentifiers.set(node.cell, node.identifier);

        String className = node.cell->classInfo()->className;
        if (node.cell->isObject() && className == JSObject::info()->className) {
            flags |= static_cast<unsigned>(NodeFlags::ObjectSubtype);

            // Skip calculating a class name if this object has a `constructor` own property.
            // These cases are typically F.prototype objects and we want to treat these as
            // "Object" in snapshots and not get the name of the prototype's parent.
            JSObject* object = asObject(node.cell);
            if (JSGlobalObject* globalObject = object->globalObject()) {
                PropertySlot slot(object, PropertySlot::InternalMethodType::VMInquiry, &vm);
                if (!object->getOwnPropertySlot(object, globalObject, vm.propertyNames->constructor, slot))
                    className = JSObject::calculatedClassName(object);
            }
        }

        if (m_client)
            className = m_client->heapSnapshotBuilderOverrideClassName(*this, node.cell, className);

        auto result = classNameIndexes.add(className, nextClassNameIndex);
        if (result.isNewEntry)
            nextClassNameIndex++;
        unsigned classNameIndex = result.iterator->value;

        void* wrappedAddress = nullptr;
        unsigned labelIndex = 0;
        if (!node.cell->isString() && !node.cell->isHeapBigInt()) {
            Structure* structure = node.cell->structure();
            if (!structure || !structure->globalObject())
                flags |= static_cast<unsigned>(NodeFlags::Internal);

            if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
                StringBuilder nodeLabel(OverflowPolicy::RecordOverflow);
                auto it = m_cellLabels.find(node.cell);
                if (it != m_cellLabels.end())
                    nodeLabel.append(it->value);

                if (nodeLabel.isEmpty()) {
                    if (auto* object = jsDynamicCast<JSObject*>(node.cell)) {
                        if (auto* function = jsDynamicCast<JSFunction*>(object))
                            nodeLabel.append(function->calculatedDisplayName(vm));
                    }
                }

                String description = descriptionForNode(node);
                if (description.length()) {
                    if (nodeLabel.length())
                        nodeLabel.append(' ');
                    nodeLabel.append(description);
                }

                if (nodeLabel.hasOverflowed()) [[unlikely]] {
                    out.print("Overflowed, aborting.");
                    m_hasOverflowed = true;
                    return;
                }

                if (!nodeLabel.isEmpty() && m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
                    auto result = labelIndexes.add(nodeLabel.toString(), nextLabelIndex);
                    if (result.isNewEntry)
                        nextLabelIndex++;
                    labelIndex = result.iterator->value;
                }

                wrappedAddress = m_wrappedObjectPointers.get(node.cell);
            }
        }

        if (m_client && m_client->heapSnapshotBuilderIsElement(*this, node.cell))
            flags |= static_cast<unsigned>(NodeFlags::ElementSubtype);

        // <nodeId>, <sizeInBytes>, <nodeClassNameIndex>, <flags>, [<labelIndex>, <cellAddress>, <wrappedAddress>]
        out.print(',', node.identifier, ',', node.cell->estimatedSizeInBytes(vm), ',', classNameIndex, ',', flags);
        if (m_snapshotType == SnapshotType::GCDebuggingSnapshot)
            out.print(',', labelIndex, ",\"0x"_s, hex(reinterpret_cast<uintptr_t>(node.cell), Lowercase), "\",\"0x"_s, hex(reinterpret_cast<uintptr_t>(wrappedAddress), Lowercase), '"');
    };

    bool firstEdge = true;
    auto appendEdgeJSON = [&] (const HeapSnapshotEdge& edge) {
        if (!firstEdge)
            out.print(',');
        firstEdge = false;

        // <fromNodeId>, <toNodeId>, <edgeTypeIndex>, <edgeExtraData>
        out.print(edge.from.identifier, ',', edge.to.identifier, ',', static_cast<std::underlying_type_t<EdgeType>>(edge.type), ',');
        switch (edge.type) {
        case EdgeType::Property:
        case EdgeType::Variable: {
            auto result = edgeNameIndexes.add(edge.u.name, nextEdgeNameIndex);
            if (result.isNewEntry)
                nextEdgeNameIndex++;
            unsigned edgeNameIndex = result.iterator->value;
            out.print(edgeNameIndex);
            break;
        }
        case EdgeType::Index:
            out.print(edge.u.index);
            break;
        default:
            // No data for this edge type.
            out.print('0');
            break;
        }
    };

    // version
    out.print("{\"version\":3"_s);

    // type
    out.print(",\"type\":\""_s, snapshotTypeToString(m_snapshotType), '"');

    // nodes
    out.print(",\"nodes\":["_s);
    // <root>
    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot)
        out.print("0,0,0,0,0,\"0x0\",\"0x0\""_s);
    else
        out.print("0,0,0,0"_s);

    for (HeapSnapshot* snapshot = m_profiler.mostRecentSnapshot(); snapshot; snapshot = snapshot->previous()) {
        for (auto& node : snapshot->m_nodes)
            appendNodeJSON(node);
    }
    out.print(']');

    // node class names
    out.print(",\"nodeClassNames\":["_s);
    Vector<String> orderedClassNames(classNameIndexes.size());
    for (auto& entry : classNameIndexes)
        orderedClassNames[entry.value] = entry.key;
    classNameIndexes.clear();
    bool firstClassName = true;
    for (auto& className : orderedClassNames) {
        if (!firstClassName)
            out.print(',');
        firstClassName = false;
        printJSONString(className);
    }
    orderedClassNames.clear();
    out.print(']');

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
                if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
                    if (Options::verboseHeapSnapshotLogging())
                        WTFLogAlways("Failed to find node for from-edge cell %p", edge.from.cell);
                }
                return true;
            }
            edge.from.identifier = fromLookup->value;
        }

        if (!edge.to.cell)
            edge.to.identifier = 0;
        else {
            auto toLookup = allowedNodeIdentifiers.find(edge.to.cell);
            if (toLookup == allowedNodeIdentifiers.end()) {
                if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
                    if (Options::verboseHeapSnapshotLogging())
                        WTFLogAlways("Failed to find node for to-edge cell %p", edge.to.cell);
                }
                return true;
            }
            edge.to.identifier = toLookup->value;
        }

        return false;
    });

    allowedNodeIdentifiers.clear();
    m_edges.shrinkToFit();

    // Sort edges based on from identifier.
    std::ranges::sort(m_edges, [&](const auto& a, const auto& b) {
        return a.from.identifier < b.from.identifier;
    });

    // edges
    out.print(",\"edges\":["_s);
    for (auto& edge : m_edges)
        appendEdgeJSON(edge);
    out.print(']');

    // edge types
    out.print(",\"edgeTypes\":[\""_s, edgeTypeToString(EdgeType::Internal), "\",\""_s, edgeTypeToString(EdgeType::Property), "\",\""_s, edgeTypeToString(EdgeType::Index), "\",\""_s, edgeTypeToString(EdgeType::Variable), "\"]"_s);

    // edge names
    out.print(",\"edgeNames\":["_s);
    Vector<UniquedStringImpl*> orderedEdgeNames(edgeNameIndexes.size());
    for (auto& entry : edgeNameIndexes)
        orderedEdgeNames[entry.value] = entry.key;
    edgeNameIndexes.clear();
    bool firstEdgeName = true;
    for (auto& edgeName : orderedEdgeNames) {
        if (!firstEdgeName)
            out.print(',');
        firstEdgeName = false;
        printJSONString(edgeName);
    }
    orderedEdgeNames.clear();
    out.print(']');

    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
        out.print(",\"roots\":["_s);

        HeapSnapshot* snapshot = m_profiler.mostRecentSnapshot();

        bool firstNode = true;
        for (auto it : m_rootData) {
            auto snapshotNode = snapshot->nodeForCell(it.key);
            if (!snapshotNode) {
                if (Options::verboseHeapSnapshotLogging())
                    WTFLogAlways("Failed to find snapshot node for cell %p", it.key);
                continue;
            }

            if (!firstNode)
                out.print(',');

            firstNode = false;
            out.print(snapshotNode.value().identifier);

            // Maybe we should just always encode the root names.
            auto rootName = rootMarkReasonDescription(it.value.markReason);
            auto result = labelIndexes.add(rootName, nextLabelIndex);
            if (result.isNewEntry)
                nextLabelIndex++;
            out.print(',', result.iterator->value);

            unsigned reachabilityReasonIndex = 0;
            if (!it.value.reachabilityFromOpaqueRootReasons.isNull()) {
                auto result = labelIndexes.add(it.value.reachabilityFromOpaqueRootReasons, nextLabelIndex);
                if (result.isNewEntry)
                    nextLabelIndex++;
                reachabilityReasonIndex = result.iterator->value;
            }
            out.print(',', reachabilityReasonIndex);
        }

        out.print(']');
    }

    if (m_snapshotType == SnapshotType::GCDebuggingSnapshot) {
        // internal node descriptions
        out.print(",\"labels\":["_s);

        Vector<String> orderedLabels(labelIndexes.size());
        for (auto& entry : labelIndexes)
            orderedLabels[entry.value] = entry.key;
        labelIndexes.clear();
        bool firstLabel = true;
        for (auto& label : orderedLabels) {
            if (!firstLabel)
                out.print(',');

            firstLabel = false;
            printJSONString(label);
        }
        orderedLabels.clear();

        out.print(']');
    }

    out.print('}');
}

} // namespace JSC
