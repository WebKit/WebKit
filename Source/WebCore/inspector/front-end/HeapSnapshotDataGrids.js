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
 * @extends {WebInspector.DataGrid}
 * @param {boolean=} deferNodeContentCreation
 */
WebInspector.HeapSnapshotSortableDataGrid = function(columns, deferNodeContentCreation)
{
    WebInspector.DataGrid.call(this, columns);
    this.addEventListener("sorting changed", this.sortingChanged, this);

    this._deferNodeContentCreation = deferNodeContentCreation;
    if (deferNodeContentCreation)
        this.scrollContainer.addEventListener("scroll", this._onScroll.bind(this), true);
}

WebInspector.HeapSnapshotSortableDataGrid.prototype = {
    dispose: function()
    {
        for (var i = 0, l = this.rootNode().children.length; i < l; ++i)
            this.rootNode().children[i].dispose();
    },

    resetSortingCache: function()
    {
        delete this._lastSortColumnIdentifier;
        delete this._lastSortAscending;
    },

    changeNameFilter: function(filter)
    {
        filter = filter.toLowerCase();
        var children = this.rootNode().children;
        for (var i = 0, l = children.length; i < l; ++i) {
            var node = children[i];
            if (node.depth === 0)
                node.revealed = node._name.toLowerCase().indexOf(filter) !== -1;
        }
        this.updateVisibleNodes();
    },

    sortingChanged: function()
    {
        var sortAscending = this.sortOrder === "ascending";
        var sortColumnIdentifier = this.sortColumnIdentifier;
        if (this._lastSortColumnIdentifier === sortColumnIdentifier && this._lastSortAscending === sortAscending)
            return;
        this._lastSortColumnIdentifier = sortColumnIdentifier;
        this._lastSortAscending = sortAscending;
        var sortFields = this._sortFields(sortColumnIdentifier, sortAscending);

        function SortByTwoFields(nodeA, nodeB)
        {
            var field1 = nodeA[sortFields[0]];
            var field2 = nodeB[sortFields[0]];
            var result = field1 < field2 ? -1 : (field1 > field2 ? 1 : 0);
            if (!sortFields[1])
                result = -result;
            if (result !== 0)
                return result;
            field1 = nodeA[sortFields[2]];
            field2 = nodeB[sortFields[2]];
            result = field1 < field2 ? -1 : (field1 > field2 ? 1 : 0);
            if (!sortFields[3])
                result = -result;
            return result;
        }
        this._performSorting(SortByTwoFields);
    },

    updateVisibleNodes: function()
    {
        if (!this._deferNodeContentCreation)
            return;
        var scrollTop = this.scrollContainer.scrollTop;
        var rowHeight = 16;

        var height = this.scrollContainer.offsetHeight;
        var visibleRowsCount = Math.round(height / rowHeight) + 1;

        var children = this.rootNode().children;

        var i = 0;
        while (i < children.length) {
            if (children[i].revealed) {
                var top = children[i].element.offsetTop;
                if (top >= scrollTop)
                    break;
            }
            ++i;
        }

        while (i < children.length && visibleRowsCount) {
            if (children[i].revealed) {
                children[i].ensureContentCreated();
                --visibleRowsCount;
            }
            ++i;
        }
    },

    onResize: function()
    {
        this.updateVisibleNodes();
    },

    _onScroll: function(event)
    {
        this.updateVisibleNodes();
    },

    _performSorting: function(sortFunction)
    {
        this.recursiveSortingEnter();
        var children = this.rootNode().children;
        this.rootNode().removeChildren();
        children.sort(sortFunction);
        for (var i = 0, l = children.length; i < l; ++i) {
            var child = children[i];
            var revealed = child.revealed;
            this.rootNode().appendChild(child);
            child.revealed = revealed;
            if (child.expanded)
                child.sort();
        }
        this.recursiveSortingLeave();
        this.updateVisibleNodes();
    },

    recursiveSortingEnter: function()
    {
        if (!("_recursiveSortingDepth" in this))
            this._recursiveSortingDepth = 1;
        else
            ++this._recursiveSortingDepth;
    },

    recursiveSortingLeave: function()
    {
        if (!("_recursiveSortingDepth" in this))
            return;
        if (!--this._recursiveSortingDepth) {
            delete this._recursiveSortingDepth;
            this.dispatchEventToListeners("sorting complete");
        }
    }
};

