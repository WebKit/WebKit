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


/**
 * @constructor
 */
WebInspector.NativeHeapGraph = function(rawGraph)
{
    this._rawGraph = rawGraph;

    this._nodeFieldCount = 5;
    this._nodeTypeOffset = 0;
    this._nodeSizeOffset = 1;
    this._nodeClassNameOffset = 2;
    this._nodeNameOffset = 3;
    this._nodeEdgeCountOffset = 4;
    this._nodeFirstEdgeOffset = this._nodeEdgeCountOffset;

    this._edgeFieldCount = 3;
    this._edgeTypeOffset = 0;
    this._edgeTargetOffset = 1;
    this._edgeNameOffset = 2;

    this._nodeCount = rawGraph.nodes.length / this._nodeFieldCount;
    this._nodes = rawGraph.nodes;
    this._edges = rawGraph.edges;
    this._strings = rawGraph.strings;

    this._calculateNodeEdgeIndexes();
}

WebInspector.NativeHeapGraph.prototype = {
    rootNodes: function()
    {
        var nodeHasIncomingEdges = new Uint8Array(this._nodeCount);
        var edges = this._edges;
        var edgesLength = edges.length;
        var edgeFieldCount = this._edgeFieldCount;
        var nodeFieldCount = this._nodeFieldCount;
        for (var i = this._edgeTargetOffset; i < edgesLength; i += edgeFieldCount) {
            var targetIndex = edges[i];
            nodeHasIncomingEdges[targetIndex] = 1;
        }
        var roots = [];
        var nodeCount = nodeHasIncomingEdges.length;
        for (var i = 0; i < nodeCount; i++) {
            if (!nodeHasIncomingEdges[i])
                roots.push(new WebInspector.NativeHeapGraph.Node(this, i * nodeFieldCount));
        }
        return roots;
    },

    _calculateNodeEdgeIndexes: function()
    {
        var nodes = this._nodes;
        var nodeFieldCount = this._nodeFieldCount;
        var nodeLength = nodes.length;
        var firstEdgeIndex = 0;
        for (var i = this._nodeEdgeCountOffset; i < nodeLength; i += nodeFieldCount) {
            var count = nodes[i];
            nodes[i] = firstEdgeIndex;
            firstEdgeIndex += count;
        }
        this._addDummyNode();
    },

    _addDummyNode: function()
    {
        var firstEdgePosition = this._nodes.length + this._nodeFirstEdgeOffset;
        for (var i = 0; i < this._nodeFieldCount; i++)
            this._nodes.push(0);
        this._nodes[firstEdgePosition] = this._edges.length;
    }
}


/**
 * @constructor
 * @param {WebInspector.NativeHeapGraph} graph
 * @param {number} position
 */
WebInspector.NativeHeapGraph.Edge = function(graph, position)
{
    this._graph = graph;
    this._position = position;
}

WebInspector.NativeHeapGraph.Edge.prototype = {
    type: function()
    {
        return this._getStringField(this._graph._edgeTypeOffset);
    },

    name: function()
    {
        return this._getStringField(this._graph._edgeNameOffset);
    },

    target: function()
    {
        var edges = this._graph._edges;
        var targetPosition = edges[this._position + this._graph._edgeTargetOffset] * this._graph._nodeFieldCount;
        return new WebInspector.NativeHeapGraph.Node(this._graph, targetPosition);
    },

    _getStringField: function(offset)
    {
        var typeIndex = this._graph._edges[this._position + offset];
        return this._graph._rawGraph.strings[typeIndex];
    },

    toString: function()
    {
        return "Edge#" + this._position + " -" + this.name() + "-> " + this.target();
    }

}


/**
 * @constructor
 * @param {WebInspector.NativeHeapGraph} graph
 * @param {number} position
 */
WebInspector.NativeHeapGraph.Node = function(graph, position)
{
    this._graph = graph;
    this._position = position;
}

WebInspector.NativeHeapGraph.Node.prototype = {
    id: function()
    {
        return this._position / this._graph._nodeFieldCount;
    },

    type: function()
    {
        return this._getStringField(this._graph._nodeTypeOffset);
    },

    size: function()
    {
        return this._graph._nodes[this._position + this._graph._nodeSizeOffset];
    },

    className: function()
    {
        return this._getStringField(this._graph._nodeClassNameOffset);
    },

    name: function()
    {
        return this._getStringField(this._graph._nodeNameOffset);
    },

    hasReferencedNodes: function()
    {
        return this._afterLastEdgePosition() > this._firstEdgePoistion();
    },

    referencedNodes: function()
    {
        var edges = this._graph._edges;
        var nodes = this._graph._nodes;
        var edgeFieldCount = this._graph._edgeFieldCount;
        var nodeFieldCount = this._graph._nodeFieldCount;

        var firstEdgePosition = this._firstEdgePoistion();
        var afterLastEdgePosition = this._afterLastEdgePosition();
        var result = [];
        for (var i = firstEdgePosition + this._graph._edgeTargetOffset; i < afterLastEdgePosition; i += edgeFieldCount)
            result.push(new WebInspector.NativeHeapGraph.Node(this._graph, edges[i] * nodeFieldCount));
        return result;
    },

    outgoingEdges: function()
    {
        var edges = this._graph._edges;
        var edgeFieldCount = this._graph._edgeFieldCount;

        var firstEdgePosition = this._firstEdgePoistion();
        var afterLastEdgePosition = this._afterLastEdgePosition();
        var result = [];
        for (var i = firstEdgePosition; i < afterLastEdgePosition; i += edgeFieldCount)
            result.push(new WebInspector.NativeHeapGraph.Edge(this._graph, i));
        return result;
    },

    targetOfEdge: function(edgeName)
    {
        return this.targetsOfAllEdges(edgeName)[0];
    },

    targetsOfAllEdges: function(edgeName)
    {
        var edges = this._graph._edges;
        var edgeFieldCount = this._graph._edgeFieldCount;

        var firstEdgePosition = this._firstEdgePoistion();
        var afterLastEdgePosition = this._afterLastEdgePosition();

        var edge = new WebInspector.NativeHeapGraph.Edge(this._graph, firstEdgePosition)
        var result = [];
        for (var i = firstEdgePosition; i < afterLastEdgePosition; i += edgeFieldCount) {
            edge._position = i;
            if (edge.name() === edgeName)
                result.push(edge.target());
        }
        return result;
    },

    _firstEdgePoistion: function()
    {
        return this._graph._nodes[this._position + this._graph._nodeFirstEdgeOffset] * this._graph._edgeFieldCount;
    },

    _afterLastEdgePosition: function()
    {
        return this._graph._nodes[this._position + this._graph._nodeFieldCount + this._graph._nodeFirstEdgeOffset] * this._graph._edgeFieldCount;
    },

    _getStringField: function(offset)
    {
        var typeIndex = this._graph._nodes[this._position + offset];
        return this._graph._rawGraph.strings[typeIndex];
    },

    toString: function()
    {
        return "Node#" + this.id() + " " + this.name() + "(" + this.className() + ")";
    }
}
