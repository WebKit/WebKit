/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.TimelinePanel = function()
{
    WebInspector.Panel.call(this);
    this.element.addStyleClass("timeline");

    this._overviewPane = new WebInspector.TimelineOverviewPane(this.categories);
    this._overviewPane.addEventListener("window changed", this._windowChanged, this);
    this._overviewPane.addEventListener("filter changed", this._refresh, this);
    this.element.appendChild(this._overviewPane.element);

    this._sidebarBackgroundElement = document.createElement("div");
    this._sidebarBackgroundElement.className = "sidebar timeline-sidebar-background";
    this.element.appendChild(this._sidebarBackgroundElement);

    this._containerElement = document.createElement("div");
    this._containerElement.id = "timeline-container";
    this._containerElement.addEventListener("scroll", this._onScroll.bind(this), false);
    this.element.appendChild(this._containerElement);

    this.createSidebar(this._containerElement, this._containerElement);
    var itemsTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("RECORDS"), {}, true);
    itemsTreeElement.expanded = true;
    this.sidebarTree.appendChild(itemsTreeElement);

    this._sidebarListElement = document.createElement("div");
    this.sidebarElement.appendChild(this._sidebarListElement);

    this._containerContentElement = document.createElement("div");
    this._containerContentElement.id = "resources-container-content";
    this._containerElement.appendChild(this._containerContentElement);

    this._timelineGrid = new WebInspector.TimelineGrid();
    this._itemsGraphsElement = this._timelineGrid.itemsGraphsElement;
    this._itemsGraphsElement.id = "timeline-graphs";
    this._containerContentElement.appendChild(this._timelineGrid.element);

    this._topGapElement = document.createElement("div");
    this._topGapElement.className = "timeline-gap";
    this._itemsGraphsElement.appendChild(this._topGapElement);

    this._graphRowsElement = document.createElement("div");
    this._itemsGraphsElement.appendChild(this._graphRowsElement);

    this._bottomGapElement = document.createElement("div");
    this._bottomGapElement.className = "timeline-gap";
    this._itemsGraphsElement.appendChild(this._bottomGapElement);

    this._createStatusbarButtons();

    this._records = [];
    this._sendRequestRecords = {};
    this._calculator = new WebInspector.TimelineCalculator();
    this._boundariesAreValid = true;
}

