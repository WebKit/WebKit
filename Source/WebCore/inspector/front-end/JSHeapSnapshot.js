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
 * @extends {WebInspector.HeapSnapshot}
 */
WebInspector.JSHeapSnapshot = function(profile)
{
    WebInspector.HeapSnapshot.call(this, profile);
}

WebInspector.JSHeapSnapshot.prototype = {
    createNode: function(nodeIndex)
    {
        return new WebInspector.JSHeapSnapshotNode(this, nodeIndex);
    },

    createEdge: function(edges, edgeIndex)
    {
        return new WebInspector.JSHeapSnapshotEdge(this, edges, edgeIndex);
    },

    createRetainingEdge: function(retainedNodeIndex, retainerIndex)
    {
        return new WebInspector.JSHeapSnapshotRetainerEdge(this, retainedNodeIndex, retainerIndex);
    },

    __proto__: WebInspector.HeapSnapshot.prototype
};

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotNode}
 * @param {WebInspector.JSHeapSnapshot} snapshot
 * @param {number=} nodeIndex
 */
WebInspector.JSHeapSnapshotNode = function(snapshot, nodeIndex)
{
    WebInspector.HeapSnapshotNode.call(this, snapshot, nodeIndex)
}

WebInspector.JSHeapSnapshotNode.prototype = {
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

    __proto__: WebInspector.HeapSnapshotNode.prototype
};

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotEdge}
 * @param {WebInspector.JSHeapSnapshot} snapshot
 * @param {Array.<number>} edges
 * @param {number=} edgeIndex
 */
WebInspector.JSHeapSnapshotEdge = function(snapshot, edges, edgeIndex)
{
    WebInspector.HeapSnapshotEdge.call(this, snapshot, edges, edgeIndex);
}

WebInspector.JSHeapSnapshotEdge.prototype = {
    clone: function()
    {
        return new WebInspector.JSHeapSnapshotEdge(this._snapshot, this._edges, this.edgeIndex);
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
    },

    __proto__: WebInspector.HeapSnapshotEdge.prototype
};


/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotRetainerEdge}
 * @param {WebInspector.JSHeapSnapshot} snapshot
 */
WebInspector.JSHeapSnapshotRetainerEdge = function(snapshot, retainedNodeIndex, retainerIndex)
{
    WebInspector.HeapSnapshotRetainerEdge.call(this, snapshot, retainedNodeIndex, retainerIndex);
}

WebInspector.JSHeapSnapshotRetainerEdge.prototype = {
    clone: function()
    {
        return new WebInspector.JSHeapSnapshotRetainerEdge(this._snapshot, this._retainedNodeIndex, this.retainerIndex());
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

    __proto__: WebInspector.HeapSnapshotRetainerEdge.prototype
}

