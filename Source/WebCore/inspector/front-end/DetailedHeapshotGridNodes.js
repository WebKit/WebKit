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

WebInspector.HeapSnapshotGridNode = function(tree, hasChildren, populateCount)
{
    WebInspector.DataGridNode.call(this, null, hasChildren);
    this._defaultPopulateCount = populateCount;
    this._provider = null;
    this.addEventListener("populate", this._populate, this);
}

WebInspector.HeapSnapshotGridNode.prototype = {
    createCell: function(columnIdentifier)
    {
        var cell = WebInspector.DataGridNode.prototype.createCell.call(this, columnIdentifier);
        if (this._searchMatched)
            cell.addStyleClass("highlight");
        return cell;
    },

    _populate: function(event)
    {
        WebInspector.PleaseWaitMessage.prototype.startAction(this.dataGrid.element, doPopulate.bind(this));

        function doPopulate()
        {
            this._provider.sort(this.comparator());
            this._provider.first();
            this.populateChildren();
            this.removeEventListener("populate", this._populate, this);
        }
    },

    populateChildren: function(provider, howMany, atIndex)
    {
        if (!howMany && provider) {
            howMany = provider.instancesCount;
            provider.resetInstancesCount();
        }
        provider = provider || this._provider;
        howMany = howMany || this._defaultPopulateCount;
        atIndex = atIndex || this.children.length;
        var haveSavedChildren = !!this._savedChildren;
        if (haveSavedChildren) {
            haveSavedChildren = false;
            for (var c in this._savedChildren) {
                haveSavedChildren = true;
                break;
            }
        }
        for ( ; howMany > 0 && provider.hasNext(); provider.next(), provider.incInstancesCount(), --howMany) {
            var item = provider.item;
            if (haveSavedChildren) {
                var hash = this._childHashForEntity(item);
                if (hash in this._savedChildren) {
                    this.insertChild(this._savedChildren[hash], atIndex++);
                    continue;
                }
            }
            this.insertChild(this._createChildNode(provider), atIndex++);
        }
        if (provider.hasNext())
            this.insertChild(new WebInspector.ShowMoreDataGridNode(this.populateChildren.bind(this, provider), this._defaultPopulateCount, provider.length), atIndex++);
    },

    _saveChildren: function()
    {
        this._savedChildren = {};
        for (var i = 0, childrenCount = this.children.length; i < childrenCount; ++i) {
            var child = this.children[i];
            if (child.expanded)
                this._savedChildren[this._childHashForNode(child)] = child;
        }
    },

    sort: function()
    {
        var comparator = this.comparator();
        WebInspector.PleaseWaitMessage.prototype.startAction(this.dataGrid.element, doSort.bind(this));

        function doSort()
        {
            if (!this._provider.sort(comparator))
                return;
            this._saveChildren();
            this.removeChildren();
            this._provider.first();
            this.populateChildren(this._provider);
            for (var i = 0, l = this.children.length; i < l; ++i) {
                var child = this.children[i];
                if (child.expanded)
                    child.sort();
            }
        }
    }
};

WebInspector.HeapSnapshotGridNode.prototype.__proto__ = WebInspector.DataGridNode.prototype;

WebInspector.HeapSnapshotGenericObjectNode = function(tree, node, hasChildren, populateCount)
{
    WebInspector.HeapSnapshotGridNode.call(this, tree, hasChildren, populateCount);
    this._name = node.name;
    this._type = node.type;
    this._shallowSize = node.selfSize;
    this._retainedSize = node.retainedSize;
    this._retainedSizeExact = this._shallowSize === this._retainedSize;
    this.snapshotNodeId = node.id;
    this.snapshotNodeIndex = node.nodeIndex;
};

