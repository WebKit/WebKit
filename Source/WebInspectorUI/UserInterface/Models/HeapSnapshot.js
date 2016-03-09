/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.HeapSnapshotClassCategory = class HeapSnapshotClassCategory
{
    constructor(className)
    {
        this.className = className;
        this.size = 0;
        this.count = 0;
        this.internalCount = 0;
    }
};

WebInspector.HeapSnapshot = class HeapSnapshot extends WebInspector.Object
{
    constructor(rootNode, nodes, nodeMap)
    {
        super();

        console.assert(!rootNode || rootNode instanceof WebInspector.HeapSnapshotNode);
        console.assert(nodes instanceof Array);

        this._rootNode = rootNode;
        this._nodes = nodes;
        this._nodeMap = nodeMap;

        this._identifier = WebInspector.HeapSnapshot._nextAvailableSnapshotIdentifier++;
        this._instances = nodes;

        let categories = {};
        for (let i = 0; i < nodes.length; ++i) {
            let {className, size, internal} = nodes[i];

            let category = categories[className];
            if (!category)
                category = categories[className] = new WebInspector.HeapSnapshotClassCategory(className);

            category.size += size;
            category.count++;
            if (internal)
                category.internalCount++;
        }
        this._categories = Map.fromObject(categories);

        this._totalSize = 0;
        this._totalObjectCount = 0;
        for (let {count, size} of this._categories.values()) {
            this._totalSize += size;
            this._totalObjectCount += count;
        }
    }

    // Static

    static fromPayload(payload)
    {
        let {version, nodes, nodeClassNames, edges, edgeTypes} = payload;
        console.assert(version === 1, "Only know how to handle JavaScriptCore Heap Snapshot Format Version 1");
        console.assert(edgeTypes.every((type) => type in WebInspector.HeapSnapshotEdge.EdgeType), "Unexpected edge type", edgeTypes);

        let nodeMap = new Map;

        // Turn nodes into real nodes.
        for (let i = 0, length = nodes.length; i < length; ++i) {
            let nodePayload = nodes[i];
            let id = nodePayload[0];
            let size = nodePayload[1];
            let classNameIndex = nodePayload[2];
            let internal = nodePayload[3];

            let node = new WebInspector.HeapSnapshotNode(id, nodeClassNames[classNameIndex], size, !!internal);
            nodeMap.set(id, node);
            nodes[i] = node;
        }

        // Turn edges into real edges and set them on the nodes.
        for (let i = 0, length = edges.length; i < length; ++i) {
            let edgePayload = edges[i];
            let fromIdentifier = edgePayload[0];
            let toIdentifier = edgePayload[1];
            let edgeTypeIndex = edgePayload[2];
            let data = edgePayload[3];

            let from = nodeMap.get(fromIdentifier);
            let to = nodeMap.get(toIdentifier);
            let edge = new WebInspector.HeapSnapshotEdge(from, to, edgeTypes[edgeTypeIndex], data);
            from.outgoingEdges.push(edge);
            to.incomingEdges.push(edge);
        }

        // Root node.
        let rootNode = nodeMap.get(0);
        console.assert(rootNode, "Node with identifier 0 is the synthetic <root> node.");
        console.assert(rootNode.outgoingEdges.length > 0, "This had better have children!");
        console.assert(rootNode.incomingEdges.length === 0, "This had better not have back references!");

        // Mark GC roots.
        let rootNodeEdges = rootNode.outgoingEdges;
        for (let i = 0, length = rootNodeEdges.length; i < length; ++i)
            rootNodeEdges[i].to.gcRoot = true;

        return new WebInspector.HeapSnapshot(rootNode, nodes, nodeMap);
    }

    // Public

    get rootNode() { return this._rootNode; }
    get nodes() { return this._nodes; }
    get identifier() { return this._identifier; }
    get instances() { return this._instances; }
    get categories() { return this._categories; }
    get totalSize() { return this._totalSize; }
    get totalObjectCount() { return this._totalObjectCount; }

    instancesWithClassName(className)
    {
        let results = [];
        for (let i = 0; i < this._instances.length; ++i) {
            let cell = this._instances[i];
            if (cell.className === className)
                results.push(cell);
        }
        return results;
    }

    nodeWithObjectIdentifier(id)
    {
        return this._nodeMap.get(id);
    }
};

WebInspector.HeapSnapshot._nextAvailableSnapshotIdentifier = 1;
