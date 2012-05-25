/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

/**
 * @constructor
 */
WebInspector.HeapSnapshotArraySlice = function(array, start, end)
{
    this._array = array;
    this._start = start;
    this.length = end - start;
}

WebInspector.HeapSnapshotArraySlice.prototype = {
    item: function(index)
    {
        return this._array[this._start + index];
    },

    slice: function(start, end)
    {
        if (typeof end === "undefined")
            end = this.length;
        return this._array.subarray(this._start + start, this._start + end);
    }
}

/**
 * @constructor
 * @param {number=} edgeIndex
 */
WebInspector.HeapSnapshotEdge = function(snapshot, edges, edgeIndex)
{
    this._snapshot = snapshot;
    this._edges = edges;
    this.edgeIndex = edgeIndex || 0;
}

WebInspector.HeapSnapshotEdge.prototype = {
    clone: function()
    {
        return new WebInspector.HeapSnapshotEdge(this._snapshot, this._edges, this.edgeIndex);
    },

    hasStringName: function()
    {
        if (!this.isShortcut())
            return this._hasStringName();
        return isNaN(parseInt(this._name(), 10));
    },

    isElement: function()
    {
        return this._type() === this._snapshot._edgeElementType;
    },

    isHidden: function()
    {
        return this._type() === this._snapshot._edgeHiddenType;
    },

    isWeak: function()
    {
        return this._type() === this._snapshot._edgeWeakType;
    },

    isInternal: function()
    {
        return this._type() === this._snapshot._edgeInternalType;
    },

    isInvisible: function()
    {
        return this._type() === this._snapshot._edgeInvisibleType;
    },

    isShortcut: function()
    {
        return this._type() === this._snapshot._edgeShortcutType;
    },

    name: function()
    {
        if (!this.isShortcut())
            return this._name();
        var numName = parseInt(this._name(), 10);
        return isNaN(numName) ? this._name() : numName;
    },

    node: function()
    {
        return new WebInspector.HeapSnapshotNode(this._snapshot, this.nodeIndex());
    },

    nodeIndex: function()
    {
        return this._edges.item(this.edgeIndex + this._snapshot._edgeToNodeOffset);
    },

    rawEdges: function()
    {
        return this._edges;
    },

    toString: function()
    {
        var name = this.name();
        switch (this.type()) {
        case "context": return "->" + name;
        case "element": return "[" + name + "]";
        case "weak": return "[[" + name + "]]";
        case "property":
            return name.indexOf(" ") === -1 ? "." + name : "[\"" + name + "\"]";
        case "shortcut":
            if (typeof name === "string")
                return name.indexOf(" ") === -1 ? "." + name : "[\"" + name + "\"]";
            else
                return "[" + name + "]";
        case "internal":
        case "hidden":
        case "invisible":
            return "{" + name + "}";
        };
        return "?" + name + "?";
    },

    type: function()
    {
        return this._snapshot._edgeTypes[this._type()];
    },

    _hasStringName: function()
    {
        return !this.isElement() && !this.isHidden() && !this.isWeak();
    },

    _name: function()
    {
        return this._hasStringName() ? this._snapshot._strings[this._nameOrIndex()] : this._nameOrIndex();
    },

    _nameOrIndex: function()
    {
        return this._edges.item(this.edgeIndex + this._snapshot._edgeNameOffset);
    },

    _type: function()
    {
        return this._edges.item(this.edgeIndex + this._snapshot._edgeTypeOffset);
    }
};

/**
 * @constructor
 */
WebInspector.HeapSnapshotEdgeIterator = function(edge)
{
    this.edge = edge;
}

WebInspector.HeapSnapshotEdgeIterator.prototype = {
    first: function()
    {
        this.edge.edgeIndex = 0;
    },

    hasNext: function()
    {
        return this.edge.edgeIndex < this.edge._edges.length;
    },

    index: function()
    {
        return this.edge.edgeIndex;
    },

    setIndex: function(newIndex)
    {
        this.edge.edgeIndex = newIndex;
    },

    item: function()
    {
        return this.edge;
    },

    next: function()
    {
        this.edge.edgeIndex += this.edge._snapshot._edgeFieldsCount;
    }
};

/**
 * @constructor
 */
WebInspector.HeapSnapshotRetainerEdge = function(snapshot, retainedNodeIndex, retainerIndex)
{
    this._snapshot = snapshot;
    this._retainedNodeIndex = retainedNodeIndex;

    var retainedNodeOrdinal = retainedNodeIndex / snapshot._nodeFieldCount;
    this._firstRetainer = snapshot._firstRetainerIndex[retainedNodeOrdinal];
    this._retainersCount = snapshot._firstRetainerIndex[retainedNodeOrdinal + 1] - this._firstRetainer;

    this.setRetainerIndex(retainerIndex);
}

WebInspector.HeapSnapshotRetainerEdge.prototype = {
    clone: function()
    {
        return new WebInspector.HeapSnapshotRetainerEdge(this._snapshot, this._retainedNodeIndex, this.retainerIndex());
    },

    hasStringName: function()
    {
        return this._edge().hasStringName();
    },

    isElement: function()
    {
        return this._edge().isElement();
    },

    isHidden: function()
    {
        return this._edge().isHidden();
    },

    isInternal: function()
    {
        return this._edge().isInternal();
    },

    isInvisible: function()
    {
        return this._edge().isInvisible();
    },

    isShortcut: function()
    {
        return this._edge().isShortcut();
    },

    isWeak: function()
    {
        return this._edge().isWeak();
    },

    name: function()
    {
        return this._edge().name();
    },

    node: function()
    {
        return this._node();
    },

    nodeIndex: function()
    {
        return this._nodeIndex;
    },

    retainerIndex: function()
    {
        return this._retainerIndex;
    },

    setRetainerIndex: function(newIndex)
    {
        if (newIndex !== this._retainerIndex) {
            this._retainerIndex = newIndex;
            this.edgeIndex = newIndex;
        }
    },

    set edgeIndex(edgeIndex)
    {
        var retainerIndex = this._firstRetainer + edgeIndex;
        this._globalEdgeIndex = this._snapshot._retainingEdges[retainerIndex];
        this._nodeIndex = this._snapshot._retainingNodes[retainerIndex];
        delete this._edgeInstance;
        delete this._nodeInstance;
    },

    _node: function()
    {
        if (!this._nodeInstance)
            this._nodeInstance = new WebInspector.HeapSnapshotNode(this._snapshot, this._nodeIndex);
        return this._nodeInstance;
    },

    _edge: function()
    {
        if (!this._edgeInstance) {
            var edgeIndex = this._globalEdgeIndex - this._node()._edgeIndexesStart();
            this._edgeInstance = new WebInspector.HeapSnapshotEdge(this._snapshot, this._node().rawEdges(), edgeIndex);
        }
        return this._edgeInstance;
    },

    toString: function()
    {
        return this._edge().toString();
    },

    type: function()
    {
        return this._edge().type();
    }
}

/**
 * @constructor
 */
WebInspector.HeapSnapshotRetainerEdgeIterator = function(retainer)
{
    this.retainer = retainer;
}

WebInspector.HeapSnapshotRetainerEdgeIterator.prototype = {
    first: function()
    {
        this.retainer.setRetainerIndex(0);
    },

    hasNext: function()
    {
        return this.retainer.retainerIndex() < this.retainer._retainersCount;
    },

    index: function()
    {
        return this.retainer.retainerIndex();
    },

    setIndex: function(newIndex)
    {
        this.retainer.setRetainerIndex(newIndex);
    },

    item: function()
    {
        return this.retainer;
    },

    next: function()
    {
        this.retainer.setRetainerIndex(this.retainer.retainerIndex() + 1);
    }
};

/**
 * @constructor
 * @param {number=} nodeIndex
 */
WebInspector.HeapSnapshotNode = function(snapshot, nodeIndex)
{
    this._snapshot = snapshot;
    this._firstNodeIndex = nodeIndex;
    this.nodeIndex = nodeIndex;
}

