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
 */
WebInspector.NativeMemorySnapshotView = function(profile)
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("nativeMemoryProfiler.css");
    this._profile = profile;
    this.element.addStyleClass("memory-chart-view");

    var pieChart = new WebInspector.NativeMemoryPieChart(profile._memoryBlock);
    pieChart.element.addStyleClass("fill");
    pieChart.show(this.element);
}

WebInspector.NativeMemorySnapshotView.prototype = {
    dispose: function()
    {
    },

    get statusBarItems()
    {
        return [];
    },

    get profile()
    {
        return this._profile;
    }
}

WebInspector.NativeMemorySnapshotView.prototype.__proto__ = WebInspector.View.prototype;

/**
 * @constructor
 * @extends {WebInspector.ProfileType}
 */
WebInspector.NativeMemoryProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.NativeMemoryProfileType.TypeId, WebInspector.UIString("Take Native Memory Snapshot"));
    this._nextProfileUid = 1;
}

WebInspector.NativeMemoryProfileType.TypeId = "NATIVE_MEMORY";

WebInspector.NativeMemoryProfileType.prototype = {
    get buttonTooltip()
    {
        return WebInspector.UIString("Take native memory snapshot.");
    },

    /**
     * @override
     * @return {boolean}
     */
    buttonClicked: function()
    {
        var profilesPanel = WebInspector.panels.profiles;
        var profileHeader = new WebInspector.NativeMemoryProfileHeader(this, WebInspector.UIString("Snapshot %d", this._nextProfileUid), this._nextProfileUid);
        ++this._nextProfileUid;
        profileHeader.isTemporary = true;
        profilesPanel.addProfileHeader(profileHeader);
        function didReceiveMemorySnapshot(error, memoryBlock)
        {
            if (memoryBlock.size && memoryBlock.children) {
                var knownSize = 0;
                for (var i = 0; i < memoryBlock.children.length; i++) {
                    var size = memoryBlock.children[i].size;
                    if (size)
                        knownSize += size;
                }
                var unknownSize = memoryBlock.size - knownSize;

                if (unknownSize) {
                    memoryBlock.children.push({
                        name: "Unknown",
                        size: unknownSize
                    });
                }
            }
            profileHeader._memoryBlock = memoryBlock;
            profileHeader.isTemporary = false;
        }
        MemoryAgent.getProcessMemoryDistribution(didReceiveMemorySnapshot.bind(this));
        return false;
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("MEMORY DISTRIBUTION");
    },

    get description()
    {
        return WebInspector.UIString("Native memory snapshot profiles show memory distribution among browser subsystems");
    },

    /**
     * @override
     * @param {string=} title
     * @return {WebInspector.ProfileHeader}
     */
    createTemporaryProfile: function(title)
    {
        title = title || WebInspector.UIString("Snapshotting\u2026");
        return new WebInspector.NativeMemoryProfileHeader(this, title);
    },

    /**
     * @override
     * @param {ProfilerAgent.ProfileHeader} profile
     * @return {WebInspector.ProfileHeader}
     */
    createProfile: function(profile)
    {
        return new WebInspector.NativeMemoryProfileHeader(this, profile.title, -1);
    }
}

WebInspector.NativeMemoryProfileType.prototype.__proto__ = WebInspector.ProfileType.prototype;

/**
 * @constructor
 * @extends {WebInspector.ProfileHeader}
 * @param {WebInspector.NativeMemoryProfileType} type
 * @param {string} title
 * @param {number=} uid
 */
WebInspector.NativeMemoryProfileHeader = function(type, title, uid)
{
    WebInspector.ProfileHeader.call(this, type, title, uid);

    /**
     * @type {MemoryAgent.MemoryBlock}
     */
    this._memoryBlock = null;
}

WebInspector.NativeMemoryProfileHeader.prototype = {
    /**
     * @override
     */
    createSidebarTreeElement: function()
    {
        return new WebInspector.ProfileSidebarTreeElement(this, WebInspector.UIString("Snapshot %d"), "heap-snapshot-sidebar-tree-item");
    },

    /**
     * @override
     */
    createView: function()
    {
        return new WebInspector.NativeMemorySnapshotView(this);
    }
}

WebInspector.NativeMemoryProfileHeader.prototype.__proto__ = WebInspector.ProfileHeader.prototype;

/**
 * @constructor
 * @param {string} fillStyle
 * @param {string} name
 * @param {string} description
 */
WebInspector.MemoryBlockViewProperties = function(fillStyle, name, description)
{
    this._fillStyle = fillStyle;
    this._name = name;
    this._description = description;
}

/**
 * @type {Object.<string, WebInspector.MemoryBlockViewProperties>}
 */
WebInspector.MemoryBlockViewProperties._standardBlocks = null;

WebInspector.MemoryBlockViewProperties._initialize = function()
{
    if (WebInspector.MemoryBlockViewProperties._standardBlocks)
        return;
    WebInspector.MemoryBlockViewProperties._standardBlocks = {};
    function addBlock(fillStyle, name, description)
    {
        WebInspector.MemoryBlockViewProperties._standardBlocks[name] = new WebInspector.MemoryBlockViewProperties(fillStyle, name, WebInspector.UIString(description));
    }
    addBlock("rgba(255, 255, 255, 0.8)", "ProcessPrivateMemory", "Total");
    addBlock("rgba(240, 240, 250, 0.8)", "Unknown", "Unknown");
    addBlock("rgba(250, 200, 200, 0.8)", "JSHeapAllocated", "JavaScript heap");
    addBlock("rgba(200, 250, 200, 0.8)", "JSHeapUsed", "Used JavaScript heap");
    addBlock("rgba(200, 170, 200, 0.8)", "MemoryCache", "Memory cache resources");
    addBlock("rgba(250, 250, 150, 0.8)", "RenderTreeAllocated", "Render tree");
    addBlock("rgba(200, 150, 150, 0.8)", "RenderTreeUsed", "Render tree used");
}

