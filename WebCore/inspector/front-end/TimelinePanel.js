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
    WebInspector.AbstractTimelinePanel.call(this);

    this.element.addStyleClass("timeline");

    this._createOverview();
    this.createInterface();
    this.containerElement.id = "timeline-container";
    this.summaryBar.element.id = "timeline-summary";
    this.itemsGraphsElement.id = "timeline-graphs";

    this._createStatusbarButtons();

    this.calculator = new WebInspector.TimelineCalculator();
    for (category in this.categories)
        this.showCategory(category);
    this._sendRequestRecords = {};
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

    populateSidebar: function()
    {
        this.itemsTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("RECORDS"), {}, true);
        this.itemsTreeElement.expanded = true;
        this.sidebarTree.appendChild(this.itemsTreeElement);
    },

    _createStatusbarButtons: function()
    {
        this.toggleTimelineButton = new WebInspector.StatusBarButton("", "record-profile-status-bar-item");
        this.toggleTimelineButton.addEventListener("click", this._toggleTimelineButtonClicked.bind(this), false);

        this.clearButton = new WebInspector.StatusBarButton("", "timeline-clear-status-bar-item");
        this.clearButton.addEventListener("click", this.reset.bind(this), false);
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
        var formattedRecord = this._formatRecord(record);
        // Glue subsequent records with same category and title together if they are closer than 100ms to each other.
        if (this._lastRecord && (!record.children || !record.children.length) &&
                this._lastRecord.category == formattedRecord.category &&
                this._lastRecord.title == formattedRecord.title &&
                this._lastRecord.details == formattedRecord.details &&
                formattedRecord.startTime - this._lastRecord.endTime < 0.1) {
            this._lastRecord.endTime = formattedRecord.endTime;
            this._lastRecord.count++;
            this.refreshItem(this._lastRecord);
        } else {
            this.addItem(formattedRecord);

            for (var i = 0; record.children && i < record.children.length; ++i)
                this.addRecordToTimeline(record.children[i]);
            this._lastRecord = record.children && record.children.length ? null : formattedRecord;
        }
    },

    createItemTreeElement: function(item)
    {
        return new WebInspector.TimelineRecordTreeElement(item);
    },

    createItemGraph: function(item)
    {
        return new WebInspector.TimelineGraph(item);
    },

    _toggleTimelineButtonClicked: function()
    {
        if (this.toggleTimelineButton.toggled)
            InspectorController.stopTimelineProfiler();
        else
            InspectorController.startTimelineProfiler();
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

        // Make resource receive record last since request was sent; make finish record last since response received.
        if (record.type === WebInspector.TimelineAgent.RecordType.ResourceSendRequest) {
            this._sendRequestRecords[record.data.identifier] = formattedRecord;
        } else if (record.type === WebInspector.TimelineAgent.RecordType.ResourceReceiveResponse) {
            var sendRequestRecord = this._sendRequestRecords[record.data.identifier];
            sendRequestRecord._responseReceivedFormattedTime = formattedRecord.startTime;
            formattedRecord.startTime = sendRequestRecord.startTime;
            sendRequestRecord.details = this._getRecordDetails(record);
            this.refreshItem(sendRequestRecord);
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
            return WebInspector.displayNameForURL(this._sendRequestRecords[record.data.identifier].data.url);
        case WebInspector.TimelineAgent.RecordType.MarkTimeline:
            return record.data.message;
        default:
            return "";
        }
    },

    reset: function()
    {
        WebInspector.AbstractTimelinePanel.prototype.reset.call(this);
        this._lastRecord = null;
        this._overviewCalculator.reset();
        for (var category in this.categories)
            this._categoryGraphs[category].clearChunks();
        this._setWindowPosition(0, this._overviewGridElement.clientWidth);
        this._sendRequestRecords = {};
    },

    _createOverview: function()
    {
        var overviewPanelElement = document.createElement("div");
        overviewPanelElement.id = "timeline-overview-panel";
        this.element.appendChild(overviewPanelElement);

        this._overviewSidebarElement = document.createElement("div");
        this._overviewSidebarElement.id = "timeline-overview-sidebar";
        overviewPanelElement.appendChild(this._overviewSidebarElement);

        var overviewTreeElement = document.createElement("ol");
        overviewTreeElement.className = "sidebar-tree";
        this._overviewSidebarElement.appendChild(overviewTreeElement);
        var sidebarTree = new TreeOutline(overviewTreeElement);

        var categoriesTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("TIMELINES"), {}, true);
        categoriesTreeElement.expanded = true;
        sidebarTree.appendChild(categoriesTreeElement);

        for (var category in this.categories)
            categoriesTreeElement.appendChild(new WebInspector.TimelineCategoryTreeElement(this.categories[category]));

        this._overviewGridElement = document.createElement("div");
        this._overviewGridElement.id = "timeline-overview-grid";
        overviewPanelElement.appendChild(this._overviewGridElement);
        this._overviewGrid = new WebInspector.TimelineGrid(this._overviewGridElement);
        this._overviewGrid.itemsGraphsElement.id = "timeline-overview-graphs";

        this._categoryGraphs = {};
        for (var category in this.categories) {
            var categoryGraph = new WebInspector.TimelineCategoryGraph(this.categories[category]);
            this._categoryGraphs[category] = categoryGraph;
            this._overviewGrid.itemsGraphsElement.appendChild(categoryGraph.graphElement);
        }
        this._overviewGrid.setScrollAndDividerTop(0, 0);

        this._overviewWindowElement = document.createElement("div");
        this._overviewWindowElement.id = "timeline-overview-window";
        this._overviewWindowElement.addEventListener("mousedown", this._dragWindow.bind(this), false);
        this._overviewGridElement.appendChild(this._overviewWindowElement);

        this._leftResizeElement = document.createElement("div");
        this._leftResizeElement.className = "timeline-window-resizer";
        this._leftResizeElement.style.left = 0;
        this._overviewGridElement.appendChild(this._leftResizeElement);
        this._leftResizeElement.addEventListener("mousedown", this._resizeWindow.bind(this, this._leftResizeElement), false);

        this._rightResizeElement = document.createElement("div");
        this._rightResizeElement.className = "timeline-window-resizer timeline-window-resizer-right";
        this._rightResizeElement.style.right = 0;
        this._overviewGridElement.appendChild(this._rightResizeElement);
        this._rightResizeElement.addEventListener("mousedown", this._resizeWindow.bind(this, this._rightResizeElement), false);

        this._overviewCalculator = new WebInspector.TimelineCalculator();

        var separatorElement = document.createElement("div");
        separatorElement.id = "timeline-overview-separator";
        this.element.appendChild(separatorElement);
    },

    setSidebarWidth: function(width)
    {
        WebInspector.AbstractTimelinePanel.prototype.setSidebarWidth.call(this, width);
        this._overviewSidebarElement.style.width = width + "px";
    },

    updateMainViewWidth: function(width)
    {
        WebInspector.AbstractTimelinePanel.prototype.updateMainViewWidth.call(this, width);
        this._overviewGridElement.style.left = width + "px";
    },

    updateGraphDividersIfNeeded: function()
    {
        WebInspector.AbstractTimelinePanel.prototype.updateGraphDividersIfNeeded.call(this);
        this._overviewGrid.updateDividers(true, this._overviewCalculator);
    },

    refresh: function()
    {
        WebInspector.AbstractTimelinePanel.prototype.refresh.call(this);
        this.adjustScrollPosition();

        // Clear summary bars.
        var timelines = {};
        for (var category in this.categories) {
            timelines[category] = [];
            this._categoryGraphs[category].clearChunks();
        }

        // Create sparse arrays with 101 cells each to fill with chunks for a given category.
        for (var i = 0; i < this.items.length; ++i) {
            var record = this.items[i];
            this._overviewCalculator.updateBoundaries(record);
            var percentages = this._overviewCalculator.computeBarGraphPercentages(record);
            var end = Math.round(percentages.end);
            var categoryName = record.category.name;
            for (var j = Math.round(percentages.start); j <= end; ++j)
                timelines[categoryName][j] = true;
        }

        // Convert sparse arrays to continuous segments, render graphs for each.
        for (var category in this.categories) {
            var timeline = timelines[category];
            window.timelineSaved = timeline;
            var chunkStart = -1;
            for (var j = 0; j < 101; ++j) {
                if (timeline[j]) {
                    if (chunkStart === -1)
                        chunkStart = j;
                } else {
                    if (chunkStart !== -1) {
                        this._categoryGraphs[category].addChunk(chunkStart, j);
                        chunkStart = -1;
                    }
                }
            }
            if (chunkStart !== -1) {
                this._categoryGraphs[category].addChunk(chunkStart, 100);
                chunkStart = -1;
            }
        }
        this._overviewGrid.updateDividers(true, this._overviewCalculator);
    },

    _resizeWindow: function(resizeElement, event)
    {
        WebInspector.elementDragStart(resizeElement, this._windowResizeDragging.bind(this, resizeElement), this._endWindowDragging.bind(this), event, "col-resize");
    },

    _windowResizeDragging: function(resizeElement, event)
    {
        if (resizeElement === this._leftResizeElement)
            this._resizeWindowLeft(event.pageX - this._overviewGridElement.offsetLeft);
        else
            this._resizeWindowRight(event.pageX - this._overviewGridElement.offsetLeft);
        event.preventDefault();
    },

    _dragWindow: function(event)
    {
        WebInspector.elementDragStart(this._overviewWindowElement, this._windowDragging.bind(this, event.pageX,
            this._leftResizeElement.offsetLeft, this._rightResizeElement.offsetLeft), this._endWindowDragging.bind(this), event, "ew-resize");
    },

    _windowDragging: function(startX, windowLeft, windowRight, event)
    {
        var delta = event.pageX - startX;
        var start = windowLeft + delta;
        var end = windowRight + delta;
        var windowSize = windowRight - windowLeft;

        if (start < 0) {
            start = 0;
            end = windowSize;
        }

        if (end > this._overviewGridElement.clientWidth) {
            end = this._overviewGridElement.clientWidth;
            start = end - windowSize;
        }
        this._setWindowPosition(start, end);

        event.preventDefault();
    },

    _resizeWindowLeft: function(start)
    {
        // Glue to edge.
        if (start < 10)
            start = 0;
        this._setWindowPosition(start, null);
    },

    _resizeWindowRight: function(end)
    {
        // Glue to edge.
        if (end > this._overviewGridElement.clientWidth - 10)
            end = this._overviewGridElement.clientWidth;
        this._setWindowPosition(null, end);
    },

    _setWindowPosition: function(start, end)
    {
        this.calculator.reset();
        this.invalidateAllItems();
        if (typeof start === "number") {
          if (start > this._rightResizeElement.offsetLeft - 25)
              start = this._rightResizeElement.offsetLeft - 25;

          this.calculator.windowLeft = start / this._overviewGridElement.clientWidth;
          this._leftResizeElement.style.left = this.calculator.windowLeft*100 + "%";
          this._overviewWindowElement.style.left = this.calculator.windowLeft*100 + "%";
        }
        if (typeof end === "number") {
            if (end < this._leftResizeElement.offsetLeft + 30)
                end = this._leftResizeElement.offsetLeft + 30;

            this.calculator.windowRight = end / this._overviewGridElement.clientWidth;
            this._rightResizeElement.style.left = this.calculator.windowRight*100 + "%";
        }
        this._overviewWindowElement.style.width = (this.calculator.windowRight - this.calculator.windowLeft)*100 + "%";
        this.needsRefresh = true;
    },

    _endWindowDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
    }
}