WebInspector.HeapSnapshotNode.prototype = {
    canBeQueried: function()
    {
        var flags = this._snapshot._flagsOfNode(this);
        return !!(flags & this._snapshot._nodeFlags.canBeQueried);
    },

    isPageObject: function()
    {
        var flags = this._snapshot._flagsOfNode(this);
        return !!(flags & this._snapshot._nodeFlags.pageObject);
    },

    distanceToWindow: function()
    {
        return this._snapshot._distancesToWindow[this.nodeIndex / this._snapshot._nodeFieldCount];
    },

    className: function()
    {
        var type = this.type();
        switch (type) {
        case "hidden":
            return WebInspector.UIString("(system)");
        case "object":
        case "native":
            return this.name();
        case "code":
            return WebInspector.UIString("(compiled code)");
        default:
            return "(" + type + ")";
        }
    },

    classIndex: function()
    {
        var snapshot = this._snapshot;
        var nodes = snapshot._nodes;
        var type = nodes[this.nodeIndex + snapshot._nodeTypeOffset];;
        if (type === snapshot._nodeObjectType || type === snapshot._nodeNativeType)
            return nodes[this.nodeIndex + snapshot._nodeNameOffset];
        return -1 - type;
    },

    dominatorIndex: function()
    {
        var nodeFieldCount = this._snapshot._nodeFieldCount;
        return this._snapshot._dominatorsTree[this.nodeIndex / this._snapshot._nodeFieldCount] * nodeFieldCount;
    },

    edges: function()
    {
        return new WebInspector.HeapSnapshotEdgeIterator(new WebInspector.HeapSnapshotEdge(this._snapshot, this.rawEdges()));
    },

    edgesCount: function()
    {
        return (this._edgeIndexesEnd() - this._edgeIndexesStart()) / this._snapshot._edgeFieldsCount;
    },

    flags: function()
    {
        return this._snapshot._flagsOfNode(this);
    },

    id: function()
    {
        var snapshot = this._snapshot;
        return snapshot._nodes[this.nodeIndex + snapshot._nodeIdOffset];
    },

    isHidden: function()
    {
        return this._type() === this._snapshot._nodeHiddenType;
    },

    isNative: function()
    {
        return this._type() === this._snapshot._nodeNativeType;
    },

    isSynthetic: function()
    {
        return this._type() === this._snapshot._nodeSyntheticType;
    },

    isWindow: function()
    {
        const windowRE = /^Window/;
        return windowRE.test(this.name());
    },

    isDetachedDOMTreesRoot: function()
    {
        return this.name() === "(Detached DOM trees)";
    },

    isDetachedDOMTree: function()
    {
        const detachedDOMTreeRE = /^Detached DOM tree/;
        return detachedDOMTreeRE.test(this.className());
    },

    isRoot: function()
    {
        return this.nodeIndex === this._snapshot._rootNodeIndex;
    },

    name: function()
    {
        return this._snapshot._strings[this._name()];
    },

    rawEdges: function()
    {
        return new WebInspector.HeapSnapshotArraySlice(this._snapshot._containmentEdges, this._edgeIndexesStart(), this._edgeIndexesEnd());
    },

    retainedSize: function()
    {
        return this._snapshot._retainedSizes[this.nodeIndex / this._snapshot._nodeFieldCount];
    },

    retainers: function()
    {
        return new WebInspector.HeapSnapshotRetainerEdgeIterator(new WebInspector.HeapSnapshotRetainerEdge(this._snapshot, this.nodeIndex, 0));
    },

    selfSize: function()
    {
        var snapshot = this._snapshot;
        return snapshot._nodes[this.nodeIndex + snapshot._nodeSelfSizeOffset];
    },

    type: function()
    {
        return this._snapshot._nodeTypes[this._type()];
    },

    _name: function()
    {
        var snapshot = this._snapshot;
        return snapshot._nodes[this.nodeIndex + snapshot._nodeNameOffset];
    },

    _edgeIndexesStart: function()
    {
        var snapshot = this._snapshot;
        return snapshot._nodes[this.nodeIndex + snapshot._firstEdgeIndexOffset];
    },

    _edgeIndexesEnd: function()
    {
        var snapshot = this._snapshot;
        return snapshot._nodes[this._nextNodeIndex() + snapshot._firstEdgeIndexOffset]
    },

    _nextNodeIndex: function()
    {
        return this.nodeIndex + this._snapshot._nodeFieldCount;
    },

    _type: function()
    {
        var snapshot = this._snapshot;
        return snapshot._nodes[this.nodeIndex + snapshot._nodeTypeOffset];
    }
};

/**
 * @constructor
 */
WebInspector.HeapSnapshotNodeIterator = function(node)
{
    this.node = node;
    this._nodesLength = node._snapshot._realNodesLength;
}

WebInspector.HeapSnapshotNodeIterator.prototype = {
    first: function()
    {
        this.node.nodeIndex = this.node._firstNodeIndex;
    },

    hasNext: function()
    {
        return this.node.nodeIndex < this._nodesLength;
    },

    index: function()
    {
        return this.node.nodeIndex;
    },

    setIndex: function(newIndex)
    {
        this.node.nodeIndex = newIndex;
    },

    item: function()
    {
        return this.node;
    },

    next: function()
    {
        this.node.nodeIndex = this.node._nextNodeIndex();
    }
}

/**
 * @constructor
 */
WebInspector.HeapSnapshot = function(profile)
{
    this.uid = profile.snapshot.uid;
    this._nodes = profile.nodes;
    this._containmentEdges = profile.edges;
    /** @type{HeapSnapshotMetainfo} */
    this._metaNode = profile.snapshot.meta;
    this._strings = profile.strings;

    this._snapshotDiffs = {};
    this._aggregatesForDiff = null;

    this._init();
}

/**
 * @constructor
 */
function HeapSnapshotMetainfo()
{
    // New format.
    this.node_fields = [];
    this.node_types = [];
    this.edge_fields = [];
    this.edge_types = [];

    // Old format.
    this.fields = [];
    this.types = [];
}

/**
 * @constructor
 */
function HeapSnapshotHeader()
{
    // New format.
    this.title = "";
    this.uid = 0;
    this.meta = new HeapSnapshotMetainfo();
    this.node_count = 0;
    this.edge_count = 0;
}

