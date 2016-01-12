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

        this.navigationSidebarTreeOutline.element.classList.add("rendering-frame");

        var scopeBarItems = [];
        for (var key in WebInspector.RenderingFrameTimelineView.DurationFilter) {
            var value = WebInspector.RenderingFrameTimelineView.DurationFilter[key];
            scopeBarItems.push(new WebInspector.ScopeBarItem(value, WebInspector.RenderingFrameTimelineView.displayNameForDurationFilter(value)));
        }

        this._scopeBar = new WebInspector.ScopeBar("rendering-frame-scope-bar", scopeBarItems, scopeBarItems[0], true);
        this._scopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        let columns = {totalTime: {}, scriptTime: {}, layoutTime: {}, paintTime: {}, otherTime: {}, startTime: {}, location: {}};

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

        this._dataGrid = new WebInspector.TimelineDataGrid(this.navigationSidebarTreeOutline, columns, this);
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

    get navigationSidebarTreeOutlineLabel()
    {
        return WebInspector.UIString("Records");
    }

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

    canShowContentViewForTreeElement(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement)
            return !!treeElement.profileNode.sourceCodeLocation;
        return super.canShowContentViewForTreeElement(treeElement);
    }

    showContentViewForTreeElement(treeElement)
    {
        if (treeElement instanceof WebInspector.ProfileNodeTreeElement) {
            if (treeElement.profileNode.sourceCodeLocation)
                WebInspector.showOriginalOrFormattedSourceCodeLocation(treeElement.profileNode.sourceCodeLocation);
            return;
        }

        super.showContentViewForTreeElement(treeElement);
    }

    treeElementDeselected(treeElement)
    {
        var dataGridNode = this._dataGrid.dataGridNodeForTreeElement(treeElement);
        if (!dataGridNode)
            return;

        dataGridNode.deselect();
    }

    treeElementSelected(treeElement, selectedByUser)
    {
        if (this._dataGrid.shouldIgnoreSelectionEvent())
            return;

        super.treeElementSelected(treeElement, selectedByUser);
    }

    treeElementPathComponentSelected(event)
    {
        var dataGridNode = this._dataGrid.dataGridNodeForTreeElement(event.data.pathComponent.generalTreeElement);
        if (!dataGridNode)
            return;
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

        for (var renderingFrameTimelineRecord of this._pendingRecords) {
            console.assert(renderingFrameTimelineRecord instanceof WebInspector.RenderingFrameTimelineRecord);

            var treeElement = new WebInspector.TimelineRecordTreeElement(renderingFrameTimelineRecord);
            var dataGridNode = new WebInspector.RenderingFrameTimelineDataGridNode(renderingFrameTimelineRecord, this.zeroTime);
            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);

            var stack = [{children: renderingFrameTimelineRecord.children, parentTreeElement: treeElement, index: 0}];
            while (stack.length) {
                var entry = stack.lastValue;
                if (entry.index >= entry.children.length) {
                    stack.pop();
                    continue;
                }

                var childRecord = entry.children[entry.index];
                var childTreeElement = null;
                if (childRecord.type === WebInspector.TimelineRecord.Type.Layout) {
                    childTreeElement = new WebInspector.TimelineRecordTreeElement(childRecord, WebInspector.SourceCodeLocation.NameStyle.Short);
                    if (childRecord.width && childRecord.height) {
                        let subtitle = document.createElement("span");
                        subtitle.textContent = WebInspector.UIString("%d \u2A09 %d").format(childRecord.width, childRecord.height);
                        childTreeElement.subtitle = subtitle;
                    }
                    var layoutDataGridNode = new WebInspector.LayoutTimelineDataGridNode(childRecord, this.zeroTime);

                    this._dataGrid.addRowInSortOrder(childTreeElement, layoutDataGridNode, entry.parentTreeElement);
                } else if (childRecord.type === WebInspector.TimelineRecord.Type.Script) {
                    var rootNodes = [];
                    if (childRecord.profile) {
                        // FIXME: Support using the bottom-up tree once it is implemented.
                        rootNodes = childRecord.profile.topDownRootNodes;
                    }

                    childTreeElement = new WebInspector.TimelineRecordTreeElement(childRecord, WebInspector.SourceCodeLocation.NameStyle.Short, rootNodes.length);
                    var scriptDataGridNode = new WebInspector.ScriptTimelineDataGridNode(childRecord, this.zeroTime);

                    this._dataGrid.addRowInSortOrder(childTreeElement, scriptDataGridNode, entry.parentTreeElement);

                    for (var profileNode of rootNodes) {
                        var profileNodeTreeElement = new WebInspector.ProfileNodeTreeElement(profileNode, this);
                        var profileNodeDataGridNode = new WebInspector.ProfileNodeDataGridNode(profileNode, this.zeroTime, this.startTime, this.endTime);
                        this._dataGrid.addRowInSortOrder(profileNodeTreeElement, profileNodeDataGridNode, childTreeElement);
                    }
                }

                if (childTreeElement && childRecord.children.length)
                    stack.push({children: childRecord.children, parentTreeElement: childTreeElement, index: 0});
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
        this.timelineSidebarPanel.updateFilter();
    }
};

WebInspector.RenderingFrameTimelineView.DurationFilter = {
    All: "rendering-frame-timeline-view-duration-filter-all",
    OverOneMillisecond: "rendering-frame-timeline-view-duration-filter-over-1-ms",
    OverFifteenMilliseconds: "rendering-frame-timeline-view-duration-filter-over-15-ms"
};

