/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * @param {WebInspector.CPUProfileView} cpuProfileView
 */
WebInspector.FlameChart = function(cpuProfileView)
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("flameChart.css");

    this.element.className = "flame-chart";
    this._canvas = this.element.createChild("canvas");
    this._cpuProfileView = cpuProfileView;
    this._xScaleFactor = 0.0;
    this._barHeight = 10;
    this._minWidth = 1;
    this.element.onmousemove = this._onMouseMove.bind(this);
    this.element.onclick = this._onClick.bind(this);
    this._popoverHelper = new WebInspector.PopoverHelper(this.element, this._getPopoverAnchor.bind(this), this._showPopover.bind(this));
    this._popoverHelper.setTimeout(250);
    this._anchorElement = this.element.createChild("span");
    this._anchorElement.className = "item-anchor";
    this._linkifier = new WebInspector.Linkifier();
    this._highlightedNodeIndex = -1;
}

WebInspector.FlameChart.Events = {
    SelectedNode: "SelectedNode"
}

WebInspector.FlameChart.prototype = {
    _nodeCount: function()
    {
        var nodes = this._cpuProfileView.profileHead.children.slice();

        var nodeCount = 0;
        while (nodes.length) {
            var node = nodes.pop();
            ++nodeCount;
            nodes = nodes.concat(node.children);
        }
        return nodeCount;
    },

    _calculateTimelineData: function()
    {
        if (this._timelineData)
            return this._timelineData;

        if (!this._cpuProfileView.profileHead)
            return null;

        var nodeCount = this._nodeCount();
        var functionColorPairs = { };
        var currentColorIndex = 0;

        var startTimes = new Float32Array(nodeCount);
        var durations = new Float32Array(nodeCount);
        var depths = new Uint8Array(nodeCount);
        var nodes = new Array(nodeCount);
        var colorPairs = new Array(nodeCount);
        var index = 0;

        function appendReversedArray(toArray, fromArray)
        {
            for (var i = fromArray.length - 1; i >= 0; --i)
                toArray.push(fromArray[i]);
        }

        var stack = [];
        appendReversedArray(stack, this._cpuProfileView.profileHead.children);

        var levelOffsets = /** @type {Array.<!number>} */ ([0]);
        var levelExitIndexes = /** @type {Array.<!number>} */ ([0]);

        while (stack.length) {
            var level = levelOffsets.length - 1;
            var node = stack.pop();
            var offset = levelOffsets[level];

            var functionUID = node.functionName + ":" + node.url + ":" + node.lineNumber;
            var colorPair = functionColorPairs[functionUID];
            if (!colorPair) {
                ++currentColorIndex;
                var hue = (currentColorIndex * 5 + 11 * (currentColorIndex % 2)) % 360;
                functionColorPairs[functionUID] = colorPair = {highlighted: "hsl(" + hue + ", 100%, 33%)", normal: "hsl(" + hue + ", 100%, 66%)"};
            }

            colorPairs[index] = colorPair;
            depths[index] = level;
            durations[index] = node.totalTime;
            startTimes[index] = offset;
            nodes[index] = node;

            ++index;

            levelOffsets[level] += node.totalTime;
            if (node.children.length) {
                levelExitIndexes.push(stack.length);
                levelOffsets.push(offset + node.selfTime / 2);
                appendReversedArray(stack, node.children);
            }

            while (stack.length === levelExitIndexes[levelExitIndexes.length - 1]) {
                levelOffsets.pop();
                levelExitIndexes.pop();
            }
        }

        this._timelineData = {
            nodeCount: nodeCount,
            totalTime: this._cpuProfileView.profileHead.totalTime,
            startTimes: startTimes,
            durations: durations,
            depths: depths,
            colorPairs: colorPairs,
            nodes: nodes
        }

        return this._timelineData;
    },

    _getPopoverAnchor: function()
    {
        if (this._highlightedNodeIndex === -1)
            return null;
        return this._anchorElement;
    },

    _showPopover: function(anchor, popover)
    {
        var node = this._timelineData.nodes[this._highlightedNodeIndex];
        var contentHelper = new WebInspector.PopoverContentHelper(node.functionName);
        contentHelper.appendTextRow(WebInspector.UIString("Total time"), Number.secondsToString(node.totalTime / 1000, true));
        contentHelper.appendTextRow(WebInspector.UIString("Self time"), Number.secondsToString(node.selfTime / 1000, true));
        if (node.numberOfCalls)
            contentHelper.appendTextRow(WebInspector.UIString("Number of calls"), node.numberOfCalls);
        if (node.url) {
            var link = this._linkifier.linkifyLocation(node.url, node.lineNumber);
            contentHelper.appendElementRow("Location", link);
        }

        popover.show(contentHelper._contentTable, anchor);
    },

    _hidePopover: function()
    {
        this._popoverHelper.hidePopover();
        this._linkifier.reset();
    },

    _onClick: function(e)
    {
        if (this._highlightedNodeIndex === -1)
            return;
        var node = this._timelineData.nodes[this._highlightedNodeIndex];
        this.dispatchEventToListeners(WebInspector.FlameChart.Events.SelectedNode, node);
    },

    _onMouseMove: function(e)
    {
        var nodeIndex = this._coordinatesToNodeIndex(e.offsetX, e.offsetY);
        if (nodeIndex === this._highlightedNodeIndex)
            return;
        this._highlightedNodeIndex = nodeIndex;
        this.update();

        if (nodeIndex === -1)
            return;

        var timelineData = this._timelineData;

        var style = this._anchorElement.style;
        style.width = Math.floor(timelineData.durations[nodeIndex] * this._xScaleFactor) + "px";
        style.height = this._barHeight + "px";
        style.left = Math.floor(timelineData.startTimes[nodeIndex] * this._xScaleFactor) + "px";
        style.top = Math.floor(this._canvas.height - (timelineData.depths[nodeIndex] + 1) * this._barHeight) + "px";
    },

    /**
     * @param {!number} x
     * @param {!number} y
     */
    _coordinatesToNodeIndex: function(x, y)
    {
        var timelineData = this._timelineData;
        if (!timelineData)
            return -1;
        var cursorTime = x / this._xScaleFactor;
        var cursorLevel = Math.floor((this._canvas.height - y) / this._barHeight);

        for (var i = 0; i < timelineData.nodeCount; ++i) {
            if (cursorTime < timelineData.startTimes[i])
                return -1;
            if (cursorTime < (timelineData.startTimes[i] + timelineData.durations[i])
                && cursorLevel === timelineData.depths[i])
                return i;
        }
        return -1;
    },

    onResize: function()
    {
        this._hidePopover();
        this.update();
    },

    /**
     * @param {!number} height
     * @param {!number} width
     */
    draw: function(width, height)
    {
        var timelineData = this._calculateTimelineData();
        if (!timelineData)
            return;
        this._canvas.height = height;
        this._canvas.width = width;
        var xScaleFactor = this._xScaleFactor = width / timelineData.totalTime;
        var barHeight = this._barHeight;

        var context = this._canvas.getContext("2d");

        for (var i = 0; i < timelineData.nodeCount; ++i) {
            var barWidth = Math.floor(timelineData.durations[i] * xScaleFactor);
            if (barWidth < this._minWidth)
                continue;
            var x = Math.floor(timelineData.startTimes[i] * xScaleFactor);
            var y = height - (timelineData.depths[i] + 1) * barHeight;

            var colorPair = timelineData.colorPairs[i];
            var color;
            if (this._highlightedNodeIndex === i)
                color =  colorPair.highlighted;
            else
                color = colorPair.normal;

            context.beginPath();
            context.rect(x, y, barWidth - 1, barHeight - 1);
            context.fillStyle = color;
            context.fill();
        }
    },

    update: function()
    {
        this.draw(this.element.clientWidth, this.element.clientHeight);
    },

    __proto__: WebInspector.View.prototype
};