WebInspector.HeapSnapshotSortableDataGrid.prototype.__proto__ = WebInspector.DataGrid.prototype;

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotSortableDataGrid}
 * @param {Object=} columns
 */
WebInspector.HeapSnapshotContainmentDataGrid = function(columns)
{
    columns = columns || {
        object: { title: WebInspector.UIString("Object"), disclosure: true, sortable: true },
        shallowSize: { title: WebInspector.UIString("Shallow Size"), width: "120px", sortable: true },
        retainedSize: { title: WebInspector.UIString("Retained Size"), width: "120px", sortable: true, sort: "descending" }
    };
    WebInspector.HeapSnapshotSortableDataGrid.call(this, columns);
}

WebInspector.HeapSnapshotContainmentDataGrid.prototype = {
    _defaultPopulateCount: 100,

    expandRoute: function(route)
    {
        function nextStep(parent, hopIndex)
        {
            if (hopIndex >= route.length) {
                parent.element.scrollIntoViewIfNeeded(true);
                parent.select();
                return;
            }
            var nodeIndex = route[hopIndex];
            for (var i = 0, l = parent.children.length; i < l; ++i) {
                var child = parent.children[i];
                if (child.snapshotNodeIndex === nodeIndex) {
                    if (child.expanded)
                        nextStep(child, hopIndex + 1);
                    else {
                        function afterExpand()
                        {
                            child.removeEventListener("populate complete", afterExpand, null);
                            var lastChild = child.children[child.children.length - 1];
                            if (!lastChild.showAll)
                                nextStep(child, hopIndex + 1);
                            else {
                                child.addEventListener("populate complete", afterExpand, null);
                                lastChild.showAll.click();
                            }
                        }
                        child.addEventListener("populate complete", afterExpand, null);
                        child.expand();
                    }
                    break;
                }
            }
        }
        nextStep(this.rootNode(), 0);
    },

    setDataSource: function(snapshotView, snapshot, nodeIndex)
    {
        this.snapshotView = snapshotView;
        this.snapshot = snapshot;
        var fakeEdge = { nodeIndex: nodeIndex || this.snapshot.rootNodeIndex };
        this.setRootNode(new WebInspector.HeapSnapshotObjectNode(this, false, fakeEdge, null));
        this.rootNode().sort();
    },

    sortingChanged: function()
    {
        this.rootNode().sort();
    }
};

WebInspector.HeapSnapshotContainmentDataGrid.prototype.__proto__ = WebInspector.HeapSnapshotSortableDataGrid.prototype;

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotContainmentDataGrid}
 */
WebInspector.HeapSnapshotRetainmentDataGrid = function()
{
    this.showRetainingEdges = true;
    var columns = {
        object: { title: WebInspector.UIString("Object"), disclosure: true, sortable: true },
        shallowSize: { title: WebInspector.UIString("Shallow Size"), width: "120px", sortable: true },
        retainedSize: { title: WebInspector.UIString("Retained Size"), width: "120px", sortable: true },
        distanceToWindow: { title: WebInspector.UIString("Distance"), width: "80px", sortable: true, sort: "ascending" }
    };
    WebInspector.HeapSnapshotContainmentDataGrid.call(this, columns);
}

WebInspector.HeapSnapshotRetainmentDataGrid.prototype = {
    _sortFields: function(sortColumn, sortAscending)
    {
        return {
            object: ["_name", sortAscending, "_count", false],
            count: ["_count", sortAscending, "_name", true],
            shallowSize: ["_shallowSize", sortAscending, "_name", true],
            retainedSize: ["_retainedSize", sortAscending, "_name", true],
            distanceToWindow: ["_distanceToWindow", sortAscending, "_name", true]
        }[sortColumn];
    },

    reset: function()
    {
        this.rootNode().removeChildren();
        this.resetSortingCache();
    },
}