WebInspector.HeapSnapshotGenericObjectNode.prototype = {
    createCell: function(columnIdentifier)
    {
        var cell = columnIdentifier !== "object" ? WebInspector.DataGridNode.prototype.createCell.call(this, columnIdentifier) : this._createObjectCell();
        if (this._searchMatched)
            cell.addStyleClass("highlight");
        return cell;
    },

    _createObjectCell: function()
    {
        var cell = document.createElement("td");
        cell.className = "object-column";
        var div = document.createElement("div");
        div.className = "source-code event-properties";
        div.style.overflow = "hidden";
        var data = this.data["object"];
        if (this._prefixObjectCell)
            this._prefixObjectCell(div, data);
        var valueSpan = document.createElement("span");
        valueSpan.className = "value console-formatted-" + data.valueStyle;
        valueSpan.textContent = data.value;
        div.appendChild(valueSpan);
        cell.appendChild(div);
        cell.addStyleClass("disclosure");
        if (this.depth)
            cell.style.setProperty("padding-left", (this.depth * this.dataGrid.indentWidth) + "px");
        return cell;
    },

    get _countPercent()
    {
        return this._count / this.tree.snapshot.nodesCount * 100.0;
    },

    get data()
    {
        var data = this._emptyData();

        var value = this._name;
        var valueStyle = "object";
        switch (this._type) {
        case "string":
            value = "\"" + value + "\"";
            valueStyle = "string";
            break;
        case "regexp":
            value = "/" + value + "/";
            valueStyle = "string";
            break;
        case "closure":
            value = "function " + value + "()";
            valueStyle = "function";
            break;
        case "number":
            valueStyle = "number";
            break;
        case "hidden":
            valueStyle = "null";
            break;
        case "array":
            value += "[]";
            break;
        };
        data["object"] = { valueStyle: valueStyle, value: value + " @" + this.snapshotNodeId };

        var view = this.dataGrid.snapshotView;
        data["shallowSize"] = view.showShallowSizeAsPercent ? WebInspector.UIString("%.2f%%", this._shallowSizePercent) : Number.bytesToString(this._shallowSize);
        data["retainedSize"] = (this._retainedSizeExact ? "" : "\u2248") + (view.showRetainedSizeAsPercent ? WebInspector.UIString("%.2f%%", this._retainedSizePercent) : Number.bytesToString(this._retainedSize));

        return this._enhanceData ? this._enhanceData(data) : data;
    },

    set exactRetainedSize(size)
    {
        this._retainedSize = size;
        this._retainedSizeExact = true;
        this.refresh();
    },

    get _retainedSizePercent()
    {
        return this._retainedSize / this.dataGrid.snapshot.totalSize * 100.0;
    },

    get _shallowSizePercent()
    {
        return this._shallowSize / this.dataGrid.snapshot.totalSize * 100.0;
    }
}

WebInspector.HeapSnapshotGenericObjectNode.prototype.__proto__ = WebInspector.HeapSnapshotGridNode.prototype;

WebInspector.HeapSnapshotObjectNode = function(tree, edge)
{
    var node = edge.node;
    var provider = this._createProvider(tree.snapshot, node.rawEdges);
    WebInspector.HeapSnapshotGenericObjectNode.call(this, tree, node, !provider.isEmpty, 100);
    this._referenceName = edge.name;
    this._referenceType = edge.type;
    this._provider = provider;
}

