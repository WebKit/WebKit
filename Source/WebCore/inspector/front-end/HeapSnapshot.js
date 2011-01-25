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

WebInspector.HeapSnapshotEdgesIterator = function(snapshot, edges)
{
    this._snapshot = snapshot;
    this._edges = edges;
    this._edgeIndex = 0;
}

WebInspector.HeapSnapshotEdgesIterator.prototype = {
    get done()
    {
        return this._edgeIndex >= this._edges.length;
    },

    get isElement()
    {
        return this._type() === this._snapshot._edgeElementType;
    },

    get isHidden()
    {
        return this._type() === this._snapshot._edgeHiddenType;
    },

    get name()
    {
        return this.isElement || this.isHidden ? this._nameOrIndex() : this._snapshot._strings[this._nameOrIndex()];
    },

    next: function()
    {
        this._edgeIndex += this._snapshot._edgeFieldsCount;
    },

    get node()
    {
        return new WebInspector.HeapSnapshotNodeWrapper(this._snapshot, this.nodeIndex);
    },

    get nodeIndex()
    {
        return this._edges[this._edgeIndex + this._snapshot._edgeToNodeOffset];
    },

    _nameOrIndex: function()
    {
        return this._edges[this._edgeIndex + this._snapshot._edgeNameOffset];
    },

    _type: function()
    {
        return this._edges[this._edgeIndex + this._snapshot._edgeTypeOffset];
    }
};

WebInspector.HeapSnapshotNodeWrapper = function(snapshot, nodeIndex)
{
    this._snapshot = snapshot;
    this._nodes = snapshot._nodes;
    this._nodeIndex = nodeIndex;
}

WebInspector.HeapSnapshotNodeWrapper.prototype = {
    get edges()
    {
        return new WebInspector.HeapSnapshotEdgesIterator(this._snapshot, this._edges());
    },

    get edgesCount()
    {
        return this._nodes[this._nodeIndex + this._snapshot._edgesCountOffset];
    },

    get instancesCount()
    {
        return this._nodes[this._nodeIndex + this._snapshot._nodeInstancesCountOffset];
    },

    get isHidden()
    {
        return this._type() === this._snapshot._nodeHiddenType;
    },

    get name()
    {
        return this._snapshot._strings[this._name()];
    },

    get selfSize()
    {
        return this._nodes[this._nodeIndex + this._snapshot._nodeSelfSizeOffset]; 
    },

    _name: function()
    {
        return this._nodes[this._nodeIndex + this._snapshot._nodeNameOffset]; 
    },

    _edges: function()
    {
        var firstEdgeIndex = this._nodeIndex + this._snapshot._firstEdgeOffset;
        return this._nodes.slice(firstEdgeIndex, firstEdgeIndex + this.edgesCount * this._snapshot._edgeFieldsCount);
    },

    _type: function()
    {
        return this._nodes[this._nodeIndex + this._snapshot._nodeTypeOffset];
    }
};

WebInspector.HeapSnapshot = function(profile)
{
    this._profile = profile;
    this._nodes = profile.nodes;
    this._strings = profile.strings;

    this._init();
}

WebInspector.HeapSnapshot.prototype = {
    _init: function()
    {
        this._metaNodeIndex = 0;
        this._rootNodeIndex = 1;
        var meta = this._nodes[this._metaNodeIndex];
        this._nodeTypeOffset = meta.fields.indexOf("type");
        this._nodeNameOffset = meta.fields.indexOf("name");
        this._nodeIdOffset = meta.fields.indexOf("id");
        this._nodeInstancesCountOffset = this._nodeIdOffset;
        this._nodeSelfSizeOffset = meta.fields.indexOf("self_size");
        this._edgesCountOffset = meta.fields.indexOf("children_count");
        this._firstEdgeOffset = meta.fields.indexOf("children");
        this._nodeTypes = meta.types[this._nodeTypeOffset];
        this._nodeHiddenType = this._nodeTypes.indexOf("hidden");
        var edgesMeta = meta.types[this._firstEdgeOffset];
        this._edgeFieldsCount = edgesMeta.fields.length;
        this._edgeTypeOffset = edgesMeta.fields.indexOf("type");
        this._edgeNameOffset = edgesMeta.fields.indexOf("name_or_index");
        this._edgeToNodeOffset = edgesMeta.fields.indexOf("to_node");
        this._edgeTypes = edgesMeta.types[this._edgeTypeOffset];
        this._edgeElementType = this._edgeTypes.indexOf("element");
        this._edgeHiddenType = this._edgeTypes.indexOf("hidden");
    },

    get rootEdges()
    {
        return (new WebInspector.HeapSnapshotNodeWrapper(this, this._rootNodeIndex)).edges;
    }
};
