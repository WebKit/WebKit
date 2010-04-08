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

WebInspector.TimelineOverviewPane = function(categories)
{
    this.element = document.createElement("div");
    this.element.id = "timeline-overview-panel";

    this._categories = categories;
    this._overviewSidebarElement = document.createElement("div");
    this._overviewSidebarElement.id = "timeline-overview-sidebar";
    this.element.appendChild(this._overviewSidebarElement);

    var overviewTreeElement = document.createElement("ol");
    overviewTreeElement.className = "sidebar-tree";
    this._overviewSidebarElement.appendChild(overviewTreeElement);
    var sidebarTree = new TreeOutline(overviewTreeElement);

    var categoriesTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("TIMELINES"), {}, true);
    categoriesTreeElement.expanded = true;
    sidebarTree.appendChild(categoriesTreeElement);
    for (var categoryName in this._categories) {
        var category = this._categories[categoryName];
        categoriesTreeElement.appendChild(new WebInspector.TimelineCategoryTreeElement(category, this._onCheckboxClicked.bind(this, category)));
    }

    this._overviewGrid = new WebInspector.TimelineGrid();
    this._overviewGrid.element.id = "timeline-overview-grid";
    this._overviewGrid.itemsGraphsElement.id = "timeline-overview-graphs";
    this.element.appendChild(this._overviewGrid.element);

    this._categoryGraphs = {};
    var i = 0;
    for (var category in this._categories) {
        var categoryGraph = new WebInspector.TimelineCategoryGraph(this._categories[category], i++ % 2);
        this._categoryGraphs[category] = categoryGraph;
        this._overviewGrid.itemsGraphsElement.appendChild(categoryGraph.graphElement);
    }
    this._overviewGrid.setScrollAndDividerTop(0, 0);

    this._overviewWindowElement = document.createElement("div");
    this._overviewWindowElement.id = "timeline-overview-window";
    this._overviewWindowElement.addEventListener("mousedown", this._dragWindow.bind(this), false);
    this._overviewGrid.element.appendChild(this._overviewWindowElement);

    this._leftResizeElement = document.createElement("div");
    this._leftResizeElement.className = "timeline-window-resizer";
    this._leftResizeElement.style.left = 0;
    this._overviewGrid.element.appendChild(this._leftResizeElement);
    this._leftResizeElement.addEventListener("mousedown", this._resizeWindow.bind(this, this._leftResizeElement), false);

    this._rightResizeElement = document.createElement("div");
    this._rightResizeElement.className = "timeline-window-resizer timeline-window-resizer-right";
    this._rightResizeElement.style.right = 0;
    this._overviewGrid.element.appendChild(this._rightResizeElement);
    this._rightResizeElement.addEventListener("mousedown", this._resizeWindow.bind(this, this._rightResizeElement), false);

    this._overviewCalculator = new WebInspector.TimelineOverviewCalculator();

    var separatorElement = document.createElement("div");
    separatorElement.id = "timeline-overview-separator";
    this.element.appendChild(separatorElement);

    this.windowLeft = 0.0;
    this.windowRight = 1.0;
}

