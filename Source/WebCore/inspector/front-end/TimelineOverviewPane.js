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
 * @extends {WebInspector.View}
 * @implements {WebInspector.TimelinePresentationModel.Filter}
 * @param {WebInspector.TimelineModel} model
 */
WebInspector.TimelineOverviewPane = function(model)
{
    WebInspector.View.call(this);
    this.element.id = "timeline-overview-panel";

    this._windowStartTime = 0;
    this._windowEndTime = Infinity;
    this._eventDividers = [];

    this._model = model;

    this._topPaneSidebarElement = document.createElement("div");
    this._topPaneSidebarElement.id = "timeline-overview-sidebar";

    var overviewTreeElement = document.createElement("ol");
    overviewTreeElement.className = "sidebar-tree";
    this._topPaneSidebarElement.appendChild(overviewTreeElement);
    this.element.appendChild(this._topPaneSidebarElement);

    var topPaneSidebarTree = new TreeOutline(overviewTreeElement);

    var eventsOverviewItem = new WebInspector.SidebarTreeElement("timeline-overview-sidebar-events", WebInspector.UIString("Events"));
    topPaneSidebarTree.appendChild(eventsOverviewItem);
    eventsOverviewItem.revealAndSelect(false);
    eventsOverviewItem.onselect = this._showEvents.bind(this);

    if (Capabilities.timelineSupportsFrameInstrumentation) {
        var framesOverviewItem = new WebInspector.SidebarTreeElement("timeline-overview-sidebar-frames", WebInspector.UIString("Frames"));
        topPaneSidebarTree.appendChild(framesOverviewItem);
        framesOverviewItem.onselect = this._showFrames.bind(this);
    }

    var memoryOverviewItem = new WebInspector.SidebarTreeElement("timeline-overview-sidebar-memory", WebInspector.UIString("Memory"));
    topPaneSidebarTree.appendChild(memoryOverviewItem);
    memoryOverviewItem.onselect = this._showMemoryGraph.bind(this);

    this._currentMode = WebInspector.TimelineOverviewPane.Mode.EventsHorizontal;

    this._overviewContainer = this.element.createChild("div", "fill");
    this._overviewContainer.id = "timeline-overview-container";

    this._overviewGrid = new WebInspector.TimelineGrid();
    this._overviewGrid.element.id = "timeline-overview-grid";
    this._overviewGrid.itemsGraphsElement.id = "timeline-overview-timelines";

    this._overviewContainer.appendChild(this._overviewGrid.element);

    this._heapGraph = new WebInspector.HeapGraph(this._model);
    this._heapGraph.element.id = "timeline-overview-memory";
    this._overviewGrid.element.insertBefore(this._heapGraph.element, this._overviewGrid.itemsGraphsElement);

    this._overviewWindow = new WebInspector.TimelineOverviewWindow(this._overviewContainer);
    this._overviewWindow.addEventListener(WebInspector.TimelineOverviewWindow.Events.WindowChanged, this._onWindowChanged, this);

    var separatorElement = document.createElement("div");
    separatorElement.id = "timeline-overview-separator";
    this.element.appendChild(separatorElement);

    this._categoryStrips = new WebInspector.TimelineCategoryStrips(this._model);
    this._overviewGrid.itemsGraphsElement.appendChild(this._categoryStrips.element);
 
    var categories = WebInspector.TimelinePresentationModel.categories();
    for (var category in categories)
        categories[category].addEventListener(WebInspector.TimelineCategory.Events.VisibilityChanged, this._onCategoryVisibilityChanged, this);

    this._overviewGrid.setScrollAndDividerTop(0, 0);
    this._overviewCalculator = new WebInspector.TimelineOverviewCalculator();

    model.addEventListener(WebInspector.TimelineModel.Events.RecordAdded, this._onRecordAdded, this);
    model.addEventListener(WebInspector.TimelineModel.Events.RecordsCleared, this._reset, this);
}

WebInspector.TimelineOverviewPane.MinSelectableSize = 12;

WebInspector.TimelineOverviewPane.WindowScrollSpeedFactor = .3;