WebInspector.TimelinePanel.prototype.__proto__ = WebInspector.AbstractTimelinePanel.prototype;


WebInspector.TimelineCategory = function(name, title, color)
{
    WebInspector.AbstractTimelineCategory.call(this, name, title, color);
}

WebInspector.TimelineCategory.prototype = {
}

WebInspector.TimelineCategory.prototype.__proto__ = WebInspector.AbstractTimelineCategory.prototype;



WebInspector.TimelineCategoryTreeElement = function(category)
{
    this._category = category;

    // Pass an empty title, the title gets made later in onattach.
    TreeElement.call(this, "", null, false);
}

WebInspector.TimelineCategoryTreeElement.prototype = {
    onattach: function()
    {
        this.listItemElement.removeChildren();
        this.listItemElement.addStyleClass("timeline-category-tree-item");
        this.listItemElement.addStyleClass("timeline-category-" + this._category.name);

        var label = document.createElement("label");

        var checkElement = document.createElement("input");
        checkElement.type = "checkbox";
        checkElement.className = "timeline-category-checkbox";
        checkElement.checked = true;
        checkElement.addEventListener("click", this._onCheckboxClicked.bind(this));
        label.appendChild(checkElement);

        var typeElement = document.createElement("span");
        typeElement.className = "type";
        typeElement.textContent = this._category.title;
        label.appendChild(typeElement);

        this.listItemElement.appendChild(label);
    },

    _onCheckboxClicked: function (event) {
        if (event.target.checked)
            WebInspector.panels.timeline.showCategory(this._category.name);
        else {
            WebInspector.panels.timeline.hideCategory(this._category.name);
            WebInspector.panels.timeline.adjustScrollPosition();
        }
        WebInspector.panels.timeline._categoryGraphs[this._category.name].dimmed = !event.target.checked;
    }
}

