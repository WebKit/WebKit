/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.TimelineDataGrid = function(treeOutline, columns, delegate, editCallback, deleteCallback)
{
    WebInspector.DataGrid.call(this, columns, editCallback, deleteCallback);

    this._treeOutlineDataGridSynchronizer = new WebInspector.TreeOutlineDataGridSynchronizer(treeOutline, this, delegate);

    this.element.classList.add(WebInspector.TimelineDataGrid.StyleClassName);

    this._filterableColumns = [];

    // Check if any of the cells can be filtered.
    for (var identifier in columns) {
        var scopeBar = columns[identifier].scopeBar;
        if (!scopeBar)
            continue;
        this._filterableColumns.push(identifier);
        scopeBar.columnIdenfifier = identifier;
        scopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectedItemsDidChange, this);
    }

    if (this._filterableColumns.length > 1) {
        console.error("Creating a TimelineDataGrid with more than one filterable column is not yet supported.");
        return;
    }

    if (this._filterableColumns.length) {
        var items = [new WebInspector.FlexibleSpaceNavigationItem, this.columns.get(this._filterableColumns[0]).get("scopeBar"), new WebInspector.FlexibleSpaceNavigationItem];
        this._navigationBar = new WebInspector.NavigationBar(null, items);
        var container = this.element.appendChild(document.createElement("div"));
        container.className = "navigation-bar-container";
        container.appendChild(this._navigationBar.element);
    }

    this.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridSelectedNodeChanged, this);
    this.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._sort, this);

    window.addEventListener("resize", this._windowResized.bind(this));
}

WebInspector.TimelineDataGrid.StyleClassName = "timeline";
WebInspector.TimelineDataGrid.DelayedPopoverShowTimeout = 250;
WebInspector.TimelineDataGrid.DelayedPopoverHideContentClearTimeout = 500;

WebInspector.TimelineDataGrid.Event = {
    FiltersDidChange: "timelinedatagrid-filters-did-change"
};

WebInspector.TimelineDataGrid.createColumnScopeBar = function(prefix, dictionary)
{
    prefix = prefix + "-timeline-data-grid-";

    var keys = Object.keys(dictionary).filter(function(key) {
        return typeof dictionary[key] === "string" || dictionary[key] instanceof String;
    });

    var scopeBarItems = keys.map(function(key) {
        var value = dictionary[key];
        var id = prefix + value;
        var label = dictionary.displayName(value, true);
        var item = new WebInspector.ScopeBarItem(id, label);
        item.value = value;
        return item;
    });

    scopeBarItems.unshift(new WebInspector.ScopeBarItem(prefix + "type-all", WebInspector.UIString("All"), true));

    return new WebInspector.ScopeBar(prefix + "scope-bar", scopeBarItems, scopeBarItems[0]);
};