WebInspector.HeapSnapshotRetainmentDataGrid.prototype.__proto__ = WebInspector.HeapSnapshotContainmentDataGrid.prototype;

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotSortableDataGrid}
 */
WebInspector.HeapSnapshotConstructorsDataGrid = function()
{
    var columns = {
        object: { title: WebInspector.UIString("Constructor"), disclosure: true, sortable: true },
        distanceToWindow: { title: WebInspector.UIString("Distance"), width: "90px", sortable: true },
        count: { title: WebInspector.UIString("Objects Count"), width: "90px", sortable: true },
        shallowSize: { title: WebInspector.UIString("Shallow Size"), width: "120px", sortable: true },
        retainedSize: { title: WebInspector.UIString("Retained Size"), width: "120px", sort: "descending", sortable: true }
    };
    WebInspector.HeapSnapshotSortableDataGrid.call(this, columns, true);
    this._profileIndex = -1;
}

WebInspector.HeapSnapshotConstructorsDataGrid.prototype = {
    _defaultPopulateCount: 100,

    _sortFields: function(sortColumn, sortAscending)
    {
        return {
            object: ["_name", sortAscending, "_count", false],
            distanceToWindow: ["_distanceToWindow", sortAscending, "_retainedSize", true],
            count: ["_count", sortAscending, "_name", true],
            shallowSize: ["_shallowSize", sortAscending, "_name", true],
            retainedSize: ["_retainedSize", sortAscending, "_name", true]
        }[sortColumn];
    },

    setDataSource: function(snapshotView, snapshot)
    {
        this.snapshotView = snapshotView;
        this.snapshot = snapshot;
        if (this._profileIndex === -1)
            this.populateChildren();
    },

    populateChildren: function()
    {
        function aggregatesReceived(key, aggregates)
        {
            for (var constructor in aggregates)
                this.rootNode().appendChild(new WebInspector.HeapSnapshotConstructorNode(this, constructor, aggregates[constructor], key));
            this.sortingChanged();
        }

        this.dispose();
        this.rootNode().removeChildren();
        this.resetSortingCache();

        var key = this._profileIndex === -1 ? "allObjects" : this._minNodeId + ".." + this._maxNodeId;
        var filter = this._profileIndex === -1 ? null : "function(node) { var id = node.id; return id > " + this._minNodeId + " && id <= " + this._maxNodeId + "; }";

        this.snapshot.aggregates(false, key, filter, aggregatesReceived.bind(this, key));
    },

    _filterSelectIndexChanged: function(profiles, profileIndex)
    {
        this._profileIndex = profileIndex;

        delete this._maxNodeId;
        delete this._minNodeId;

        if (this._profileIndex !== -1) {
            this._minNodeId = profileIndex > 0 ? profiles[profileIndex - 1].maxJSObjectId : 0;
            this._maxNodeId = profiles[profileIndex].maxJSObjectId;
        }

        this.populateChildren();
    },

};

WebInspector.HeapSnapshotConstructorsDataGrid.prototype.__proto__ = WebInspector.HeapSnapshotSortableDataGrid.prototype;

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotSortableDataGrid}
 */
WebInspector.HeapSnapshotDiffDataGrid = function()
{
    var columns = {
        object: { title: WebInspector.UIString("Constructor"), disclosure: true, sortable: true },
        addedCount: { title: WebInspector.UIString("# New"), width: "72px", sortable: true },
        removedCount: { title: WebInspector.UIString("# Deleted"), width: "72px", sortable: true },
        countDelta: { title: "# Delta", width: "64px", sortable: true },
        addedSize: { title: WebInspector.UIString("Alloc. Size"), width: "72px", sortable: true, sort: "descending" },
        removedSize: { title: WebInspector.UIString("Freed Size"), width: "72px", sortable: true },
        sizeDelta: { title: "Size Delta", width: "72px", sortable: true }
    };
    WebInspector.HeapSnapshotSortableDataGrid.call(this, columns, true);
}