WebInspector.TimelineOverviewPane.ResizerOffset = 3.5; // half pixel because offset values are not rounded but ceiled

WebInspector.TimelineOverviewPane.Mode = {
    EventsVertical: "EventsVertical",
    EventsHorizontal: "EventsHorizontal",
    Memory: "Memory"
};

WebInspector.TimelineOverviewPane.Events = {
    ModeChanged: "ModeChanged",
    WindowChanged: "WindowChanged"
};

WebInspector.TimelineOverviewPane.prototype = {
    wasShown: function()
    {
        this._update();
    },

    onResize: function()
    {
        this._update();
    },

    _showEvents: function()
    {
        if (this._currentMode === WebInspector.TimelineOverviewPane.Mode.EventsHorizontal)
            return;
        this._heapGraph.hide();
        this._setVerticalOverview(false);
        this._overviewGrid.itemsGraphsElement.removeStyleClass("hidden");
        this._setMode(WebInspector.TimelineOverviewPane.Mode.EventsHorizontal);
    },

    _showFrames: function()
    {
        if (this._currentMode === WebInspector.TimelineOverviewPane.Mode.EventsVertical)
            return;
        this._heapGraph.hide();
        this._setVerticalOverview(true);
        this._overviewGrid.itemsGraphsElement.removeStyleClass("hidden");
        this._setMode(WebInspector.TimelineOverviewPane.Mode.EventsVertical);
    },

    _showMemoryGraph: function()
    {
        if (this._currentMode === WebInspector.TimelineOverviewPane.Mode.Memory)
            return;
        this._setVerticalOverview(false);
        this._overviewGrid.itemsGraphsElement.addStyleClass("hidden");
        this._heapGraph.show();
        this._setMode(WebInspector.TimelineOverviewPane.Mode.Memory);
    },

    _setMode: function(newMode)
    {
        this._currentMode = newMode;
        this.dispatchEventToListeners(WebInspector.TimelineOverviewPane.Events.ModeChanged, this._currentMode);
        this._update();
    },

    _setVerticalOverview: function(enabled)
    {
        if (!enabled === !this._verticalOverview)
            return;
        if (enabled) {
            this._verticalOverview = new WebInspector.TimelineVerticalOverview(this._model);
            this._verticalOverview.show(this._overviewContainer);
        } else {
            this._verticalOverview.detach();
            this._verticalOverview = null;
            this._overviewGrid.itemsGraphsElement.removeStyleClass("hidden");
            this._categoryStrips.update();
        }
    },

    _onCategoryVisibilityChanged: function(event)
    {
        if (this._currentMode === WebInspector.TimelineOverviewPane.Mode.EventsHorizontal)
            this._categoryStrips.update();
    },

    _update: function()
    {
        delete this._refreshTimeout;

        this._overviewCalculator.setWindow(this._model.minimumRecordTime(), this._model.maximumRecordTime());
        this._overviewCalculator.setDisplayWindow(0, this._overviewContainer.clientWidth);

        if (this._heapGraph.visible)
            this._heapGraph.update();
        else if (this._verticalOverview)
            this._verticalOverview.update();
        else
            this._categoryStrips.update();

        this._overviewGrid.updateDividers(this._overviewCalculator);
        this._updateEventDividers();
    },

    _updateEventDividers: function()
    {
        var records = this._eventDividers;
        this._overviewGrid.removeEventDividers();
        var dividers = [];
        for (var i = 0; i < records.length; ++i) {
            var record = records[i];
            var positions = this._overviewCalculator.computeBarGraphPercentages(record);
            var dividerPosition = Math.round(positions.start * 10);
            if (dividers[dividerPosition])
                continue;
            var divider = WebInspector.TimelinePresentationModel.createEventDivider(record.type);
            divider.style.left = positions.start + "%";
            dividers[dividerPosition] = divider;
        }
        this._overviewGrid.addEventDividers(dividers);
    },

    sidebarResized: function(width)
    {
        this._overviewContainer.style.left = width + "px";
        this._topPaneSidebarElement.style.width = width + "px";
    },

    /**
     * @param {WebInspector.TimelineFrame} frame
     */
    addFrame: function(frame)
    {
        this._verticalOverview.addFrame(frame);
        this._scheduleRefresh();
    },

    /**
     * @param {WebInspector.TimelineFrame} frame
     */
    zoomToFrame: function(frame)
    {
        var window = this._verticalOverview.framePosition(frame);
        if (!window)
            return;

        this._overviewWindow._setWindowPosition(window.start, window.end);
    },

    _onRecordAdded: function(event)
    {
        var record = event.data;
        var eventDividers = this._eventDividers;
        function addEventDividers(record)
        {
            if (WebInspector.TimelinePresentationModel.isEventDivider(record))
                eventDividers.push(record);
        }
        WebInspector.TimelinePresentationModel.forAllRecords([record], addEventDividers);
        this._scheduleRefresh();
    },

    _reset: function()
    {
        this._windowStartTime = 0;
        this._windowEndTime = Infinity;
        this._overviewWindow.reset();
        this._overviewCalculator.reset();
        this._eventDividers = [];
        this._overviewGrid.updateDividers(this._overviewCalculator);
        if (this._verticalOverview)
            this._verticalOverview.reset();
        this._update();
    },

    /**
     * @param {WebInspector.TimelinePresentationModel.Record} record
     */
    accept: function(record)
    {
        return record.endTime >= this._windowStartTime && record.startTime <= this._windowEndTime;
    },

    windowStartTime: function()
    {
        return this._windowStartTime || this._model.minimumRecordTime();
    },

    windowEndTime: function()
    {
        return this._windowEndTime < Infinity ? this._windowEndTime : this._model.maximumRecordTime();
    },

    windowLeft: function()
    {
        return this._overviewWindow.windowLeft;
    },

    windowRight: function()
    {
        return this._overviewWindow.windowRight;
    },

    _onWindowChanged: function()
    {
        if (this._verticalOverview) {
            var times = this._verticalOverview.getWindowTimes(this.windowLeft(), this.windowRight());
            this._windowStartTime = times.startTime;
            this._windowEndTime = times.endTime;
        } else {
            var absoluteMin = this._model.minimumRecordTime();
            var absoluteMax = this._model.maximumRecordTime();
            this._windowStartTime = absoluteMin + (absoluteMax - absoluteMin) * this.windowLeft();
            this._windowEndTime = absoluteMin + (absoluteMax - absoluteMin) * this.windowRight();
        }
        this.dispatchEventToListeners(WebInspector.TimelineOverviewPane.Events.WindowChanged);
    },

    /**
     * @param {boolean} value
     */
    setShowShortEvents: function(value)
    {
        this._categoryStrips.setShowShortEvents(value);
    },

    _scheduleRefresh: function()
    {
        if (this._refreshTimeout)
            return;
        if (!this.isShowing())
            return;
        this._refreshTimeout = setTimeout(this._update.bind(this), 100);
    }
}

