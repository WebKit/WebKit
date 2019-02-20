/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

// nodes
// [<0:id>, <1:size>, <2:classNameTableIndex>, <3:internal>, <4:labelIndex>, <5:address>, <6:wrapped address>]
let nodeFieldCount = 7;
const nodeIdOffset = 0;
const nodeSizeOffset = 1;
const nodeClassNameOffset = 2;
const nodeInternalOffset = 3;
const nodeLabelOffset = 4;
const nodeAddressOffset = 5;
const nodeWrappedAddressOffset = 6;

// edges
// [<0:fromId>, <1:toId>, <2:typeTableIndex>, <3:edgeDataIndexOrEdgeNameIndex>]
const edgeFieldCount = 4;
const edgeFromIdOffset = 0;
const edgeToIdOffset = 1;
const edgeTypeOffset = 2;
const edgeDataOffset = 3;

// roots
// [<0:id>, <1:labelIndex>]
let rootFieldCount = 3;
const rootIdOffset = 0;
const rootLabelOffset = 1;
const reachabilityReasonOffset = 2;

// Other constants.
const rootNodeIndex = 0;
const rootNodeOrdinal = 0;
const rootNodeIdentifier = 0;

// Terminology:
//   - `nodeIndex` is an index into the `nodes` list.
//   - `nodeOrdinal` is the order of the node in the `nodes` list. (nodeIndex / nodeFieldCount).
//   - `nodeIdentifier` is the node's id value. (nodes[nodeIndex + nodeIdOffset]).
//   - `edgeIndex` is an index into the `edges` list.
//
// Lists:
//   - _nodeOrdinalToFirstOutgoingEdge - `nodeOrdinal` to `edgeIndex` in `edges`.
//     Iterate edges by walking `edges` (edgeFieldCount) and checking if fromIdentifier is current.
//   - _nodeOrdinalToFirstIncomingEdge - `nodeOrdinal` to `incomingEdgeIndex` in `incomingEdges`.
//     Iterate edges by walking `incomingEdges` until `nodeOrdinal+1`'s first incoming edge index.
//   - _nodeOrdinalToDominatorNodeOrdinal - `nodeOrdinal` to `nodeOrdinal` of dominator.
//   - _nodeOrdinalToRetainedSizes - `nodeOrdinal` to retain size value.
//   - _nodeOrdinalIsDead - `nodeOrdinal` is dead or alive.
//
// Temporary Lists:
//   - nodeOrdinalToPostOrderIndex - `nodeOrdinal` to a `postOrderIndex`.
//   - postOrderIndexToNodeOrdinal - `postOrderIndex` to a `nodeOrdinal`.

let nextSnapshotIdentifier = 1;

