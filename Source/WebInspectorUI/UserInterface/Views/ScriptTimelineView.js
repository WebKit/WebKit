/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.ScriptTimelineView = function(recording)
{
    WebInspector.TimelineView.call(this);

    this.navigationSidebarTreeOutline.onselect = this._treeElementSelected.bind(this);
    this.navigationSidebarTreeOutline.element.classList.add(WebInspector.ScriptTimelineView.TreeOutlineStyleClassName);

    var columns = {location: {}, callCount: {}, startTime: {}, totalTime: {}, selfTime: {}, averageTime: {}};

    columns.location.title = WebInspector.UIString("Location");
    columns.location.width = "15%";

    columns.callCount.title = WebInspector.UIString("Calls");
    columns.callCount.width = "5%";
    columns.callCount.aligned = "right";

    columns.startTime.title = WebInspector.UIString("Start Time");
    columns.startTime.width = "10%";
    columns.startTime.aligned = "right";

    columns.totalTime.title = WebInspector.UIString("Total Time");
    columns.totalTime.width = "10%";
    columns.totalTime.aligned = "right";

    columns.selfTime.title = WebInspector.UIString("Self Time");
    columns.selfTime.width = "10%";
    columns.selfTime.aligned = "right";

    columns.averageTime.title = WebInspector.UIString("Average Time");
    columns.averageTime.width = "10%";
    columns.averageTime.aligned = "right";

    for (var column in columns)
        columns[column].sortable = true;

    this._dataGrid = new WebInspector.ScriptTimelineDataGrid(this.navigationSidebarTreeOutline, columns, this);
    this._dataGrid.addEventListener(WebInspector.TimelineDataGrid.Event.FiltersDidChange, this._dataGridFiltersDidChange, this);
    this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
    this._dataGrid.sortColumnIdentifier = "startTime";
    this._dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;

    this.element.classList.add(WebInspector.ScriptTimelineView.StyleClassName);
    this.element.appendChild(this._dataGrid.element);

    var scriptTimeline = recording.timelines.get(WebInspector.TimelineRecord.Type.Script);
    scriptTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._scriptTimelineRecordAdded, this);

    this._pendingRecords = [];
};

WebInspector.ScriptTimelineView.StyleClassName = "script";
WebInspector.ScriptTimelineView.TreeOutlineStyleClassName = "script";