WebInspector.TimelinePanel.prototype = {
    toolbarItemClass: "timeline",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Timeline");
    },

    get statusBarItems()
    {
        return [this.toggleTimelineButton.element, this.clearButton.element];
    },

    get categories()
    {
        if (!this._categories) {
            this._categories = {
                loading: new WebInspector.TimelineCategory("loading", WebInspector.UIString("Loading"), "rgb(47,102,236)"),
                scripting: new WebInspector.TimelineCategory("scripting", WebInspector.UIString("Scripting"), "rgb(157,231,119)"),
                rendering: new WebInspector.TimelineCategory("rendering", WebInspector.UIString("Rendering"), "rgb(164,60,255)")
            };
        }
        return this._categories;
    },

    _createStatusbarButtons: function()
    {
        this.toggleTimelineButton = new WebInspector.StatusBarButton("", "record-profile-status-bar-item");
        this.toggleTimelineButton.addEventListener("click", this._toggleTimelineButtonClicked.bind(this), false);

        this.clearButton = new WebInspector.StatusBarButton("", "timeline-clear-status-bar-item");
        this.clearButton.addEventListener("click", this.reset.bind(this), false);
    },

    _toggleTimelineButtonClicked: function()
    {
        if (this.toggleTimelineButton.toggled)
            InspectorBackend.stopTimelineProfiler();
        else
            InspectorBackend.startTimelineProfiler();
    },

    timelineWasStarted: function()
    {
        this.toggleTimelineButton.toggled = true;
    },

    timelineWasStopped: function()
    {
        this.toggleTimelineButton.toggled = false;
    },

    addRecordToTimeline: function(record)
    {
        this._innerAddRecordToTimeline(record, this._records);
        this._scheduleRefresh();
    },

    _innerAddRecordToTimeline: function(record, collection)
    {
        var formattedRecord = this._formatRecord(record);

        // Glue subsequent records with same category and title together if they are closer than 100ms to each other.
        if (this._lastRecord && (!record.children || !record.children.length) &&
                this._lastRecord.category == formattedRecord.category &&
                this._lastRecord.title == formattedRecord.title &&
                this._lastRecord.details == formattedRecord.details &&
                formattedRecord.startTime - this._lastRecord.endTime < 0.1) {
            this._lastRecord.endTime = formattedRecord.endTime;
            this._lastRecord.count++;
        } else {
            collection.push(formattedRecord);
            for (var i = 0; record.children && i < record.children.length; ++i) {
                if (!formattedRecord.children)
                    formattedRecord.children = [];
                var formattedChild = this._innerAddRecordToTimeline(record.children[i], formattedRecord.children);
                formattedChild.parent = formattedRecord;
            }
            this._lastRecord = record.children && record.children.length ? null : formattedRecord;
        }
        return formattedRecord;
    },

    _formatRecord: function(record)
    {
        var recordTypes = WebInspector.TimelineAgent.RecordType;
        if (!this._recordStyles) {
            this._recordStyles = {};
            this._recordStyles[recordTypes.EventDispatch] = { title: WebInspector.UIString("Event"), category: this.categories.scripting };
            this._recordStyles[recordTypes.Layout] = { title: WebInspector.UIString("Layout"), category: this.categories.rendering };
            this._recordStyles[recordTypes.RecalculateStyles] = { title: WebInspector.UIString("Recalculate Style"), category: this.categories.rendering };
            this._recordStyles[recordTypes.Paint] = { title: WebInspector.UIString("Paint"), category: this.categories.rendering };
            this._recordStyles[recordTypes.ParseHTML] = { title: WebInspector.UIString("Parse"), category: this.categories.loading };
            this._recordStyles[recordTypes.TimerInstall] = { title: WebInspector.UIString("Install Timer"), category: this.categories.scripting };
            this._recordStyles[recordTypes.TimerRemove] = { title: WebInspector.UIString("Remove Timer"), category: this.categories.scripting };
            this._recordStyles[recordTypes.TimerFire] = { title: WebInspector.UIString("Timer Fired"), category: this.categories.scripting };
            this._recordStyles[recordTypes.XHRReadyStateChange] = { title: WebInspector.UIString("XHR Ready State Change"), category: this.categories.scripting };
            this._recordStyles[recordTypes.XHRLoad] = { title: WebInspector.UIString("XHR Load"), category: this.categories.scripting };
            this._recordStyles[recordTypes.EvaluateScript] = { title: WebInspector.UIString("Evaluate Script"), category: this.categories.scripting };
            this._recordStyles[recordTypes.MarkTimeline] = { title: WebInspector.UIString("Mark"), category: this.categories.scripting };
            this._recordStyles[recordTypes.ResourceSendRequest] = { title: WebInspector.UIString("Send Request"), category: this.categories.loading };
            this._recordStyles[recordTypes.ResourceReceiveResponse] = { title: WebInspector.UIString("Receive Response"), category: this.categories.loading };
            this._recordStyles[recordTypes.ResourceFinish] = { title: WebInspector.UIString("Finish Loading"), category: this.categories.loading };
        }

        var style = this._recordStyles[record.type];
        if (!style)
            style = this._recordStyles[recordTypes.EventDispatch];

        var formattedRecord = {};
        formattedRecord.category = style.category;
        formattedRecord.title = style.title;
        formattedRecord.startTime = record.startTime / 1000;
        formattedRecord.data = record.data;
        formattedRecord.count = 1;
        formattedRecord.type = record.type;
        formattedRecord.endTime = (typeof record.endTime !== "undefined") ? record.endTime / 1000 : formattedRecord.startTime;
        formattedRecord.record = record;

        // Make resource receive record last since request was sent; make finish record last since response received.
        if (record.type === WebInspector.TimelineAgent.RecordType.ResourceSendRequest) {
            this._sendRequestRecords[record.data.identifier] = formattedRecord;
        } else if (record.type === WebInspector.TimelineAgent.RecordType.ResourceReceiveResponse) {
            var sendRequestRecord = this._sendRequestRecords[record.data.identifier];
            if (sendRequestRecord) { // False if we started instrumentation in the middle of request.
                sendRequestRecord._responseReceivedFormattedTime = formattedRecord.startTime;
                formattedRecord.startTime = sendRequestRecord.startTime;
                sendRequestRecord.details = this._getRecordDetails(record);
            }
        } else if (record.type === WebInspector.TimelineAgent.RecordType.ResourceFinish) {
            var sendRequestRecord = this._sendRequestRecords[record.data.identifier];
            if (sendRequestRecord) // False for main resource.
                formattedRecord.startTime = sendRequestRecord._responseReceivedFormattedTime;
        }
        formattedRecord.details = this._getRecordDetails(record);

        return formattedRecord;
    },

    _getRecordDetails: function(record)
    {
        switch (record.type) {
        case WebInspector.TimelineAgent.RecordType.EventDispatch:
            return record.data ? record.data.type : "";
        case WebInspector.TimelineAgent.RecordType.Paint:
            return record.data.width + "\u2009\u00d7\u2009" + record.data.height;
        case WebInspector.TimelineAgent.RecordType.TimerInstall:
        case WebInspector.TimelineAgent.RecordType.TimerRemove:
        case WebInspector.TimelineAgent.RecordType.TimerFire:
            return record.data.timerId;
        case WebInspector.TimelineAgent.RecordType.XHRReadyStateChange:
        case WebInspector.TimelineAgent.RecordType.XHRLoad:
        case WebInspector.TimelineAgent.RecordType.EvaluateScript:
        case WebInspector.TimelineAgent.RecordType.ResourceSendRequest:
            return WebInspector.displayNameForURL(record.data.url);
        case WebInspector.TimelineAgent.RecordType.ResourceReceiveResponse:
        case WebInspector.TimelineAgent.RecordType.ResourceFinish:
            var sendRequestRecord = this._sendRequestRecords[record.data.identifier];
            return sendRequestRecord ? WebInspector.displayNameForURL(sendRequestRecord.data.url) : "";
        case WebInspector.TimelineAgent.RecordType.MarkTimeline:
            return record.data.message;
        default:
            return "";
        }
    },

    setSidebarWidth: function(width)
    {
        WebInspector.Panel.prototype.setSidebarWidth.call(this, width);
        this._sidebarBackgroundElement.style.width = width + "px";
        this._overviewPane.setSidebarWidth(width);
    },

    updateMainViewWidth: function(width)
    {
        this._containerContentElement.style.left = width + "px";
        this._scheduleRefresh();
        this._overviewPane.updateMainViewWidth(width);
    },

    resize: function() {
        this._scheduleRefresh();
    },

    reset: function()
    {
        this._lastRecord = null;
        this._sendRequestRecords = {};
        this._records = [];
        this._boundariesAreValid = false;
        this._overviewPane.reset();
        this._adjustScrollPosition(0);
        this._refresh();
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);

        if (this._needsRefresh)
            this._refresh();
    },

    _onScroll: function(event)
    {
        var scrollTop = this._containerElement.scrollTop;
        var dividersTop = Math.max(0, scrollTop);
        this._timelineGrid.setScrollAndDividerTop(scrollTop, dividersTop);
        this._scheduleRefresh(true);
    },

    _windowChanged: function()
    {
        this._scheduleRefresh();
    },

    _scheduleRefresh: function(preserveBoundaries)
    {
        this._boundariesAreValid &= preserveBoundaries;
        if (this._needsRefresh)
            return;
        this._needsRefresh = true;

        if (this.visible && !("_refreshTimeout" in this)) {
            if (preserveBoundaries)
                this._refresh();
            else
                this._refreshTimeout = setTimeout(this._refresh.bind(this), 100);
        }
    },

    _refresh: function()
    {
        this._needsRefresh = false;
        if ("_refreshTimeout" in this) {
            clearTimeout(this._refreshTimeout);
            delete this._refreshTimeout;
        }
      
        if (!this._boundariesAreValid)
            this._overviewPane.update(this._records);
        this._refreshRecords(!this._boundariesAreValid);
        this._boundariesAreValid = true;
    },

    _refreshRecords: function(updateBoundaries)
    {
        if (updateBoundaries) {
            this._calculator.reset();
            this._calculator.windowLeft = this._overviewPane.windowLeft;
            this._calculator.windowRight = this._overviewPane.windowRight;

            for (var i = 0; i < this._records.length; ++i)
                this._calculator.updateBoundaries(this._records[i]);

            this._calculator.calculateWindow();
        }

        var recordsInWindow = [];
        for (var i = 0; i < this._records.length; ++i) {
            var record = this._records[i];
            var percentages = this._calculator.computeBarGraphPercentages(record);
            if (percentages.start < 100 && percentages.end >= 0 && !record.category.hidden)
                this._addToRecordsWindow(record, recordsInWindow);
        }

        // Calculate the visible area.
        var visibleTop = this._containerElement.scrollTop;
        var visibleBottom = visibleTop + this._containerElement.clientHeight;

        // Define row height, should be in sync with styles for timeline graphs.
        const rowHeight = 18;
        const expandOffset = 15;

        // Convert visible area to visible indexes. Always include top-level record for a visible nested record.
        var startIndex = Math.max(0, Math.min(Math.floor(visibleTop / rowHeight) - 1, recordsInWindow.length - 1));
        while (startIndex > 0 && recordsInWindow[startIndex].parent)
            startIndex--;
        var endIndex = Math.min(recordsInWindow.length, Math.ceil(visibleBottom / rowHeight));
        while (endIndex < recordsInWindow.length - 1 && recordsInWindow[endIndex].parent)
            endIndex++;

        // Resize gaps first.
        const top = (startIndex * rowHeight) + "px";
        this._topGapElement.style.height = top;
        this.sidebarElement.style.top = top;
        this.sidebarResizeElement.style.top = top;
        this._bottomGapElement.style.height = (recordsInWindow.length - endIndex) * rowHeight + "px";

        // Update visible rows.
        var listRowElement = this._sidebarListElement.firstChild;
        var width = this._graphRowsElement.offsetWidth;
        this._itemsGraphsElement.removeChild(this._graphRowsElement);
        var graphRowElement = this._graphRowsElement.firstChild;
        var scheduleRefreshCallback = this._scheduleRefresh.bind(this, true);
        for (var i = startIndex; i < endIndex; ++i) {
            var record = recordsInWindow[i];
            var isEven = !(i % 2);

            if (!listRowElement) {
                listRowElement = new WebInspector.TimelineRecordListRow().element;
                this._sidebarListElement.appendChild(listRowElement);
            }
            if (!graphRowElement) {
                graphRowElement = new WebInspector.TimelineRecordGraphRow(this._itemsGraphsElement, scheduleRefreshCallback, rowHeight).element;
                this._graphRowsElement.appendChild(graphRowElement);
            }

            listRowElement.listRow.update(record, isEven);
            graphRowElement.graphRow.update(record, isEven, this._calculator, width, expandOffset, i);

            listRowElement = listRowElement.nextSibling;
            graphRowElement = graphRowElement.nextSibling;
        }

        // Remove extra rows.
        while (listRowElement) {
            var nextElement = listRowElement.nextSibling;
            listRowElement.listRow.dispose();
            listRowElement = nextElement;
        }
        while (graphRowElement) {
            var nextElement = graphRowElement.nextSibling;
            graphRowElement.graphRow.dispose();
            graphRowElement = nextElement;
        }

        this._itemsGraphsElement.insertBefore(this._graphRowsElement, this._bottomGapElement);
        // Reserve some room for expand / collapse controls to the left for records that start at 0ms.
        var timelinePaddingLeft = this._calculator.windowLeft === 0 ? expandOffset : 0;
        if (updateBoundaries)
            this._timelineGrid.updateDividers(true, this._calculator, timelinePaddingLeft);
        this._adjustScrollPosition((recordsInWindow.length + 1) * rowHeight);
    },

    _addToRecordsWindow: function(record, recordsWindow)
    {
        recordsWindow.push(record);
        if (!record.collapsed) {
            var index = recordsWindow.length;
            for (var i = 0; record.children && i < record.children.length; ++i)
                this._addToRecordsWindow(record.children[i], recordsWindow);
            record.visibleChildrenCount = recordsWindow.length - index;
        }
    },

    _adjustScrollPosition: function(totalHeight)
    {
        // Prevent the container from being scrolled off the end.
        if ((this._containerElement.scrollTop + this._containerElement.offsetHeight) > totalHeight + 1)
            this._containerElement.scrollTop = (totalHeight - this._containerElement.offsetHeight);
    }
}