WebInspector.TimelineCategoryTreeElement.prototype.__proto__ = TreeElement.prototype;


WebInspector.TimelineRecordTreeElement = function(record)
{
    this._record = record;

    // Pass an empty title, the title gets made later in onattach.
    TreeElement.call(this, "", null, false);
}

WebInspector.TimelineRecordTreeElement.prototype = {
    onattach: function()
    {
        this.listItemElement.removeChildren();
        this.listItemElement.addStyleClass("timeline-tree-item");
        this.listItemElement.addStyleClass("timeline-category-" + this._record.category.name);

        var iconElement = document.createElement("span");
        iconElement.className = "timeline-tree-icon";
        this.listItemElement.appendChild(iconElement);

        this.typeElement = document.createElement("span");
        this.typeElement.className = "type";
        this.typeElement.textContent = this._record.title;
        this.listItemElement.appendChild(this.typeElement);

        if (this._record.details) {
            var separatorElement = document.createElement("span");
            separatorElement.className = "separator";
            separatorElement.textContent = " ";

            this.dataElement = document.createElement("span");
            this.dataElement.className = "data dimmed";
            this.dataElement.textContent = "(" + this._record.details + ")";
            this.dataElement.title = this._record.details;

            this.listItemElement.appendChild(separatorElement);
            this.listItemElement.appendChild(this.dataElement);
        }
    },

    refresh: function()
    {
        if (this._record.details) {
            this.dataElement.textContent = "(" + this._record.details + ")";
            this.dataElement.title = this._record.details;
        }

        if (this._record.count <= 1)
            return;

        if (!this.repeatCountElement) {
            this.repeatCountElement = document.createElement("span");
            this.repeatCountElement.className = "count";
            this.listItemElement.appendChild(this.repeatCountElement);
        }

        this.repeatCountElement.textContent = "\u2009\u00d7\u2009" + this._record.count;
    }
}