WebInspector.MemoryBlockViewProperties._forMemoryBlock = function(memoryBlock)
{
    WebInspector.MemoryBlockViewProperties._initialize();
    var result = WebInspector.MemoryBlockViewProperties._standardBlocks[memoryBlock.name];
    if (result)
        return result;
    return new WebInspector.MemoryBlockViewProperties("rgba(20, 200, 20, 0.8)", memoryBlock.name, memoryBlock.name);
}


/**
 * @constructor
 * @extends {WebInspector.View}
 * @param {MemoryAgent.MemoryBlock} memorySnapshot
 */
WebInspector.NativeMemoryPieChart = function(memorySnapshot)
{
    WebInspector.View.call(this);
    this._memorySnapshot = memorySnapshot;
    this.element = document.createElement("div");
    this.element.addStyleClass("memory-pie-chart-container");
    this._memoryBlockList = this.element.createChild("div", "memory-blocks-list");

    this._canvasContainer = this.element.createChild("div", "memory-pie-chart");
    this._canvas = this._canvasContainer.createChild("canvas");
    this._addBlockLabels(memorySnapshot, true);
}

WebInspector.NativeMemoryPieChart.prototype = {
    /**
     * @override
     */
    onResize: function()
    {
        this._updateSize();
        this._paint();
    },

    _updateSize: function()
    {
        var width = this._canvasContainer.clientWidth - 5;
        var height = this._canvasContainer.clientHeight - 5;
        this._canvas.width = width;
        this._canvas.height = height;
    },

    _addBlockLabels: function(memoryBlock, includeChildren)
    {
        var viewProperties = WebInspector.MemoryBlockViewProperties._forMemoryBlock(memoryBlock);
        var title = viewProperties._description + ": " + Number.bytesToString(memoryBlock.size);

        var swatchElement = this._memoryBlockList.createChild("div", "item");
        swatchElement.createChild("div", "swatch").style.backgroundColor = viewProperties._fillStyle;
        swatchElement.createChild("span", "title").textContent = WebInspector.UIString(title);

        if (!memoryBlock.children || !includeChildren)
            return;
        for (var i = 0; i < memoryBlock.children.length; i++)
            this._addBlockLabels(memoryBlock.children[i], false);
    },

    _paint: function()
    {
        this._clear();
        var width = this._canvas.width;
        var height = this._canvas.height;

        var x = width / 2;
        var y = height / 2;
        var radius = 200;

        var ctx = this._canvas.getContext("2d");
        ctx.beginPath();
        ctx.arc(x, y, radius, 0, Math.PI*2, false);
        ctx.lineWidth = 1;
        ctx.strokeStyle = "rgba(130, 130, 130, 0.8)";
        ctx.stroke();
        ctx.closePath();

        var startAngle = - Math.PI / 2;
        var currentAngle = startAngle;
        var memoryBlock = this._memorySnapshot;

        function paintPercentAndLabel(fraction, title, midAngle)
        {
            ctx.beginPath();
            ctx.font = "14px Arial";
            ctx.fillStyle = "rgba(10, 10, 10, 0.8)";

            var textX = x + radius * Math.cos(midAngle) / 2;
            var textY = y + radius * Math.sin(midAngle) / 2;
            ctx.fillText((100 * fraction).toFixed(0) + "%", textX, textY);

            textX = x + radius * Math.cos(midAngle);
            textY = y + radius * Math.sin(midAngle);
            if (midAngle <= startAngle + Math.PI) {
                textX += 10;
                textY += 10;
            } else {
                var metrics = ctx.measureText(title);
                textX -= metrics.width + 10;
            }
            ctx.fillText(title, textX, textY);
            ctx.closePath();
        }

        if (!memoryBlock.children)
            return;
        var total = memoryBlock.size;
        for (var i = 0; i < memoryBlock.children.length; i++) {
            var child = memoryBlock.children[i];
            if (!child.size)
                continue;
            var viewProperties = WebInspector.MemoryBlockViewProperties._forMemoryBlock(child);
            var angleSpan = Math.PI * 2 * (child.size / total);
            ctx.beginPath();
            ctx.moveTo(x, y);
            ctx.lineTo(x + radius * Math.cos(currentAngle), y + radius * Math.sin(currentAngle));
            ctx.arc(x, y, radius, currentAngle, currentAngle + angleSpan, false);
            ctx.lineWidth = 0.5;
            ctx.lineTo(x, y);
            ctx.fillStyle = viewProperties._fillStyle;
            ctx.strokeStyle = "rgba(100, 100, 100, 0.8)";
            ctx.fill();
            ctx.stroke();
            ctx.closePath();

            paintPercentAndLabel(child.size / total, viewProperties._description, currentAngle + angleSpan / 2);

            currentAngle += angleSpan;
        }
    },

    _clear: function() {
        var ctx = this._canvas.getContext("2d");
        ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    }
}

WebInspector.NativeMemoryPieChart.prototype.__proto__ = WebInspector.View.prototype;
