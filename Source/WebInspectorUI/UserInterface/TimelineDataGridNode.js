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
        this._timelineRecordBars = [];
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
        if (!this._graphDataSource)
            return {};

        var records = this.records || [];
        return {graph: records.length ? records[0].startTime : 0};
    },

    createCellContent: function(columnIdentifier, cell)
    {
        if (columnIdentifier === "graph" && this._graphDataSource) {
            this.refreshGraph();
            return this._graphContainerElement;
        }

        var value = this.data[columnIdentifier];
        if (!value)
            return "\u2014";

        if (value instanceof WebInspector.SourceCodeLocation) {
            if (value.sourceCode instanceof WebInspector.Resource) {
                cell.classList.add(WebInspector.ResourceTreeElement.ResourceIconStyleClassName);
                cell.classList.add(value.sourceCode.type);
            } else if (value.sourceCode instanceof WebInspector.Script) {
                if (value.sourceCode.url) {
                    cell.classList.add(WebInspector.ResourceTreeElement.ResourceIconStyleClassName);
                    cell.classList.add(WebInspector.Resource.Type.Script);
                } else
                    cell.classList.add(WebInspector.ScriptTreeElement.AnonymousScriptIconStyleClassName);
            } else
                console.error("Unknown SourceCode subclass.");

            // Give the whole cell a tooltip and keep it up to date.
            value.populateLiveDisplayLocationTooltip(cell);

            var fragment = document.createDocumentFragment();

            var goToArrowButtonLink = WebInspector.createSourceCodeLocationLink(value, false, true);
            fragment.appendChild(goToArrowButtonLink);

            var icon = document.createElement("div");
            icon.className = WebInspector.ScriptTimelineDataGridNode.IconStyleClassName;
            fragment.appendChild(icon);

            var titleElement = document.createElement("span");
            value.populateLiveDisplayLocationString(titleElement, "textContent");
            fragment.appendChild(titleElement);

            return fragment;
        }

        if (value instanceof WebInspector.CallFrame) {
            var callFrame = value;

            var isAnonymousFunction = false;
            var functionName = callFrame.functionName;
            if (!functionName) {
                functionName = WebInspector.UIString("(anonymous function)");
                isAnonymousFunction = true;
            }

            cell.classList.add(WebInspector.CallFrameTreeElement.FunctionIconStyleClassName);

            var fragment = document.createDocumentFragment();

            if (callFrame.sourceCodeLocation && callFrame.sourceCodeLocation.sourceCode) {
                // Give the whole cell a tooltip and keep it up to date.
                callFrame.sourceCodeLocation.populateLiveDisplayLocationTooltip(cell);

                var goToArrowButtonLink = WebInspector.createSourceCodeLocationLink(callFrame.sourceCodeLocation, false, true);
                fragment.appendChild(goToArrowButtonLink);

                var icon = document.createElement("div");
                icon.className = WebInspector.LayoutTimelineDataGridNode.IconStyleClassName;
                fragment.appendChild(icon);

                if (isAnonymousFunction) {
                    // For anonymous functions we show the resource or script icon and name.
                    if (callFrame.sourceCodeLocation.sourceCode instanceof WebInspector.Resource) {
                        cell.classList.add(WebInspector.ResourceTreeElement.ResourceIconStyleClassName);
                        cell.classList.add(callFrame.sourceCodeLocation.sourceCode.type);
                    } else if (callFrame.sourceCodeLocation.sourceCode instanceof WebInspector.Script) {
                        if (callFrame.sourceCodeLocation.sourceCode.url) {
                            cell.classList.add(WebInspector.ResourceTreeElement.ResourceIconStyleClassName);
                            cell.classList.add(WebInspector.Resource.Type.Script);
                        } else
                            cell.classList.add(WebInspector.ScriptTreeElement.AnonymousScriptIconStyleClassName);
                    } else
                        console.error("Unknown SourceCode subclass.");

                    var titleElement = document.createElement("span");
                    callFrame.sourceCodeLocation.populateLiveDisplayLocationString(titleElement, "textContent");

                    fragment.appendChild(titleElement);
                } else {
                    // Show the function name and icon.
                    cell.classList.add(WebInspector.CallFrameTreeElement.FunctionIconStyleClassName);

                    fragment.appendChild(document.createTextNode(functionName));

                    var subtitleElement = document.createElement("span");
                    subtitleElement.className = WebInspector.LayoutTimelineDataGridNode.SubtitleStyleClassName;
                    callFrame.sourceCodeLocation.populateLiveDisplayLocationString(subtitleElement, "textContent");

                    fragment.appendChild(subtitleElement);
                }

                return fragment;
            }

            var icon = document.createElement("div");
            icon.className = WebInspector.LayoutTimelineDataGridNode.IconStyleClassName;
            fragment.appendChild(icon);

            fragment.appendChild(document.createTextNode(functionName));

            return fragment;
        }

        return WebInspector.DataGridNode.prototype.createCellContent.call(this, columnIdentifier, cell);
    },

    refresh: function()
    {
        if (this._graphDataSource && this._graphOnly) {
            this.refreshGraph();
            return;
        }

        WebInspector.DataGridNode.prototype.refresh.call(this);
    },

    refreshGraph: function()
    {
        if (!this._graphDataSource)
            return;

        if (this._scheduledGraphRefreshIdentifier) {
            cancelAnimationFrame(this._scheduledGraphRefreshIdentifier);
            delete this._scheduledGraphRefreshIdentifier;
        }

        var records = this.records;
        if (!records || !records.length)
            return;

        // Fast path for single records.
        if (records.length === 1) {
            var record = records[0];
            var timelineRecordBar = this._timelineRecordBars[0];

            if (timelineRecordBar && timelineRecordBar.record !== record) {
                timelineRecordBar.element.remove();
                timelineRecordBar = null;
            }

            if (!timelineRecordBar)
                timelineRecordBar = this._timelineRecordBars[0] = new WebInspector.TimelineRecordBar(record);

            if (timelineRecordBar.refresh(this._graphDataSource)) {
                if (!timelineRecordBar.element.parentNode)
                    this._graphContainerElement.appendChild(timelineRecordBar.element);
            } else
                timelineRecordBar.element.remove();

            return;
        }

        // Multiple records attempt to share a bar if their time is close to prevent overlapping bars.
        var startTime = this._graphDataSource.startTime;
        var currentTime = this._graphDataSource.currentTime;
        var endTime = this._graphDataSource.endTime;
        var duration = endTime - startTime;
        var visibleWidth = this._graphContainerElement.offsetWidth;
        var secondsPerPixel = duration / visibleWidth;

        var recordBarIndex = 0;
        var barRecords = [];

        function createBar(barRecords)
        {
            var timelineRecordBar = this._timelineRecordBars[recordBarIndex];
            if (!timelineRecordBar)
                timelineRecordBar = this._timelineRecordBars[recordBarIndex] = new WebInspector.TimelineRecordBar;
            timelineRecordBar.records = barRecords;
            timelineRecordBar.refresh(this._graphDataSource);
            if (!timelineRecordBar.element.parentNode)
                this._graphContainerElement.appendChild(timelineRecordBar.element);
            ++recordBarIndex;
        }

        for (var record of records) {
            // Combining multiple record bars is not supported with records that have inactive time.
            // ResourceTimelineRecord is the only one right, and it is always a single record handled above.
            console.assert(!record.usesActiveStartTime);

            if (isNaN(record.startTime))
                continue;

            // If this bar is completely before the bounds of the graph, skip this record.
            if (record.endTime < startTime)
                continue;

            // If this record is completely after the current time or end time, break out now.
            // Records are sorted, so all records after this will be beyond the current or end time too.
            if (record.startTime > currentTime || record.startTime > endTime)
                break;

            // Check if the previous record can be combined with the current record, if not make a new bar.
            if (barRecords.length && WebInspector.TimelineRecordBar.recordsCannotBeCombined(barRecords, record, secondsPerPixel)) {
                createBar.call(this, barRecords);
                barRecords = [];
            }

            barRecords.push(record);
        }

        // Create the bar for the last record if needed.
        if (barRecords.length)
            createBar.call(this, barRecords);

        // Remove the remaining unused TimelineRecordBars.
        for (; recordBarIndex < this._timelineRecordBars.length; ++recordBarIndex) {
            this._timelineRecordBars[recordBarIndex].records = null;
            this._timelineRecordBars[recordBarIndex].element.remove();
        }
    },

    needsGraphRefresh: function()
    {
        if (!this._graphDataSource || this._scheduledGraphRefreshIdentifier)
            return;

        this._scheduledGraphRefreshIdentifier = requestAnimationFrame(this.refreshGraph.bind(this));
    }
};