WebInspector.HeapSnapshotDiffDataGrid.prototype = {
    _defaultPopulateCount: 50,

    _sortFields: function(sortColumn, sortAscending)
    {
        return {
            object: ["_name", sortAscending, "_count", false],
            addedCount: ["_addedCount", sortAscending, "_name", true],
            removedCount: ["_removedCount", sortAscending, "_name", true],
            countDelta: ["_countDelta", sortAscending, "_name", true],
            addedSize: ["_addedSize", sortAscending, "_name", true],
            removedSize: ["_removedSize", sortAscending, "_name", true],
            sizeDelta: ["_sizeDelta", sortAscending, "_name", true]
        }[sortColumn];
    },

    setDataSource: function(snapshotView, snapshot)
    {
        this.snapshotView = snapshotView;
        this.snapshot = snapshot;
    },

    _baseProfileIndexChanged: function(loader, profileIndex)
    {
        loader(profileIndex, this.setBaseDataSource.bind(this));
    },

    setBaseDataSource: function(baseSnapshot)
    {
        this.baseSnapshot = baseSnapshot;
        this.dispose();
        this.rootNode().removeChildren();
        this.resetSortingCache();
        if (this.baseSnapshot === this.snapshot) {
            this.dispatchEventToListeners("sorting complete");
            return;
        }
        this.populateChildren();
    },

    populateChildren: function()
    {
        function baseAggregatesReceived(baseClasses)
        {
            function aggregatesReceived(classes)
            {
                var nodeCount = 0;
                var nodes = [];
                for (var clss in baseClasses)
                    nodes.push(new WebInspector.HeapSnapshotDiffNode(this, clss, baseClasses[clss], classes[clss]));
                for (clss in classes) {
                    if (!(clss in baseClasses))
                        nodes.push(new WebInspector.HeapSnapshotDiffNode(this, clss, null, classes[clss]));
                }
                nodeCount = nodes.length;
                function addNodeIfNonZeroDiff(boundNode, zeroDiff)
                {
                    if (!zeroDiff)
                        this.rootNode().appendChild(boundNode);
                    if (!--nodeCount)
                        this.sortingChanged();
                }
                for (var i = 0, l = nodes.length; i < l; ++i) {
                    var node = nodes[i];
                    node.calculateDiff(this, addNodeIfNonZeroDiff.bind(this, node));
                }
            }
            this.snapshot.aggregates(true, "allObjects", null, aggregatesReceived.bind(this));
        }
        this.baseSnapshot.aggregates(true, "allObjects", null, baseAggregatesReceived.bind(this));
    }
};

WebInspector.HeapSnapshotDiffDataGrid.prototype.__proto__ = WebInspector.HeapSnapshotSortableDataGrid.prototype;

/**
 * @constructor
 * @extends {WebInspector.HeapSnapshotSortableDataGrid}
 */
WebInspector.HeapSnapshotDominatorsDataGrid = function()
{
    var columns = {
        object: { title: WebInspector.UIString("Object"), disclosure: true, sortable: true },
        shallowSize: { title: WebInspector.UIString("Shallow Size"), width: "120px", sortable: true },
        retainedSize: { title: WebInspector.UIString("Retained Size"), width: "120px", sort: "descending", sortable: true }
    };
    WebInspector.HeapSnapshotSortableDataGrid.call(this, columns);
}

WebInspector.HeapSnapshotDominatorsDataGrid.prototype = {
    _defaultPopulateCount: 25,

    setDataSource: function(snapshotView, snapshot)
    {
        this.snapshotView = snapshotView;
        this.snapshot = snapshot;

        var fakeNode = { nodeIndex: this.snapshot.rootNodeIndex };
        this.setRootNode(new WebInspector.HeapSnapshotDominatorObjectNode(this, fakeNode));
        this.rootNode().sort();
    },

    sortingChanged: function()
    {
        this.rootNode().sort();
    }
};

WebInspector.HeapSnapshotDominatorsDataGrid.prototype.__proto__ = WebInspector.HeapSnapshotSortableDataGrid.prototype;
