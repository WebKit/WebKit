/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.TimelineDataGridNode = function(graphOnly, graphDataSource)
{
    WebInspector.DataGridNode.call(this, {});

    this._graphOnly = graphOnly || false;
    this._graphDataSource = graphDataSource || null;

    if (graphDataSource) {
        this._graphContainerElement = document.createElement("div");
        this._timelineRecordBarMap = new Map;
    }
};

WebInspector.Object.addConstructorFunctions(WebInspector.TimelineDataGridNode);

WebInspector.TimelineDataGridNode.prototype = {
    constructor: WebInspector.TimelineDataGridNode,
    __proto__: WebInspector.DataGridNode.prototype,

    // Public

    get records()
    {
        // Implemented by subclasses.
        return [];
    },

    get graphDataSource()
    {
        return this._graphDataSource;
    },

    get data()
    {
        var records = this.records || [];
        return {graph: records.length ? records[0].startTime : 0};
    },

    createCellContent: function(columnIdentifier, cell)
    {
        if (columnIdentifier === "graph") {
            this.refreshGraph();
            return this._graphContainerElement;
        }

        return WebInspector.DataGridNode.prototype.createCellContent.call(this, columnIdentifier, cell);
    },

    refresh: function()
    {
        if (this._graphOnly) {
            this.refreshGraph();
            return;
        }

        WebInspector.DataGridNode.prototype.refresh.call(this);
    },

    refreshGraph: function()
    {
        if (this._scheduledGraphRefreshIdentifier) {
            cancelAnimationFrame(this._scheduledGraphRefreshIdentifier);
            delete this._scheduledGraphRefreshIdentifier;
        }

        var records = this.records || [];
        for (var record of records) {
            var timelineRecordBar = this._timelineRecordBarMap.get(record);
            if (!timelineRecordBar) {
                timelineRecordBar = new WebInspector.TimelineRecordBar(record);
                this._timelineRecordBarMap.set(record, timelineRecordBar);
            }

            if (timelineRecordBar.refresh(this._graphDataSource)) {
                if (!timelineRecordBar.element.parentNode)
                    this._graphContainerElement.appendChild(timelineRecordBar.element);
            } else {
                timelineRecordBar.element.remove();
            }
        }
    },

    needsGraphRefresh: function()
    {
        if (this._scheduledGraphRefreshIdentifier)
            return;

        this._scheduledGraphRefreshIdentifier = requestAnimationFrame(this.refreshGraph.bind(this));
    }
};