WebInspector.TimelinePanel.prototype.__proto__ = WebInspector.Panel.prototype;


WebInspector.TimelineCategory = function(name, title, color)
{
    this.name = name;
    this.title = title;
    this.color = color;
}


WebInspector.TimelineCalculator = function()
{
    this.reset();
    this.windowLeft = 0.0;
    this.windowRight = 1.0;
    this._uiString = WebInspector.UIString.bind(WebInspector);
}

WebInspector.TimelineCalculator.prototype = {
    computeBarGraphPercentages: function(record)
    {
        var start = (record.startTime - this.minimumBoundary) / this.boundarySpan * 100;
        var end = (record.endTime - this.minimumBoundary) / this.boundarySpan * 100;
        return {start: start, end: end};
    },

    calculateWindow: function()
    {
        this.minimumBoundary = this._absoluteMinimumBoundary + this.windowLeft * (this._absoluteMaximumBoundary - this._absoluteMinimumBoundary);
        this.maximumBoundary = this._absoluteMinimumBoundary + this.windowRight * (this._absoluteMaximumBoundary - this._absoluteMinimumBoundary);
        this.boundarySpan = this.maximumBoundary - this.minimumBoundary;
    },

    reset: function()
    {
        this._absoluteMinimumBoundary = -1;
        this._absoluteMaximumBoundary = -1;
    },

    updateBoundaries: function(record)
    {
        var lowerBound = record.startTime;
        if (this._absoluteMinimumBoundary === -1 || lowerBound < this._absoluteMinimumBoundary)
            this._absoluteMinimumBoundary = lowerBound;

        var upperBound = record.endTime;
        if (this._absoluteMaximumBoundary === -1 || upperBound > this._absoluteMaximumBoundary)
            this._absoluteMaximumBoundary = upperBound;
    },

    formatValue: function(value)
    {
        return Number.secondsToString(value + this.minimumBoundary - this._absoluteMinimumBoundary, this._uiString);
    }
}


