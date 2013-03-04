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
    this._yScaleFactor = 10;
    this._minWidth = 3;
    this.element.onmousemove = this._onMouseMove.bind(this);
}

WebInspector.FlameChart.prototype = {
    _onMouseMove: function(e)
    {
        var node = this._coordinatesToNode(e.offsetX, e.offsetY);
        if (node !== this._highlightedNode) {
            this._highlightedNode = node;
            this.update();
        }
    },

    /**
     * @param {!number} x
     * @param {!number} y
     */
    _coordinatesToNode: function(x, y)
    {
        var cursorOffset = x / this._xScaleFactor;
        var cursorLevel = (this._canvas.height - y) / this._yScaleFactor;
        this._highlightedNode = null;
        var cursorNode;

        function findNodeCallback(offset, level, node)
        {
            if (cursorLevel > level && cursorLevel < level + 1 && cursorOffset > offset && cursorOffset < offset + node.totalTime)
                cursorNode = node;
        }
        this._forEachNode(findNodeCallback.bind(this));
        return cursorNode;
    },

    onResize: function()
    {
        this.draw(this.element.clientWidth, this.element.clientHeight);
    },

    /**
     * @return {Array.<!ProfilerAgent.CPUProfileNode> }
     */
    _rootNodes: function()
    {
        if (this._rootNodesArray)
            return this._rootNodesArray.slice();

        var profileHead = this._cpuProfileView.profileHead;
        if (!profileHead)
            return null;
        var totalTime = 0;
        var nodes = [];
        for (var i = 0; i < profileHead.children.length; ++i) {
            var node = profileHead.children[i];
            if (node.functionName === "(program)" || node.functionName === "(idle)")
                continue;
            totalTime += node.totalTime;
            nodes.push(node);
        }

        this._rootNodesArray = nodes;
        this._totalTime = totalTime;
        return nodes.slice();
    },

    /**
     * @param {!number} height
     * @param {!number} width
     */
    draw: function(width, height)
    {
        if (!this._rootNodes())
            return;

        var margin = 0;
        this._canvas.height = height - margin;
        this._canvas.width = width - margin;

        this._xScaleFactor = width / this._totalTime;
        this._colorIndex = 0;

        this._context = this._canvas.getContext("2d");

        this._forEachNode(this._drawNode.bind(this));
    },

    _drawNode: function(offset, level, node)
    {
        ++this._colorIndex;
        var hue = (this._colorIndex * 2 + 11 * (this._colorIndex % 2)) % 360;
        var lightness = this._highlightedNode === node ? 33 : 67;
        var color = "hsl(" + hue + ", 100%, " + lightness + "%)";
        this._drawBar(this._context, offset, level, node, color);
    },

    _forEachNode: function(callback)
    {
        var nodes = this._rootNodes();
        var levelOffsets = /** @type {Array.<!number>} */ [0];
        var levelExitIndexes = [0];

        while (nodes.length) {
            var level = levelOffsets.length - 1;
            var node = nodes.pop();
            if (node.totalTime * this._xScaleFactor > this._minWidth) {
                var offset = levelOffsets[level];
                callback(offset, level, node);
                levelOffsets[level] += node.totalTime;
                if (node.children.length) {
                    levelExitIndexes.push(nodes.length);
                    levelOffsets.push(offset + node.selfTime / 2);
                    nodes = nodes.concat(node.children);
                }
            }
            while (nodes.length === levelExitIndexes[levelExitIndexes.length - 1]) {
                levelOffsets.pop();
                levelExitIndexes.pop();
            }
        }
    },

    /**
     * @param {Object} context
     * @param {number} offset
     * @param {number} level
     * @param {!ProfilerAgent.CPUProfileNode} node
     * @param {!WebInspector.Color} hslColor
     */
    _drawBar: function(context, offset, level, node, hslColor)
    {
        var width = node.totalTime * this._xScaleFactor;
        var height = this._yScaleFactor;
        var x = offset * this._xScaleFactor;
        var y = this._canvas.height - level * this._yScaleFactor - height;
        context.beginPath();
        context.rect(x, y, width - 1, height - 1);
        context.fillStyle = hslColor;
        context.fill();
    },

    update: function()
    {
        this.draw(this.element.clientWidth, this.element.clientHeight);
    },

    __proto__: WebInspector.View.prototype
};

//@ sourceURL=http://localhost/inspector/front-end/FlameChart.js