WebInspector.HeapSnapshotObjectNode.prototype = {
    _createChildNode: function(provider)
    {
        return new WebInspector.HeapSnapshotObjectNode(this.dataGrid, provider.item);
    },

    _createProvider: function(snapshot, rawEdges)
    {
        var showHiddenData = WebInspector.DetailedHeapshotView.prototype.showHiddenData;
        return new WebInspector.HeapSnapshotEdgesProvider(
            snapshot,
            rawEdges,
            function(edge) {
                return !edge.isInvisible
                    && (showHiddenData || (!edge.isHidden && !edge.node.isHidden));
            });
    },

    _childHashForEntity: function(edge)
    {
        return edge.type + "#" + edge.name;
    },

    _childHashForNode: function(childNode)
    {
        return childNode._referenceType + "#" + childNode._referenceName;
    },

    comparator: function()
    {
        var sortAscending = this.dataGrid.sortOrder === "ascending";
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;
        var sortFields = {
            object: ["!edgeName", sortAscending, "retainedSize", false],
            count: ["!edgeName", true, "retainedSize", false],
            shallowSize: ["selfSize", sortAscending, "!edgeName", true],
            retainedSize: ["retainedSize", sortAscending, "!edgeName", true]
        }[sortColumnIdentifier] || ["!edgeName", true, "retainedSize", false];
        return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);
    },

    _emptyData: function()
    {
        return {count:"", addedCount: "", removedCount: "", countDelta:"", addedSize: "", removedSize: "", sizeDelta: ""};
    },

    _enhanceData: function(data)
    {
        var name = this._referenceName;
        if (name === "") name = "(empty)";
        var nameClass = "name";
        switch (this._referenceType) {
        case "context":
            nameClass = "console-formatted-number";
            break;
        case "internal":
        case "hidden":
            nameClass = "console-formatted-null";
            break;
        }
        data["object"].nameClass = nameClass;
        data["object"].name = name;
        return data;
    },

    _prefixObjectCell: function(div, data)
    {
        var nameSpan = document.createElement("span");
        nameSpan.className = data.nameClass;
        nameSpan.textContent = data.name;
        var separatorSpan = document.createElement("span");
        separatorSpan.className = "separator";
        separatorSpan.textContent = ": ";
        div.appendChild(nameSpan);
        div.appendChild(separatorSpan);
    }
}

WebInspector.HeapSnapshotObjectNode.prototype.__proto__ = WebInspector.HeapSnapshotGenericObjectNode.prototype;

WebInspector.HeapSnapshotInstanceNode = function(tree, baseSnapshot, snapshot, node)
{
    var provider = this._createProvider(baseSnapshot || snapshot, node.rawEdges);  
    WebInspector.HeapSnapshotGenericObjectNode.call(this, tree, node, !provider.isEmpty, 100);
    this._isDeletedNode = !!baseSnapshot;
    this._provider = provider;    
};

WebInspector.HeapSnapshotInstanceNode.prototype = {
    _createChildNode: function(provider)
    {
        return new WebInspector.HeapSnapshotObjectNode(this.dataGrid, provider.item);
    },

    _createProvider: function(snapshot, rawEdges)
    {
        var showHiddenData = WebInspector.DetailedHeapshotView.prototype.showHiddenData;
        return new WebInspector.HeapSnapshotEdgesProvider(
            snapshot,
            rawEdges,
            function(edge) {
                return !edge.isInvisible
                    && (showHiddenData || (!edge.isHidden && !edge.node.isHidden));
            });
    },

    _childHashForEntity: function(edge)
    {
        return edge.type + "#" + edge.name;
    },

    _childHashForNode: function(childNode)
    {
        return childNode._referenceType + "#" + childNode._referenceName;
    },

    comparator: function()
    {
        var sortAscending = this.dataGrid.sortOrder === "ascending";
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;
        var sortFields = {
            object: ["!edgeName", sortAscending, "retainedSize", false],
            count: ["!edgeName", true, "retainedSize", false],
            addedSize: ["selfSize", sortAscending, "!edgeName", true],
            removedSize: ["selfSize", sortAscending, "!edgeName", true],
            shallowSize: ["selfSize", sortAscending, "!edgeName", true],
            retainedSize: ["retainedSize", sortAscending, "!edgeName", true]
        }[sortColumnIdentifier] || ["!edgeName", true, "retainedSize", false];
        return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);
    },

    _emptyData: function()
    {
        return {count:"", countDelta:"", sizeDelta: ""};
    },

    _enhanceData: function(data)
    {
        if (this._isDeletedNode) {
            data["addedCount"] = "";
            data["addedSize"] = "";
            data["removedCount"] = "\u2022";
            data["removedSize"] = Number.bytesToString(this._shallowSize);
        } else {
            data["addedCount"] = "\u2022";
            data["addedSize"] = Number.bytesToString(this._shallowSize);
            data["removedCount"] = "";
            data["removedSize"] = "";
        }
        return data;
    }
}