WebInspector.TimelineRecordListRow = function()
{
    this.element = document.createElement("div");
    this.element.listRow = this;
    var iconElement = document.createElement("span");
    iconElement.className = "timeline-tree-icon";
    this.element.appendChild(iconElement);

    this._typeElement = document.createElement("span");
    this._typeElement.className = "type";
    this.element.appendChild(this._typeElement);

    var separatorElement = document.createElement("span");
    separatorElement.className = "separator";
    separatorElement.textContent = " ";

    this._dataElement = document.createElement("span");
    this._dataElement.className = "data dimmed";

    this._repeatCountElement = document.createElement("span");
    this._repeatCountElement.className = "count";

    this.element.appendChild(separatorElement);
    this.element.appendChild(this._dataElement);
    this.element.appendChild(this._repeatCountElement);
}

WebInspector.TimelineRecordListRow.prototype = {
    update: function(record, isEven)
    {
        this.element.className = "timeline-tree-item timeline-category-" + record.category.name + (isEven ? " even" : "");
        this._typeElement.textContent = record.title;

        if (record.details) {
            this._dataElement.textContent = "(" + record.details + ")";
            this._dataElement.title = record.details;
        } else {
            this._dataElement.textContent = "";
            this._dataElement.title = "";
        }

        if (record.count > 1)
            this._repeatCountElement.textContent = "\u2009\u00d7\u2009" + record.count;
        else
            this._repeatCountElement.textContent = "";
    },

    dispose: function()
    {
        this.element.parentElement.removeChild(this.element);
    }
}