WebInspector.HeapSnapshot.prototype = {
    _init: function()
    {
        var meta = this._metaNode;
        this._rootNodeIndex = 0;

        this._nodeTypeOffset = meta.node_fields.indexOf("type");
        this._nodeNameOffset = meta.node_fields.indexOf("name");
        this._nodeIdOffset = meta.node_fields.indexOf("id");
        this._nodeSelfSizeOffset = meta.node_fields.indexOf("self_size");
        this._firstEdgeIndexOffset = meta.node_fields.indexOf("edges_index");
        this._nodeFieldCount = meta.node_fields.length;

        this._nodeTypes = meta.node_types[this._nodeTypeOffset];
        this._nodeHiddenType = this._nodeTypes.indexOf("hidden");
        this._nodeObjectType = this._nodeTypes.indexOf("object");
        this._nodeNativeType = this._nodeTypes.indexOf("native");
        this._nodeCodeType = this._nodeTypes.indexOf("code");
        this._nodeSyntheticType = this._nodeTypes.indexOf("synthetic");

        this._edgeFieldsCount = meta.edge_fields.length;
        this._edgeTypeOffset = meta.edge_fields.indexOf("type");
        this._edgeNameOffset = meta.edge_fields.indexOf("name_or_index");
        this._edgeToNodeOffset = meta.edge_fields.indexOf("to_node");

        this._edgeTypes = meta.edge_types[this._edgeTypeOffset];
        this._edgeTypes.push("invisible");
        this._edgeElementType = this._edgeTypes.indexOf("element");
        this._edgeHiddenType = this._edgeTypes.indexOf("hidden");
        this._edgeInternalType = this._edgeTypes.indexOf("internal");
        this._edgeShortcutType = this._edgeTypes.indexOf("shortcut");
        this._edgeWeakType = this._edgeTypes.indexOf("weak");
        this._edgeInvisibleType = this._edgeTypes.indexOf("invisible");

        this._nodeFlags = { // bit flags
            canBeQueried: 1,
            detachedDOMTreeNode: 2,
            pageObject: 4, // The idea is to track separately the objects owned by the page and the objects owned by debugger.

            visitedMarkerMask: 0x0ffff, // bits: 0,1111,1111,1111,1111
            visitedMarker:     0x10000  // bits: 1,0000,0000,0000,0000
        };

        this._realNodesLength = this._nodes.length;
        this.nodeCount = this._realNodesLength / this._nodeFieldCount;
        this._edgeCount = this._containmentEdges.length / this._edgeFieldsCount;

        // Add an extra node and make its first edge field point to the end of edges array.
        var nodes = this._nodes;
        this._nodes = new Uint32Array(this._realNodesLength + this._nodeFieldCount);
        this._nodes.set(nodes);
        this._nodes[this._realNodesLength + this._firstEdgeIndexOffset] = this._containmentEdges.length;

        this._markInvisibleEdges();
        this._buildRetainers();
        this._calculateFlags();
        this._calculateObjectToWindowDistance();
        var result = this._buildPostOrderIndex();
        // Actually it is array that maps node ordinal number to dominator node ordinal number.
        this._dominatorsTree = this._buildDominatorTree(result.postOrderIndex2NodeOrdinal, result.nodeOrdinal2PostOrderIndex);
        this._calculateRetainedSizes();
        this._buildDominatedNodes();
    },

    _buildRetainers: function()
    {
        var retainingNodes = this._retainingNodes = new Uint32Array(this._edgeCount);
        var retainingEdges = this._retainingEdges = new Uint32Array(this._edgeCount);
        // Index of the first retainer in the _retainingNodes and _retainingEdges
        // arrays. Addressed by retained node index.
        var firstRetainerIndex = this._firstRetainerIndex = new Uint32Array(this.nodeCount + 1);

        var containmentEdges = this._containmentEdges;
        var edgeFieldsCount = this._edgeFieldsCount;
        var nodeFieldCount = this._nodeFieldCount;
        var edgeToNodeOffset = this._edgeToNodeOffset;
        var nodes = this._nodes;
        var firstEdgeIndexOffset = this._firstEdgeIndexOffset;

        for (var toNodeFieldIndex = edgeToNodeOffset, l = containmentEdges.length; toNodeFieldIndex < l; toNodeFieldIndex += edgeFieldsCount) {
            var toNodeIndex = containmentEdges[toNodeFieldIndex];
            if (toNodeIndex % nodeFieldCount)
                throw new Error("Invalid toNodeIndex " + toNodeIndex);
            ++firstRetainerIndex[toNodeIndex / nodeFieldCount];
        }
        for (var i = 0, firstUnusedRetainerSlot = 0, l = this.nodeCount; i < l; i++) {
            var retainersCount = firstRetainerIndex[i];
            firstRetainerIndex[i] = firstUnusedRetainerSlot;
            retainingNodes[firstUnusedRetainerSlot] = retainersCount;
            firstUnusedRetainerSlot += retainersCount;
        }
        firstRetainerIndex[this.nodeCount] = retainingNodes.length;

        var srcNodeIndex = 0;
        var nextNodeFirstEdgeIndex = nodes[firstEdgeIndexOffset];
        var nodesLength = this._realNodesLength;
        while (srcNodeIndex < nodesLength) {
            var firstEdgeIndex = nextNodeFirstEdgeIndex;
            var nextNodeIndex = srcNodeIndex + nodeFieldCount;
            nextNodeFirstEdgeIndex = nodes[nextNodeIndex + firstEdgeIndexOffset];
            for (var edgeIndex = firstEdgeIndex; edgeIndex < nextNodeFirstEdgeIndex; edgeIndex += edgeFieldsCount) {
                var toNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
                if (toNodeIndex % nodeFieldCount)
                    throw new Error("Invalid toNodeIndex " + toNodeIndex);
                var firstRetainerSlotIndex = firstRetainerIndex[toNodeIndex / nodeFieldCount];
                var nextUnusedRetainerSlotIndex = firstRetainerSlotIndex + (--retainingNodes[firstRetainerSlotIndex]);
                retainingNodes[nextUnusedRetainerSlotIndex] = srcNodeIndex;
                retainingEdges[nextUnusedRetainerSlotIndex] = edgeIndex;
            }
            srcNodeIndex = nextNodeIndex;
        }
    },

    dispose: function()
    {
        delete this._nodes;
        delete this._strings;
        delete this._retainingEdges;
        delete this._retainingNodes;
        delete this._firstRetainerIndex;
        if (this._aggregates) {
            delete this._aggregates;
            delete this._aggregatesSortedFlags;
        }
        delete this._dominatedNodes;
        delete this._firstDominatedNodeIndex;
        delete this._flags;
        delete this._distancesToWindow;
        delete this._dominatorsTree;
    },

    _allNodes: function()
    {
        return new WebInspector.HeapSnapshotNodeIterator(this.rootNode());
    },

    rootNode: function()
    {
        return new WebInspector.HeapSnapshotNode(this, this._rootNodeIndex);
    },

    get rootNodeIndex()
    {
        return this._rootNodeIndex;
    },

    get totalSize()
    {
        return this.rootNode().retainedSize();
    },

    _getDominatedIndex: function(nodeIndex)
    {
        if (nodeIndex % this._nodeFieldCount)
            throw new Error("Invalid nodeIndex: " + nodeIndex);
        return this._firstDominatedNodeIndex[nodeIndex / this._nodeFieldCount];
    },

    _dominatedNodesOfNode: function(node)
    {
        var dominatedIndexFrom = this._getDominatedIndex(node.nodeIndex);
        var dominatedIndexTo = this._getDominatedIndex(node._nextNodeIndex());
        return new WebInspector.HeapSnapshotArraySlice(this._dominatedNodes, dominatedIndexFrom, dominatedIndexTo);
    },

    _flagsOfNode: function(node)
    {
        return this._flags[node.nodeIndex / this._nodeFieldCount];
    },

    /**
     * @param {boolean} sortedIndexes
     * @param {string} key
     * @param {string=} filterString
     */
    aggregates: function(sortedIndexes, key, filterString)
    {
        if (!this._aggregates) {
            this._aggregates = {};
            this._aggregatesSortedFlags = {};
        }

        var aggregatesByClassName = this._aggregates[key];
        if (aggregatesByClassName) {
            if (sortedIndexes && !this._aggregatesSortedFlags[key]) {
                this._sortAggregateIndexes(aggregatesByClassName);
                this._aggregatesSortedFlags[key] = sortedIndexes;
            }
            return aggregatesByClassName;
        }

        var filter;
        if (filterString)
            filter = this._parseFilter(filterString);

        var aggregates = this._buildAggregates(filter);
        this._calculateClassesRetainedSize(aggregates.aggregatesByClassIndex, filter);
        aggregatesByClassName = aggregates.aggregatesByClassName;

        if (sortedIndexes)
            this._sortAggregateIndexes(aggregatesByClassName);

        this._aggregatesSortedFlags[key] = sortedIndexes;
        this._aggregates[key] = aggregatesByClassName;

        return aggregatesByClassName;
    },

    aggregatesForDiff: function()
    {
        if (this._aggregatesForDiff)
            return this._aggregatesForDiff;

        var aggregatesByClassName = this.aggregates(true, "allObjects");
        this._aggregatesForDiff  = {};

        var node = new WebInspector.HeapSnapshotNode(this);
        for (var className in aggregatesByClassName) {
            var aggregate = aggregatesByClassName[className];
            var indexes = aggregate.idxs;
            var ids = new Array(indexes.length);
            var selfSizes = new Array(indexes.length);
            for (var i = 0; i < indexes.length; i++) {
                node.nodeIndex = indexes[i];
                ids[i] = node.id();
                selfSizes[i] = node.selfSize();
            }

            this._aggregatesForDiff[className] = {
                indexes: indexes,
                ids: ids,
                selfSizes: selfSizes
            };
        }
        return this._aggregatesForDiff;
    },

    _calculateObjectToWindowDistance: function()
    {
        var nodeFieldCount = this._nodeFieldCount;
        var distances = new Uint32Array(this.nodeCount);

        // bfs for Window roots
        var nodesToVisit = new Uint32Array(this.nodeCount);
        var nodesToVisitLength = 0;
        for (var iter = this.rootNode().edges(); iter.hasNext(); iter.next()) {
            var node = iter.edge.node();
            if (node.isWindow()) {
                nodesToVisit[nodesToVisitLength++] = node.nodeIndex;
                distances[node.nodeIndex / nodeFieldCount] = 0;
            }
        }
        this._bfs(nodesToVisit, nodesToVisitLength, distances);

        // bfs for root
        nodesToVisitLength = 0;
        nodesToVisit[nodesToVisitLength++] = this._rootNodeIndex;
        distances[this._rootNodeIndex / nodeFieldCount] = 0;
        this._bfs(nodesToVisit, nodesToVisitLength, distances);
        this._distancesToWindow = distances;
    },

    _bfs: function(nodesToVisit, nodesToVisitLength, distances)
    {
        // Peload fields into local variables for better performance.
        var edgeFieldsCount = this._edgeFieldsCount;
        var containmentEdges = this._containmentEdges;
        var nodeFieldCount = this._nodeFieldCount;
        var firstEdgeIndexOffset = this._firstEdgeIndexOffset;
        var edgeToNodeOffset = this._edgeToNodeOffset;
        var nodes = this._nodes;
        var nodeCount = this.nodeCount;
        var containmentEdgesLength = containmentEdges.length;

        var index = 0;
        while (index < nodesToVisitLength) {
            var nodeIndex = nodesToVisit[index++]; // shift generates too much garbage.
            var nodeOrdinal = nodeIndex / nodeFieldCount;
            var distance = distances[nodeOrdinal] + 1;
            var firstEdgeIndex = nodes[nodeIndex + firstEdgeIndexOffset];
            var edgesEnd = nodes[nodeIndex + firstEdgeIndexOffset + nodeFieldCount];
            for (var edgeToNodeIndex = firstEdgeIndex + edgeToNodeOffset; edgeToNodeIndex < edgesEnd; edgeToNodeIndex += edgeFieldsCount) {
                var childNodeIndex = containmentEdges[edgeToNodeIndex];
                var childNodeOrdinal = childNodeIndex / nodeFieldCount;
                if (distances[childNodeOrdinal])
                    continue;
                distances[childNodeOrdinal] = distance;
                nodesToVisit[nodesToVisitLength++] = childNodeIndex;
            }
        }
        if (nodesToVisitLength > nodeCount)
            throw new Error("BFS failed. Nodes to visit (" + nodesToVisitLength + ") is more than nodes count (" + nodeCount + ")");
    },

    _buildAggregates: function(filter)
    {
        var aggregates = {};
        var aggregatesByClassName = {};
        var classIndexes = [];
        var nodes = this._nodes;
        var flags = this._flags;
        var nodesLength = this._realNodesLength;
        var nodeNativeType = this._nodeNativeType;
        var nodeFieldCount = this._nodeFieldCount;
        var selfSizeOffset = this._nodeSelfSizeOffset;
        var nodeTypeOffset = this._nodeTypeOffset;
        var pageObjectFlag = this._nodeFlags.pageObject;
        var node = new WebInspector.HeapSnapshotNode(this, this._rootNodeIndex);
        var distancesToWindow = this._distancesToWindow;

        for (var nodeIndex = this._rootNodeIndex; nodeIndex < nodesLength; nodeIndex += nodeFieldCount) {
            var nodeOrdinal = nodeIndex / nodeFieldCount;
            if (!(flags[nodeOrdinal] & pageObjectFlag))
                continue;
            node.nodeIndex = nodeIndex;
            if (filter && !filter(node))
                continue;
            var selfSize = nodes[nodeIndex + selfSizeOffset];
            if (!selfSize && nodes[nodeIndex + nodeTypeOffset] !== nodeNativeType)
                continue;
            var classIndex = node.classIndex();
            if (!(classIndex in aggregates)) {
                var nodeType = node.type();
                var nameMatters = nodeType === "object" || nodeType === "native";
                var value = {
                    count: 1,
                    distanceToWindow: distancesToWindow[nodeOrdinal],
                    self: selfSize,
                    maxRet: 0,
                    type: nodeType,
                    name: nameMatters ? node.name() : null,
                    idxs: [nodeIndex]
                };
                aggregates[classIndex] = value;
                classIndexes.push(classIndex);
                aggregatesByClassName[node.className()] = value;
            } else {
                var clss = aggregates[classIndex];
                clss.distanceToWindow = Math.min(clss.distanceToWindow, distancesToWindow[nodeOrdinal]);
                ++clss.count;
                clss.self += selfSize;
                clss.idxs.push(nodeIndex);
            }
        }

        // Shave off provisionally allocated space.
        for (var i = 0, l = classIndexes.length; i < l; ++i) {
            var classIndex = classIndexes[i];
            aggregates[classIndex].idxs = aggregates[classIndex].idxs.slice();
        }
        return {aggregatesByClassName: aggregatesByClassName, aggregatesByClassIndex: aggregates};
    },

    _calculateClassesRetainedSize: function(aggregates, filter)
    {
        var rootNodeIndex = this._rootNodeIndex;
        var node = new WebInspector.HeapSnapshotNode(this, rootNodeIndex);
        var list = [rootNodeIndex];
        var sizes = [-1];
        var classes = [];
        var seenClassNameIndexes = {};
        var nodeFieldCount = this._nodeFieldCount;
        var nodeTypeOffset = this._nodeTypeOffset;
        var nodeNativeType = this._nodeNativeType;
        var dominatedNodes = this._dominatedNodes;
        var nodes = this._nodes;
        var flags = this._flags;
        var pageObjectFlag = this._nodeFlags.pageObject;
        var firstDominatedNodeIndex = this._firstDominatedNodeIndex;

        while (list.length) {
            var nodeIndex = list.pop();
            node.nodeIndex = nodeIndex;
            var classIndex = node.classIndex();
            var seen = !!seenClassNameIndexes[classIndex];
            var nodeOrdinal = nodeIndex / nodeFieldCount;
            var dominatedIndexFrom = firstDominatedNodeIndex[nodeOrdinal];
            var dominatedIndexTo = firstDominatedNodeIndex[nodeOrdinal + 1];

            if (!seen &&
                (flags[nodeOrdinal] & pageObjectFlag) &&
                (!filter || filter(node)) &&
                (node.selfSize() || nodes[nodeIndex + nodeTypeOffset] === nodeNativeType)
               ) {
                aggregates[classIndex].maxRet += node.retainedSize();
                if (dominatedIndexFrom !== dominatedIndexTo) {
                    seenClassNameIndexes[classIndex] = true;
                    sizes.push(list.length);
                    classes.push(classIndex);
                }
            }
            for (var i = dominatedIndexFrom; i < dominatedIndexTo; i++)
                list.push(dominatedNodes[i]);

            var l = list.length;
            while (sizes[sizes.length - 1] === l) {
                sizes.pop();
                classIndex = classes.pop();
                seenClassNameIndexes[classIndex] = false;
            }
        }
    },

    _sortAggregateIndexes: function(aggregates)
    {
        var nodeA = new WebInspector.HeapSnapshotNode(this);
        var nodeB = new WebInspector.HeapSnapshotNode(this);
        for (var clss in aggregates)
            aggregates[clss].idxs.sort(
                function(idxA, idxB) {
                    nodeA.nodeIndex = idxA;
                    nodeB.nodeIndex = idxB;
                    return nodeA.id() < nodeB.id() ? -1 : 1;
                });
    },

    _buildPostOrderIndex: function()
    {
        var nodeFieldCount = this._nodeFieldCount;
        var nodes = this._nodes;
        var nodeCount = this.nodeCount;
        var rootNodeIndex = this._rootNodeIndex;

        var edgeFieldsCount = this._edgeFieldsCount;
        var edgeToNodeOffset = this._edgeToNodeOffset;
        var edgeTypeOffset = this._edgeTypeOffset;
        var edgeShortcutType = this._edgeShortcutType;
        var firstEdgeIndexOffset = this._firstEdgeIndexOffset;
        var containmentEdges = this._containmentEdges;
        var containmentEdgesLength = this._containmentEdges.length;

        var flags = this._flags;
        var flag = this._nodeFlags.pageObject;

        var nodesToVisit = new Uint32Array(nodeCount);
        var postOrderIndex2NodeOrdinal = new Uint32Array(nodeCount);
        var nodeOrdinal2PostOrderIndex = new Uint32Array(nodeCount);
        var painted = new Uint8Array(nodeCount);
        var nodesToVisitLength = 0;
        var postOrderIndex = 0;
        var grey = 1;
        var black = 2;

        nodesToVisit[nodesToVisitLength++] = this._rootNodeIndex;
        painted[this._rootNodeIndex / nodeFieldCount] = grey;

        while (nodesToVisitLength) {
            var nodeIndex = nodesToVisit[nodesToVisitLength - 1];
            var nodeOrdinal = nodeIndex / nodeFieldCount;
            if (painted[nodeOrdinal] === grey) {
                painted[nodeOrdinal] = black;
                var nodeFlag = flags[nodeOrdinal] & flag;
                var beginEdgeIndex = nodes[nodeIndex + firstEdgeIndexOffset];
                var endEdgeIndex = nodes[nodeIndex + firstEdgeIndexOffset + nodeFieldCount];
                for (var edgeIndex = beginEdgeIndex; edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
                    if (nodeIndex !== rootNodeIndex && containmentEdges[edgeIndex + edgeTypeOffset] === edgeShortcutType)
                        continue;
                    var childNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
                    var childNodeOrdinal = childNodeIndex / nodeFieldCount;
                    var childNodeFlag = flags[childNodeOrdinal] & flag;
                    // We are skipping the edges from non-page-owned nodes to page-owned nodes.
                    // Otherwise the dominators for the objects that also were retained by debugger would be affected.
                    if (nodeIndex !== rootNodeIndex && childNodeFlag && !nodeFlag)
                        continue;
                    if (!painted[childNodeOrdinal]) {
                        painted[childNodeOrdinal] = grey;
                        nodesToVisit[nodesToVisitLength++] = childNodeIndex;
                    }
                }
            } else {
                nodeOrdinal2PostOrderIndex[nodeOrdinal] = postOrderIndex;
                postOrderIndex2NodeOrdinal[postOrderIndex++] = nodeOrdinal;
                --nodesToVisitLength;
            }
        }

        if (postOrderIndex !== nodeCount)
            throw new Error("Postordering failed. " + (nodeCount - postOrderIndex) + " hanging nodes");

        return {postOrderIndex2NodeOrdinal: postOrderIndex2NodeOrdinal, nodeOrdinal2PostOrderIndex: nodeOrdinal2PostOrderIndex};
    },

    // The algorithm is based on the article:
    // K. Cooper, T. Harvey and K. Kennedy "A Simple, Fast Dominance Algorithm"
    // Softw. Pract. Exper. 4 (2001), pp. 1-10.
    /**
     * @param {Array.<number>} postOrderIndex2NodeOrdinal
     * @param {Array.<number>} nodeOrdinal2PostOrderIndex
     */
    _buildDominatorTree: function(postOrderIndex2NodeOrdinal, nodeOrdinal2PostOrderIndex)
    {
        var nodeFieldCount = this._nodeFieldCount;
        var nodes = this._nodes;
        var firstRetainerIndex = this._firstRetainerIndex;
        var retainingNodes = this._retainingNodes;
        var retainingEdges = this._retainingEdges;
        var edgeFieldsCount = this._edgeFieldsCount;
        var edgeToNodeOffset = this._edgeToNodeOffset;
        var edgeTypeOffset = this._edgeTypeOffset;
        var edgeShortcutType = this._edgeShortcutType;
        var firstEdgeIndexOffset = this._firstEdgeIndexOffset;
        var containmentEdges = this._containmentEdges;
        var containmentEdgesLength = this._containmentEdges.length;
        var rootNodeIndex = this._rootNodeIndex;

        var flags = this._flags;
        var flag = this._nodeFlags.pageObject;

        var nodesCount = postOrderIndex2NodeOrdinal.length;
        var rootPostOrderedIndex = nodesCount - 1;
        var noEntry = nodesCount;
        var dominators = new Uint32Array(nodesCount);
        for (var i = 0; i < rootPostOrderedIndex; ++i)
            dominators[i] = noEntry;
        dominators[rootPostOrderedIndex] = rootPostOrderedIndex;

        // The affected array is used to mark entries which dominators
        // have to be racalculated because of changes in their retainers.
        var affected = new Uint8Array(nodesCount);

        { // Mark the root direct children as affected.
            var nodeIndex = this._rootNodeIndex;
            var beginEdgeToNodeFieldIndex = nodes[nodeIndex + firstEdgeIndexOffset] + edgeToNodeOffset;
            var endEdgeToNodeFieldIndex = nodes[nodeIndex + nodeFieldCount + firstEdgeIndexOffset];
            for (var toNodeFieldIndex = beginEdgeToNodeFieldIndex;
                 toNodeFieldIndex < endEdgeToNodeFieldIndex;
                 toNodeFieldIndex += edgeFieldsCount) {
                var childNodeOrdinal = containmentEdges[toNodeFieldIndex] / nodeFieldCount;
                affected[nodeOrdinal2PostOrderIndex[childNodeOrdinal]] = 1;
            }
        }

        var changed = true;
        while (changed) {
            changed = false;
            for (var postOrderIndex = rootPostOrderedIndex - 1; postOrderIndex >= 0; --postOrderIndex) {
                if (affected[postOrderIndex] === 0)
                    continue;
                affected[postOrderIndex] = 0;
                // If dominator of the entry has already been set to root,
                // then it can't propagate any further.
                if (dominators[postOrderIndex] === rootPostOrderedIndex)
                    continue;
                var nodeOrdinal = postOrderIndex2NodeOrdinal[postOrderIndex];
                var nodeFlag = !!(flags[nodeOrdinal] & flag);
                var newDominatorIndex = noEntry;
                var beginRetainerIndex = firstRetainerIndex[nodeOrdinal];
                var endRetainerIndex = firstRetainerIndex[nodeOrdinal + 1];
                for (var retainerIndex = beginRetainerIndex; retainerIndex < endRetainerIndex; ++retainerIndex) {
                    var retainerEdgeIndex = retainingEdges[retainerIndex];
                    var retainerEdgeType = containmentEdges[retainerEdgeIndex + edgeTypeOffset];
                    var retainerNodeIndex = retainingNodes[retainerIndex];
                    if (retainerNodeIndex !== rootNodeIndex && retainerEdgeType === edgeShortcutType)
                        continue;
                    var retainerNodeOrdinal = retainerNodeIndex / nodeFieldCount;
                    var retainerNodeFlag = !!(flags[retainerNodeOrdinal] & flag);
                    // We are skipping the edges from non-page-owned nodes to page-owned nodes.
                    // Otherwise the dominators for the objects that also were retained by debugger would be affected.
                    if (retainerNodeIndex !== rootNodeIndex && nodeFlag && !retainerNodeFlag)
                        continue;
                    var retanerPostOrderIndex = nodeOrdinal2PostOrderIndex[retainerNodeOrdinal];
                    if (dominators[retanerPostOrderIndex] !== noEntry) {
                        if (newDominatorIndex === noEntry)
                            newDominatorIndex = retanerPostOrderIndex;
                        else {
                            while (retanerPostOrderIndex !== newDominatorIndex) {
                                while (retanerPostOrderIndex < newDominatorIndex)
                                    retanerPostOrderIndex = dominators[retanerPostOrderIndex];
                                while (newDominatorIndex < retanerPostOrderIndex)
                                    newDominatorIndex = dominators[newDominatorIndex];
                            }
                        }
                        // If idom has already reached the root, it doesn't make sense
                        // to check other retainers.
                        if (newDominatorIndex === rootPostOrderedIndex)
                            break;
                    }
                }
                if (newDominatorIndex !== noEntry && dominators[postOrderIndex] !== newDominatorIndex) {
                    dominators[postOrderIndex] = newDominatorIndex;
                    changed = true;
                    nodeOrdinal = postOrderIndex2NodeOrdinal[postOrderIndex];
                    nodeIndex = nodeOrdinal * nodeFieldCount;
                    beginEdgeToNodeFieldIndex = nodes[nodeIndex + firstEdgeIndexOffset] + edgeToNodeOffset;
                    endEdgeToNodeFieldIndex = nodes[nodeIndex + firstEdgeIndexOffset + nodeFieldCount];
                    for (var toNodeFieldIndex = beginEdgeToNodeFieldIndex;
                         toNodeFieldIndex < endEdgeToNodeFieldIndex;
                         toNodeFieldIndex += edgeFieldsCount) {
                        var childNodeOrdinal = containmentEdges[toNodeFieldIndex] / nodeFieldCount;
                        affected[nodeOrdinal2PostOrderIndex[childNodeOrdinal]] = 1;
                    }
                }
            }
        }

        var dominatorsTree = new Uint32Array(nodesCount);
        for (var postOrderIndex = 0, l = dominators.length; postOrderIndex < l; ++postOrderIndex) {
            var nodeOrdinal = postOrderIndex2NodeOrdinal[postOrderIndex];
            dominatorsTree[nodeOrdinal] = postOrderIndex2NodeOrdinal[dominators[postOrderIndex]];
        }
        return dominatorsTree;
    },

    _calculateRetainedSizes: function()
    {
        // As for the dominators tree we only know parent nodes, not
        // children, to sum up total sizes we "bubble" node's self size
        // adding it to all of its parents.
        var nodes = this._nodes;
        var nodeSelfSizeOffset = this._nodeSelfSizeOffset;
        var nodeFieldCount = this._nodeFieldCount;
        var dominatorsTree = this._dominatorsTree;
        var retainedSizes = new Uint32Array(this.nodeCount);
        var rootNodeOrdinal = this._rootNodeIndex / nodeFieldCount;

        for (var nodeOrdinal = 0, nodeSelfSizeIndex = nodeSelfSizeOffset, l = this.nodeCount;
             nodeOrdinal < l;
             ++nodeOrdinal, nodeSelfSizeIndex += nodeFieldCount) {
            var nodeSelfSize = nodes[nodeSelfSizeIndex];
            var currentNodeOrdinal = nodeOrdinal;
            retainedSizes[currentNodeOrdinal] += nodeSelfSize;
            do {
                currentNodeOrdinal = dominatorsTree[currentNodeOrdinal];
                retainedSizes[currentNodeOrdinal] += nodeSelfSize;
            } while (currentNodeOrdinal !== rootNodeOrdinal);
        }
        this._retainedSizes = retainedSizes;
    },

    _buildDominatedNodes: function()
    {
        // Builds up two arrays:
        //  - "dominatedNodes" is a continuous array, where each node owns an
        //    interval (can be empty) with corresponding dominated nodes.
        //  - "indexArray" is an array of indexes in the "dominatedNodes"
        //    with the same positions as in the _nodeIndex.
        var indexArray = this._firstDominatedNodeIndex = new Uint32Array(this.nodeCount + 1);
        // All nodes except the root have dominators.
        var dominatedNodes = this._dominatedNodes = new Uint32Array(this.nodeCount - 1);

        // Count the number of dominated nodes for each node. Skip the root (node at
        // index 0) as it is the only node that dominates itself.
        var nodeFieldCount = this._nodeFieldCount;
        var dominatorsTree = this._dominatorsTree;
        for (var nodeOrdinal = 1, l = this.nodeCount; nodeOrdinal < l; ++nodeOrdinal)
            ++indexArray[dominatorsTree[nodeOrdinal]];
        // Put in the first slot of each dominatedNodes slice the count of entries
        // that will be filled.
        var firstDominatedNodeIndex = 0;
        for (var i = 0, l = this.nodeCount; i < l; ++i) {
            var dominatedCount = dominatedNodes[firstDominatedNodeIndex] = indexArray[i];
            indexArray[i] = firstDominatedNodeIndex;
            firstDominatedNodeIndex += dominatedCount;
        }
        indexArray[this.nodeCount] = dominatedNodes.length;
        // Fill up the dominatedNodes array with indexes of dominated nodes. Skip the root (node at
        // index 0) as it is the only node that dominates itself.
        for (var nodeOrdinal = 1, l = this.nodeCount; nodeOrdinal < l; ++nodeOrdinal) {
            var dominatorOrdinal = dominatorsTree[nodeOrdinal];
            var dominatedRefIndex = indexArray[dominatorOrdinal];
            dominatedRefIndex += (--dominatedNodes[dominatedRefIndex]);
            dominatedNodes[dominatedRefIndex] = nodeOrdinal * nodeFieldCount;
        }
    },

    _markInvisibleEdges: function()
    {
        // Mark hidden edges of global objects as invisible.
        // FIXME: This is a temporary measure. Normally, we should
        // really hide all hidden nodes.
        for (var iter = this.rootNode().edges(); iter.hasNext(); iter.next()) {
            var edge = iter.edge;
            if (!edge.isShortcut())
                continue;
            var node = edge.node();
            var propNames = {};
            for (var innerIter = node.edges(); innerIter.hasNext(); innerIter.next()) {
                var globalObjEdge = innerIter.edge;
                if (globalObjEdge.isShortcut())
                    propNames[globalObjEdge._nameOrIndex()] = true;
            }
            for (innerIter.first(); innerIter.hasNext(); innerIter.next()) {
                var globalObjEdge = innerIter.edge;
                if (!globalObjEdge.isShortcut()
                    && globalObjEdge.node().isHidden()
                    && globalObjEdge._hasStringName()
                    && (globalObjEdge._nameOrIndex() in propNames))
                    this._containmentEdges[globalObjEdge._edges._start + globalObjEdge.edgeIndex + this._edgeTypeOffset] = this._edgeInvisibleType;
            }
        }
    },

    _numbersComparator: function(a, b)
    {
        return a < b ? -1 : (a > b ? 1 : 0);
    },

    _markDetachedDOMTreeNodes: function()
    {
        var flag = this._nodeFlags.detachedDOMTreeNode;
        var detachedDOMTreesRoot;
        for (var iter = this.rootNode().edges(); iter.hasNext(); iter.next()) {
            var node = iter.edge.node();
            if (node.isDetachedDOMTreesRoot()) {
                detachedDOMTreesRoot = node;
                break;
            }
        }

        if (!detachedDOMTreesRoot)
            return;

        for (var iter = detachedDOMTreesRoot.edges(); iter.hasNext(); iter.next()) {
            var node = iter.edge.node();
            if (node.isDetachedDOMTree()) {
                for (var edgesIter = node.edges(); edgesIter.hasNext(); edgesIter.next())
                    this._flags[edgesIter.edge.node().nodeIndex / this._nodeFieldCount] |= flag;
            }
        }
    },

    _markPageOwnedNodes: function()
    {
        var edgeShortcutType = this._edgeShortcutType;
        var edgeToNodeOffset = this._edgeToNodeOffset;
        var edgeTypeOffset = this._edgeTypeOffset;
        var edgeFieldsCount = this._edgeFieldsCount;
        var edgeWeakType = this._edgeWeakType;
        var firstEdgeIndexOffset = this._firstEdgeIndexOffset;
        var containmentEdges = this._containmentEdges;
        var containmentEdgesLength = containmentEdges.length;
        var nodes = this._nodes;
        var nodeFieldCount = this._nodeFieldCount;
        var nodesCount = this.nodeCount;

        var flags = this._flags;
        var flag = this._nodeFlags.pageObject;
        var visitedMarker = this._nodeFlags.visitedMarker;
        var visitedMarkerMask = this._nodeFlags.visitedMarkerMask;
        var markerAndFlag = visitedMarker | flag;

        var nodesToVisit = new Uint32Array(nodesCount);
        var nodesToVisitLength = 0;

        for (var edgeIndex = nodes[this._rootNodeIndex + firstEdgeIndexOffset], endEdgeIndex = nodes[this._rootNodeIndex + nodeFieldCount + firstEdgeIndexOffset];
             edgeIndex < endEdgeIndex;
             edgeIndex += edgeFieldsCount) {
            if (containmentEdges[edgeIndex + edgeTypeOffset] === edgeShortcutType) {
                var nodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
                nodesToVisit[nodesToVisitLength++] = nodeIndex;
                flags[nodeIndex / nodeFieldCount] |= visitedMarker;
            }
        }

        while (nodesToVisitLength) {
            var nodeIndex = nodesToVisit[--nodesToVisitLength];
            var nodeOrdinal = nodeIndex / nodeFieldCount;
            flags[nodeOrdinal] |= flag;
            flags[nodeOrdinal] &= visitedMarkerMask;
            var beginEdgeIndex = nodes[nodeIndex + firstEdgeIndexOffset];
            var endEdgeIndex = nodeOrdinal < nodesCount - 1
                ? nodes[nodeIndex + firstEdgeIndexOffset + nodeFieldCount]
                : containmentEdgesLength;
            for (var edgeIndex = beginEdgeIndex; edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
                var childNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
                var childNodeOrdinal = childNodeIndex / nodeFieldCount;
                if (flags[childNodeOrdinal] & markerAndFlag)
                    continue;
                var type = containmentEdges[edgeIndex + edgeTypeOffset];
                if (type === edgeWeakType)
                    continue;
                nodesToVisit[nodesToVisitLength++] = childNodeIndex;
                flags[childNodeOrdinal] |= visitedMarker;
            }
        }
    },

    _markQueriableHeapObjects: function()
    {
        // Allow runtime properties query for objects accessible from Window objects
        // via regular properties, and for DOM wrappers. Trying to access random objects
        // can cause a crash due to insonsistent state of internal properties of wrappers.
        var flag = this._nodeFlags.canBeQueried;
        var hiddenEdgeType = this._edgeHiddenType;
        var internalEdgeType = this._edgeInternalType;
        var invisibleEdgeType = this._edgeInvisibleType;
        var edgeToNodeOffset = this._edgeToNodeOffset;
        var edgeTypeOffset = this._edgeTypeOffset;
        var edgeFieldsCount = this._edgeFieldsCount;
        var containmentEdges = this._containmentEdges;
        var nodes = this._nodes;
        var nodeCount = this.nodeCount;
        var nodeFieldCount = this._nodeFieldCount;
        var firstEdgeIndexOffset = this._firstEdgeIndexOffset;

        var flags = this._flags;
        var list = [];

        for (var iter = this.rootNode().edges(); iter.hasNext(); iter.next()) {
            if (iter.edge.node().isWindow())
                list.push(iter.edge.node().nodeIndex);
        }

        while (list.length) {
            var nodeIndex = list.pop();
            var nodeOrdinal = nodeIndex / nodeFieldCount;
            if (flags[nodeOrdinal] & flag)
                continue;
            flags[nodeOrdinal] |= flag;
            var beginEdgeIndex = nodes[nodeIndex + firstEdgeIndexOffset];
            var endEdgeIndex = nodes[nodeIndex + firstEdgeIndexOffset + nodeFieldCount];
            for (var edgeIndex = beginEdgeIndex; edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
                var childNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
                if (flags[childNodeIndex / nodeFieldCount] & flag)
                    continue;
                var type = containmentEdges[edgeIndex + edgeTypeOffset];
                if (type === hiddenEdgeType || type === invisibleEdgeType || type === internalEdgeType)
                    continue;
                list.push(childNodeIndex);
            }
        }
    },

    _calculateFlags: function()
    {
        this._flags = new Uint32Array(this.nodeCount);
        this._markDetachedDOMTreeNodes();
        this._markQueriableHeapObjects();
        this._markPageOwnedNodes();
    },

    calculateSnapshotDiff: function(baseSnapshotId, baseSnapshotAggregates)
    {
        var snapshotDiff = this._snapshotDiffs[baseSnapshotId];
        if (snapshotDiff)
            return snapshotDiff;
        snapshotDiff = {};

        var aggregates = this.aggregates(true, "allObjects");
        for (var className in baseSnapshotAggregates) {
            var baseAggregate = baseSnapshotAggregates[className];
            var diff = this._calculateDiffForClass(baseAggregate, aggregates[className]);
            if (diff)
                snapshotDiff[className] = diff;
        }
        var emptyBaseAggregate = { ids: [], indexes: [], selfSizes: [] };
        for (var className in aggregates) {
            if (className in baseSnapshotAggregates)
                continue;
            snapshotDiff[className] = this._calculateDiffForClass(emptyBaseAggregate, aggregates[className]);
        }

        this._snapshotDiffs[baseSnapshotId] = snapshotDiff;
        return snapshotDiff;
    },

    _calculateDiffForClass: function(baseAggregate, aggregate)
    {
        var baseIds = baseAggregate.ids;
        var baseIndexes = baseAggregate.indexes;
        var baseSelfSizes = baseAggregate.selfSizes;

        var indexes = aggregate ? aggregate.idxs : [];

        var i = 0, l = baseIds.length;
        var j = 0, m = indexes.length;
        var diff = { addedCount: 0,
                     removedCount: 0,
                     addedSize: 0,
                     removedSize: 0,
                     deletedIndexes: [],
                     addedIndexes: [] };

        var nodeB = new WebInspector.HeapSnapshotNode(this, indexes[j]);
        while (i < l && j < m) {
            var nodeAId = baseIds[i];
            if (nodeAId < nodeB.id()) {
                diff.deletedIndexes.push(baseIndexes[i]);
                diff.removedCount++;
                diff.removedSize += baseSelfSizes[i];
                ++i;
            } else if (nodeAId > nodeB.id()) { // Native nodes(e.g. dom groups) may have ids less than max JS object id in the base snapshot
                diff.addedIndexes.push(indexes[j]);
                diff.addedCount++;
                diff.addedSize += nodeB.selfSize();
                nodeB.nodeIndex = indexes[++j];
            } else { // nodeAId === nodeB.id()
                ++i;
                nodeB.nodeIndex = indexes[++j];
            }
        }
        while (i < l) {
            diff.deletedIndexes.push(baseIndexes[i]);
            diff.removedCount++;
            diff.removedSize += baseSelfSizes[i];
            ++i;
        }
        while (j < m) {
            diff.addedIndexes.push(indexes[j]);
            diff.addedCount++;
            diff.addedSize += nodeB.selfSize();
            nodeB.nodeIndex = indexes[++j];
        }
        diff.countDelta = diff.addedCount - diff.removedCount;
        diff.sizeDelta = diff.addedSize - diff.removedSize;
        if (!diff.addedCount && !diff.removedCount)
            return null;
        return diff;
    },

    _nodeForSnapshotObjectId: function(snapshotObjectId)
    {
        for (var it = this._allNodes(); it.hasNext(); it.next()) {
            if (it.node.id() === snapshotObjectId)
                return it.node;
        }
        return null;
    },

    nodeClassName: function(snapshotObjectId)
    {
        var node = this._nodeForSnapshotObjectId(snapshotObjectId);
        if (node)
            return node.className();
        return null;
    },

    dominatorIdsForNode: function(snapshotObjectId)
    {
        var node = this._nodeForSnapshotObjectId(snapshotObjectId);
        if (!node)
            return null;
        var result = [];
        while (!node.isRoot()) {
            result.push(node.id());
            node.nodeIndex = node.dominatorIndex();
        }
        return result;
    },

    _parseFilter: function(filter)
    {
        if (!filter)
            return null;
        var parsedFilter = eval("(function(){return " + filter + "})()");
        return parsedFilter.bind(this);
    },

    createEdgesProvider: function(nodeIndex, filter)
    {
        var node = new WebInspector.HeapSnapshotNode(this, nodeIndex);
        return new WebInspector.HeapSnapshotEdgesProvider(this, this._parseFilter(filter), node.edges());
    },

    createRetainingEdgesProvider: function(nodeIndex, filter)
    {
        var node = new WebInspector.HeapSnapshotNode(this, nodeIndex);
        return new WebInspector.HeapSnapshotEdgesProvider(this, this._parseFilter(filter), node.retainers());
    },

    createAddedNodesProvider: function(baseSnapshotId, className)
    {
        var snapshotDiff = this._snapshotDiffs[baseSnapshotId];
        var diffForClass = snapshotDiff[className];
        return new WebInspector.HeapSnapshotNodesProvider(this, null, diffForClass.addedIndexes);
    },

    createDeletedNodesProvider: function(nodeIndexes)
    {
        return new WebInspector.HeapSnapshotNodesProvider(this, null, nodeIndexes);
    },

    createNodesProviderForClass: function(className, aggregatesKey)
    {
        function filter(node) {
            return node.isPageObject();
        }
        return new WebInspector.HeapSnapshotNodesProvider(this, filter, this.aggregates(false, aggregatesKey)[className].idxs);
    },

    createNodesProviderForDominator: function(nodeIndex)
    {
        var node = new WebInspector.HeapSnapshotNode(this, nodeIndex);
        return new WebInspector.HeapSnapshotNodesProvider(this, null, this._dominatedNodesOfNode(node));
    },

    updateStaticData: function()
    {
        return {nodeCount: this.nodeCount, rootNodeIndex: this._rootNodeIndex, totalSize: this.totalSize, uid: this.uid, nodeFlags: this._nodeFlags};
    }
};

