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
    this.navigationSidebarTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
    this.navigationSidebarTreeOutline.element.classList.add(WebInspector.ScriptTimelineView.TreeOutlineStyleClassName);

    var columns = {eventType: {}, location: {}, startTime: {}, duration: {}};

    columns.eventType.title = WebInspector.UIString("Type");
    columns.eventType.width = "15%";
    columns.eventType.scopeBar = WebInspector.TimelineDataGrid.createColumnScopeBar("script", WebInspector.ScriptTimelineRecord.EventType);
    columns.eventType.hidden = true;

    columns.location.title = WebInspector.UIString("Location");
    columns.location.width = "15%";

    columns.startTime.title = WebInspector.UIString("Start Time");
    columns.startTime.width = "10%";
    columns.startTime.aligned = "right";
    columns.startTime.sort = "ascending";

    columns.duration.title = WebInspector.UIString("Duration");
    columns.duration.width = "10%";
    columns.duration.aligned = "right";

    for (var column in columns)
        columns[column].sortable = true;

    this._dataGrid = new WebInspector.ScriptTimelineDataGrid(this.navigationSidebarTreeOutline, columns);
    this._dataGrid.addEventListener(WebInspector.TimelineDataGrid.Event.FiltersDidChange, this._dataGridFiltersDidChange, this);

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

        for (var scriptTimelineRecord of this._pendingRecords) {
            var treeElement = new WebInspector.TimelineRecordTreeElement(scriptTimelineRecord, true, true);
            var dataGridNode = new WebInspector.ScriptTimelineDataGridNode(scriptTimelineRecord, this.zeroTime);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);
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

    _treeElementSelected: function(treeElement, selectedByUser)
    {
        if (this._dataGrid.shouldIgnoreSelectionEvent())
            return;

        if (!WebInspector.timelineSidebarPanel.canShowDifferentContentView())
            return;

        if (treeElement instanceof WebInspector.FolderTreeElement)
            return;

        if (!(treeElement instanceof WebInspector.TimelineRecordTreeElement)) {
            console.error("Unknown tree element selected.");
            return;
        }

        if (!treeElement.record.sourceCodeLocation) {
            WebInspector.timelineSidebarPanel.showTimelineView(WebInspector.TimelineRecord.Type.Script);
            return;
        }

        WebInspector.resourceSidebarPanel.showOriginalOrFormattedSourceCodeLocation(treeElement.record.sourceCodeLocation);
    }
};