WebInspector.HeapSnapshotInstanceNode.prototype.__proto__ = WebInspector.HeapSnapshotGenericObjectNode.prototype;

WebInspector.HeapSnapshotConstructorNode = function(tree, className, aggregate)
{
    WebInspector.HeapSnapshotGridNode.call(this, tree, aggregate.count > 0, 100);
    this._name = className;
    this._count = aggregate.count;
    this._shallowSize = aggregate.self;
    this._retainedSize = aggregate.maxRet;
    this._provider = this._createNodesProvider(tree.snapshot, aggregate.type, className);
}

WebInspector.HeapSnapshotConstructorNode.prototype = {
    _createChildNode: function(provider)
    {
        return new WebInspector.HeapSnapshotInstanceNode(this.dataGrid, null, this.dataGrid.snapshot, provider.item);
    },

    _createNodesProvider: function(snapshot, nodeType, nodeClassName)
    {
        return new WebInspector.HeapSnapshotNodesProvider(
            snapshot,
            snapshot.allNodes,
            function (node) {
                 return node.type === nodeType
                    && (nodeClassName === null || node.className === nodeClassName);
            });
    },

    comparator: function()
    {
        var sortAscending = this.dataGrid.sortOrder === "ascending";
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;
        var sortFields = {
            object: ["id", sortAscending, "retainedSize", false],
            count: ["id", true, "retainedSize", false],
            shallowSize: ["selfSize", sortAscending, "id", true],
            retainedSize: ["retainedSize", sortAscending, "id", true]
        }[sortColumnIdentifier];
        return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);
    },

    _childHashForEntity: function(node)
    {
        return node.id;
    },

    _childHashForNode: function(childNode)
    {
        return childNode.snapshotNodeId;
    },

    get data()
    {
        var data = {object: this._name, count: this._count};
        var view = this.dataGrid.snapshotView;
        data["count"] = view.showCountAsPercent ? WebInspector.UIString("%.2f%%", this._countPercent) : this._count;
        data["shallowSize"] = view.showShallowSizeAsPercent ? WebInspector.UIString("%.2f%%", this._shallowSizePercent) : Number.bytesToString(this._shallowSize);
        data["retainedSize"] = "> " + (view.showRetainedSizeAsPercent ? WebInspector.UIString("%.2f%%", this._retainedSizePercent) : Number.bytesToString(this._retainedSize));
        return data;
    },

    get _countPercent()
    {
        return this._count / this.dataGrid.snapshot.nodesCount * 100.0;
    },

    get _retainedSizePercent()
    {
        return this._retainedSize / this.dataGrid.snapshot.totalSize * 100.0;
    },

    get _shallowSizePercent()
    {
        return this._shallowSize / this.dataGrid.snapshot.totalSize * 100.0;
    }
};

WebInspector.HeapSnapshotConstructorNode.prototype.__proto__ = WebInspector.HeapSnapshotGridNode.prototype;

WebInspector.HeapSnapshotIteratorsTuple = function(it1, it2)
{
    this._it1 = it1;
    this._it2 = it2;
}

WebInspector.HeapSnapshotIteratorsTuple.prototype = {
    first: function()
    {
        this._it1.first();
        this._it2.first();
    },

    resetInstancesCount: function()
    {
        this._it1.resetInstancesCount();
        this._it2.resetInstancesCount();
    },

    sort: function(comparator)
    {
        this._it1.sort(comparator);
        this._it2.sort(comparator);
    }
};