/**
 * @constructor
 * @param {Array.<number>=} unfilteredIterationOrder
 */
WebInspector.HeapSnapshotFilteredOrderedIterator = function(iterator, filter, unfilteredIterationOrder)
{
    this._filter = filter;
    this._iterator = iterator;
    this._unfilteredIterationOrder = unfilteredIterationOrder;
    this._iterationOrder = null;
    this._position = 0;
    this._currentComparator = null;
    this._sortedPrefixLength = 0;
}

WebInspector.HeapSnapshotFilteredOrderedIterator.prototype = {
    _createIterationOrder: function()
    {
        if (this._iterationOrder)
            return;
        if (this._unfilteredIterationOrder && !this._filter) {
            this._iterationOrder = this._unfilteredIterationOrder.slice(0);
            this._unfilteredIterationOrder = null;
            return;
        }
        this._iterationOrder = [];
        var iterator = this._iterator;
        if (!this._unfilteredIterationOrder && !this._filter) {
            for (iterator.first(); iterator.hasNext(); iterator.next())
                this._iterationOrder.push(iterator.index());
        } else if (!this._unfilteredIterationOrder) {
            for (iterator.first(); iterator.hasNext(); iterator.next()) {
                if (this._filter(iterator.item()))
                    this._iterationOrder.push(iterator.index());
            }
        } else {
            var order = this._unfilteredIterationOrder.constructor === Array ?
                this._unfilteredIterationOrder : this._unfilteredIterationOrder.slice(0);
            for (var i = 0, l = order.length; i < l; ++i) {
                iterator.setIndex(order[i]);
                if (this._filter(iterator.item()))
                    this._iterationOrder.push(iterator.index());
            }
            this._unfilteredIterationOrder = null;
        }
    },

    first: function()
    {
        this._position = 0;
    },

    hasNext: function()
    {
        return this._position < this._iterationOrder.length;
    },

    isEmpty: function()
    {
        if (this._iterationOrder)
            return !this._iterationOrder.length;
        if (this._unfilteredIterationOrder && !this._filter)
            return !this._unfilteredIterationOrder.length;
        var iterator = this._iterator;
        if (!this._unfilteredIterationOrder && !this._filter) {
            iterator.first();
            return !iterator.hasNext();
        } else if (!this._unfilteredIterationOrder) {
            for (iterator.first(); iterator.hasNext(); iterator.next())
                if (this._filter(iterator.item()))
                    return false;
        } else {
            var order = this._unfilteredIterationOrder.constructor === Array ?
                this._unfilteredIterationOrder : this._unfilteredIterationOrder.slice(0);
            for (var i = 0, l = order.length; i < l; ++i) {
                iterator.setIndex(order[i]);
                if (this._filter(iterator.item()))
                    return false;
            }
        }
        return true;
    },

    item: function()
    {
        this._iterator.setIndex(this._iterationOrder[this._position]);
        return this._iterator.item();
    },

    get length()
    {
        this._createIterationOrder();
        return this._iterationOrder.length;
    },

    next: function()
    {
        ++this._position;
    },

    /**
     * @param {number} begin
     * @param {number} end
     */
    serializeItemsRange: function(begin, end)
    {
        this._createIterationOrder();
        if (begin > end)
            throw new Error("Start position > end position: " + begin + " > " + end);
        if (end >= this._iterationOrder.length)
            end = this._iterationOrder.length;
        if (this._sortedPrefixLength < end) {
            this.sort(this._currentComparator, this._sortedPrefixLength, this._iterationOrder.length - 1, end - this._sortedPrefixLength);
            this._sortedPrefixLength = end;
        }

        this._position = begin;
        var startPosition = this._position;
        var count = end - begin;
        var result = new Array(count);
        for (var i = 0 ; i < count && this.hasNext(); ++i, this.next())
            result[i] = this.serializeItem(this.item());
        result.length = i;
        result.totalLength = this._iterationOrder.length;

        result.startPosition = startPosition;
        result.endPosition = this._position;
        return result;
    },

    sortAll: function()
    {
        this._createIterationOrder();
        if (this._sortedPrefixLength === this._iterationOrder.length)
            return;
        this.sort(this._currentComparator, this._sortedPrefixLength, this._iterationOrder.length - 1, this._iterationOrder.length);
        this._sortedPrefixLength = this._iterationOrder.length;
    },

    sortAndRewind: function(comparator)
    {
        this._currentComparator = comparator;
        this._sortedPrefixLength = 0;
        this.first();
    }
}

WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator = function(fieldNames)
{
    return {fieldName1:fieldNames[0], ascending1:fieldNames[1], fieldName2:fieldNames[2], ascending2:fieldNames[3]};
}

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotFilteredOrderedIterator}
 */
WebInspector.HeapSnapshotEdgesProvider = function(snapshot, filter, edgesIter)
{
    this.snapshot = snapshot;
    WebInspector.HeapSnapshotFilteredOrderedIterator.call(this, edgesIter, filter);
}

WebInspector.HeapSnapshotEdgesProvider.prototype = {
    serializeItem: function(edge)
    {
        return {
            name: edge.name(),
            propertyAccessor: edge.toString(),
            node: WebInspector.HeapSnapshotNodesProvider.prototype.serializeItem(edge.node()),
            nodeIndex: edge.nodeIndex(),
            type: edge.type(),
            distanceToWindow: edge.node().distanceToWindow()
        };
    },

    sort: function(comparator, leftBound, rightBound, count)
    {
        var fieldName1 = comparator.fieldName1;
        var fieldName2 = comparator.fieldName2;
        var ascending1 = comparator.ascending1;
        var ascending2 = comparator.ascending2;

        var edgeA = this._iterator.item().clone();
        var edgeB = edgeA.clone();
        var nodeA = new WebInspector.HeapSnapshotNode(this.snapshot);
        var nodeB = new WebInspector.HeapSnapshotNode(this.snapshot);

        function compareEdgeFieldName(ascending, indexA, indexB)
        {
            edgeA.edgeIndex = indexA;
            edgeB.edgeIndex = indexB;
            if (edgeB.name() === "__proto__") return -1;
            if (edgeA.name() === "__proto__") return 1;
            var result =
                edgeA.hasStringName() === edgeB.hasStringName() ?
                (edgeA.name() < edgeB.name() ? -1 : (edgeA.name() > edgeB.name() ? 1 : 0)) :
                (edgeA.hasStringName() ? -1 : 1);
            return ascending ? result : -result;
        }

        function compareNodeField(fieldName, ascending, indexA, indexB)
        {
            edgeA.edgeIndex = indexA;
            nodeA.nodeIndex = edgeA.nodeIndex();
            var valueA = nodeA[fieldName];

            edgeB.edgeIndex = indexB;
            nodeB.nodeIndex = edgeB.nodeIndex();
            var valueB = nodeB[fieldName];

            var result = valueA < valueB ? -1 : (valueA > valueB ? 1 : 0);
            return ascending ? result : -result;
        }

        function compareEdgeAndNode(indexA, indexB) {
            var result = compareEdgeFieldName(ascending1, indexA, indexB);
            if (result === 0)
                result = compareNodeField(fieldName2, ascending2, indexA, indexB);
            return result;
        }

        function compareNodeAndEdge(indexA, indexB) {
            var result = compareNodeField(fieldName1, ascending1, indexA, indexB);
            if (result === 0)
                result = compareEdgeFieldName(ascending2, indexA, indexB);
            return result;
        }

        function compareNodeAndNode(indexA, indexB) {
            var result = compareNodeField(fieldName1, ascending1, indexA, indexB);
            if (result === 0)
                result = compareNodeField(fieldName2, ascending2, indexA, indexB);
            return result;
        }

        if (fieldName1 === "!edgeName")
            this._iterationOrder.sortRange(compareEdgeAndNode, leftBound, rightBound, count);
        else if (fieldName2 === "!edgeName")
            this._iterationOrder.sortRange(compareNodeAndEdge, leftBound, rightBound, count);
        else
            this._iterationOrder.sortRange(compareNodeAndNode, leftBound, rightBound, count);
    }
};

