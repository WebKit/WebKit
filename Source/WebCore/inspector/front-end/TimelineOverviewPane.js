/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @implements {WebInspector.TimelinePresentationModel.Filter}
 */
WebInspector.TimelineOverviewPane = function(presentationModel)
{
    this.element = document.createElement("div");
    this.element.className = "fill";

    this._startAtZero = false;
    this._topLevelRecordIndex = 0;
    this._windowIndexLeft = 0;
    this._windowIndexRight = null;

    this._presentationModel = presentationModel;

    this.element = document.createElement("div");
    this.element.id = "timeline-overview-panel";

    this._topPaneSidebarElement = document.createElement("div");
    this._topPaneSidebarElement.id = "timeline-overview-sidebar";

    var overviewTreeElement = document.createElement("ol");
    overviewTreeElement.className = "sidebar-tree";
    this._topPaneSidebarElement.appendChild(overviewTreeElement);
    this.element.appendChild(this._topPaneSidebarElement);

    var topPaneSidebarTree = new TreeOutline(overviewTreeElement);
    this._timelinesOverviewItem = new WebInspector.SidebarTreeElement("resources-time-graph-sidebar-item", WebInspector.UIString("Timelines"));
    topPaneSidebarTree.appendChild(this._timelinesOverviewItem);
    this._timelinesOverviewItem.revealAndSelect(false);
    this._timelinesOverviewItem.onselect = this._showTimelines.bind(this);

    this._memoryOverviewItem = new WebInspector.SidebarTreeElement("resources-size-graph-sidebar-item", WebInspector.UIString("Memory"));
    topPaneSidebarTree.appendChild(this._memoryOverviewItem);
    this._memoryOverviewItem.onselect = this._showMemoryGraph.bind(this);

    this._overviewGrid = new WebInspector.TimelineGrid();
    this._overviewGrid.element.id = "timeline-overview-grid";
    this._overviewGrid.itemsGraphsElement.id = "timeline-overview-timelines";

    this._heapGraph = new WebInspector.HeapGraph();
    this._heapGraph.element.id = "timeline-overview-memory";
    this._overviewGrid.element.insertBefore(this._heapGraph.element, this._overviewGrid.itemsGraphsElement);

    this._overviewWindow = new WebInspector.TimelineOverviewWindow(this._overviewGrid.element);
    this._overviewWindow.addEventListener(WebInspector.TimelineOverviewWindow.Events.WindowChanged, this._fireWindowChanged, this);

    this.element.appendChild(this._overviewGrid.element);

    var separatorElement = document.createElement("div");
    separatorElement.id = "timeline-overview-separator";
    this.element.appendChild(separatorElement);
 
    this._categoryGraphs = {};
    var i = 0;
    var categories = this._presentationModel.categories;
    for (var category in categories) {
        var categoryGraph = new WebInspector.TimelineCategoryGraph(categories[category], i++ % 2);
        this._categoryGraphs[category] = categoryGraph;
        this._overviewGrid.itemsGraphsElement.appendChild(categoryGraph.graphElement);
        categories[category].addEventListener(WebInspector.TimelineCategory.Events.VisibilityChanged, this._onCategoryVisibilityChanged, this);
    }

    this._overviewGrid.setScrollAndDividerTop(0, 0);

    this._overviewCalculator = new WebInspector.TimelineOverviewCalculator();
}

WebInspector.TimelineOverviewPane.MinSelectableSize = 12;

WebInspector.TimelineOverviewPane.WindowScrollSpeedFactor = .3;

WebInspector.TimelineOverviewPane.ResizerOffset = 3.5; // half pixel because offset values are not rounded but ceiled

WebInspector.TimelineOverviewPane.Mode = {
    Events: "Events",
    Memory: "Memory"
};

WebInspector.TimelineOverviewPane.Events = {
    ModeChanged: "ModeChanged",
    WindowChanged: "WindowChanged"
};

