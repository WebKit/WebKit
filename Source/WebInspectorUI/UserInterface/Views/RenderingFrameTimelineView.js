/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.RenderingFrameTimelineView = function(timeline, extraArguments)
{
    WebInspector.TimelineView.call(this, timeline, extraArguments);

    console.assert(WebInspector.TimelineRecord.Type.RenderingFrame);

    this.navigationSidebarTreeOutline.element.classList.add("rendering-frame");

    var columns = {location: {}, startTime: {}, scriptTime: {}, paintTime: {}, layoutTime: {}, otherTime: {}, totalTime: {}};

    columns.location.title = WebInspector.UIString("Location");

    columns.startTime.title = WebInspector.UIString("Start Time");
    columns.startTime.width = "15%";
    columns.startTime.aligned = "right";

    columns.scriptTime.title = WebInspector.RenderingFrameTimelineRecord.displayNameForTaskType(WebInspector.RenderingFrameTimelineRecord.TaskType.Script);
    columns.scriptTime.width = "10%";
    columns.scriptTime.aligned = "right";

    columns.layoutTime.title = WebInspector.RenderingFrameTimelineRecord.displayNameForTaskType(WebInspector.RenderingFrameTimelineRecord.TaskType.Layout);
    columns.layoutTime.width = "10%";
    columns.layoutTime.aligned = "right";

    columns.paintTime.title = WebInspector.RenderingFrameTimelineRecord.displayNameForTaskType(WebInspector.RenderingFrameTimelineRecord.TaskType.Paint);
    columns.paintTime.width = "10%";
    columns.paintTime.aligned = "right";

    columns.otherTime.title = WebInspector.RenderingFrameTimelineRecord.displayNameForTaskType(WebInspector.RenderingFrameTimelineRecord.TaskType.Other);
    columns.otherTime.width = "10%";
    columns.otherTime.aligned = "right";

    columns.totalTime.title = WebInspector.UIString("Total Time");
    columns.totalTime.width = "15%";
    columns.totalTime.aligned = "right";

    for (var column in columns)
        columns[column].sortable = true;

    this._dataGrid = new WebInspector.TimelineDataGrid(this.navigationSidebarTreeOutline, columns, this);
    this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
    this._dataGrid.sortColumnIdentifier = "startTime";
    this._dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;

    this.element.classList.add("rendering-frame");
    this.element.appendChild(this._dataGrid.element);

    timeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._renderingFrameTimelineRecordAdded, this);

    this._pendingRecords = [];
};

WebInspector.RenderingFrameTimelineView.prototype = {
    constructor: WebInspector.RenderingFrameTimelineView,
    __proto__: WebInspector.TimelineView.prototype,

    // Public

    get navigationSidebarTreeOutlineLabel()
    {
        return WebInspector.UIString("Records");
    },

    shown: function()
    {
        WebInspector.ContentView.prototype.shown.call(this);

        this._dataGrid.shown();
    },

    hidden: function()
    {
        this._dataGrid.hidden();

        WebInspector.ContentView.prototype.hidden.call(this);
    },

    closed: function()
    {
        console.assert(this.representedObject instanceof WebInspector.Timeline);
        this.representedObject.removeEventListener(null, null, this);

        this._dataGrid.closed();
    },

    updateLayout: function()
    {
        WebInspector.TimelineView.prototype.updateLayout.call(this);

        this._dataGrid.updateLayout();

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

    filterDidChange: function()
    {
        WebInspector.TimelineView.prototype.filterDidChange.call(this);
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

    canShowContentViewForTreeElement: function(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement)
            return !!treeElement.profileNode.sourceCodeLocation;
        return WebInspector.TimelineView.prototype.canShowContentViewForTreeElement(treeElement);
    },

    showContentViewForTreeElement: function(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement) {
            if (treeElement.profileNode.sourceCodeLocation)
                WebInspector.showOriginalOrFormattedSourceCodeLocation(treeElement.profileNode.sourceCodeLocation);
            return;
        }

        WebInspector.TimelineView.prototype.showContentViewForTreeElement.call(this, treeElement);
    },

    treeElementSelected: function(treeElement, selectedByUser)
    {
        if (this._dataGrid.shouldIgnoreSelectionEvent())
            return;

        WebInspector.TimelineView.prototype.treeElementSelected.call(this, treeElement, selectedByUser);
    },

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

    // Private

    _processPendingRecords: function()
    {
        if (!this._pendingRecords.length)
            return;

        for (var renderingFrameTimelineRecord of this._pendingRecords) {
            console.assert(renderingFrameTimelineRecord instanceof WebInspector.RenderingFrameTimelineRecord);

            var treeElement = new WebInspector.TimelineRecordTreeElement(renderingFrameTimelineRecord);
            var dataGridNode = new WebInspector.RenderingFrameTimelineDataGridNode(renderingFrameTimelineRecord, this.zeroTime);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);

            for (var childRecord of renderingFrameTimelineRecord.children) {
                if (childRecord.type === WebInspector.TimelineRecord.Type.Layout) {
                    var layoutTreeElement = new WebInspector.TimelineRecordTreeElement(childRecord, WebInspector.SourceCodeLocation.NameStyle.Short);
                    var layoutDataGridNode = new WebInspector.LayoutTimelineDataGridNode(childRecord, this.zeroTime);

                    this._dataGrid.addRowInSortOrder(layoutTreeElement, layoutDataGridNode, treeElement);
                } else if (childRecord.type === WebInspector.TimelineRecord.Type.Script) {
                    var rootNodes = [];
                    if (childRecord.profile) {
                        // FIXME: Support using the bottom-up tree once it is implemented.
                        rootNodes = childRecord.profile.topDownRootNodes;
                    }

                    var zeroTime = this.zeroTime;
                    var scriptTreeElement = new WebInspector.TimelineRecordTreeElement(childRecord, WebInspector.SourceCodeLocation.NameStyle.Short, rootNodes.length);
                    var scriptDataGridNode = new WebInspector.ScriptTimelineDataGridNode(childRecord, zeroTime);

                    this._dataGrid.addRowInSortOrder(scriptTreeElement, scriptDataGridNode, treeElement);

                    var startTime = this.startTime;
                    var endTime = this.endTime;

                    for (var profileNode of rootNodes) {
                        var profileNodeTreeElement = new WebInspector.ProfileNodeTreeElement(profileNode, this);
                        var profileNodeDataGridNode = new WebInspector.ProfileNodeDataGridNode(profileNode, zeroTime, startTime, endTime);
                        this._dataGrid.addRowInSortOrder(profileNodeTreeElement, profileNodeDataGridNode, scriptTreeElement);
                    }
                }
            }
        }

        this._pendingRecords = [];
    },

    _renderingFrameTimelineRecordAdded: function(event)
    {
        var renderingFrameTimelineRecord = event.data.record;
        console.assert(renderingFrameTimelineRecord instanceof WebInspector.RenderingFrameTimelineRecord);

        this._pendingRecords.push(renderingFrameTimelineRecord);

        this.needsLayout();
    },

    _dataGridNodeSelected: function(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }
};