WebInspector.TimelineRecordGraphRow = function(graphContainer, refreshCallback, rowHeight)
{
    this.element = document.createElement("div");
    this.element.graphRow = this;

    this._barAreaElement = document.createElement("div");
    this._barAreaElement.className = "timeline-graph-bar-area";
    this.element.appendChild(this._barAreaElement);

    this._barElement = document.createElement("div");
    this._barElement.className = "timeline-graph-bar";
    this._barAreaElement.appendChild(this._barElement);

    this._expandElement = document.createElement("div");
    this._expandElement.className = "timeline-expandable";
    graphContainer.appendChild(this._expandElement);

    var leftBorder = document.createElement("div");
    leftBorder.className = "timeline-expandable-left";
    this._expandElement.appendChild(leftBorder);

    this._expandElement.addEventListener("click", this._onClick.bind(this));
    this._refreshCallback = refreshCallback;
    this._rowHeight = rowHeight;
}

WebInspector.TimelineRecordGraphRow.prototype = {
    update: function(record, isEven, calculator, clientWidth, expandOffset, index)
    {
        this._record = record;
        this.element.className = "timeline-graph-side timeline-category-" + record.category.name + (isEven ? " even" : "");
        var percentages = calculator.computeBarGraphPercentages(record);
        var left = percentages.start / 100 * clientWidth;
        var width = (percentages.end - percentages.start) / 100 * clientWidth;
        this._barElement.style.left = (left + expandOffset) + "px";
        this._barElement.style.width = width + "px";

        if (record.visibleChildrenCount) {
            this._expandElement.style.top = index * this._rowHeight + "px";
            this._expandElement.style.left = left + "px";
            this._expandElement.style.width = Math.max(12, width + 25) + "px";
            if (!record.collapsed) {
                this._expandElement.style.height = (record.visibleChildrenCount + 1) * this._rowHeight + "px";
                this._expandElement.addStyleClass("timeline-expandable-expanded");
                this._expandElement.removeStyleClass("timeline-expandable-collapsed");
            } else {
                this._expandElement.style.height = this._rowHeight + "px";
                this._expandElement.addStyleClass("timeline-expandable-collapsed");
                this._expandElement.removeStyleClass("timeline-expandable-expanded");
            }
            this._expandElement.removeStyleClass("hidden");
        } else {
            this._expandElement.addStyleClass("hidden");
        }
    },

    _onClick: function(event)
    {
        this._record.collapsed = !this._record.collapsed;
        this._refreshCallback();
    },

    dispose: function()
    {
        this.element.parentElement.removeChild(this.element);
        this._expandElement.parentElement.removeChild(this._expandElement);
    }
}