WebInspector.TimelineOverviewPane.prototype = {
    _showTimelines: function()
    {
        this._timelinesOverviewItem.revealAndSelect(false);
        this._heapGraph.hide();
        this._overviewGrid.itemsGraphsElement.removeStyleClass("hidden");
        this.dispatchEventToListeners(WebInspector.TimelineOverviewPane.Events.ModeChanged, WebInspector.TimelineOverviewPane.Mode.Events);
    },

    _showMemoryGraph: function()
    {
        this._memoryOverviewItem.revealAndSelect(false);
        this._heapGraph.show();
        this._heapGraph.update(this._records);
        this._overviewGrid.itemsGraphsElement.addStyleClass("hidden");
        this.dispatchEventToListeners(WebInspector.TimelineOverviewPane.Events.ModeChanged, WebInspector.TimelineOverviewPane.Mode.Memory);
    },

    /**
     * @param {boolean} enabled
     */
    setStartAtZero: function(enabled)
    {
        if (this._startAtZero === enabled)
            return;
        this._startAtZero = enabled;
        if (enabled) {
            if (this._heapGraph.visible)
                this._showTimelines();
            this.element.addStyleClass("timeline-start-at-zero");
            this._startAtZeroOverview = new WebInspector.TimelineStartAtZeroOverview(this);
            this._startAtZeroOverview.show(this.element);
            this._startAtZeroOverview.update(this._records);
        } else {
            this.element.removeStyleClass("timeline-start-at-zero");
            this._startAtZeroOverview.detach();
            this._startAtZeroOverview = null;
            this._showTimelines();
        }
    },

    _onCategoryVisibilityChanged: function(event)
    {
        var category = event.data;
        this._categoryGraphs[category.name].dimmed = category.hidden;
    },

    update: function(records, showShortEvents)
    {
        this._topLevelRecordIndex = 0;
        this._records = records;
        this._showShortEvents = showShortEvents;
        // Clear summary bars.
        var timelines = {};
        for (var category in this._presentationModel.categories) {
            timelines[category] = [];
            this._categoryGraphs[category].clearChunks();
        }

        // Create sparse arrays with 101 cells each to fill with chunks for a given category.
        this._overviewCalculator.reset();
        
        function updateBoundaries(record)
        {
            this._overviewCalculator.updateBoundaries(record);
            return false;
        }
        
        WebInspector.TimelinePanel.forAllRecords(records, updateBoundaries.bind(this));

        function markPercentagesForRecord(record)
        {
            if (!(this._showShortEvents || record.isLong()))
                return;
            var percentages = this._overviewCalculator.computeBarGraphPercentages(record);

            var end = Math.round(percentages.end);
            var categoryName = record.category.name;
            for (var j = Math.round(percentages.start); j <= end; ++j)
                timelines[categoryName][j] = true;
        }
        WebInspector.TimelinePanel.forAllRecords(records, markPercentagesForRecord.bind(this));

        // Convert sparse arrays to continuous segments, render graphs for each.
        for (var category in this._presentationModel.categories) {
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

        this._heapGraph.setSize(this._overviewGrid.element.offsetWidth, 60);
        if (this._heapGraph.visible)
            this._heapGraph.update(records);
        if (this._startAtZeroOverview)
            this._startAtZeroOverview.update(records);
        this._overviewGrid.updateDividers(true, this._overviewCalculator);
    },

    updateEventDividers: function(records, dividerConstructor)
    {
        this._overviewGrid.removeEventDividers();
        var dividers = [];
        for (var i = 0; i < records.length; ++i) {
            var record = records[i];
            var positions = this._overviewCalculator.computeBarGraphPercentages(record);
            var dividerPosition = Math.round(positions.start * 10);
            if (dividers[dividerPosition])
                continue;
            var divider = dividerConstructor(record);
            divider.style.left = positions.start + "%";
            dividers[dividerPosition] = divider;
        }
        this._overviewGrid.addEventDividers(dividers);
    },

    sidebarResized: function(width)
    {
        this._overviewGrid.element.style.left = width + "px";
        this._topPaneSidebarElement.style.width = width + "px";
    },

    reset: function()
    {
        this._overviewWindow.reset();
        this._overviewCalculator.reset();
        this._overviewGrid.updateDividers(true, this._overviewCalculator);
        if (this._startAtZeroOverview)
            this._startAtZeroOverview.reset();
    },

    scrollWindow: function(event)
    {
        this._overviewWindow.scrollWindow(event);
    },

    /**
     * @param {WebInspector.TimelinePresentationModel.Record} record
     */
    accept: function(record)
    {
        if (this._startAtZeroOverview) {
            if (record.parent === this._presentationModel.rootRecord())
                ++this._topLevelRecordIndex;
            return this._topLevelRecordIndex > this._windowIndexLeft &&
                (typeof this._windowIndexRight !== "number" || this._topLevelRecordIndex <= this._windowIndexRight);
        } else {
            var absoluteMin = this._presentationModel.minimumRecordTime();
            var absoluteMax = this._presentationModel.maximumRecordTime();
            var windowLeft = absoluteMin + (absoluteMax - absoluteMin) * this.windowLeft();
            var windowRight = absoluteMin + (absoluteMax - absoluteMin) * this.windowRight();
            return record.endTime >= windowLeft && record.startTime <= windowRight;
        }
    },

    _setWindowIndices: function(indexLeft, indexRight)
    {
        this._windowIndexLeft = indexLeft;
        this._windowIndexRight = indexRight;
        this._fireWindowChanged();
    },

    windowLeft: function()
    {
        return this._overviewWindow.windowLeft;
    },

    windowRight: function()
    {
        return this._overviewWindow.windowRight;
    },

    _fireWindowChanged: function()
    {
        this.dispatchEventToListeners(WebInspector.TimelineOverviewPane.Events.WindowChanged);
    }
}

WebInspector.TimelineOverviewPane.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {Element} parentElement
 */
WebInspector.TimelineOverviewWindow = function(parentElement)
{
    this._parentElement = parentElement;

    this.windowLeft = 0.0;
    this.windowRight = 1.0;

    this._parentElement.addEventListener("mousedown", this._dragWindow.bind(this), true);
    this._parentElement.addEventListener("mousewheel", this.scrollWindow.bind(this), true);
    this._parentElement.addEventListener("dblclick", this._resizeWindowMaximum.bind(this), true);

    this._overviewWindowElement = document.createElement("div");
    this._overviewWindowElement.className = "timeline-overview-window";
    parentElement.appendChild(this._overviewWindowElement);

    this._overviewWindowBordersElement = document.createElement("div");
    this._overviewWindowBordersElement.className = "timeline-overview-window-rulers";
    parentElement.appendChild(this._overviewWindowBordersElement);

    var overviewDividersBackground = document.createElement("div");
    overviewDividersBackground.className = "timeline-overview-dividers-background";
    parentElement.appendChild(overviewDividersBackground);

    this._leftResizeElement = document.createElement("div");
    this._leftResizeElement.className = "timeline-window-resizer";
    this._leftResizeElement.style.left = 0;
    parentElement.appendChild(this._leftResizeElement);

    this._rightResizeElement = document.createElement("div");
    this._rightResizeElement.className = "timeline-window-resizer timeline-window-resizer-right";
    this._rightResizeElement.style.right = 0;
    parentElement.appendChild(this._rightResizeElement);
}

WebInspector.TimelineOverviewWindow.Events = {
    WindowChanged: "WindowChanged"
}

WebInspector.TimelineOverviewWindow.prototype = {
    reset: function()
    {
        this.windowLeft = 0.0;
        this.windowRight = 1.0;

        this._overviewWindowElement.style.left = "0%";
        this._overviewWindowElement.style.width = "100%";
        this._overviewWindowBordersElement.style.left = "0%";
        this._overviewWindowBordersElement.style.right = "0%";
        this._leftResizeElement.style.left = "0%";
        this._rightResizeElement.style.left = "100%";
    },

    _resizeWindow: function(resizeElement, event)
    {
        WebInspector.elementDragStart(resizeElement, this._windowResizeDragging.bind(this, resizeElement), this._endWindowDragging.bind(this), event, "ew-resize");
    },

    _windowResizeDragging: function(resizeElement, event)
    {
        if (resizeElement === this._leftResizeElement)
            this._resizeWindowLeft(event.pageX - this._parentElement.offsetLeft);
        else
            this._resizeWindowRight(event.pageX - this._parentElement.offsetLeft);
        event.preventDefault();
    },

    _dragWindow: function(event)
    {
        var node = event.target;
        while (node) {
            if (node.hasStyleClass("resources-dividers-label-bar")) {
                WebInspector.elementDragStart(this._overviewWindowElement, this._windowDragging.bind(this, event.pageX,
                    this._leftResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset, this._rightResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset), this._endWindowDragging.bind(this), event, "ew-resize");
                break;
            } else if (node === this._parentElement) {
                var position = event.pageX - this._parentElement.offsetLeft;
                this._overviewWindowSelector = new WebInspector.TimelineOverviewPane.WindowSelector(this._parentElement, position);
                WebInspector.elementDragStart(null, this._windowSelectorDragging.bind(this), this._endWindowSelectorDragging.bind(this), event, "ew-resize");
                break;
            } else if (node === this._leftResizeElement || node === this._rightResizeElement) {
                this._resizeWindow(node, event);
                break;
            }
            node = node.parentNode;
        }
    },

    _windowSelectorDragging: function(event)
    {
        this._overviewWindowSelector._updatePosition(event.pageX - this._parentElement.offsetLeft);
        event.preventDefault();
    },

    _endWindowSelectorDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
        var window = this._overviewWindowSelector._close(event.pageX - this._parentElement.offsetLeft);
        delete this._overviewWindowSelector;
        if (window.end - window.start < WebInspector.TimelineOverviewPane.MinSelectableSize) {
            if (this._parentElement.clientWidth - window.end > WebInspector.TimelineOverviewPane.MinSelectableSize)
                window.end = window.start + WebInspector.TimelineOverviewPane.MinSelectableSize;
            else
                window.start = window.end - WebInspector.TimelineOverviewPane.MinSelectableSize;
        }
        this._setWindowPosition(window.start, window.end);
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

        if (end > this._parentElement.clientWidth) {
            end = this._parentElement.clientWidth;
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
        else if (start > this._rightResizeElement.offsetLeft -  4)
            start = this._rightResizeElement.offsetLeft - 4;
        this._setWindowPosition(start, null);
    },

    _resizeWindowRight: function(end)
    {
        // Glue to edge.
        if (end > this._parentElement.clientWidth - 10)
            end = this._parentElement.clientWidth;
        else if (end < this._leftResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.MinSelectableSize)
            end = this._leftResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.MinSelectableSize;
        this._setWindowPosition(null, end);
    },

    _resizeWindowMaximum: function()
    {
        this._setWindowPosition(0, this._parentElement.clientWidth);
    },

    _setWindowPosition: function(start, end)
    {
        var clientWidth = this._parentElement.clientWidth;
        const rulerAdjustment = 1 / clientWidth;
        if (typeof start === "number") {
            this.windowLeft = start / clientWidth;
            this._leftResizeElement.style.left = this.windowLeft * 100 + "%";
            this._overviewWindowElement.style.left = this.windowLeft * 100 + "%";
            this._overviewWindowBordersElement.style.left = (this.windowLeft - rulerAdjustment) * 100 + "%";
        }
        if (typeof end === "number") {
            this.windowRight = end / clientWidth;
            this._rightResizeElement.style.left = this.windowRight * 100 + "%";
        }
        this._overviewWindowElement.style.width = (this.windowRight - this.windowLeft) * 100 + "%";
        this._overviewWindowBordersElement.style.right = (1 - this.windowRight + 2 * rulerAdjustment) * 100 + "%";
        this.dispatchEventToListeners(WebInspector.TimelineOverviewWindow.Events.WindowChanged);
    },

    _endWindowDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
    },

    scrollWindow: function(event)
    {
        if (typeof event.wheelDeltaX === "number" && event.wheelDeltaX !== 0) {
            this._windowDragging(event.pageX + Math.round(event.wheelDeltaX * WebInspector.TimelineOverviewPane.WindowScrollSpeedFactor),
                this._leftResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset,
                this._rightResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset,
                event);
        }
    }
}

