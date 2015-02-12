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

WebInspector.LayoutTimelineView = function(timeline)
{
    WebInspector.TimelineView.call(this, timeline);

    console.assert(timeline.type === WebInspector.TimelineRecord.Type.Layout);

    this.navigationSidebarTreeOutline.onselect = this._treeElementSelected.bind(this);
    this.navigationSidebarTreeOutline.ondeselect = this._treeElementDeselected.bind(this);
    this.navigationSidebarTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
    this.navigationSidebarTreeOutline.element.classList.add(WebInspector.LayoutTimelineView.TreeOutlineStyleClassName);

    var columns = {eventType: {}, initiatorCallFrame: {}, width: {}, height: {}, startTime: {}, duration: {}};

    columns.eventType.title = WebInspector.UIString("Type");
    columns.eventType.width = "15%";

    var typeToLabelMap = new Map;
    for (var key in WebInspector.LayoutTimelineRecord.EventType) {
        var value = WebInspector.LayoutTimelineRecord.EventType[key];
        typeToLabelMap.set(value, WebInspector.LayoutTimelineRecord.displayNameForEventType(value));
    }

    columns.eventType.scopeBar = WebInspector.TimelineDataGrid.createColumnScopeBar("layout", typeToLabelMap);
    columns.eventType.hidden = true;

    columns.initiatorCallFrame.title = WebInspector.UIString("Initiator");
    columns.initiatorCallFrame.width = "25%";

    columns.width.title = WebInspector.UIString("Width");
    columns.width.width = "8%";

    columns.height.title = WebInspector.UIString("Height");
    columns.height.width = "8%";

    columns.startTime.title = WebInspector.UIString("Start Time");
    columns.startTime.width = "8%";
    columns.startTime.aligned = "right";

    columns.duration.title = WebInspector.UIString("Duration");
    columns.duration.width = "8%";
    columns.duration.aligned = "right";

    for (var column in columns)
        columns[column].sortable = true;

    this._dataGrid = new WebInspector.LayoutTimelineDataGrid(this.navigationSidebarTreeOutline, columns);
    this._dataGrid.addEventListener(WebInspector.TimelineDataGrid.Event.FiltersDidChange, this._dataGridFiltersDidChange, this);
    this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);

    this._dataGrid.sortColumnIdentifier = "startTime";
    this._dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;

    this._hoveredTreeElement = null;
    this._hoveredDataGridNode = null;
    this._showingHighlight = false;
    this._showingHighlightForRecord = null;

    this._dataGrid.element.addEventListener("mouseover", this._mouseOverDataGrid.bind(this));
    this._dataGrid.element.addEventListener("mouseleave", this._mouseLeaveDataGrid.bind(this));
    this.navigationSidebarTreeOutline.element.addEventListener("mouseover", this._mouseOverTreeOutline.bind(this));
    this.navigationSidebarTreeOutline.element.addEventListener("mouseleave", this._mouseLeaveTreeOutline.bind(this));

    this.element.classList.add(WebInspector.LayoutTimelineView.StyleClassName);
    this.element.appendChild(this._dataGrid.element);

    timeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._layoutTimelineRecordAdded, this);

    this._pendingRecords = [];
};

WebInspector.LayoutTimelineView.StyleClassName = "layout";
WebInspector.LayoutTimelineView.TreeOutlineStyleClassName = "layout";