WebInspector.HeapSnapshotDiffNode = function(tree, className, baseAggregate, aggregate)
{
    if (!baseAggregate)
        baseAggregate = { count: 0, self: 0, maxRet: 0, type:aggregate.type, name:aggregate.name, idxs: [] };
    if (!aggregate)
        aggregate = { count: 0, self: 0, maxRet: 0, type:baseAggregate.type, name:baseAggregate.name, idxs: [] };
    WebInspector.HeapSnapshotGridNode.call(this, tree, true, 50);
    this._name = className;
    this._calculateDiff(tree.baseSnapshot, tree.snapshot, baseAggregate.idxs, aggregate.idxs);
    this._provider = this._createNodesProvider(tree.baseSnapshot, tree.snapshot, aggregate.type, className);
}

WebInspector.HeapSnapshotDiffNode.prototype = {
    _calculateDiff: function(baseSnapshot, snapshot, baseIndexes, currentIndexes)
    {
        var i = 0, l = baseIndexes.length;
        var j = 0, m = currentIndexes.length;
        this._addedCount = 0;
        this._removedCount = 0;
        this._addedSize = 0;
        this._removedSize = 0;
        var nodeA = new WebInspector.HeapSnapshotNode(baseSnapshot);
        var nodeB = new WebInspector.HeapSnapshotNode(snapshot);
        nodeA.nodeIndex = baseIndexes[i];
        nodeB.nodeIndex = currentIndexes[j];
        while (i < l && j < m) {
            if (nodeA.id < nodeB.id) {
                this._removedCount++;
                this._removedSize += nodeA.selfSize;
                nodeA.nodeIndex = baseIndexes[++i];
            } else if (nodeA.id > nodeB.id) {
                this._addedCount++;
                this._addedSize += nodeB.selfSize;
                nodeB.nodeIndex = currentIndexes[++j];                
            } else {
                nodeA.nodeIndex = baseIndexes[++i];
                nodeB.nodeIndex = currentIndexes[++j];                
            }
        }
        while (i < l) {
            this._removedCount++;
            this._removedSize += nodeA.selfSize;
            nodeA.nodeIndex = baseIndexes[++i];
        }
        while (j < m) {
            this._addedCount++;
            this._addedSize += nodeB.selfSize;
            nodeB.nodeIndex = currentIndexes[++j];                
        }
        this._countDelta = this._addedCount - this._removedCount;
        this._sizeDelta = this._addedSize - this._removedSize;
    },

    _createChildNode: function(provider)
    {
        if (provider === this._provider._it1)
            return new WebInspector.HeapSnapshotInstanceNode(this.dataGrid, null, provider.snapshot, provider.item);
        else
            return new WebInspector.HeapSnapshotInstanceNode(this.dataGrid, provider.snapshot, null, provider.item);
    },

    _createNodesProvider: function(baseSnapshot, snapshot, nodeType, nodeClassName)
    {
        return new WebInspector.HeapSnapshotIteratorsTuple(
            createProvider(snapshot, baseSnapshot), createProvider(baseSnapshot, snapshot));

        function createProvider(snapshot, otherSnapshot)
        {
            return new WebInspector.HeapSnapshotNodesProvider(
                snapshot,
                snapshot.allNodes,
                function (node) {
                     return node.type === nodeType
                         && (nodeClassName === null || node.className === nodeClassName)
                         && !(node.id in otherSnapshot.idsMap);
                });
        }
    },

    comparator: function()
    {
        var sortAscending = this.dataGrid.sortOrder === "ascending";
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;
        var sortFields = {
            object: ["id", sortAscending, "selfSize", false],
            addedCount: ["selfSize", sortAscending, "id", true],
            removedCount: ["selfSize", sortAscending, "id", true],
            countDelta: ["selfSize", sortAscending, "id", true],
            addedSize: ["selfSize", sortAscending, "id", true],
            removedSize: ["selfSize", sortAscending, "id", true],
            sizeDelta: ["selfSize", sortAscending, "id", true]
        }[sortColumnIdentifier];
        return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);
    },

    populateChildren: function(provider, howMany, atIndex)
    {
        if (!provider && !howMany) {
            WebInspector.HeapSnapshotGridNode.prototype.populateChildren.call(this, this._provider._it1, this._defaultPopulateCount);
            WebInspector.HeapSnapshotGridNode.prototype.populateChildren.call(this, this._provider._it2, this._defaultPopulateCount);
        } else if (!howMany) {
            WebInspector.HeapSnapshotGridNode.prototype.populateChildren.call(this, this._provider._it1);
            WebInspector.HeapSnapshotGridNode.prototype.populateChildren.call(this, this._provider._it2);
        } else
            WebInspector.HeapSnapshotGridNode.prototype.populateChildren.call(this, provider, howMany, atIndex);
    },

    _signForDelta: function(delta)
    {
        if (delta === 0)
            return "";
        if (delta > 0)
            return "+";
        else
            return "\u2212";  // Math minus sign, same width as plus.
    },

    get data()
    {
        var data = {object: this._name};

        data["addedCount"] = this._addedCount;
        data["removedCount"] = this._removedCount;
        data["countDelta"] = WebInspector.UIString("%s%d", this._signForDelta(this._countDelta), Math.abs(this._countDelta));
        data["addedSize"] = Number.bytesToString(this._addedSize);
        data["removedSize"] = Number.bytesToString(this._removedSize);
        data["sizeDelta"] = WebInspector.UIString("%s%s", this._signForDelta(this._sizeDelta), Number.bytesToString(Math.abs(this._sizeDelta)));

        return data;
    },

    get zeroDiff()
    {
        return this._addedCount === 0 && this._removedCount === 0;
    }
};