WebInspector.TimelineOverviewWindow.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 */
WebInspector.TimelineOverviewCalculator = function()
{
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
        var result = false;
        if (typeof this.minimumBoundary === "undefined" || record.startTime < this.minimumBoundary) {
            this.minimumBoundary = record.startTime;
            result = true;
        }
        if (typeof this.maximumBoundary === "undefined" || record.endTime > this.maximumBoundary) {
            this.maximumBoundary = record.endTime;
            result = true;
        }
        return result;
    },

    get boundarySpan()
    {
        return this.maximumBoundary - this.minimumBoundary;
    },

    formatTime: function(value)
    {
        return Number.secondsToString(value);
    }
}

/**
 * @constructor
 */
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

/**
 * @constructor
 */
WebInspector.TimelineOverviewPane.WindowSelector = function(parent, position)
{
    this._startPosition = position;
    this._width = parent.offsetWidth;
    this._windowSelector = document.createElement("div");
    this._windowSelector.className = "timeline-window-selector";
    this._windowSelector.style.left = this._startPosition + "px";
    this._windowSelector.style.right = this._width - this._startPosition +  + "px";
    parent.appendChild(this._windowSelector);
}

WebInspector.TimelineOverviewPane.WindowSelector.prototype = {
    _createSelectorElement: function(parent, left, width, height)
    {
        var selectorElement = document.createElement("div");
        selectorElement.className = "timeline-window-selector";
        selectorElement.style.left = left + "px";
        selectorElement.style.width = width + "px";
        selectorElement.style.top = "0px";
        selectorElement.style.height = height + "px";
        parent.appendChild(selectorElement);
        return selectorElement;
    },

    _close: function(position)
    {
        position = Math.max(0, Math.min(position, this._width));
        this._windowSelector.parentNode.removeChild(this._windowSelector);
        return this._startPosition < position ? {start: this._startPosition, end: position} : {start: position, end: this._startPosition};
    },

    _updatePosition: function(position)
    {
        position = Math.max(0, Math.min(position, this._width));
        if (position < this._startPosition) {
            this._windowSelector.style.left = position + "px";
            this._windowSelector.style.right = this._width - this._startPosition + "px";
        } else {
            this._windowSelector.style.left = this._startPosition + "px";
            this._windowSelector.style.right = this._width - position + "px";
        }
    }
}

