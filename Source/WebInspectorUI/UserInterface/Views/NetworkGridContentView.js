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

WebInspector.NetworkGridContentView = function(representedObject, extraArguments)
{
    console.assert(extraArguments);
    console.assert(extraArguments.networkSidebarPanel instanceof WebInspector.NetworkSidebarPanel);

    WebInspector.ContentView.call(this, representedObject);

    this._networkSidebarPanel = extraArguments.networkSidebarPanel;

    this._contentTreeOutline = this._networkSidebarPanel.contentTreeOutline;
    this._contentTreeOutline.onselect = this._treeElementSelected.bind(this);

    var columns = {domain: {}, type: {}, method: {}, scheme: {}, statusCode: {}, cached: {}, size: {}, transferSize: {}, requestSent: {}, latency: {}, duration: {}};

    columns.domain.title = WebInspector.UIString("Domain");
    columns.domain.width = "10%";

    columns.type.title = WebInspector.UIString("Type");
    columns.type.width = "8%";

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

    columns.transferSize.title = WebInspector.UIString("Transfered");
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

    this._dataGrid = new WebInspector.TimelineDataGrid(this._contentTreeOutline, columns);
    this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
    this._dataGrid.sortColumnIdentifier = "requestSent";
    this._dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;

    this.element.classList.add("network-grid");
    this.element.appendChild(this._dataGrid.element);

    var networkTimeline = WebInspector.timelineManager.persistentNetworkTimeline;
    networkTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);
    networkTimeline.addEventListener(WebInspector.Timeline.Event.Reset, this._networkTimelineReset, this);

    this._pendingRecords = [];
};

WebInspector.NetworkGridContentView.prototype = {
    constructor: WebInspector.NetworkGridContentView,
    __proto__: WebInspector.ContentView.prototype,

    // Public

    get navigationSidebarTreeOutline()
    {
        return this._contentTreeOutline;
    },

    get selectionPathComponents()
    {
        if (!this._contentTreeOutline.selectedTreeElement || this._contentTreeOutline.selectedTreeElement.hidden)
            return null;

        var pathComponent = new WebInspector.GeneralTreeElementPathComponent(this._contentTreeOutline.selectedTreeElement);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._treeElementPathComponentSelected, this);
        return [pathComponent];
    },

    get zeroTime()
    {
        return WebInspector.timelineManager.persistentNetworkTimeline.startTime;
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
        this._dataGrid.closed();
    },

    updateLayout: function()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledLayoutUpdateIdentifier);
            delete this._scheduledLayoutUpdateIdentifier;
        }

        this._dataGrid.updateLayout();

        this._processPendingRecords();
    },

    needsLayout: function()
    {
        if (!this._networkSidebarPanel.visible)
            return;

        if (this._scheduledLayoutUpdateIdentifier)
            return;

        this._scheduledLayoutUpdateIdentifier = requestAnimationFrame(this.updateLayout.bind(this));
    },

    reset: function()
    {
        this._contentTreeOutline.removeChildren();
        this._dataGrid.reset();
    },

    // Private

    _processPendingRecords: function()
    {
        if (!this._pendingRecords.length)
            return;

        for (var resourceTimelineRecord of this._pendingRecords) {
            // Skip the record if it already exists in the tree.
            var treeElement = this._contentTreeOutline.findTreeElement(resourceTimelineRecord.resource);
            if (treeElement)
                continue;

            treeElement = new WebInspector.ResourceTreeElement(resourceTimelineRecord.resource);
            var dataGridNode = new WebInspector.ResourceTimelineDataGridNode(resourceTimelineRecord, false, this);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);
        }

        this._pendingRecords = [];
    },

    _networkTimelineReset: function(event)
    {
        this.reset();
    },

    _networkTimelineRecordAdded: function(event)
    {
        var resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WebInspector.ResourceTimelineRecord);

        this._pendingRecords.push(resourceTimelineRecord);

        this.needsLayout();
    },

    _treeElementPathComponentSelected: function(event)
    {
        var dataGridNode = this._dataGrid.dataGridNodeForTreeElement(event.data.pathComponent.generalTreeElement);
        if (!dataGridNode)
            return;
        dataGridNode.revealAndSelect();
    },

    _treeElementSelected: function(treeElement, selectedByUser)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);

        if (!this._networkSidebarPanel.canShowDifferentContentView())
            return;

        if (treeElement instanceof WebInspector.ResourceTreeElement) {
            WebInspector.showRepresentedObject(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element", treeElement);
    },

    _dataGridNodeSelected: function(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }
};