WebInspector.TimelineRecordTreeElement.prototype.__proto__ = TreeElement.prototype;


WebInspector.TimelineCalculator = function()
{
    WebInspector.AbstractTimelineCalculator.call(this);
    this.windowLeft = 0.0;
    this.windowRight = 1.0;
}

WebInspector.TimelineCalculator.prototype = {
    computeBarGraphPercentages: function(record)
    {
        var start = (record.startTime - this.minimumBoundary) / this.boundarySpan * 100;
        var end = (record.endTime - this.minimumBoundary) / this.boundarySpan * 100;
        return {start: start, end: end};
    },

    computePercentageFromEventTime: function(eventTime)
    {
        return ((eventTime - this.minimumBoundary) / this.boundarySpan) * 100;
    },

    computeBarGraphLabels: function(record)
    {
        return {tooltip: record.title};
    },

    get minimumBoundary()
    {
        if (typeof this.windowLeft === "number")
            return this._absoluteMinimumBoundary + this.windowLeft * (this._absoluteMaximumBoundary - this._absoluteMinimumBoundary);
        return this._absoluteMinimumBoundary;
    },

    get maximumBoundary()
    {
        if (typeof this.windowLeft === "number")
            return this._absoluteMinimumBoundary + this.windowRight * (this._absoluteMaximumBoundary - this._absoluteMinimumBoundary);
        return this._absoluteMaximumBoundary;
    },

    reset: function()
    {
        delete this._absoluteMinimumBoundary;
        delete this._absoluteMaximumBoundary;
    },

    updateBoundaries: function(record)
    {
        var didChange = false;

        var lowerBound = record.startTime;

        if (typeof this._absoluteMinimumBoundary === "undefined" || lowerBound < this._absoluteMinimumBoundary) {
            this._absoluteMinimumBoundary = lowerBound;
            didChange = true;
        }

        var upperBound = record.endTime;
        if (typeof this._absoluteMaximumBoundary === "undefined" || upperBound > this._absoluteMaximumBoundary) {
            this._absoluteMaximumBoundary = upperBound;
            didChange = true;
        }

        return didChange;
    },

    formatValue: function(value)
    {
        return Number.secondsToString(value + this.minimumBoundary - this._absoluteMinimumBoundary, WebInspector.UIString.bind(WebInspector));
    }
}