/**
 * @constructor
 */
WebInspector.HeapGraph = function() {
    this._canvas = document.createElement("canvas");

    this._maxHeapSizeLabel = document.createElement("div");
    this._maxHeapSizeLabel.addStyleClass("max");
    this._maxHeapSizeLabel.addStyleClass("memory-graph-label");
    this._minHeapSizeLabel = document.createElement("div");
    this._minHeapSizeLabel.addStyleClass("min");
    this._minHeapSizeLabel.addStyleClass("memory-graph-label");

    this._element = document.createElement("div");
    this._element.addStyleClass("hidden");
    this._element.appendChild(this._canvas);
    this._element.appendChild(this._maxHeapSizeLabel);
    this._element.appendChild(this._minHeapSizeLabel);
}

WebInspector.HeapGraph.prototype = {
    get element() {
    //    return this._canvas;
        return this._element;
    },

    get visible() {
        return !this.element.hasStyleClass("hidden");
    },

    show: function() {
        this.element.removeStyleClass("hidden");
    },

    hide: function() {
        this.element.addStyleClass("hidden");
    },

    setSize: function(w, h) {
        this._canvas.width = w;
        this._canvas.height = h - 5;
    },

    update: function(records)
    {
        if (!records.length)
            return;

        const lowerOffset = 3;
        var maxUsedHeapSize = 0;
        var minUsedHeapSize = 100000000000;
        var minTime;
        var maxTime;
        WebInspector.TimelinePanel.forAllRecords(records, function(r) {
            maxUsedHeapSize = Math.max(maxUsedHeapSize, r.usedHeapSize || maxUsedHeapSize);
            minUsedHeapSize = Math.min(minUsedHeapSize, r.usedHeapSize || minUsedHeapSize);

            if (typeof minTime === "undefined" || r.startTime < minTime)
                minTime = r.startTime;
            if (typeof maxTime === "undefined" || r.endTime > maxTime)
                maxTime = r.endTime;
        });
        minUsedHeapSize = Math.min(minUsedHeapSize, maxUsedHeapSize);

        var width = this._canvas.width;
        var height = this._canvas.height - lowerOffset;
        var xFactor = width / (maxTime - minTime);
        var yFactor = height / (maxUsedHeapSize - minUsedHeapSize);

        var histogram = new Array(width);
        WebInspector.TimelinePanel.forAllRecords(records, function(r) {
            if (!r.usedHeapSize)
                return;
             var x = Math.round((r.endTime - minTime) * xFactor);
             var y = Math.round((r.usedHeapSize - minUsedHeapSize) * yFactor);
             histogram[x] = Math.max(histogram[x] || 0, y);
        });

        var ctx = this._canvas.getContext("2d");
        this._clear(ctx);

        // +1 so that the border always fit into the canvas area.
        height = height + 1;

        ctx.beginPath();
        var initialY = 0;
        for (var k = 0; k < histogram.length; k++) {
            if (histogram[k]) {
                initialY = histogram[k];
                break;
            }
        }
        ctx.moveTo(0, height - initialY);

        for (var x = 0; x < histogram.length; x++) {
             if (!histogram[x])
                 continue;
             ctx.lineTo(x, height - histogram[x]);
        }

        ctx.lineWidth = 0.5;
        ctx.strokeStyle = "rgba(20,0,0,0.8)";
        ctx.stroke();

        ctx.fillStyle = "rgba(214,225,254, 0.8);";
        ctx.lineTo(width, 60);
        ctx.lineTo(0, 60);
        ctx.lineTo(0, height - initialY);
        ctx.fill();
        ctx.closePath();

        this._maxHeapSizeLabel.textContent = Number.bytesToString(maxUsedHeapSize);
        this._minHeapSizeLabel.textContent = Number.bytesToString(minUsedHeapSize);
    },

    _clear: function(ctx) {
        ctx.fillStyle = "rgba(255,255,255,0.8)";
        ctx.fillRect(0, 0, this._canvas.width, this._canvas.height);
    },
}

