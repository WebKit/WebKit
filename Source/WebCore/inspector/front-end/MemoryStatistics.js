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
 * @param {WebInspector.TimelinePanel} timelinePanel
 * @param {number} sidebarWidth
 * @constructor
 */
WebInspector.MemoryStatistics = function(timelinePanel, sidebarWidth)
{
    this._timelinePanel = timelinePanel;
    this._counters = [];

    this._containerAnchor = timelinePanel.element.lastChild;
    this._memorySplitView = new WebInspector.SplitView(WebInspector.SplitView.SidebarPosition.Left, undefined, sidebarWidth);
    this._memorySplitView.sidebarElement.addStyleClass("sidebar");
    this._memorySplitView.element.id = "memory-graphs-container";

    this._memorySplitView.addEventListener(WebInspector.SplitView.EventTypes.Resized, this._sidebarResized.bind(this));

    this._canvasContainer = this._memorySplitView.mainElement;
    this._canvas = this._canvasContainer.createChild("canvas", "fill");
    this._canvas.id = "memory-counters-graph";
    this._memoryMarker = this._canvasContainer.createChild("div", "timeline-marker");

    this._canvasContainer.addEventListener("mouseover", this._onMouseOver.bind(this), true);
    this._canvasContainer.addEventListener("mousemove", this._onMouseOver.bind(this), true);

    // Populate sidebar
    this._counterSidebarElements = [];
    this._documents = this._createCounterSidebarElement(WebInspector.UIString("Document count:"), true);
    this._domNodes = this._createCounterSidebarElement(WebInspector.UIString("DOM node count:"), true);
    this._listeners = this._createCounterSidebarElement(WebInspector.UIString("Event listener count:"), false);

    TimelineAgent.setIncludeMemoryDetails(true);
}