WebInspector.TimelineCalculator.prototype.__proto__ = WebInspector.AbstractTimelineCalculator.prototype;


WebInspector.TimelineCategoryGraph = function(category)
{
    this._category = category;

    this._graphElement = document.createElement("div");
    this._graphElement.className = "timeline-graph-side timeline-overview-graph-side";

    this._barAreaElement = document.createElement("div");
    this._barAreaElement.className = "timeline-graph-bar-area timeline-category-" + category.name;
    this._graphElement.appendChild(this._barAreaElement);
}

WebInspector.TimelineCategoryGraph.prototype = {
    get graphElement()
    {
        return this._graphElement;
    },

    addChunk: function(start, end)
    {
        var chunk = document.createElement("div");
        chunk.className = "timeline-graph-bar";
        this._barAreaElement.appendChild(chunk);
        chunk.style.setProperty("left", start + "%");
        chunk.style.setProperty("width", (end - start) + "%");
    },

    clearChunks: function()
    {
        this._barAreaElement.removeChildren();
    },

    set dimmed(dimmed)
    {
        if (dimmed)
            this._barAreaElement.removeStyleClass("timeline-category-" + this._category.name);
        else
            this._barAreaElement.addStyleClass("timeline-category-" + this._category.name);
    }
}


WebInspector.TimelineGraph = function(record)
{
    this.record = record;

    this._graphElement = document.createElement("div");
    this._graphElement.className = "timeline-graph-side";

    this._barAreaElement = document.createElement("div");
    this._barAreaElement.className = "timeline-graph-bar-area";
    this._graphElement.appendChild(this._barAreaElement);

    this._barElement = document.createElement("div");
    this._barElement.className = "timeline-graph-bar";
    this._barAreaElement.appendChild(this._barElement);

    this._graphElement.addStyleClass("timeline-category-" + record.category.name);
}

WebInspector.TimelineGraph.prototype = {
    get graphElement()
    {
        return this._graphElement;
    },

    refreshLabelPositions: function()
    {
    },

    refresh: function(calculator)
    {
        var percentages = calculator.computeBarGraphPercentages(this.record);
        var labels = calculator.computeBarGraphLabels(this.record);

        this._percentages = percentages;

        if (percentages.start > 100 || percentages.end < 0) {
            this._graphElement.addStyleClass("hidden");
            this.record._itemsTreeElement.listItemElement.addStyleClass("hidden");
        } else {
            this._barElement.style.setProperty("left", percentages.start + "%");
            this._barElement.style.setProperty("right", (100 - percentages.end) + "%");
            this._graphElement.removeStyleClass("hidden");
            this.record._itemsTreeElement.listItemElement.removeStyleClass("hidden");
        }
        var tooltip = (labels.tooltip || "");
        this._barElement.title = tooltip;
    }
}