WebInspector.ScriptTimelineView.prototype = {
    constructor: WebInspector.ScriptTimelineView,
    __proto__: WebInspector.TimelineView.prototype,

    // Public

    get navigationSidebarTreeOutlineLabel()
    {
        return WebInspector.UIString("Records");
    },

    shown: function()
    {
        WebInspector.TimelineView.prototype.shown.call(this);

        this._dataGrid.shown();
    },

    hidden: function()
    {
        this._dataGrid.hidden();

        WebInspector.TimelineView.prototype.hidden.call(this);
    },

    updateLayout: function()
    {
        WebInspector.TimelineView.prototype.updateLayout.call(this);

        this._dataGrid.updateLayout();

        if (this.startTime !== this._oldStartTime || this.endTime !== this._oldEndTime) {
            var dataGridNode = this._dataGrid.children[0];
            while (dataGridNode) {
                dataGridNode.rangeStartTime = this.startTime;
                dataGridNode.rangeEndTime = this.endTime;
                if (dataGridNode.revealed)
                    dataGridNode.refreshIfNeeded();
                dataGridNode = dataGridNode.traverseNextNode(false, null, true);
            }

            this._oldStartTime = this.startTime;
            this._oldEndTime = this.endTime;
        }

        this._processPendingRecords();
    },

    get selectionPathComponents()
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return null;

        var pathComponents = [];

        while (dataGridNode && !dataGridNode.root) {
            var treeElement = this._dataGrid.treeElementForDataGridNode(dataGridNode);
            console.assert(treeElement);
            if (!treeElement)
                break;

            if (treeElement.hidden)
                return null;

            var pathComponent = new WebInspector.GeneralTreeElementPathComponent(treeElement);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this.treeElementPathComponentSelected, this);
            pathComponents.unshift(pathComponent);
            dataGridNode = dataGridNode.parent;
        }

        return pathComponents;
    },

    matchTreeElementAgainstCustomFilters: function(treeElement)
    {
        return this._dataGrid.treeElementMatchesActiveScopeFilters(treeElement);
    },

    reset: function()
    {
        WebInspector.TimelineView.prototype.reset.call(this);

        this._dataGrid.reset();
    },

    // Protected

    treeElementPathComponentSelected: function(event)
    {
        var dataGridNode = this._dataGrid.dataGridNodeForTreeElement(event.data.pathComponent.generalTreeElement);
        if (!dataGridNode)
            return;
        dataGridNode.revealAndSelect();
    },

    dataGridNodeForTreeElement: function(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement)
            return new WebInspector.ProfileNodeDataGridNode(treeElement.profileNode, this.zeroTime, this.startTime, this.endTime);
        return null;
    },

    populateProfileNodeTreeElement: function(treeElement)
    {
        var zeroTime = this.zeroTime;
        var startTime = this.startTime;
        var endTime = this.endTime;

        for (var childProfileNode of treeElement.profileNode.childNodes) {
            var profileNodeTreeElement = new WebInspector.ProfileNodeTreeElement(childProfileNode, this);
            var profileNodeDataGridNode = new WebInspector.ProfileNodeDataGridNode(childProfileNode, zeroTime, startTime, endTime);
            this._dataGrid.addRowInSortOrder(profileNodeTreeElement, profileNodeDataGridNode, treeElement);
        }
    },

    // Private

    _processPendingRecords: function()
    {
        if (!this._pendingRecords.length)
            return;

        for (var scriptTimelineRecord of this._pendingRecords) {
            var rootNodes = [];
            if (scriptTimelineRecord.profile) {
                // FIXME: Support using the bottom-up tree once it is implemented.
                rootNodes = scriptTimelineRecord.profile.topDownRootNodes;

                // If there is only one node, promote its children. The TimelineRecordTreeElement already reflects the root
                // node in this case (e.g. a "Load Event Dispatched" record with an "onload" root profile node).
                // FIXME: Only do this for the top-down mode. Doing this for bottom-up would be incorrect.
                if (rootNodes.length === 1)
                    rootNodes = rootNodes[0].childNodes;
            }

            var zeroTime = this.zeroTime;
            var treeElement = new WebInspector.TimelineRecordTreeElement(scriptTimelineRecord, WebInspector.SourceCodeLocation.NameStyle.Short, rootNodes.length);
            var dataGridNode = new WebInspector.ScriptTimelineDataGridNode(scriptTimelineRecord, zeroTime);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);

            var startTime = this.startTime;
            var endTime = this.endTime;

            for (var profileNode of rootNodes) {
                var profileNodeTreeElement = new WebInspector.ProfileNodeTreeElement(profileNode, this);
                var profileNodeDataGridNode = new WebInspector.ProfileNodeDataGridNode(profileNode, zeroTime, startTime, endTime);
                this._dataGrid.addRowInSortOrder(profileNodeTreeElement, profileNodeDataGridNode, treeElement);
            }
        }

        this._pendingRecords = [];
    },

    _scriptTimelineRecordAdded: function(event)
    {
        var scriptTimelineRecord = event.data.record;
        console.assert(scriptTimelineRecord instanceof WebInspector.ScriptTimelineRecord);

        this._pendingRecords.push(scriptTimelineRecord);

        this.needsLayout();
    },

    _dataGridFiltersDidChange: function(event)
    {
        WebInspector.timelineSidebarPanel.updateFilter();
    },

    _dataGridNodeSelected: function(event)
    {
        this.dispatchEventToListeners(WebInspector.TimelineView.Event.SelectionPathComponentsDidChange);
    },

    _treeElementSelected: function(treeElement, selectedByUser)
    {
        if (this._dataGrid.shouldIgnoreSelectionEvent())
            return;

        if (!WebInspector.timelineSidebarPanel.canShowDifferentContentView())
            return;

        if (treeElement instanceof WebInspector.FolderTreeElement)
            return;

        var sourceCodeLocation = null;
        if (treeElement instanceof WebInspector.TimelineRecordTreeElement)
            sourceCodeLocation = treeElement.record.sourceCodeLocation;
        else if (treeElement instanceof WebInspector.ProfileNodeTreeElement)
            sourceCodeLocation = treeElement.profileNode.sourceCodeLocation;
        else
            console.error("Unknown tree element selected.");

        if (!sourceCodeLocation) {
            WebInspector.timelineSidebarPanel.showTimelineView(WebInspector.TimelineRecord.Type.Script);
            return;
        }

        WebInspector.resourceSidebarPanel.showOriginalOrFormattedSourceCodeLocation(sourceCodeLocation);
    }
};
