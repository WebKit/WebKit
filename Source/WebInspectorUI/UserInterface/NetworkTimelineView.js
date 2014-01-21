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

WebInspector.NetworkTimelineView = function(recording)
{
    WebInspector.TimelineView.call(this);

    this.navigationSidebarTreeOutline.onselect = this._treeElementSelected.bind(this);
    this.navigationSidebarTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
    this.navigationSidebarTreeOutline.element.classList.add(WebInspector.NetworkTimelineView.TreeOutlineStyleClassName);

    var columns = {domain: {}, type: {}, method: {}, scheme: {}, statusCode: {}, cached: {}, size: {}, transferSize: {}, requestSent: {}, latency: {}, duration: {}};

    columns.domain.title = WebInspector.UIString("Domain");
    columns.domain.width = "10%";

    columns.type.title = WebInspector.UIString("Type");
    columns.type.width = "8%";
    columns.type.scopeBar = WebInspector.TimelineDataGrid.createColumnScopeBar("network", WebInspector.Resource.Type);

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
    columns.requestSent.sort = "ascending";

    columns.latency.title = WebInspector.UIString("Latency");
    columns.latency.width = "9%";
    columns.latency.aligned = "right";

    columns.duration.title = WebInspector.UIString("Duration");
    columns.duration.width = "9%";
    columns.duration.aligned = "right";

    for (var column in columns)
        columns[column].sortable = true;

    this._dataGrid = new WebInspector.TimelineDataGrid(this.navigationSidebarTreeOutline, columns);
    this._dataGrid.addEventListener(WebInspector.TimelineDataGrid.Event.FiltersDidChange, this._dataGridFiltersDidChange, this);

    this.element.classList.add(WebInspector.NetworkTimelineView.StyleClassName);
    this.element.appendChild(this._dataGrid.element);

    var networkTimeline = recording.timelines.get(WebInspector.TimelineRecord.Type.Network);
    networkTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);

    this._pendingRecords = [];
};

WebInspector.NetworkTimelineView.StyleClassName = "network";
WebInspector.NetworkTimelineView.TreeOutlineStyleClassName = "network";

WebInspector.NetworkTimelineView.prototype = {
    constructor: WebInspector.NetworkTimelineView,
    __proto__: WebInspector.TimelineView.prototype,

    // Public

    get navigationSidebarTreeOutlineLabel()
    {
        return WebInspector.UIString("Resources");
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

        this._processPendingRecords();
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

    // Private

    _processPendingRecords: function()
    {
        if (!this._pendingRecords.length)
            return;

        for (var resourceTimelineRecord of this._pendingRecords) {
            // Skip the record if it already exists in the tree.
            var treeElement = this.navigationSidebarTreeOutline.findTreeElement(resourceTimelineRecord.resource);
            if (treeElement)
                continue;

            treeElement = new WebInspector.ResourceTreeElement(resourceTimelineRecord.resource);
            var dataGridNode = new WebInspector.ResourceTimelineDataGridNode(resourceTimelineRecord, false, this);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);
        }

        this._pendingRecords = [];
    },

    _networkTimelineRecordAdded: function(event)
    {
        var resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WebInspector.ResourceTimelineRecord);

        this._pendingRecords.push(resourceTimelineRecord);

        this.needsLayout();
    },

    _dataGridFiltersDidChange: function(event)
    {
        WebInspector.timelineSidebarPanel.updateFilter();
    },

    _treeElementSelected: function(treeElement, selectedByUser)
    {
        if (this._dataGrid.shouldIgnoreSelectionEvent())
            return;

        if (!WebInspector.timelineSidebarPanel.canShowDifferentContentView())
            return;

        if (treeElement instanceof WebInspector.FolderTreeElement)
            return;

        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement) {
            WebInspector.resourceSidebarPanel.showSourceCode(treeElement.representedObject);
            return;
        }

        console.error("Unknown tree element selected.");
    }
};