WebInspector.HeapSnapshotEdgesProvider.prototype.__proto__ = WebInspector.HeapSnapshotFilteredOrderedIterator.prototype;

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotFilteredOrderedIterator}
 * @param {Array.<number>=} nodeIndexes
 */
WebInspector.HeapSnapshotNodesProvider = function(snapshot, filter, nodeIndexes)
{
    this.snapshot = snapshot;
    WebInspector.HeapSnapshotFilteredOrderedIterator.call(this, snapshot._allNodes(), filter, nodeIndexes);
}

WebInspector.HeapSnapshotNodesProvider.prototype = {
    nodePosition: function(snapshotObjectId)
    {
        this._createIterationOrder();
        if (this.isEmpty())
            return -1;
        this.sortAll();

        var node = new WebInspector.HeapSnapshotNode(this.snapshot);
        for (var i = 0; i < this._iterationOrder.length; i++) {
            node.nodeIndex = this._iterationOrder[i];
            if (node.id() === snapshotObjectId)
                return i;
        }
        return -1;
    },

    serializeItem: function(node)
    {
        return {
            id: node.id(),
            name: node.name(),
            distanceToWindow: node.distanceToWindow(),
            nodeIndex: node.nodeIndex,
            retainedSize: node.retainedSize(),
            selfSize: node.selfSize(),
            type: node.type(),
            flags: node.flags()
        };
    },

    sort: function(comparator, leftBound, rightBound, count)
    {
        var fieldName1 = comparator.fieldName1;
        var fieldName2 = comparator.fieldName2;
        var ascending1 = comparator.ascending1;
        var ascending2 = comparator.ascending2;

        var nodeA = new WebInspector.HeapSnapshotNode(this.snapshot);
        var nodeB = new WebInspector.HeapSnapshotNode(this.snapshot);

        function sortByNodeField(fieldName, ascending)
        {
            var valueOrFunctionA = nodeA[fieldName];
            var valueA = typeof valueOrFunctionA !== "function" ? valueOrFunctionA : valueOrFunctionA.call(nodeA);
            var valueOrFunctionB = nodeB[fieldName];
            var valueB = typeof valueOrFunctionB !== "function" ? valueOrFunctionB : valueOrFunctionB.call(nodeB);
            var result = valueA < valueB ? -1 : (valueA > valueB ? 1 : 0);
            return ascending ? result : -result;
        }

        function sortByComparator(indexA, indexB) {
            nodeA.nodeIndex = indexA;
            nodeB.nodeIndex = indexB;
            var result = sortByNodeField(fieldName1, ascending1);
            if (result === 0)
                result = sortByNodeField(fieldName2, ascending2);
            return result;
        }

        this._iterationOrder.sortRange(sortByComparator, leftBound, rightBound, count);
    }
};

WebInspector.HeapSnapshotNodesProvider.prototype.__proto__ = WebInspector.HeapSnapshotFilteredOrderedIterator.prototype;