WebInspector.LayoutTimelineView.prototype = {
    constructor: WebInspector.LayoutTimelineView,
    __proto__: WebInspector.TimelineView.prototype,

    // Public

    get navigationSidebarTreeOutlineLabel()
    {
        return WebInspector.UIString("Records");
    },

    shown: function()
    {
        WebInspector.TimelineView.prototype.shown.call(this);

        this._updateHighlight();

        this._dataGrid.shown();
    },

    hidden: function()
    {
        this._hideHighlightIfNeeded();

        this._dataGrid.hidden();

        WebInspector.TimelineView.prototype.hidden.call(this);
    },

    filterDidChange: function()
    {
        WebInspector.TimelineView.prototype.filterDidChange.call(this);

        this._updateHighlight();
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

        this._hideHighlightIfNeeded();

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

    // Private

    _processPendingRecords: function()
    {
        if (!this._pendingRecords.length)
            return;

        for (var layoutTimelineRecord of this._pendingRecords) {
            var treeElement = new WebInspector.TimelineRecordTreeElement(layoutTimelineRecord, WebInspector.SourceCodeLocation.NameStyle.Short);
            var dataGridNode = new WebInspector.LayoutTimelineDataGridNode(layoutTimelineRecord, this.zeroTime);

            this._dataGrid.addRowInSortOrder(treeElement, dataGridNode);
        }

        this._pendingRecords = [];
    },

    _layoutTimelineRecordAdded: function(event)
    {
        var layoutTimelineRecord = event.data.record;
        console.assert(layoutTimelineRecord instanceof WebInspector.LayoutTimelineRecord);

        this._pendingRecords.push(layoutTimelineRecord);

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

    _treeElementDeselected: function(treeElement)
    {
        if (treeElement.status)
            treeElement.status = "";

        this._updateHighlight();
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

        this._updateHighlight();

        if (!treeElement.record.sourceCodeLocation) {
            WebInspector.timelineSidebarPanel.showTimelineViewForTimeline(this.representedObject);
            return;
        }

        WebInspector.resourceSidebarPanel.showOriginalOrFormattedSourceCodeLocation(treeElement.record.sourceCodeLocation);
        this._updateTreeElementWithCloseButton(treeElement);
    },

    _updateTreeElementWithCloseButton: function(treeElement)
    {
        if (this._closeStatusButton) {
            treeElement.status = this._closeStatusButton.element;
            return;
        }

        wrappedSVGDocument(platformImagePath("Close.svg"), null, WebInspector.UIString("Close resource view"), function(element) {
            this._closeStatusButton = new WebInspector.TreeElementStatusButton(element);
            this._closeStatusButton.addEventListener(WebInspector.TreeElementStatusButton.Event.Clicked, this._closeStatusButtonClicked, this);
            if (treeElement === this.navigationSidebarTreeOutline.selectedTreeElement)
                this._updateTreeElementWithCloseButton(treeElement);
        }.bind(this));
    },

    _closeStatusButtonClicked: function(event)
    {
        this.navigationSidebarTreeOutline.selectedTreeElement.deselect();
        WebInspector.timelineSidebarPanel.showTimelineViewForTimeline(this.representedObject);
    },

    _updateHighlight: function()
    {
        var record = this._hoveredOrSelectedRecord();
        if (!record) {
            this._hideHighlightIfNeeded();
            return;
        }

        this._showHighlightForRecord(record);
    },

    _showHighlightForRecord: function(record)
    {
        if (this._showingHighlightForRecord === record)
            return;

        this._showingHighlightForRecord = record;

        const contentColor = {r: 111, g: 168, b: 220, a: 0.66};
        const outlineColor = {r: 255, g: 229, b: 153, a: 0.66};

        var quad = record.quad;
        if (quad && DOMAgent.highlightQuad) {
            DOMAgent.highlightQuad(quad.toProtocol(), contentColor, outlineColor);
            this._showingHighlight = true;
            return;
        }

        // COMPATIBILITY (iOS 6): iOS 6 included Rect information instead of Quad information. Fallback to highlighting the rect.
        var rect = record.rect;
        if (rect) {
            DOMAgent.highlightRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, contentColor, outlineColor);
            this._showingHighlight = true;
            return;
        }

        // This record doesn't have a highlight, so hide any existing highlight.
        if (this._showingHighlight) {
            this._showingHighlight = false;
            DOMAgent.hideHighlight();
        }
    },

    _hideHighlightIfNeeded: function()
    {
        this._showingHighlightForRecord = null;

        if (this._showingHighlight) {
            this._showingHighlight = false;
            DOMAgent.hideHighlight();
        }
    },

    _hoveredOrSelectedRecord: function()
    {
        if (this._hoveredDataGridNode)
            return this._hoveredDataGridNode.record;

        if (this._hoveredTreeElement)
            return this._hoveredTreeElement.record;

        if (this._dataGrid.selectedNode) {
            var treeElement = this._dataGrid.treeElementForDataGridNode(this._dataGrid.selectedNode);
            if (treeElement.revealed())
                return this._dataGrid.selectedNode.record;
        }

        return null;
    },

    _mouseOverDataGrid: function(event)
    {
        var hoveredDataGridNode = this._dataGrid.dataGridNodeFromNode(event.target);
        if (!hoveredDataGridNode)
            return;

        this._hoveredDataGridNode = hoveredDataGridNode;
        this._updateHighlight();
    },

    _mouseLeaveDataGrid: function(event)
    {
        this._hoveredDataGridNode = null;
        this._updateHighlight();
    },

    _mouseOverTreeOutline: function(event)
    {
        var hoveredTreeElement = this.navigationSidebarTreeOutline.treeElementFromNode(event.target);
        if (!hoveredTreeElement)
            return;

        this._hoveredTreeElement = hoveredTreeElement;
        this._updateHighlight();
    },

    _mouseLeaveTreeOutline: function(event)
    {
        this._hoveredTreeElement = null;
        this._updateHighlight();
    }
};