WebInspector.MemoryStatistics.prototype = {
    setTopPosition: function(top)
    {
        this._memorySplitView.element.style.top = top + "px";
        this._updateSize();
    },

    setSidebarWidth: function(width)
    {
        if (this._ignoreSidebarResize)
            return;
        this._ignoreSidebarResize = true;
        this._memorySplitView.setSidebarWidth(width);
        this._ignoreSidebarResize = false;
    },

    _sidebarResized: function(event)
    {
        if (this._ignoreSidebarResize)
            return;
        this._ignoreSidebarResize = true;
        this._timelinePanel.splitView.setSidebarWidth(event.data);
        this._ignoreSidebarResize = false;
    },

    _updateSize: function()
    {
        var height = this._canvasContainer.offsetHeight;
        this._canvas.width = this._canvasContainer.offsetWidth;
        this._canvas.height = height;
        this._updateSidebarSize(height);
    },

    _updateSidebarSize: function(height)
    {
        var length = this._counterSidebarElements.length;
        var graphHeight =  Math.round(height / length);
        var top = 0;
        for (var i = 0; i < length; i++) {
            var element = this._counterSidebarElements[i];
            element.style.top = top + "px";
            element.style.height = graphHeight + "px";
            top += graphHeight;
        }
    },

    _createCounterSidebarElement: function(title, showBottomBorder)
    {
        var container = this._memorySplitView.sidebarElement.createChild("div", "memory-counter-sidebar-info");
        var value = container.createChild("p");
        value.textContent = title;
        container._value = value.createChild("span");
        if (showBottomBorder)
            container.addStyleClass("bottom-border-visible");
        this._counterSidebarElements.push(container);
        return container;
    },

    addTimlineEvent: function(event)
    {
        var counters = event.data["counters"];
        this._counters.push({
            time: event.data.endTime,
            documentCount: counters.documents,
            nodeCount: counters.nodes,
            listenerCount: counters.jsEventListeners
        });
    },

    _draw: function()
    {
        this._calculateVisibleIndexes();
        this._calculateXValues();
        this._clear();
        var graphHeight = Math.round(this._canvas.height / 3);

        function getDocumentCount(entry)
        {
            return entry.documentCount;
        }
        this._setVerticalClip(0 * graphHeight + 2, graphHeight - 4);
        this._drawPolyline(getDocumentCount, "rgba(100,0,0,0.8)");
        this._drawBottomBound("rgba(20,20,20,0.8)");


        function getNodeCount(entry)
        {
            return entry.nodeCount;
        }
        this._setVerticalClip(1 * graphHeight + 2, graphHeight - 4);
        this._drawPolyline(getNodeCount, "rgba(0,100,0,0.8)");
        this._drawBottomBound("rgba(20,20,20,0.8)");

        function getListenerCount(entry)
        {
            return entry.listenerCount;
        }
        this._setVerticalClip(2 * graphHeight + 2, graphHeight - 4);
        this._drawPolyline(getListenerCount, "rgba(0,0,100,0.8)");
    },

    _calculateVisibleIndexes: function()
    {
        var calculator = this._timelinePanel.calculator;
        var start = calculator.minimumBoundary * 1000;
        var end = calculator.maximumBoundary * 1000;
        var firstIndex = 0;
        var lastIndex = this._counters.length - 1;
        for (var i = 0; i < this._counters.length; i++) {
            var time = this._counters[i].time;
            if (time <= start) {
                firstIndex = i;
            } else {
                if (end < time)
                    break;
                lastIndex = i;
            }
        }
        // Maximum index of element whose time <= start.
        this._minimumIndex = firstIndex;

        // Maximum index of element whose time <= end.
        this._maximumIndex = lastIndex;

        // Current window bounds.
        this._minTime = start;
        this._maxTime = end;
    },

    _onMouseOver: function(event)
    {
        var x = event.x - event.target.offsetParent.offsetLeft
        this._memoryMarker.style.left = x + "px";
        this._refreshCurrentValues(x);
    },

    _refreshCurrentValues: function(x)
    {
        if (!this._counters.length)
            return;
        var i;
        for (i = this._minimumIndex + 1; i <= this._maximumIndex; i++) {
            var statX = this._counters[i].x;
            if (x < statX)
                break;
        }
        i--;
        this._documents._value.textContent = this._counters[i].documentCount;
        this._domNodes._value.textContent = this._counters[i].nodeCount;
        this._listeners._value.textContent = this._counters[i].listenerCount;
    },

    visible: function()
    {
        return this._memorySplitView.isShowing();
    },

    show: function()
    {
        var anchor = /** @type {Element|null} */ this._containerAnchor.nextSibling;
        this._memorySplitView.show(this._timelinePanel.element, anchor);
        this._updateSize();
        setTimeout(this._draw.bind(this), 0);
    },

    refresh: function()
    {
        this._updateSize();
        this._draw();
        this._refreshCurrentValues(this._memoryMarker.offsetLeft);
    },

    hide: function()
    {
        this._memorySplitView.detach();
    },

    _setVerticalClip: function(originY, height)
    {
        this._originY = originY;
        this._clippedHeight = height;
    },

    _calculateXValues: function()
    {
        if (!this._counters.length)
            return;

        var width = this._canvas.width;
        var xFactor = width / (this._maxTime - this._minTime);

        this._counters[this._minimumIndex].x = 0;
        for (var i = this._minimumIndex + 1; i < this._maximumIndex; i++)
             this._counters[i].x = xFactor * (this._counters[i].time - this._minTime);
        this._counters[this._maximumIndex].x = width;
    },

    _drawPolyline: function(valueGetter, color)
    {
        var canvas = this._canvas;
        var ctx = canvas.getContext("2d");
        var width = canvas.width;
        var height = this._clippedHeight;
        var originY = this._originY;

        // Draw originalValue level
        ctx.beginPath();
        ctx.moveTo(0, originY + height / 2 + 0.5);
        ctx.lineTo(width, originY + height / 2 + 0.5);
        ctx.lineWidth = 0.1;
        ctx.strokeStyle = "rgb(100, 100, 100)";
        ctx.stroke();
        ctx.closePath();

        if (!this._counters.length)
            return;

        var maxValue;
        var minValue;
        for (var i = this._minimumIndex; i <= this._maximumIndex; i++) {
            var value = valueGetter(this._counters[i]);
            if (minValue === undefined || value < minValue)
                minValue = value;
            if (maxValue === undefined || value > maxValue)
                maxValue = value;
        }

        var originalValue = valueGetter(this._counters[this._minimumIndex]);

        var maxYRange = Math.max(maxValue - originalValue, originalValue - minValue);
        var yFactor = maxYRange ? height / (2 * maxYRange) : 0.5;

        ctx.beginPath();
        var currentY = originY + height / 2;
        ctx.moveTo(0, currentY);
        for (var i = this._minimumIndex; i <= this._maximumIndex; i++) {
             var x = this._counters[i].x;
             ctx.lineTo(x, currentY);
             currentY = originY + (height / 2 - (valueGetter(this._counters[i])- originalValue) * yFactor);
             ctx.lineTo(x, currentY);
        }
        ctx.lineTo(width, currentY);
        ctx.lineWidth = 1;
        ctx.strokeStyle = color;
        ctx.stroke();
        ctx.closePath();
    },

    _drawBottomBound: function(color)
    {
        var canvas = this._canvas;
        var width = canvas.width;
        var y = this._originY + this._clippedHeight + 1.5;

        var ctx = canvas.getContext("2d");
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(width, y);
        ctx.lineWidth = 0.5;
        ctx.strokeStyle = color;
        ctx.stroke();
        ctx.closePath();
    },

    _clear: function() {
        var ctx = this._canvas.getContext("2d");
        ctx.fillStyle = "rgb(255,255,255)";
        ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
        ctx.fill();
    }
}