HeapSnapshot = class HeapSnapshot
{
    constructor(objectId, snapshotDataString, title = null)
    {
        this._identifier = nextSnapshotIdentifier++;
        this._objectId = objectId;
        this._title = title;

        let json = JSON.parse(snapshotDataString);
        snapshotDataString = null;

        let {version, type, nodes, nodeClassNames, edges, edgeTypes, edgeNames, roots, labels} = json;
        console.assert(version === 1, "Expect JavaScriptCore Heap Snapshot version 1");
        console.assert(type === "GCDebugging", "Expect a GCDebugging-type snapshot");

        this._nodes = nodes;
        this._nodeCount = nodes.length / nodeFieldCount;

        this._roots = roots;
        this._rootCount = roots.length / rootFieldCount;

        this._edges = edges;
        this._edgeCount = edges.length / edgeFieldCount;

        this._edgeTypesTable = edgeTypes;
        this._edgeNamesTable = edgeNames;
        this._nodeClassNamesTable = nodeClassNames;
        this._labelsTable = labels;

        this._totalSize = 0;
        this._nodeIdentifierToOrdinal = new Map; // <node identifier> => nodeOrdinal
        this._lastNodeIdentifier = 0;
        for (let nodeIndex = 0; nodeIndex < nodes.length; nodeIndex += nodeFieldCount) {
            let nodeOrdinal = nodeIndex / nodeFieldCount;
            let nodeIdentifier = nodes[nodeIndex + nodeIdOffset];
            this._nodeIdentifierToOrdinal.set(nodeIdentifier, nodeOrdinal);
            this._totalSize += nodes[nodeIndex + nodeSizeOffset];
            if (nodeIdentifier > this._lastNodeIdentifier)
                this._lastNodeIdentifier = nodeIdentifier;
        }

        this._rootIdentifierToReasons = new Map; // <node identifier> => Set of root reasons
        for (let rootIndex = 0; rootIndex < roots.length; rootIndex += rootFieldCount) {
            let rootOrdinal = rootIndex / rootFieldCount;
            let rootIdentifier = roots[rootIndex + rootIdOffset];
            let rootReasonIndex = roots[rootIndex + rootLabelOffset];
            let rootReachabilityIndex = roots[rootIndex + reachabilityReasonOffset];

            let existingReasons = this._rootIdentifierToReasons.get(rootIdentifier);
            if (existingReasons) {
                existingReasons.add(rootReasonIndex);
                existingReasons.add(rootReachabilityIndex);
            } else
                this._rootIdentifierToReasons.set(rootIdentifier, new Set([rootReasonIndex, rootReachabilityIndex]));
        }

        // FIXME: Replace toIdentifier and fromIdentifier in edges with nodeIndex to reduce hash lookups?

        this._nodeOrdinalToFirstOutgoingEdge = new Uint32Array(this._nodeCount); // nodeOrdinal => edgeIndex
        this._buildOutgoingEdges();

        this._nodeOrdinalToFirstIncomingEdge = new Uint32Array(this._nodeCount + 1); // nodeOrdinal => incomingNodes/incomingEdges index
        this._incomingNodes = new Uint32Array(this._edgeCount); // from nodeOrdinals.
        this._incomingEdges = new Uint32Array(this._edgeCount); // edgeIndex.
        this._buildIncomingEdges();

        let {nodeOrdinalToPostOrderIndex, postOrderIndexToNodeOrdinal} = this._buildPostOrderIndexes();

        this._nodeOrdinalToDominatorNodeOrdinal = new Uint32Array(this._nodeCount);
        this._nodeOrdinalIsGCRoot = new Uint8Array(this._nodeCount);
        this._buildDominatorIndexes(nodeOrdinalToPostOrderIndex, postOrderIndexToNodeOrdinal);

        nodeOrdinalToPostOrderIndex = null;

        this._nodeOrdinalToRetainedSizes = new Uint32Array(this._nodeCount);
        this._buildRetainedSizes(postOrderIndexToNodeOrdinal);

        postOrderIndexToNodeOrdinal = null;

        this._nodeOrdinalIsDead = new Uint8Array(this._nodeCount);

        let {liveSize, categories} = HeapSnapshot.updateCategoriesAndMetadata(this);
        this._liveSize = liveSize;
        this._categories = categories;
    }

    get proxyObjectId() { return this._proxyObjectId; }
    get identifier() { return this._identifier; }
    get title() { return this._title; }
    get totalSize() { return this._totalSize; }
    get totalObjectCount() { return this._totalObjectCount; }
    get liveSize() { return this._liveSize; }
    get categories() { return this._categories; }
    get invalid() { return this._proxyObjectId === 0; }

    // Static

    static updateCategoriesAndMetadata(snapshot, allowNodeIdentifierCallback)
    {
        let liveSize = 0;
        let categories = {};

        let nodes = snapshot._nodes;
        let nodeClassNamesTable = snapshot._nodeClassNamesTable;
        let nodeOrdinalToRetainedSizes = snapshot._nodeOrdinalToRetainedSizes;
        let nodeOrdinalIsDead = snapshot._nodeOrdinalIsDead;

        // Skip the <root> node.
        let firstNodeIndex = nodeFieldCount;
        let firstNodeOrdinal = 1;
        for (let nodeIndex = firstNodeIndex, nodeOrdinal = firstNodeOrdinal; nodeIndex < nodes.length; nodeIndex += nodeFieldCount, nodeOrdinal++) {
            if (allowNodeIdentifierCallback && !allowNodeIdentifierCallback(nodes[nodeIndex + nodeIdOffset]))
                continue;

            let classNameTableIndex = nodes[nodeIndex + nodeClassNameOffset];
            let className = nodeClassNamesTable[classNameTableIndex];
            let size = nodes[nodeIndex + nodeSizeOffset];
            let retainedSize = nodeOrdinalToRetainedSizes[nodeOrdinal];
            let internal = nodes[nodeIndex + nodeInternalOffset] ? true : false;
            let dead = nodeOrdinalIsDead[nodeOrdinal] ? true : false;

            let category = categories[className];
            if (!category)
                category = categories[className] = {className, size: 0, retainedSize: 0, count: 0, internalCount: 0, deadCount: 0};

            category.size += size;
            category.retainedSize += retainedSize;
            category.count += 1;
            if (internal)
                category.internalCount += 1;
            if (dead)
                category.deadCount += 1;
            else
                liveSize += size;
        }

        return {liveSize, categories};
    }

    static allocationBucketCounts(snapshot, bucketSizes, allowNodeIdentifierCallback)
    {
        let counts = new Array(bucketSizes.length + 1);
        let remainderBucket = counts.length - 1;
        counts.fill(0);

        let nodes = snapshot._nodes;

        // Skip the <root> node.
        let firstNodeIndex = nodeFieldCount;

    outer:
        for (let nodeIndex = firstNodeIndex; nodeIndex < nodes.length; nodeIndex += nodeFieldCount) {
            if (allowNodeIdentifierCallback && !allowNodeIdentifierCallback(nodes[nodeIndex + nodeIdOffset]))
                continue;

            let size = nodes[nodeIndex + nodeSizeOffset];
            for (let i = 0; i < bucketSizes.length; ++i) {
                if (size < bucketSizes[i]) {
                    counts[i]++;
                    continue outer;
                }
            }
            counts[remainderBucket]++;
        }

        return counts;
    }

    static instancesWithClassName(snapshot, className, allowNodeIdentifierCallback)
    {
        let instances = [];

        let nodes = snapshot._nodes;
        let nodeClassNamesTable = snapshot._nodeClassNamesTable;

        // Skip the <root> node.
        let firstNodeIndex = nodeFieldCount;
        let firstNodeOrdinal = 1;
        for (let nodeIndex = firstNodeIndex, nodeOrdinal = firstNodeOrdinal; nodeIndex < nodes.length; nodeIndex += nodeFieldCount, nodeOrdinal++) {
            if (allowNodeIdentifierCallback && !allowNodeIdentifierCallback(nodes[nodeIndex + nodeIdOffset]))
                continue;

            let classNameTableIndex = nodes[nodeIndex + nodeClassNameOffset];
            if (nodeClassNamesTable[classNameTableIndex] === className)
                instances.push(nodeIndex);
        }

        return instances.map(snapshot.serializeNode, snapshot);
    }

    // Worker Methods

    allocationBucketCounts(bucketSizes)
    {
        return HeapSnapshot.allocationBucketCounts(this, bucketSizes);
    }

    instancesWithClassName(className)
    {
        return HeapSnapshot.instancesWithClassName(this, className);
    }

    update()
    {
        return HeapSnapshot.updateCategoriesAndMetadata(this);
    }

    nodeWithIdentifier(nodeIdentifier)
    {
        let nodeOrdinal = this._nodeIdentifierToOrdinal.get(nodeIdentifier);
        let nodeIndex = nodeOrdinal * nodeFieldCount;
        return this.serializeNode(nodeIndex);
    }

    shortestGCRootPath(nodeIdentifier)
    {
        // Returns an array from this node to a gcRoot node.
        // E.g. [Node (target), Edge, Node, Edge, Node (root)].
        // Internal nodes are avoided, so if the path is empty this
        // node is either a gcRoot or only reachable via Internal nodes.

        let paths = this._gcRootPaths(nodeIdentifier);
        if (!paths.length)
            return [];

        paths.sort((a, b) => a.length - b.length);

        let shortestPathWithGlobalObject = null;
        for (let path of paths) {
            let lastNodeIndex = path[path.length - 1].node;
            if (this._isNodeGlobalObject(lastNodeIndex)) {
                shortestPathWithGlobalObject = path;
                break;
            }
        }

        let shortestPath = shortestPathWithGlobalObject || paths[0];
        console.assert("node" in shortestPath[0], "Path should start with a node");
        console.assert("node" in shortestPath[shortestPath.length - 1], "Path should end with a node");

        return shortestPath.map((component) => {
            if (component.node)
                return this.serializeNode(component.node);
            return this.serializeEdge(component.edge);
        });
    }
    
    rootNodes()
    {
        let rootNodeIndexSet = new Set;

        // Identifiers can occur multiple times in the roots list.
        for (let rootIndex = 0; rootIndex < this._roots.length; rootIndex += rootFieldCount) {
            let rootOrdinal = rootIndex / rootFieldCount;
            let rootIdentifier = this._roots[rootIndex + rootIdOffset];

            let toNodeOrdinal = this._nodeIdentifierToOrdinal.get(rootIdentifier);
            let toNodeIndex = toNodeOrdinal * nodeFieldCount;

            rootNodeIndexSet.add(toNodeIndex);
        }

        return Array.from(rootNodeIndexSet.values()).map(this.serializeNode, this);
    }
    
    reasonNamesForRoot(rootIdentifier)
    {
        let reasonIndexes = this._rootIdentifierToReasons.get(rootIdentifier);
        if (!reasonIndexes)
            return [];
        return Array.from(reasonIndexes).map(this.labelForIndex, this).filter(a => a != 0)
    }

    dominatedNodes(nodeIdentifier)
    {
        let dominatedNodes = [];

        let targetNodeOrdinal = this._nodeIdentifierToOrdinal.get(nodeIdentifier);
        for (let nodeOrdinal = 0; nodeOrdinal < this._nodeCount; ++nodeOrdinal) {
            if (this._nodeOrdinalToDominatorNodeOrdinal[nodeOrdinal] === targetNodeOrdinal)
                dominatedNodes.push(nodeOrdinal * nodeFieldCount);
        }

        return dominatedNodes.map(this.serializeNode, this);
    }

    retainedNodes(nodeIdentifier)
    {
        let retainedNodes = [];
        let edges = [];

        let nodeOrdinal = this._nodeIdentifierToOrdinal.get(nodeIdentifier);
        let edgeIndex = this._nodeOrdinalToFirstOutgoingEdge[nodeOrdinal];
        for (; this._edges[edgeIndex + edgeFromIdOffset] === nodeIdentifier; edgeIndex += edgeFieldCount) {
            let toNodeIdentifier = this._edges[edgeIndex + edgeToIdOffset];
            let toNodeOrdinal = this._nodeIdentifierToOrdinal.get(toNodeIdentifier);
            let toNodeIndex = toNodeOrdinal * nodeFieldCount;
            retainedNodes.push(toNodeIndex);
            edges.push(edgeIndex);
        }

        return {
            retainedNodes: retainedNodes.map(this.serializeNode, this),
            edges: edges.map(this.serializeEdge, this),
        };
    }

    retainers(nodeIdentifier)
    {
        let retainers = [];
        let edges = [];

        let nodeOrdinal = this._nodeIdentifierToOrdinal.get(nodeIdentifier);
        let incomingEdgeIndex = this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal];
        let incomingEdgeIndexEnd = this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal + 1];
        for (let edgeIndex = incomingEdgeIndex; edgeIndex < incomingEdgeIndexEnd; ++edgeIndex) {
            let fromNodeOrdinal = this._incomingNodes[edgeIndex];
            let fromNodeIndex = fromNodeOrdinal * nodeFieldCount;
            retainers.push(fromNodeIndex);
            edges.push(this._incomingEdges[edgeIndex]);
        }

        return {
            retainers: retainers.map(this.serializeNode, this),
            edges: edges.map(this.serializeEdge, this),
        };
    }

    updateDeadNodesAndGatherCollectionData(snapshots)
    {
        let previousSnapshotIndex = snapshots.indexOf(this) - 1;
        let previousSnapshot = snapshots[previousSnapshotIndex];
        if (!previousSnapshot)
            return null;

        let lastNodeIdentifier = previousSnapshot._lastNodeIdentifier;

        // All of the node identifiers that could have existed prior to this snapshot.
        let known = new Map;
        for (let nodeIndex = 0; nodeIndex < this._nodes.length; nodeIndex += nodeFieldCount) {
            let nodeIdentifier = this._nodes[nodeIndex + nodeIdOffset];
            if (nodeIdentifier > lastNodeIdentifier)
                continue;
            known.set(nodeIdentifier, nodeIndex);
        }

        // Determine which node identifiers have since been deleted.
        let collectedNodesList = [];
        for (let nodeIndex = 0; nodeIndex < previousSnapshot._nodes.length; nodeIndex += nodeFieldCount) {
            let nodeIdentifier = previousSnapshot._nodes[nodeIndex + nodeIdOffset];
            let wasDeleted = !known.has(nodeIdentifier);
            if (wasDeleted)
                collectedNodesList.push(nodeIdentifier);
        }

        // Update dead nodes in previous snapshots.
        let affectedSnapshots = [];
        for (let snapshot of snapshots) {
            if (snapshot === this)
                break;
            if (snapshot._markDeadNodes(collectedNodesList))
                affectedSnapshots.push(snapshot._identifier);
        }

        // Convert list to a map.
        let collectedNodes = {};
        for (let i = 0; i < collectedNodesList.length; ++i)
            collectedNodes[collectedNodesList[i]] = true;

        return {
            collectedNodes,
            affectedSnapshots,
        };
    }

    // Public

    serialize()
    {
        return {
            identifier: this._identifier,
            title: this._title,
            totalSize: this._totalSize,
            totalObjectCount: this._nodeCount - 1, // <root>.
            liveSize: this._liveSize,
            categories: this._categories,
        };
    }

    serializeNode(nodeIndex)
    {
        console.assert((nodeIndex % nodeFieldCount) === 0, "Invalid nodeIndex to serialize: " + nodeIndex);

        let nodeIdentifier = this._nodes[nodeIndex + nodeIdOffset];
        let nodeOrdinal = nodeIndex / nodeFieldCount;
        let edgeIndex = this._nodeOrdinalToFirstOutgoingEdge[nodeOrdinal];
        let hasChildren = this._edges[edgeIndex + edgeFromIdOffset] === nodeIdentifier;

        let dominatorNodeOrdinal = this._nodeOrdinalToDominatorNodeOrdinal[nodeOrdinal];
        let dominatorNodeIndex = dominatorNodeOrdinal * nodeFieldCount;
        let dominatorNodeIdentifier = this._nodes[dominatorNodeIndex + nodeIdOffset];

        let result = {
            id: nodeIdentifier,
            className: this._nodeClassNamesTable[this._nodes[nodeIndex + nodeClassNameOffset]],
            size: this._nodes[nodeIndex + nodeSizeOffset],
            retainedSize: this._nodeOrdinalToRetainedSizes[nodeOrdinal],
            internal: this._nodes[nodeIndex + nodeInternalOffset] ? true : false,
            gcRoot: this._nodeOrdinalIsGCRoot[nodeOrdinal] ? true : false,
            markedRoot : this._rootIdentifierToReasons.has(nodeIdentifier),
            dead: this._nodeOrdinalIsDead[nodeOrdinal] ? true : false,
            address: this._nodes[nodeIndex + nodeAddressOffset],
            label: this._labelsTable[this._nodes[nodeIndex + nodeLabelOffset]],
            dominatorNodeIdentifier,
            hasChildren,
        };

        let wrappedAddr = this._nodes[nodeIndex + nodeWrappedAddressOffset];
        if (wrappedAddr !== "0x0")
            result.wrappedAddress = wrappedAddr;
        
        return result;
    }

    labelForIndex(index)
    {
        return this._labelsTable[index];
    }

    serializeEdge(edgeIndex)
    {
        console.assert((edgeIndex % edgeFieldCount) === 0, "Invalid edgeIndex to serialize");

        let edgeType = this._edgeTypesTable[this._edges[edgeIndex + edgeTypeOffset]];
        let edgeData = this._edges[edgeIndex + edgeDataOffset];
        switch (edgeType) {
        case "Internal":
            // edgeData can be ignored.
            break;
        case "Property":
        case "Variable":
            // edgeData is a table index.
            edgeData = this._edgeNamesTable[edgeData];
            break;
        case "Index":
            // edgeData is the index.
            break;
        default:
            console.error("Unexpected edge type: " + edgeType);
            break;
        }

        return {
            from: this._edges[edgeIndex + edgeFromIdOffset],
            to: this._edges[edgeIndex + edgeToIdOffset],
            type: edgeType,
            data: edgeData,
        };
    }

    // Private

    _buildOutgoingEdges()
    {
        let lastFromIdentifier = -1;
        for (let edgeIndex = 0; edgeIndex < this._edges.length; edgeIndex += edgeFieldCount) {
            let fromIdentifier = this._edges[edgeIndex + edgeFromIdOffset];
            console.assert(lastFromIdentifier <= fromIdentifier, "Edge list should be ordered by from node identifier");
            if (fromIdentifier !== lastFromIdentifier) {
                let nodeOrdinal = this._nodeIdentifierToOrdinal.get(fromIdentifier);
                this._nodeOrdinalToFirstOutgoingEdge[nodeOrdinal] = edgeIndex;
                lastFromIdentifier = fromIdentifier;
            }
        }
    }

    _buildIncomingEdges()
    {
        // First calculate the count of incoming edges for each node.
        for (let edgeIndex = 0; edgeIndex < this._edges.length; edgeIndex += edgeFieldCount) {
            let toIdentifier = this._edges[edgeIndex + edgeToIdOffset];
            let toNodeOrdinal = this._nodeIdentifierToOrdinal.get(toIdentifier);
            this._nodeOrdinalToFirstIncomingEdge[toNodeOrdinal]++;
        }

        // Replace the counts with what will be the resulting index by running up the counts.
        // Store the counts in what will be the edges list to use when placing edges in the list.
        let runningFirstIndex = 0;
        for (let nodeOrdinal = 0; nodeOrdinal < this._nodeCount; ++nodeOrdinal) {
            let count = this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal];
            this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal] = runningFirstIndex;
            this._incomingNodes[runningFirstIndex] = count;
            runningFirstIndex += count;
        }

        // Fill in the incoming edges list. Use the count as an offset when placing edges in the list.
        for (let edgeIndex = 0; edgeIndex < this._edges.length; edgeIndex += edgeFieldCount) {
            let fromIdentifier = this._edges[edgeIndex + edgeFromIdOffset];
            let fromNodeOrdinal = this._nodeIdentifierToOrdinal.get(fromIdentifier);
            let toIdentifier = this._edges[edgeIndex + edgeToIdOffset];
            let toNodeOrdinal = this._nodeIdentifierToOrdinal.get(toIdentifier);

            let firstIncomingEdgeIndex = this._nodeOrdinalToFirstIncomingEdge[toNodeOrdinal];
            console.assert(this._incomingNodes[firstIncomingEdgeIndex] > 0, "Should be expecting edges for this node");
            let countAsOffset = this._incomingNodes[firstIncomingEdgeIndex]--;
            let index = firstIncomingEdgeIndex + countAsOffset - 1;
            this._incomingNodes[index] = fromNodeOrdinal;
            this._incomingEdges[index] = edgeIndex;
        }

        // Duplicate value on the end. Incoming edge iteration walks firstIncomingEdge(ordinal) to firstIncomingEdge(ordinal+1).
        this._nodeOrdinalToFirstIncomingEdge[this._nodeCount] = this._nodeOrdinalToFirstIncomingEdge[this._nodeCount - 1];
    }

    _buildPostOrderIndexes()
    {
        let postOrderIndex = 0;
        let nodeOrdinalToPostOrderIndex = new Uint32Array(this._nodeCount);
        let postOrderIndexToNodeOrdinal = new Uint32Array(this._nodeCount);

        let stackNodes = new Uint32Array(this._nodeCount); // nodeOrdinal.
        let stackEdges = new Uint32Array(this._nodeCount); // edgeIndex.
        let visited = new Uint8Array(this._nodeCount);

        let stackTop = 0;
        stackNodes[stackTop] = rootNodeOrdinal;
        stackEdges[stackTop] = this._nodeOrdinalToFirstOutgoingEdge[rootNodeOrdinal];

        while (stackTop >= 0) {
            let nodeOrdinal = stackNodes[stackTop];
            let nodeIdentifier = this._nodes[(nodeOrdinal * nodeFieldCount) + nodeIdOffset];
            let edgeIndex = stackEdges[stackTop];

            if (this._edges[edgeIndex + edgeFromIdOffset] === nodeIdentifier) {
                // Prepare the next child for the current node.
                stackEdges[stackTop] += edgeFieldCount;

                let toIdentifier = this._edges[edgeIndex + edgeToIdOffset];
                let toNodeOrdinal = this._nodeIdentifierToOrdinal.get(toIdentifier);
                if (visited[toNodeOrdinal])
                    continue;

                // Child.
                stackTop++;
                stackNodes[stackTop] = toNodeOrdinal;
                stackEdges[stackTop] = this._nodeOrdinalToFirstOutgoingEdge[toNodeOrdinal];
                visited[toNodeOrdinal] = 1;
            } else {
                // Self.
                nodeOrdinalToPostOrderIndex[nodeOrdinal] = postOrderIndex;
                postOrderIndexToNodeOrdinal[postOrderIndex] = nodeOrdinal;
                postOrderIndex++;
                stackTop--;
            }
        }

        // Unvisited nodes.
        // This can happen if the parent node was disallowed on the backend, but other nodes
        // that were only referenced from that disallowed node were eventually allowed because
        // they may be generic system objects. Give these nodes a postOrderIndex anyways.
        if (postOrderIndex !== this._nodeCount) {
            // Root was the last node visited. Revert assigning it an index, add it back at the end.
            postOrderIndex--;

            // Visit unvisited nodes.
            for (let nodeOrdinal = 1; nodeOrdinal < this._nodeCount; ++nodeOrdinal) {
                if (visited[nodeOrdinal])
                    continue;
                nodeOrdinalToPostOrderIndex[nodeOrdinal] = postOrderIndex;
                postOrderIndexToNodeOrdinal[postOrderIndex] = nodeOrdinal;
                postOrderIndex++;
            }

            // Visit root again.
            nodeOrdinalToPostOrderIndex[rootNodeOrdinal] = postOrderIndex;
            postOrderIndexToNodeOrdinal[postOrderIndex] = rootNodeOrdinal;
            postOrderIndex++;
        }

        console.assert(postOrderIndex === this._nodeCount, "All nodes were visited");
        console.assert(nodeOrdinalToPostOrderIndex[rootNodeOrdinal] === this._nodeCount - 1, "Root node should have the last possible postOrderIndex");

        return {nodeOrdinalToPostOrderIndex, postOrderIndexToNodeOrdinal};
    }

    _buildDominatorIndexes(nodeOrdinalToPostOrderIndex, postOrderIndexToNodeOrdinal)
    {
        // The algorithm is based on the article:
        // K. Cooper, T. Harvey and K. Kennedy "A Simple, Fast Dominance Algorithm"

        let rootPostOrderIndex = this._nodeCount - 1;
        let noEntry = this._nodeCount;

        let affected = new Uint8Array(this._nodeCount);
        let dominators = new Uint32Array(this._nodeCount);

        // Initialize with unset value.
        dominators.fill(noEntry);

        // Mark the root's dominator value.
        dominators[rootPostOrderIndex] = rootPostOrderIndex;

        // Affect the root's children. Also use this opportunity to mark them as GC roots.
        let rootEdgeIndex = this._nodeOrdinalToFirstOutgoingEdge[rootNodeOrdinal];
        for (let edgeIndex = rootEdgeIndex; this._edges[edgeIndex + edgeFromIdOffset] === rootNodeIdentifier; edgeIndex += edgeFieldCount) {
            let toIdentifier = this._edges[edgeIndex + edgeToIdOffset];
            let toNodeOrdinal = this._nodeIdentifierToOrdinal.get(toIdentifier);
            let toPostOrderIndex = nodeOrdinalToPostOrderIndex[toNodeOrdinal];
            affected[toPostOrderIndex] = 1;
            this._nodeOrdinalIsGCRoot[toNodeOrdinal] = 1;
        }

        let changed = true;
        while (changed) {
            changed = false;

            for (let postOrderIndex = rootPostOrderIndex - 1; postOrderIndex >= 0; --postOrderIndex) {
                if (!affected[postOrderIndex])
                    continue;
                affected[postOrderIndex] = 0;

                // The dominator is already the root, nothing to do.
                if (dominators[postOrderIndex] === rootPostOrderIndex)
                    continue;

                let newDominatorIndex = noEntry;
                let nodeOrdinal = postOrderIndexToNodeOrdinal[postOrderIndex];
                let incomingEdgeIndex = this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal];
                let incomingEdgeIndexEnd = this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal + 1];
                for (let edgeIndex = incomingEdgeIndex; edgeIndex < incomingEdgeIndexEnd; ++edgeIndex) {
                    let fromNodeOrdinal = this._incomingNodes[edgeIndex];
                    let fromPostOrderIndex = nodeOrdinalToPostOrderIndex[fromNodeOrdinal];
                    if (dominators[fromPostOrderIndex] !== noEntry) {
                        if (newDominatorIndex === noEntry)
                            newDominatorIndex = fromPostOrderIndex;
                        else {
                            while (fromPostOrderIndex !== newDominatorIndex) {
                                while (fromPostOrderIndex < newDominatorIndex)
                                    fromPostOrderIndex = dominators[fromPostOrderIndex];
                                while (newDominatorIndex < fromPostOrderIndex)
                                    newDominatorIndex = dominators[newDominatorIndex];
                            }
                        }
                    }
                    if (newDominatorIndex === rootPostOrderIndex)
                        break;
                }

                // Changed. Affect children.
                if (newDominatorIndex !== noEntry && dominators[postOrderIndex] !== newDominatorIndex) {
                    dominators[postOrderIndex] = newDominatorIndex;
                    changed = true;

                    let outgoingEdgeIndex = this._nodeOrdinalToFirstOutgoingEdge[nodeOrdinal];
                    let nodeIdentifier = this._nodes[(nodeOrdinal * nodeFieldCount) + nodeIdOffset];
                    for (let edgeIndex = outgoingEdgeIndex; this._edges[edgeIndex + edgeFromIdOffset] === nodeIdentifier; edgeIndex += edgeFieldCount) {
                        let toNodeIdentifier = this._edges[edgeIndex + edgeToIdOffset];
                        let toNodeOrdinal = this._nodeIdentifierToOrdinal.get(toNodeIdentifier);
                        let toNodePostOrder = nodeOrdinalToPostOrderIndex[toNodeOrdinal];
                        affected[toNodePostOrder] = 1;
                    }
                }
            }
        }

        for (let postOrderIndex = 0; postOrderIndex < this._nodeCount; ++postOrderIndex) {
            let nodeOrdinal = postOrderIndexToNodeOrdinal[postOrderIndex];
            let dominatorNodeOrdinal = postOrderIndexToNodeOrdinal[dominators[postOrderIndex]];
            this._nodeOrdinalToDominatorNodeOrdinal[nodeOrdinal] = dominatorNodeOrdinal;
        }
    }

    _buildRetainedSizes(postOrderIndexToNodeOrdinal)
    {
        // Self size.
        for (let nodeIndex = 0, nodeOrdinal = 0; nodeOrdinal < this._nodeCount; nodeIndex += nodeFieldCount, nodeOrdinal++)
            this._nodeOrdinalToRetainedSizes[nodeOrdinal] = this._nodes[nodeIndex + nodeSizeOffset];

        // Attribute size to dominator.
        for (let postOrderIndex = 0; postOrderIndex < this._nodeCount - 1; ++postOrderIndex) {
            let nodeOrdinal = postOrderIndexToNodeOrdinal[postOrderIndex];
            let nodeRetainedSize = this._nodeOrdinalToRetainedSizes[nodeOrdinal];
            let dominatorNodeOrdinal = this._nodeOrdinalToDominatorNodeOrdinal[nodeOrdinal];
            this._nodeOrdinalToRetainedSizes[dominatorNodeOrdinal] += nodeRetainedSize;
        }
    }

    _markDeadNodes(collectedNodesList)
    {
        let affected = false;

        for (let i = 0; i < collectedNodesList.length; ++i) {
            let nodeIdentifier = collectedNodesList[i];
            if (nodeIdentifier > this._lastNodeIdentifier)
                continue;
            let nodeOrdinal = this._nodeIdentifierToOrdinal.get(nodeIdentifier);
            this._nodeOrdinalIsDead[nodeOrdinal] = 1;
            affected = true;
        }

        return affected;
    }

    _isNodeGlobalObject(nodeIndex)
    {
        let className = this._nodeClassNamesTable[this._nodes[nodeIndex + nodeClassNameOffset]];
        return className === "Window"
            || className === "JSWindowProxy"
            || className === "GlobalObject";
    }

    _gcRootPaths(nodeIdentifier)
    {
        let targetNodeOrdinal = this._nodeIdentifierToOrdinal.get(nodeIdentifier);

        if (this._nodeOrdinalIsGCRoot[targetNodeOrdinal])
            return [];

        // FIXME: Array push/pop can affect performance here, but in practice it hasn't been an issue.

        let paths = [];
        let currentPath = [];
        let visited = new Uint8Array(this._nodeCount);

        function visitNode(nodeOrdinal)
        {
            if (this._nodeOrdinalIsGCRoot[nodeOrdinal]) {
                let fullPath = currentPath.slice();
                let nodeIndex = nodeOrdinal * nodeFieldCount;
                fullPath.push({node: nodeIndex});
                paths.push(fullPath);
                return;
            }

            if (visited[nodeOrdinal])
                return;
            visited[nodeOrdinal] = 1;

            let nodeIndex = nodeOrdinal * nodeFieldCount;
            currentPath.push({node: nodeIndex});

            // Loop in reverse order because edges were added in reverse order.
            // It doesn't particularly matter other then consistency with previous code.
            let incomingEdgeIndexStart = this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal];
            let incomingEdgeIndexEnd = this._nodeOrdinalToFirstIncomingEdge[nodeOrdinal + 1];
            for (let incomingEdgeIndex = incomingEdgeIndexEnd - 1; incomingEdgeIndex >= incomingEdgeIndexStart; --incomingEdgeIndex) {
                let fromNodeOrdinal = this._incomingNodes[incomingEdgeIndex];
                let fromNodeIndex = fromNodeOrdinal * nodeFieldCount;
                // let fromNodeIsInternal = this._nodes[fromNodeIndex + nodeInternalOffset];
                // if (fromNodeIsInternal)
                //     continue;

                let edgeIndex = this._incomingEdges[incomingEdgeIndex];
                currentPath.push({edge: edgeIndex});
                visitNode.call(this, fromNodeOrdinal);
                currentPath.pop();
            }

            currentPath.pop();
        }

        visitNode.call(this, targetNodeOrdinal);

        return paths;
    }
};