/**
 * @constructor
 * @param {WebInspector.TimelineOverviewPane} pane
 * @extends {WebInspector.View}
 */
WebInspector.TimelineStartAtZeroOverview = function(pane) {
    WebInspector.View.call(this);
    this._pane = pane;
    this.element.className = "timeline-overview-start-at-zero fill";
    this._overviewElement = this.element.createChild("div", "timeline-overview-start-at-zero-bars fill");
    this._overviewWindow = new WebInspector.TimelineOverviewWindow(this.element);
    this._overviewWindow.addEventListener(WebInspector.TimelineOverviewWindow.Events.WindowChanged, this._onWindowChanged, this);
    this._recordsPerBar = 1;
}

WebInspector.TimelineStartAtZeroOverview.prototype = {
    reset: function()
    {
        this._recordsPerBar = 1;
        this._overviewWindow.reset();
    },

    update: function(records)
    {
        records = this._filterRecords(records);
        var boundarySpan = 0;
        for (var i = 0; i < records.length; ++i) {
            var record = records[i];
            var duration = record.endTime - record.startTime;
            if (boundarySpan < duration)
                boundarySpan = duration;
        }
      
        var scale = (this._overviewElement.clientHeight - 4) / boundarySpan;
        this._recordsPerBar = Math.max(1, records.length * 4 / this.element.clientWidth);
        var numberOfBars = Math.ceil(records.length / this._recordsPerBar);

        this._overviewElement.removeChildren();
        var padding = this._overviewElement.createChild("div", "padding");

        for (var i = 0; i < numberOfBars; ++i) {
            var index = Math.floor(i * this._recordsPerBar);
            var longestRecord = records[index];
            var stopIndex = Math.min(Math.floor((i + 1) * this._recordsPerBar), records.length);
            for (++index; index < stopIndex; ++index) {
                var record = records[index];
                if (longestRecord.endTime - longestRecord.startTime < record.endTime - record.startTime)
                    longestRecord = record;
            }
            this._overviewElement.insertBefore(this._buildBar(longestRecord, scale), padding);
        }
    },

    _filterRecords: function(records)
    {
        function filter(record)
        {
            return !record.category.hidden;
        }
        return records.filter(filter);
    },

    _buildBar: function(record, scale)
    {
        var outer = document.createElement("div");
        outer.className = "timeline-bar-vertical";
        var stats = record.aggregatedStats;
        var categories = Object.keys(stats);
        for (var i = 0; i < categories.length; ++i) {
            var category = categories[i];
            var duration = stats[category];
            if (!duration)
                continue;
            var bar = outer.createChild("div", "timeline-" + category);
            bar.style.height = (stats[category] * scale) + "px";
        }
        return outer;
    },

    _onWindowChanged: function()
    {
        var leftOffset = this._overviewWindow.windowLeft * this.element.clientWidth;
        var rightOffset = this._overviewWindow.windowRight * this.element.clientWidth;
        var bars = this._overviewElement.children;
        var offset0 = bars[0] ? bars[0].offsetLeft : 4;
        var barWidth = 9;
        if (bars.length > 2) {
            var offset1 = bars[bars.length - 2].offsetLeft;
            barWidth = (offset1 - offset0) / (bars.length - 2);
        }

        var leftIndex = Math.floor((leftOffset - offset0) / barWidth * this._recordsPerBar);
        var rightIndex = rightOffset + barWidth >= this.element.clientWidth ? null : Math.ceil((rightOffset - offset0 - 2) / barWidth * this._recordsPerBar);

        this._pane._setWindowIndices(leftIndex, rightIndex);
    }
}

WebInspector.TimelineStartAtZeroOverview.prototype.__proto__ = WebInspector.View.prototype;