WebInspector.TimelineOverviewPane.prototype = {
    _onCheckboxClicked: function (category, event) {
        if (event.target.checked)
            category.hidden = false;
        else
            category.hidden = true;
        this._categoryGraphs[category.name].dimmed = !event.target.checked;
        this.dispatchEventToListeners("filter changed");
    },

    update: function(records, showShortEvents)
    {
        this._showShortEvents = showShortEvents;
        // Clear summary bars.
        var timelines = {};
        for (var category in this._categories) {
            timelines[category] = [];
            this._categoryGraphs[category].clearChunks();
        }

        function forAllRecords(recordsArray, callback)
        {
            if (!recordsArray)
                return;
            for (var i = 0; i < recordsArray.length; ++i) {
                callback(recordsArray[i]);
                forAllRecords(recordsArray[i].children, callback);
            }
        }

        // Create sparse arrays with 101 cells each to fill with chunks for a given category.
        this._overviewCalculator.reset();
        forAllRecords(records, this._overviewCalculator.updateBoundaries.bind(this._overviewCalculator));

        function markTimeline(record)
        {
            if (!(this._showShortEvents || record.isLong()))
                return;
            var percentages = this._overviewCalculator.computeBarGraphPercentages(record);

            var end = Math.round(percentages.end);
            var categoryName = record.category.name;
            for (var j = Math.round(percentages.start); j <= end; ++j)
                timelines[categoryName][j] = true;
        }
        forAllRecords(records, markTimeline.bind(this));

        // Convert sparse arrays to continuous segments, render graphs for each.
        for (var category in this._categories) {
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

    updateEventDividers: function(records, dividerConstructor)
    {
        this._overviewGrid.removeEventDividers();
        for (var i = 0; i < records.length; ++i) {
            var record = records[i];
            var positions = this._overviewCalculator.computeBarGraphPercentages(record);
            var divider = dividerConstructor(record);
            divider.style.left = positions.start + "%";
            this._overviewGrid.addEventDivider(divider);
        }
    },

    setSidebarWidth: function(width)
    {
        this._overviewSidebarElement.style.width = width + "px";
    },

    updateMainViewWidth: function(width)
    {
        this._overviewGrid.element.style.left = width + "px";
    },

    reset: function()
    {
        this.windowLeft = 0.0;
        this.windowRight = 1.0;
        this._overviewWindowElement.style.left = "0%";
        this._overviewWindowElement.style.width = "100%";
        this._leftResizeElement.style.left = "0%";
        this._rightResizeElement.style.left = "100%";
        this._overviewCalculator.reset();
        this._overviewGrid.updateDividers(true, this._overviewCalculator);
    },

    _resizeWindow: function(resizeElement, event)
    {
        WebInspector.elementDragStart(resizeElement, this._windowResizeDragging.bind(this, resizeElement), this._endWindowDragging.bind(this), event, "col-resize");
    },

    _windowResizeDragging: function(resizeElement, event)
    {
        if (resizeElement === this._leftResizeElement)
            this._resizeWindowLeft(event.pageX - this._overviewGrid.element.offsetLeft);
        else
            this._resizeWindowRight(event.pageX - this._overviewGrid.element.offsetLeft);
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

        if (end > this._overviewGrid.element.clientWidth) {
            end = this._overviewGrid.element.clientWidth;
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
        if (end > this._overviewGrid.element.clientWidth - 10)
            end = this._overviewGrid.element.clientWidth;
        this._setWindowPosition(null, end);
    },

    _setWindowPosition: function(start, end)
    {
        if (typeof start === "number") {
            if (start > this._rightResizeElement.offsetLeft - 4)
                start = this._rightResizeElement.offsetLeft - 4;

            this.windowLeft = start / this._overviewGrid.element.clientWidth;
            this._leftResizeElement.style.left = this.windowLeft * 100 + "%";
            this._overviewWindowElement.style.left = this.windowLeft * 100 + "%";
        }
        if (typeof end === "number") {
            if (end < this._leftResizeElement.offsetLeft + 12)
                end = this._leftResizeElement.offsetLeft + 12;

            this.windowRight = end / this._overviewGrid.element.clientWidth;
            this._rightResizeElement.style.left = this.windowRight * 100 + "%";
        }
        this._overviewWindowElement.style.width = (this.windowRight - this.windowLeft) * 100 + "%";
        this.dispatchEventToListeners("window changed");
    },

    _endWindowDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
    }
}

WebInspector.TimelineOverviewPane.prototype.__proto__ = WebInspector.Object.prototype;


WebInspector.TimelineOverviewCalculator = function()
{
    this._uiString = WebInspector.UIString.bind(WebInspector);
}

WebInspector.TimelineOverviewCalculator.prototype = {
    computeBarGraphPercentages: function(record)
    {
        var start = (record.startTime - this.minimumBoundary) / this.boundarySpan * 100;
        var end = (record.endTime - this.minimumBoundary) / this.boundarySpan * 100;
        return {start: start, end: end};
    },

    reset: function()
    {
        delete this.minimumBoundary;
        delete this.maximumBoundary;
    },

    updateBoundaries: function(record)
    {
        if (typeof this.minimumBoundary === "undefined" || record.startTime < this.minimumBoundary) {
            this.minimumBoundary = record.startTime;
            return true;
        }
        if (typeof this.maximumBoundary === "undefined" || record.endTime > this.maximumBoundary) {
            this.maximumBoundary = record.endTime;
            return true;
        }
        return false;
    },

    get boundarySpan()
    {
        return this.maximumBoundary - this.minimumBoundary;
    },

    formatValue: function(value)
    {
        return Number.secondsToString(value, this._uiString);
    }
}


WebInspector.TimelineCategoryTreeElement = function(category, onCheckboxClicked)
{
    this._category = category;
    this._onCheckboxClicked = onCheckboxClicked;
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
        checkElement.addEventListener("click", this._onCheckboxClicked);
        label.appendChild(checkElement);

        var typeElement = document.createElement("span");
        typeElement.className = "type";
        typeElement.textContent = this._category.title;
        label.appendChild(typeElement);

        this.listItemElement.appendChild(label);
    }
}

WebInspector.TimelineCategoryTreeElement.prototype.__proto__ = TreeElement.prototype;

WebInspector.TimelineCategoryGraph = function(category, isEven)
{
    this._category = category;

    this._graphElement = document.createElement("div");
    this._graphElement.className = "timeline-graph-side timeline-overview-graph-side" + (isEven ? " even" : "");

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