WebInspector.HeapSnapshotDiffNode.prototype.__proto__ = WebInspector.HeapSnapshotGridNode.prototype;

WebInspector.HeapSnapshotDominatorObjectNode = function(tree, node)
{
    var provider = this._createProvider(tree.snapshot, node.nodeIndex);
    WebInspector.HeapSnapshotGenericObjectNode.call(this, tree, node, !provider.isEmpty, 25);
    this._provider = provider;
};

WebInspector.HeapSnapshotDominatorObjectNode.prototype = {
    _createChildNode: function(provider)
    {
        return new WebInspector.HeapSnapshotDominatorObjectNode(this.dataGrid, provider.item);
    },

    _createProvider: function(snapshot, nodeIndex)
    {
        var showHiddenData = WebInspector.DetailedHeapshotView.prototype.showHiddenData;
        return new WebInspector.HeapSnapshotNodesProvider(
            snapshot,
            snapshot.allNodes,
            function (node) {
                 var dominatorIndex = node.dominatorIndex();
                 return dominatorIndex === nodeIndex
                     && dominatorIndex !== node.nodeIndex
                     && (showHiddenData || !node.isHidden);
            });
    },

    _childHashForEntity: function(node)
    {
        return node.id;
    },

    _childHashForNode: function(childNode)
    {
        return childNode.snapshotNodeId;
    },

    comparator: function()
    {
        var sortAscending = this.dataGrid.sortOrder === "ascending";
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;
        var sortFields = {
            object: ["id", sortAscending, "retainedSize", false],
            shallowSize: ["selfSize", sortAscending, "id", true],
            retainedSize: ["retainedSize", sortAscending, "id", true]
        }[sortColumnIdentifier];
        return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);
    },

    _emptyData: function()
    {
        return {};
    }
};

WebInspector.HeapSnapshotDominatorObjectNode.prototype.__proto__ = WebInspector.HeapSnapshotGenericObjectNode.prototype;

function MixInSnapshotNodeFunctions(sourcePrototype, targetPrototype)
{
    targetPrototype._childHashForEntity = sourcePrototype._childHashForEntity;
    targetPrototype._childHashForNode = sourcePrototype._childHashForNode;
    targetPrototype.comparator = sourcePrototype.comparator;
    targetPrototype._createChildNode = sourcePrototype._createChildNode;
    targetPrototype._createProvider = sourcePrototype._createProvider;
    targetPrototype.populateChildren = sourcePrototype.populateChildren;
    targetPrototype._saveChildren = sourcePrototype._saveChildren;
    targetPrototype.sort = sourcePrototype.sort;
}
