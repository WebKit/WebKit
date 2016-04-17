/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

WebInspector.NetworkTimelineView = class NetworkTimelineView extends WebInspector.TimelineView
{
    constructor(timeline, extraArguments)
    {
        super(timeline, extraArguments);

        console.assert(timeline.type === WebInspector.TimelineRecord.Type.Network);

        let columns = {name: {}, domain: {}, type: {}, method: {}, scheme: {}, statusCode: {}, cached: {}, size: {}, transferSize: {}, requestSent: {}, latency: {}, duration: {}};

        columns.name.title = WebInspector.UIString("Name");
        columns.name.icon = true;
        columns.name.width = "10%";

        columns.domain.title = WebInspector.UIString("Domain");
        columns.domain.width = "10%";

        columns.type.title = WebInspector.UIString("Type");
        columns.type.width = "8%";

        var typeToLabelMap = new Map;
        for (var key in WebInspector.Resource.Type) {
            var value = WebInspector.Resource.Type[key];
            typeToLabelMap.set(value, WebInspector.Resource.displayNameForType(value, true));
        }

        columns.type.scopeBar = WebInspector.TimelineDataGrid.createColumnScopeBar("network", typeToLabelMap);
        this._scopeBar = columns.type.scopeBar;

        columns.method.title = WebInspector.UIString("Method");
        columns.method.width = "6%";

        columns.scheme.title = WebInspector.UIString("Scheme");
        columns.scheme.width = "6%";

        columns.statusCode.title = WebInspector.UIString("Status");
        columns.statusCode.width = "6%";

        columns.cached.title = WebInspector.UIString("Cached");
        columns.cached.width = "6%";

        columns.size.title = WebInspector.UIString("Size");
        columns.size.width = "8%";
        columns.size.aligned = "right";

        columns.transferSize.title = WebInspector.UIString("Transferred");
        columns.transferSize.width = "8%";
        columns.transferSize.aligned = "right";

        columns.requestSent.title = WebInspector.UIString("Start Time");
        columns.requestSent.width = "9%";
        columns.requestSent.aligned = "right";

        columns.latency.title = WebInspector.UIString("Latency");
        columns.latency.width = "9%";
        columns.latency.aligned = "right";

        columns.duration.title = WebInspector.UIString("Duration");
        columns.duration.width = "9%";
        columns.duration.aligned = "right";

        for (var column in columns)
            columns[column].sortable = true;

        this._dataGrid = new WebInspector.TimelineDataGrid(columns);
        this._dataGrid.addEventListener(WebInspector.TimelineDataGrid.Event.FiltersDidChange, this._dataGridFiltersDidChange, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
        this._dataGrid.sortColumnIdentifierSetting = new WebInspector.Setting("network-timeline-view-sort", "requestSent");
        this._dataGrid.sortOrderSetting = new WebInspector.Setting("network-timeline-view-sort-order", WebInspector.DataGrid.SortOrder.Ascending);

        this.element.classList.add("network");
        this.addSubview(this._dataGrid);

        timeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);

        this._pendingRecords = [];
        this._resourceDataGridNodeMap = new Map;
    }

    // Public

    get selectionPathComponents()
    {
        if (!this._dataGrid.selectedNode || this._dataGrid.selectedNode.hidden)
            return null;

        console.assert(this._dataGrid.selectedNode instanceof WebInspector.ResourceTimelineDataGridNode);

        let pathComponent = new WebInspector.TimelineDataGridNodePathComponent(this._dataGrid.selectedNode, this._dataGrid.selectedNode.resource);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this.dataGridNodePathComponentSelected, this);
        return [pathComponent];
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

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        return this._dataGrid.treeElementMatchesActiveScopeFilters(treeElement);
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
        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement)
            return true;
        return super.canShowContentViewForTreeElement(treeElement);
    }

    showContentViewForTreeElement(treeElement)
    {
        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement) {
            WebInspector.showSourceCode(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element selected.", treeElement);
    }

    dataGridNodePathComponentSelected(event)
    {
        let pathComponent = event.data.pathComponent;
        console.assert(pathComponent instanceof WebInspector.TimelineDataGridNodePathComponent);

        let dataGridNode = pathComponent.timelineDataGridNode;
        console.assert(dataGridNode.dataGrid === this._dataGrid);

        dataGridNode.revealAndSelect();
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

        for (let resourceTimelineRecord of this._pendingRecords) {
            // Skip the record if it already exists in the grid.
            // FIXME: replace with this._dataGrid.findDataGridNode(resourceTimelineRecord.resource) once <https://webkit.org/b/155305> is fixed.
            let dataGridNode = this._resourceDataGridNodeMap.get(resourceTimelineRecord.resource);
            if (dataGridNode)
                continue;

            dataGridNode = new WebInspector.ResourceTimelineDataGridNode(resourceTimelineRecord, false, this);
            this._resourceDataGridNodeMap.set(resourceTimelineRecord.resource, dataGridNode);

            this._dataGrid.addRowInSortOrder(null, dataGridNode);
        }

        this._pendingRecords = [];
    }

    _networkTimelineRecordAdded(event)
    {
        var resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WebInspector.ResourceTimelineRecord);

        this._pendingRecords.push(resourceTimelineRecord);

        this.needsLayout();
    }

    _dataGridFiltersDidChange(event)
    {
        // FIXME: <https://webkit.org/b/154924> Web Inspector: hook up grid row filtering in the new Timelines UI
    }

    _dataGridNodeSelected(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }
};
