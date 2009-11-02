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

    this.createInterface();
    this.summaryBar.element.id = "timeline-summary";
    this.itemsGraphsElement.id = "timeline-graphs";

    this._createStatusbarButtons();

    this.calculator = new WebInspector.TimelineCalculator();

    this.filter(this.filterAllElement, false);
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
                rendering: new WebInspector.TimelineCategory("rendering", WebInspector.UIString("Rendering"), "rgb(164,60,255)"),
                other: new WebInspector.TimelineCategory("other", WebInspector.UIString("Other"), "rgb(186,186,186)")
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
        if (!this._recordStyles) {
            this._recordStyles = {};
            var recordTypes = WebInspector.TimelineAgent.RecordType;
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
            this._recordStyles["Other"] = { title: WebInspector.UIString("Other"), icon: 0, category: this.categories.other };
        }

        var style = this._recordStyles[record.type];
        if (!style)
            style = this._recordStyles["Other"];

        var formattedRecord = {};
        formattedRecord.category = style.category;
        formattedRecord.title = style.title;
        formattedRecord.startTime = record.startTime / 1000;
        formattedRecord.data = record.data;
        formattedRecord.count = 1;
        formattedRecord.type = record.type;
        formattedRecord.details = this._getRecordDetails(record);
        formattedRecord.endTime = (typeof record.endTime !== "undefined") ? record.endTime / 1000 : formattedRecord.startTime;
        return formattedRecord;
    },
    
    _getRecordDetails: function(record)
    {
        switch (record.type) {
        case WebInspector.TimelineAgent.RecordType.EventDispatch:
            return record.data.type;
        case WebInspector.TimelineAgent.RecordType.TimerInstall:
        case WebInspector.TimelineAgent.RecordType.TimerRemove:
        case WebInspector.TimelineAgent.RecordType.TimerFire:
            return record.data.timerId;
        case WebInspector.TimelineAgent.RecordType.XHRReadyStateChange:
        case WebInspector.TimelineAgent.RecordType.XHRLoad:
        case WebInspector.TimelineAgent.RecordType.EvaluateScript:
            return record.data.url;
        default:
            return "";
        }
    },

    reset: function()
    {
        WebInspector.AbstractTimelinePanel.prototype.reset.call(this);
        this._lastRecord = null;
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

            var dataElement = document.createElement("span");
            dataElement.className = "data";
            dataElement.textContent = "(" + this._record.details + ")";
            dataElement.addStyleClass("dimmed");
            this.listItemElement.appendChild(separatorElement);
            this.listItemElement.appendChild(dataElement);
        }
    },

    refresh: function()
    {
        if (this._record.count > 1)
            this.typeElement.textContent = this._record.title + " x " + this._record.count;
    }
}

WebInspector.TimelineRecordTreeElement.prototype.__proto__ = TreeElement.prototype;


WebInspector.TimelineCalculator = function()
{
    WebInspector.AbstractTimelineCalculator.call(this);
}

WebInspector.TimelineCalculator.prototype = {
    computeBarGraphPercentages: function(record)
    {
        var start = ((record.startTime - this.minimumBoundary) / this.boundarySpan) * 100;
        var end = ((record.endTime - this.minimumBoundary) / this.boundarySpan) * 100;
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

    updateBoundaries: function(record)
    {
        var didChange = false;

        var lowerBound = record.startTime;

        if (typeof this.minimumBoundary === "undefined" || lowerBound < this.minimumBoundary) {
            this.minimumBoundary = lowerBound;
            didChange = true;
        }

        var upperBound = record.endTime;
        if (typeof this.maximumBoundary === "undefined" || upperBound > this.maximumBoundary) {
            this.maximumBoundary = upperBound;
            didChange = true;
        }

        return didChange;
    },

    formatValue: function(value)
    {
        return Number.secondsToString(value, WebInspector.UIString.bind(WebInspector));
    },

}

WebInspector.TimelineCalculator.prototype.__proto__ = WebInspector.AbstractTimelineCalculator.prototype;


WebInspector.TimelineGraph = function(record)
{
    this.record = record;

    this._graphElement = document.createElement("div");
    this._graphElement.className = "timeline-graph-side";

    this._barAreaElement = document.createElement("div");
    this._barAreaElement.className = "timeline-graph-bar-area hidden";
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

        this._barAreaElement.removeStyleClass("hidden");

        if (!this._graphElement.hasStyleClass("timeline-category-" + this.record.category.name)) {
            this._graphElement.removeMatchingStyleClasses("timeline-category-\\w+");
            this._graphElement.addStyleClass("timeline-category-" + this.record.category.name);
        }

        this._barElement.style.setProperty("left", percentages.start + "%");
        this._barElement.style.setProperty("right", (100 - percentages.end) + "%");

        var tooltip = (labels.tooltip || "");
        this._barElement.title = tooltip;
    }
}
