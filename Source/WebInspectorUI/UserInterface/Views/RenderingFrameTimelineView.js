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

WebInspector.RenderingFrameTimelineView = class RenderingFrameTimelineView extends WebInspector.TimelineView
{
    constructor(timeline, extraArguments)
    {
        super(timeline, extraArguments);

        console.assert(WebInspector.TimelineRecord.Type.RenderingFrame);

        var scopeBarItems = [];
        for (var key in WebInspector.RenderingFrameTimelineView.DurationFilter) {
            var value = WebInspector.RenderingFrameTimelineView.DurationFilter[key];
            scopeBarItems.push(new WebInspector.ScopeBarItem(value, WebInspector.RenderingFrameTimelineView.displayNameForDurationFilter(value)));
        }

        this._scopeBar = new WebInspector.ScopeBar("rendering-frame-scope-bar", scopeBarItems, scopeBarItems[0], true);
        this._scopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        let columns = {name: {}, totalTime: {}, scriptTime: {}, layoutTime: {}, paintTime: {}, otherTime: {}, startTime: {}, location: {}};

        columns.name.title = WebInspector.UIString("Name");
        columns.name.width = "20%";
        columns.name.icon = true;
        columns.name.disclosure = true;

        columns.totalTime.title = WebInspector.UIString("Total Time");
        columns.totalTime.width = "15%";
        columns.totalTime.aligned = "right";

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

        columns.startTime.title = WebInspector.UIString("Start Time");
        columns.startTime.width = "15%";
        columns.startTime.aligned = "right";

        columns.location.title = WebInspector.UIString("Location");

        for (var column in columns)
            columns[column].sortable = true;

        this._dataGrid = new WebInspector.TimelineDataGrid(null, columns, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
        this._dataGrid.sortColumnIdentifierSetting = new WebInspector.Setting("rendering-frame-timeline-view-sort", "startTime");
        this._dataGrid.sortOrderSetting = new WebInspector.Setting("rendering-frame-timeline-view-sort-order", WebInspector.DataGrid.SortOrder.Ascending);

        this.element.classList.add("rendering-frame");
        this.addSubview(this._dataGrid);

        timeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._renderingFrameTimelineRecordAdded, this);

        this._pendingRecords = [];
    }

    static displayNameForDurationFilter(filter)
    {
        switch (filter) {
            case WebInspector.RenderingFrameTimelineView.DurationFilter.All:
                return WebInspector.UIString("All");
            case WebInspector.RenderingFrameTimelineView.DurationFilter.OverOneMillisecond:
                return WebInspector.UIString("Over 1 ms");
            case WebInspector.RenderingFrameTimelineView.DurationFilter.OverFifteenMilliseconds:
                return WebInspector.UIString("Over 15 ms");
            default:
                console.error("Unknown filter type", filter);
        }

        return null;
    }

    // Public

    shown()
    {
        super.shown();

        this._dataGrid.shown();
    }

    hidden()
    {
        this._dataGrid.hidden();

        super.hidden();
    }

    closed()
    {
        console.assert(this.representedObject instanceof WebInspector.Timeline);
        this.representedObject.removeEventListener(null, null, this);

        this._dataGrid.closed();
    }

    get selectionPathComponents()
    {
        let dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode || dataGridNode.hidden)
            return null;

        let pathComponents = [];

        while (dataGridNode && !dataGridNode.root) {
            console.assert(dataGridNode instanceof WebInspector.TimelineDataGridNode);
            if (dataGridNode.hidden)
                return null;

            let pathComponent = new WebInspector.TimelineDataGridNodePathComponent(dataGridNode);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this.dataGridNodePathComponentSelected, this);
            pathComponents.unshift(pathComponent);
            dataGridNode = dataGridNode.parent;
        }

        return pathComponents;
    }

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        console.assert(this._scopeBar.selectedItems.length === 1);
        var selectedScopeBarItem = this._scopeBar.selectedItems[0];
        if (!selectedScopeBarItem || selectedScopeBarItem.id === WebInspector.RenderingFrameTimelineView.DurationFilter.All)
            return true;

        while (treeElement && !(treeElement.record instanceof WebInspector.RenderingFrameTimelineRecord))
            treeElement = treeElement.parent;

        console.assert(treeElement, "Cannot apply duration filter: no RenderingFrameTimelineRecord found.");
        if (!treeElement)
            return false;

        var minimumDuration = selectedScopeBarItem.id === WebInspector.RenderingFrameTimelineView.DurationFilter.OverOneMillisecond ? 0.001 : 0.015;
        return treeElement.record.duration > minimumDuration;
    }

    reset()
    {
        super.reset();

        this._dataGrid.reset();

        this._pendingRecords = [];
    }

    // Protected

    dataGridNodePathComponentSelected(event)
    {
        let dataGridNode = event.data.pathComponent.timelineDataGridNode;
        console.assert(dataGridNode.dataGrid === this._dataGrid);

        dataGridNode.revealAndSelect();
    }

    dataGridNodeForTreeElement(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement)
            return new WebInspector.ProfileNodeDataGridNode(treeElement.profileNode, this.zeroTime, this.startTime, this.endTime);
        return null;
    }

    layout()
    {
        this._processPendingRecords();
    }

    // Private

    _processPendingRecords()
    {
        if (!this._pendingRecords.length)
            return;

        for (let renderingFrameTimelineRecord of this._pendingRecords) {
            console.assert(renderingFrameTimelineRecord instanceof WebInspector.RenderingFrameTimelineRecord);

            let dataGridNode = new WebInspector.RenderingFrameTimelineDataGridNode(renderingFrameTimelineRecord, this.zeroTime);
            this._dataGrid.addRowInSortOrder(null, dataGridNode);

            let stack = [{children: renderingFrameTimelineRecord.children, parentDataGridNode: dataGridNode, index: 0}];
            while (stack.length) {
                let entry = stack.lastValue;
                if (entry.index >= entry.children.length) {
                    stack.pop();
                    continue;
                }

                let childRecord = entry.children[entry.index];
                let childDataGridNode = null;
                if (childRecord.type === WebInspector.TimelineRecord.Type.Layout) {
                    childDataGridNode = new WebInspector.LayoutTimelineDataGridNode(childRecord, this.zeroTime);

                    this._dataGrid.addRowInSortOrder(null, childDataGridNode, entry.parentDataGridNode);
                } else if (childRecord.type === WebInspector.TimelineRecord.Type.Script) {
                    let rootNodes = [];
                    if (childRecord.profile) {
                        // FIXME: Support using the bottom-up tree once it is implemented.
                        rootNodes = childRecord.profile.topDownRootNodes;
                    }

                    childDataGridNode = new WebInspector.ScriptTimelineDataGridNode(childRecord, this.zeroTime);

                    this._dataGrid.addRowInSortOrder(null, childDataGridNode, entry.parentDataGridNode);

                    for (let profileNode of rootNodes) {
                        let profileNodeDataGridNode = new WebInspector.ProfileNodeDataGridNode(profileNode, this.zeroTime, this.startTime, this.endTime);
                        this._dataGrid.addRowInSortOrder(null, profileNodeDataGridNode, childDataGridNode);
                    }
                }

                if (childDataGridNode && childRecord.children.length)
                    stack.push({children: childRecord.children, parentDataGridNode: childDataGridNode, index: 0});
                ++entry.index;
            }
        }

        this._pendingRecords = [];
    }

    _renderingFrameTimelineRecordAdded(event)
    {
        var renderingFrameTimelineRecord = event.data.record;
        console.assert(renderingFrameTimelineRecord instanceof WebInspector.RenderingFrameTimelineRecord);
        console.assert(renderingFrameTimelineRecord.children.length, "Missing child records for rendering frame.");

        this._pendingRecords.push(renderingFrameTimelineRecord);

        this.needsLayout();
    }

    _dataGridNodeSelected(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _scopeBarSelectionDidChange(event)
    {
        // FIXME: <https://webkit.org/b/154924> Web Inspector: hook up grid row filtering in the new Timelines UI
    }
};

WebInspector.RenderingFrameTimelineView.DurationFilter = {
    All: "rendering-frame-timeline-view-duration-filter-all",
    OverOneMillisecond: "rendering-frame-timeline-view-duration-filter-over-1-ms",
    OverFifteenMilliseconds: "rendering-frame-timeline-view-duration-filter-over-15-ms"
};