WebInspector.TimelineOverviewPane.prototype.__proto__ = WebInspector.View.prototype;

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
    this._parentElement.addEventListener("mousewheel", this._onMouseWheel.bind(this), true);
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
        // Only drag upon left button. Right will likely cause a context menu.
        if (event.button)
            return;
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
        if (window.end === window.start) { // Click, not drag.
            var middle = window.end;
            window.start = Math.max(0, middle - WebInspector.TimelineOverviewPane.MinSelectableSize / 2);
            window.end = Math.min(this._parentElement.clientWidth, middle + WebInspector.TimelineOverviewPane.MinSelectableSize / 2);
        } else if (window.end - window.start < WebInspector.TimelineOverviewPane.MinSelectableSize) {
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

    _onMouseWheel: function(event)
    {
        const zoomFactor = 1.1;
        const mouseWheelZoomSpeed = 1 / 120;

        if (typeof event.wheelDeltaY === "number" && event.wheelDeltaY) {
            var referencePoint = event.pageX - this._parentElement.offsetLeft;
            this._zoom(Math.pow(zoomFactor, -event.wheelDeltaY * mouseWheelZoomSpeed), referencePoint);
        }
        if (typeof event.wheelDeltaX === "number" && event.wheelDeltaX) {
            this._windowDragging(event.pageX + Math.round(event.wheelDeltaX * WebInspector.TimelineOverviewPane.WindowScrollSpeedFactor),
                this._leftResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset,
                this._rightResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset,
                event);
        }
    },

    /**
     * @param {number} factor
     * @param {number} referencePoint
     */
    _zoom: function(factor, referencePoint)
    {
        var left = this._leftResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset;
        var right = this._rightResizeElement.offsetLeft + WebInspector.TimelineOverviewPane.ResizerOffset;

        if (factor < 1 && factor * (right - left) < WebInspector.TimelineOverviewPane.MinSelectableSize)
            return;
        left = Math.max(0, referencePoint + (left - referencePoint) * factor);
        right = Math.min(this._parentElement.clientWidth, referencePoint + (right - referencePoint) * factor);
        this._setWindowPosition(left, right);
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
    /**
     * @param {number} time
     */
    computePosition: function(time)
    {
        return (time - this.minimumBoundary) / this.boundarySpan * this._workingArea + this.paddingLeft;
    },

    computeBarGraphPercentages: function(record)
    {
        var start = (WebInspector.TimelineModel.startTimeInSeconds(record) - this.minimumBoundary) / this.boundarySpan * 100;
        var end = (WebInspector.TimelineModel.endTimeInSeconds(record) - this.minimumBoundary) / this.boundarySpan * 100;
        return {start: start, end: end};
    },

    /**
     * @param {number=} minimum
     * @param {number=} maximum
     */
    setWindow: function(minimum, maximum)
    {
        this.minimumBoundary = minimum >= 0 ? minimum : undefined;
        this.maximumBoundary = maximum >= 0 ? maximum : undefined;
        this.boundarySpan = this.maximumBoundary - this.minimumBoundary;
    },

    /**
     * @param {number} paddingLeft
     * @param {number} clientWidth
     */
    setDisplayWindow: function(paddingLeft, clientWidth)
    {
        this._workingArea = clientWidth - paddingLeft;
        this.paddingLeft = paddingLeft;
    },

    reset: function()
    {
        this.setWindow();
    },

    formatTime: function(value)
    {
        return Number.secondsToString(value);
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
 * @param {WebInspector.TimelineModel} model
 */
WebInspector.HeapGraph = function(model)
{
    this._canvas = document.createElement("canvas");
    this._model = model;

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
    /**
     * @return {Node}
     */
    get element()
    {
        return this._element;
    },

    /**
     * @return {boolean}
     */
    get visible()
    {
        return !this.element.hasStyleClass("hidden");
    },

    show: function()
    {
        this.element.removeStyleClass("hidden");
    },

    hide: function()
    {
        this.element.addStyleClass("hidden");
    },

    update: function()
    {
        var records = this._model.records;
        if (!records.length)
            return;

        const yPadding = 5;
        this._canvas.width = this.element.clientWidth;
        this._canvas.height = this.element.clientHeight - yPadding;

        const lowerOffset = 3;
        var maxUsedHeapSize = 0;
        var minUsedHeapSize = 100000000000;
        var minTime = this._model.minimumRecordTime();
        var maxTime = this._model.maximumRecordTime();;
        WebInspector.TimelinePresentationModel.forAllRecords(records, function(r) {
            maxUsedHeapSize = Math.max(maxUsedHeapSize, r.usedHeapSize || maxUsedHeapSize);
            minUsedHeapSize = Math.min(minUsedHeapSize, r.usedHeapSize || minUsedHeapSize);
        });
        minUsedHeapSize = Math.min(minUsedHeapSize, maxUsedHeapSize);

        var width = this._canvas.width;
        var height = this._canvas.height - lowerOffset;
        var xFactor = width / (maxTime - minTime);
        var yFactor = height / (maxUsedHeapSize - minUsedHeapSize);

        var histogram = new Array(width);
        WebInspector.TimelinePresentationModel.forAllRecords(records, function(r) {
            if (!r.usedHeapSize)
                return;
            var x = Math.round((WebInspector.TimelineModel.endTimeInSeconds(r) - minTime) * xFactor);
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

    _clear: function(ctx)
    {
        ctx.fillStyle = "rgba(255,255,255,0.8)";
        ctx.fillRect(0, 0, this._canvas.width, this._canvas.height);
    },
}

/**
 * @constructor
 * @param {WebInspector.TimelineModel} model
 */
WebInspector.TimelineCategoryStrips = function(model)
{
    this._model = model;
    this.element = document.createElement("canvas");
    this._context = this.element.getContext("2d");

    this._fillStyles = {};
    var categories = WebInspector.TimelinePresentationModel.categories();
    for (var category in categories)
        this._fillStyles[category] = WebInspector.TimelinePresentationModel.createFillStyleForCategory(this._context, 0, WebInspector.TimelineCategoryStrips._innerStripHeight, categories[category]);

    this._disabledCategoryFillStyle = WebInspector.TimelinePresentationModel.createFillStyle(this._context, 0, WebInspector.TimelineCategoryStrips._innerStripHeight,
        "rgb(218, 218, 218)", "rgb(170, 170, 170)", "rgb(143, 143, 143)");

    this._disabledCategoryBorderStyle = "rgb(143, 143, 143)";
}

/** @const */
WebInspector.TimelineCategoryStrips._canvasHeight = 60;
/** @const */
WebInspector.TimelineCategoryStrips._numberOfStrips = 3;
/** @const */
WebInspector.TimelineCategoryStrips._stripHeight = Math.round(WebInspector.TimelineCategoryStrips._canvasHeight  / WebInspector.TimelineCategoryStrips._numberOfStrips);
/** @const */
WebInspector.TimelineCategoryStrips._stripPadding = 4;
/** @const */
WebInspector.TimelineCategoryStrips._innerStripHeight = WebInspector.TimelineCategoryStrips._stripHeight - 2 * WebInspector.TimelineCategoryStrips._stripPadding;

WebInspector.TimelineCategoryStrips.prototype = {
    update: function()
    {
        // Use real world, 1:1 coordinates in canvas. This will also take care of clearing it.
        this.element.width = this.element.parentElement.clientWidth;
        this.element.height = WebInspector.TimelineCategoryStrips._canvasHeight;

        var timeOffset = this._model.minimumRecordTime();
        var timeSpan = this._model.maximumRecordTime() - timeOffset;
        var scale = this.element.width / timeSpan;

        var lastBarByGroup = [];

        this._context.fillStyle = "rgba(0, 0, 0, 0.05)";
        for (var i = 1; i < WebInspector.TimelineCategoryStrips._numberOfStrips; i += 2)
            this._context.fillRect(0.5, i * WebInspector.TimelineCategoryStrips._stripHeight + 0.5, this.element.width, WebInspector.TimelineCategoryStrips._stripHeight);

        function appendRecord(record)
        {
            var isLong = WebInspector.TimelineModel.durationInSeconds(record) > WebInspector.TimelinePresentationModel.shortRecordThreshold;
            if (!(this._showShortEvents || isLong))
                return;
            if (record.type === WebInspector.TimelineModel.RecordType.BeginFrame)
                return;
            var recordStart = Math.floor((WebInspector.TimelineModel.startTimeInSeconds(record) - timeOffset) * scale);
            var recordEnd = Math.ceil((WebInspector.TimelineModel.endTimeInSeconds(record) - timeOffset) * scale);
            var category = WebInspector.TimelinePresentationModel.categoryForRecord(record);
            var bar = lastBarByGroup[category.overviewStripGroupIndex];
            // This bar may be merged with previous -- so just adjust the previous bar.
            const barsMergeThreshold = 2;
            if (bar && bar.category === category && bar.end + barsMergeThreshold >= recordStart) {
                bar.end = recordEnd;
                return;
            }
            if (bar)
                this._renderBar(bar.start, bar.end, bar.category);
            lastBarByGroup[category.overviewStripGroupIndex] = { start: recordStart, end: recordEnd, category: category };
        }
        WebInspector.TimelinePresentationModel.forAllRecords(this._model.records, appendRecord.bind(this));
        for (var i = 0; i < lastBarByGroup.length; ++i) {
            if (lastBarByGroup[i])
                this._renderBar(lastBarByGroup[i].start, lastBarByGroup[i].end, lastBarByGroup[i].category);
        }
    },

    /**
     * @param {boolean} value
     */
    setShowShortEvents: function(value)
    {
        this._showShortEvents = value;
        this.update();
    },

    _renderBar: function(begin, end, category)
    {
        var x = begin + 0.5;
        var y = category.overviewStripGroupIndex * WebInspector.TimelineCategoryStrips._stripHeight + WebInspector.TimelineCategoryStrips._stripPadding + 0.5;
        var width = Math.max(end - begin, 1);

        this._context.save();
        this._context.translate(x, y);
        this._context.fillStyle = category.hidden ? this._disabledCategoryFillStyle : this._fillStyles[category.name];
        this._context.fillRect(0, 0, width, WebInspector.TimelineCategoryStrips._innerStripHeight);
        this._context.strokeStyle = category.hidden ? this._disabledCategoryBorderStyle : category.borderColor;
        this._context.strokeRect(0, 0, width, WebInspector.TimelineCategoryStrips._innerStripHeight);
        this._context.restore();
    }
}

/**
 * @constructor
 * @extends {WebInspector.View}
 * @param {WebInspector.TimelineModel} model
 */
WebInspector.TimelineVerticalOverview = function(model)
{
    WebInspector.View.call(this);
    this.element = document.createElement("canvas");
    this.element.className = "timeline-vertical-overview-bars fill";
    this._model = model;
    this.reset();

    this._outerPadding = 4;
    this._maxInnerBarWidth = 10;

    // The below two are really computed by update() -- but let's have something so that getWindowTimes() is happy.
    this._actualPadding = 5;
    this._actualOuterBarWidth = this._maxInnerBarWidth + this._actualPadding;

    this._context = this.element.getContext("2d");

    this._fillStyles = {};
    var categories = WebInspector.TimelinePresentationModel.categories();
    for (var category in categories)
        this._fillStyles[category] = WebInspector.TimelinePresentationModel.createFillStyleForCategory(this._context, this._maxInnerBarWidth, 0, categories[category]);
}

WebInspector.TimelineVerticalOverview.prototype = {
    reset: function()
    {
        this._recordsPerBar = 1;
        this._barTimes = [];
        this._frames = [];
    },

    update: function()
    {
        const minBarWidth = 4;
        this._framesPerBar = Math.max(1, this._frames.length * minBarWidth / this.element.clientWidth);
        this._barTimes = [];
        var visibleFrames = this._aggregateFrames(this._framesPerBar);

        const paddingTop = 4;
        var scale = (this.element.clientHeight - paddingTop) / this._normalBarLength;

        this._renderBars(visibleFrames, scale);
    },

    /**
     * @param {WebInspector.TimelineFrame} frame
     */
    addFrame: function(frame)
    {
        this._frames.push(frame);
    },

    framePosition: function(frame)
    {
        var frameNumber = this._frames.indexOf(frame);
        if (frameNumber < 0)
            return;
        var barNumber = Math.floor(frameNumber / this._framesPerBar);
        var firstBar = this._framesPerBar > 1 ? barNumber : Math.max(barNumber - 1, 0);
        var lastBar = this._framesPerBar > 1 ? barNumber : Math.min(barNumber + 1, this._barTimes.length - 1);
        return {
            start: Math.ceil(this._barNumberToScreenPosition(firstBar) - this._actualPadding / 2),
            end: Math.floor(this._barNumberToScreenPosition(lastBar + 1) - this._actualPadding / 2)
        }
    },

    /**
     * @param {number} framesPerBar
     */
    _aggregateFrames: function(framesPerBar)
    {
        var visibleFrames = [];
        var durations = [];
        var longestFrameTime = 0;

        for (var barNumber = 0, currentFrame = 0; currentFrame < this._frames.length; ++barNumber) {
            var barStartTime = this._frames[currentFrame].startTime;
            var longestFrame = null;

            for (var lastFrame = Math.min(Math.floor((barNumber + 1) * framesPerBar), this._frames.length);
                 currentFrame < lastFrame; ++currentFrame) {
                if (!longestFrame || longestFrame.duration < this._frames[currentFrame].duration)
                    longestFrame = this._frames[currentFrame];
            }
            var barEndTime = this._frames[currentFrame - 1].endTime;
            if (longestFrame) {
                longestFrameTime = Math.max(longestFrameTime, longestFrame.duration);
                visibleFrames.push(longestFrame);
                this._barTimes.push({ startTime: barStartTime, endTime: barEndTime });
                durations.push(longestFrame.duration);
            }
        }
        // Do not let occasional very long frames to dwarf the majority -- use at most 3 * median frame length for scale.
        this._normalBarLength = Math.min(longestFrameTime, 3 * durations.qselect(Math.floor(durations.length / 2)));
        return visibleFrames;
    },

    _renderBars: function(frames, scale)
    {
        // Use real world, 1:1 coordinates in canvas. This will also take care of clearing it.
        this.element.width = this.element.clientWidth;
        this.element.height = this.element.clientHeight;

        const maxPadding = 5;
        this._actualOuterBarWidth = Math.min((this.element.width - 2 * this._outerPadding) / frames.length, this._maxInnerBarWidth + maxPadding);
        this._actualPadding = Math.min(Math.floor(this._actualOuterBarWidth / 3), maxPadding);

        for (var i = 0; i < frames.length; ++i) {
            var width = this._actualOuterBarWidth - this._actualPadding;
            this._renderBar(this._barNumberToScreenPosition(i), width, frames[i], scale);
        }
    },

    _barNumberToScreenPosition: function(n)
    {
        return this._outerPadding + this._actualOuterBarWidth * n;
    },

    _renderBar: function(left, width, frame, scale)
    {
        var categories = Object.keys(frame.timeByCategory);
        if (!categories.length)
            return;
        var x = Math.floor(left) + 0.5;
        width = Math.floor(width);

        for (var i = 0, bottomOffset = this.element.height; i < categories.length; ++i) {
            var category = categories[i];
            var duration = frame.timeByCategory[category];

            if (!duration)
                continue;
            var height = duration * scale;
            var y = Math.floor(bottomOffset - height) + 0.5;

            this._context.save();
            this._context.translate(x, 0);
            this._context.scale(width / this._maxInnerBarWidth, 1);
            this._context.fillStyle = this._fillStyles[category];
            this._context.fillRect(0, y, this._maxInnerBarWidth, Math.floor(height));
            this._context.restore();

            this._context.strokeStyle = WebInspector.TimelinePresentationModel.categories()[category].borderColor;
            this._context.strokeRect(x, y, width, Math.floor(height));
            bottomOffset -= height - 1;
        }
        // Draw a contour for the rest of frame time that we did not instrument.
        var nonCPUTime = frame.duration - frame.cpuTime;
        var y0 = Math.floor(bottomOffset - nonCPUTime * scale) + 0.5;
        var y1 = Math.floor(bottomOffset) + 0.5;

        this._context.strokeStyle = "rgb(90, 90, 90)";
        this._context.beginPath();
        this._context.moveTo(x, y1);
        this._context.lineTo(x, y0);
        this._context.lineTo(x + width, y0);
        this._context.lineTo(x + width, y1);
        this._context.stroke();
    },

    getWindowTimes: function(windowLeft, windowRight)
    {
        var windowSpan = this.element.clientWidth;
        var leftOffset = windowLeft * windowSpan - this._outerPadding + this._actualPadding;
        var rightOffset = windowRight * windowSpan - this._outerPadding;
        var bars = this.element.children;
        var firstBar = Math.floor(Math.max(leftOffset, 0) / this._actualOuterBarWidth);
        var lastBar = Math.min(Math.floor(rightOffset / this._actualOuterBarWidth), this._barTimes.length - 1);
        const snapToRightTolerancePixels = 3;
        return {
            startTime: firstBar >= this._barTimes.length ? Infinity : this._barTimes[firstBar].startTime,
            endTime: rightOffset + snapToRightTolerancePixels > windowSpan ? Infinity : this._barTimes[lastBar].endTime
        }
    }
}

WebInspector.TimelineVerticalOverview.prototype.__proto__ = WebInspector.View.prototype;