WebInspector.TimelineDataGrid.prototype = {
    constructor: WebInspector.TimelineDataGrid,
    __proto__: WebInspector.DataGrid.prototype,

    // Public

    reset: function()
    {
        // May be overridden by subclasses. If so, they should call the superclass.

        this._hidePopover();
    },

    shown: function()
    {
        // May be overridden by subclasses. If so, they should call the superclass.

        this._treeOutlineDataGridSynchronizer.synchronize();
    },

    hidden: function()
    {
        // May be overridden by subclasses. If so, they should call the superclass.

        this._hidePopover();
    },

    treeElementForDataGridNode: function(dataGridNode)
    {
        return this._treeOutlineDataGridSynchronizer.treeElementForDataGridNode(dataGridNode);
    },

    dataGridNodeForTreeElement: function(treeElement)
    {
        return this._treeOutlineDataGridSynchronizer.dataGridNodeForTreeElement(treeElement);
    },

    callFramePopoverAnchorElement: function()
    {
        // Implemented by subclasses.
        return null;
    },

    updateLayout: function()
    {
        WebInspector.DataGrid.prototype.updateLayout.call(this);

        if (this._navigationBar)
            this._navigationBar.updateLayout();
    },

    treeElementMatchesActiveScopeFilters: function(treeElement)
    {
        var dataGridNode = this._treeOutlineDataGridSynchronizer.dataGridNodeForTreeElement(treeElement);
        console.assert(dataGridNode);

        for (var identifier of this._filterableColumns) {
            var scopeBar = this.columns.get(identifier).get("scopeBar");
            if (!scopeBar || scopeBar.defaultItem.selected)
                continue;

            var value = dataGridNode.data[identifier];
            var matchesFilter = scopeBar.selectedItems.some(function(scopeBarItem) {
                return scopeBarItem.value === value;
            });

            if (!matchesFilter)
                return false;
        }

        return true;
    },

    addRowInSortOrder: function(treeElement, dataGridNode, parentElement)
    {
        this._treeOutlineDataGridSynchronizer.associate(treeElement, dataGridNode);

        parentElement = parentElement || this._treeOutlineDataGridSynchronizer.treeOutline;
        parentNode = parentElement.root ? this : this._treeOutlineDataGridSynchronizer.dataGridNodeForTreeElement(parentElement);

        console.assert(parentNode);

        if (this.sortColumnIdentifier) {
            var insertionIndex = insertionIndexForObjectInListSortedByFunction(dataGridNode, parentNode.children, this._sortComparator.bind(this));

            // Insert into the parent, which will cause the synchronizer to insert into the data grid.
            parentElement.insertChild(treeElement, insertionIndex);
        } else {
            // Append to the parent, which will cause the synchronizer to append to the data grid.
            parentElement.appendChild(treeElement);
        }
    },

    shouldIgnoreSelectionEvent: function()
    {
        return this._ignoreSelectionEvent || false;
    },

    // Protected

    dataGridNodeNeedsRefresh: function(dataGridNode)
    {
        if (!this._dirtyDataGridNodes)
            this._dirtyDataGridNodes = new Set;
        this._dirtyDataGridNodes.add(dataGridNode);

        if (this._scheduledDataGridNodeRefreshIdentifier)
            return;

        this._scheduledDataGridNodeRefreshIdentifier = requestAnimationFrame(this._refreshDirtyDataGridNodes.bind(this));
    },

    // Private

    _refreshDirtyDataGridNodes: function()
    {
        if (this._scheduledDataGridNodeRefreshIdentifier) {
            cancelAnimationFrame(this._scheduledDataGridNodeRefreshIdentifier);
            delete this._scheduledDataGridNodeRefreshIdentifier;
        }

        if (!this._dirtyDataGridNodes)
            return;

        var selectedNode = this.selectedNode;
        var sortComparator = this._sortComparator.bind(this);
        var treeOutline = this._treeOutlineDataGridSynchronizer.treeOutline;

        this._treeOutlineDataGridSynchronizer.enabled = false;

        for (var dataGridNode of this._dirtyDataGridNodes) {
            dataGridNode.refresh();

            if (!this.sortColumnIdentifier)
                continue;

            if (dataGridNode === selectedNode)
                this._ignoreSelectionEvent = true;

            var treeElement = this._treeOutlineDataGridSynchronizer.treeElementForDataGridNode(dataGridNode);
            console.assert(treeElement);

            treeOutline.removeChild(treeElement);
            this.removeChild(dataGridNode);

            var insertionIndex = insertionIndexForObjectInListSortedByFunction(dataGridNode, this.children, sortComparator);
            treeOutline.insertChild(treeElement, insertionIndex);
            this.insertChild(dataGridNode, insertionIndex);

            // Adding the tree element back to the tree outline subjects it to filters.
            // Make sure we keep the hidden state in-sync while the synchronizer is disabled.
            dataGridNode.element.classList.toggle("hidden", treeElement.hidden);

            if (dataGridNode === selectedNode) {
                selectedNode.revealAndSelect();
                delete this._ignoreSelectionEvent;
            }
        }

        this._treeOutlineDataGridSynchronizer.enabled = true;

        delete this._dirtyDataGridNodes;
    },

    _sort: function()
    {
        var sortColumnIdentifier = this.sortColumnIdentifier;
        if (!sortColumnIdentifier)
            return;

        var selectedNode = this.selectedNode;
        this._ignoreSelectionEvent = true;

        this._treeOutlineDataGridSynchronizer.enabled = false;

        var treeOutline = this._treeOutlineDataGridSynchronizer.treeOutline;
        if (treeOutline.selectedTreeElement)
            treeOutline.selectedTreeElement.deselect(true);

        // Collect parent nodes that need their children sorted. So this in two phases since
        // traverseNextNode would get confused if we sort the tree while traversing it.
        var parentDataGridNodes = [this];
        var currentDataGridNode = this.children[0];
        while (currentDataGridNode) {
            if (currentDataGridNode.children.length)
                parentDataGridNodes.push(currentDataGridNode);
            currentDataGridNode = currentDataGridNode.traverseNextNode(false, null, true);
        }

        // Sort the children of collected parent nodes.
        for (var parentDataGridNode of parentDataGridNodes) {
            var parentTreeElement = parentDataGridNode === this ? treeOutline : this._treeOutlineDataGridSynchronizer.treeElementForDataGridNode(parentDataGridNode);
            console.assert(parentTreeElement);

            var childDataGridNodes = parentDataGridNode.children.slice();

            parentDataGridNode.removeChildren();
            parentTreeElement.removeChildren();

            childDataGridNodes.sort(this._sortComparator.bind(this));

            for (var dataGridNode of childDataGridNodes) {
                var treeElement = this._treeOutlineDataGridSynchronizer.treeElementForDataGridNode(dataGridNode);
                console.assert(treeElement);

                parentTreeElement.appendChild(treeElement);
                parentDataGridNode.appendChild(dataGridNode);

                // Adding the tree element back to the tree outline subjects it to filters.
                // Make sure we keep the hidden state in-sync while the synchronizer is disabled.
                dataGridNode.element.classList.toggle("hidden", treeElement.hidden);
            }
        }

        this._treeOutlineDataGridSynchronizer.enabled = true;

        if (selectedNode)
            selectedNode.revealAndSelect();

        delete this._ignoreSelectionEvent;
    },

    _sortComparator: function(node1, node2)
    {
        var sortColumnIdentifier = this.sortColumnIdentifier;
        if (!sortColumnIdentifier)
            return 0;

        var sortDirection = this.sortOrder === "ascending" ? 1 : -1;

        var value1 = node1.data[sortColumnIdentifier];
        var value2 = node2.data[sortColumnIdentifier];

        if (typeof value1 === "number" && typeof value2 === "number") {
            if (isNaN(value1) && isNaN(value2))
                return 0;
            if (isNaN(value1))
                return -sortDirection;
            if (isNaN(value2))
                return sortDirection;
            return (value1 - value2) * sortDirection;
        }

        if (typeof value1 === "string" && typeof value2 === "string")
            return value1.localeCompare(value2) * sortDirection;

        if (value1 instanceof WebInspector.CallFrame || value2 instanceof WebInspector.CallFrame) {
            // Sort by function name if available, then fall back to the source code object.
            value1 = value1 && value1.functionName ? value1.functionName : (value1 && value1.sourceCodeLocation ? value1.sourceCodeLocation.sourceCode : "");
            value2 = value2 && value2.functionName ? value2.functionName : (value2 && value2.sourceCodeLocation ? value2.sourceCodeLocation.sourceCode : "");
        }

        if (value1 instanceof WebInspector.SourceCode || value2 instanceof WebInspector.SourceCode) {
            value1 = value1 ? value1.displayName || "" : "";
            value2 = value2 ? value2.displayName || "" : "";
        }

        // For everything else (mostly booleans).
        return (value1 < value2 ? -1 : (value1 > value2 ? 1 : 0)) * sortDirection;
    },

    _scopeBarSelectedItemsDidChange: function(event)
    {
        var columnIdentifier = event.target.columnIdenfifier;
        this.dispatchEventToListeners(WebInspector.TimelineDataGrid.Event.FiltersDidChange, {columnIdentifier: columnIdentifier});
    },

    _dataGridSelectedNodeChanged: function(event)
    {
        if (!this.selectedNode) {
            this._hidePopover();
            return;
        }

        var record = this.selectedNode.record;
        if (!record || !record.callFrames || !record.callFrames.length) {
            this._hidePopover();
            return;
        }

        this._showPopoverForSelectedNodeSoon();
    },

    _windowResized: function(event)
    {
        if (this._popover && this._popover.visible)
            this._updatePopoverForSelectedNode(false);
    },

    _showPopoverForSelectedNodeSoon: function()
    {
        if (this._showPopoverTimeout)
            return;

        function delayedWork()
        {
            if (!this._popover)
                this._popover = new WebInspector.Popover;

            this._updatePopoverForSelectedNode(true);
        }

        this._showPopoverTimeout = setTimeout(delayedWork.bind(this), WebInspector.TimelineDataGrid.DelayedPopoverShowTimeout);
    },

    _hidePopover: function()
    {
        if (this._showPopoverTimeout) {
            clearTimeout(this._showPopoverTimeout);
            delete this._showPopoverTimeout;
        }

        if (this._popover)
            this._popover.dismiss();

        function delayedWork()
        {
            if (this._popoverCallStackTreeOutline)
                this._popoverCallStackTreeOutline.removeChildren();
        }

        if (this._hidePopoverContentClearTimeout)
            clearTimeout(this._hidePopoverContentClearTimeout);
        this._hidePopoverContentClearTimeout = setTimeout(delayedWork.bind(this), WebInspector.TimelineDataGrid.DelayedPopoverHideContentClearTimeout);
    },

    _updatePopoverForSelectedNode: function(updateContent)
    {
        if (!this._popover || !this.selectedNode)
            return;

        var targetPopoverElement = this.callFramePopoverAnchorElement();
        console.assert(targetPopoverElement, "TimelineDataGrid subclass should always return a valid element from callFramePopoverAnchorElement.");
        if (!targetPopoverElement)
            return;

        var targetFrame = WebInspector.Rect.rectFromClientRect(targetPopoverElement.getBoundingClientRect());

        // The element might be hidden if it does not have a width and height.
        if (!targetFrame.size.width && !targetFrame.size.height)
            return;

        if (this._hidePopoverContentClearTimeout) {
            clearTimeout(this._hidePopoverContentClearTimeout);
            delete this._hidePopoverContentClearTimeout;
        }

        if (updateContent)
            this._popover.content = this._createPopoverContent();

        this._popover.present(targetFrame.pad(2), [WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MIN_Y, WebInspector.RectEdge.MAX_X]);
    },

    _createPopoverContent: function()
    {
        if (!this._popoverCallStackTreeOutline) {
            var contentElement = document.createElement("ol");
            contentElement.classList.add("timeline-data-grid-tree-outline");
            this._popoverCallStackTreeOutline = new TreeOutline(contentElement);
            this._popoverCallStackTreeOutline.onselect = this._popoverCallStackTreeElementSelected.bind(this);
        } else
            this._popoverCallStackTreeOutline.removeChildren();

        var callFrames = this.selectedNode.record.callFrames;
        for (var i = 0 ; i < callFrames.length; ++i) {
            var callFrameTreeElement = new WebInspector.CallFrameTreeElement(callFrames[i]);
            this._popoverCallStackTreeOutline.appendChild(callFrameTreeElement);
        }

        var content = document.createElement("div");
        content.className = "timeline-data-grid-popover";
        content.appendChild(this._popoverCallStackTreeOutline.element);
        return content;
    },

    _popoverCallStackTreeElementSelected: function(treeElement, selectedByUser)
    {
        this._popover.dismiss();

        console.assert(treeElement instanceof WebInspector.CallFrameTreeElement, "TreeElements in TimelineDataGrid popover should always be CallFrameTreeElements");
        var callFrame = treeElement.callFrame;
        if (!callFrame.sourceCodeLocation)
            return;

        WebInspector.resourceSidebarPanel.showSourceCodeLocation(callFrame.sourceCodeLocation);
    }
};
