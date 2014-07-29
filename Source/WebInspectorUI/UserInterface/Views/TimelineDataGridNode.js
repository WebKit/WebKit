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

WebInspector.TimelineDataGridNode = function(graphOnly, graphDataSource, hasChildren)
{
    WebInspector.DataGridNode.call(this, {}, hasChildren);

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

    collapse: function()
    {
        WebInspector.DataGridNode.prototype.collapse.call(this);

        if (!this._graphDataSource || !this.revealed)
            return;

        // Refresh to show child bars in our graph now that we collapsed.
        this.refreshGraph();
    },

    expand: function()
    {
        WebInspector.DataGridNode.prototype.expand.call(this);

        if (!this._graphDataSource || !this.revealed)
            return;

        // Refresh to remove child bars from our graph now that we expanded.
        this.refreshGraph();

        // Refresh child graphs since they haven't been updating while we were collapsed.
        var childNode = this.children[0];
        while (childNode) {
            if (childNode instanceof WebInspector.TimelineDataGridNode)
                childNode.refreshGraph();
            childNode = childNode.traverseNextNode(true, this);
        }
    },

    createCellContent: function(columnIdentifier, cell)
    {
        if (columnIdentifier === "graph" && this._graphDataSource) {
            this.needsGraphRefresh();
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
            this.needsGraphRefresh();
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

        // We are not visible, but an ancestor will draw our graph.
        // They need notified by using our needsGraphRefresh.
        console.assert(this.revealed);
        if (!this.revealed)
            return;

        var secondsPerPixel = this._graphDataSource.secondsPerPixel;
        console.assert(isFinite(secondsPerPixel) && secondsPerPixel > 0);

        var recordBarIndex = 0;

        function createBar(records, renderMode)
        {
            var timelineRecordBar = this._timelineRecordBars[recordBarIndex];
            if (!timelineRecordBar)
                timelineRecordBar = this._timelineRecordBars[recordBarIndex] = new WebInspector.TimelineRecordBar(records, renderMode);
            else {
                timelineRecordBar.renderMode = renderMode;
                timelineRecordBar.records = records;
            }
            timelineRecordBar.refresh(this._graphDataSource);
            if (!timelineRecordBar.element.parentNode)
                this._graphContainerElement.appendChild(timelineRecordBar.element);
            ++recordBarIndex;
        }

        var boundCreateBar = createBar.bind(this);

        if (this.expanded) {
            // When expanded just use the records for this node.
            WebInspector.TimelineRecordBar.createCombinedBars(this.records, secondsPerPixel, this._graphDataSource, boundCreateBar);
        } else {
            // When collapsed use the records for this node and its descendants.
            // To share bars better, group records by type.

            var recordTypeMap = new Map;

            function collectRecordsByType(records)
            {
                for (var record of records) {
                    var typedRecords = recordTypeMap.get(record.type);
                    if (!typedRecords) {
                        typedRecords = [];
                        recordTypeMap.set(record.type, typedRecords);
                    }

                    typedRecords.push(record);
                }
            }

            collectRecordsByType(this.records);

            var childNode = this.children[0];
            while (childNode) {
                if (childNode instanceof WebInspector.TimelineDataGridNode)
                    collectRecordsByType(childNode.records);
                childNode = childNode.traverseNextNode(false, this);
            }

            for (var records of recordTypeMap.values())
                WebInspector.TimelineRecordBar.createCombinedBars(records, secondsPerPixel, this._graphDataSource, boundCreateBar);
        }

        // Remove the remaining unused TimelineRecordBars.
        for (; recordBarIndex < this._timelineRecordBars.length; ++recordBarIndex) {
            this._timelineRecordBars[recordBarIndex].records = null;
            this._timelineRecordBars[recordBarIndex].element.remove();
        }
    },

    needsGraphRefresh: function()
    {
        if (!this.revealed) {
            // We are not visible, but an ancestor will be drawing our graph.
            // Notify the next visible ancestor that their graph needs to refresh.
            var ancestor = this;
            while (ancestor && !ancestor.root) {
                if (ancestor.revealed && ancestor instanceof WebInspector.TimelineDataGridNode) {
                    ancestor.needsGraphRefresh();
                    return;
                }

                ancestor = ancestor.parent;
            }

            return;
        }

        if (!this._graphDataSource || this._scheduledGraphRefreshIdentifier)
            return;

        this._scheduledGraphRefreshIdentifier = requestAnimationFrame(this.refreshGraph.bind(this));
    },

    // Protected

    isRecordVisible: function(record)
    {
        if (!this._graphDataSource)
            return false;

        if (isNaN(record.startTime))
            return false;

        // If this bar is completely before the bounds of the graph, not visible.
        if (record.endTime < this.graphDataSource.startTime)
            return false;

        // If this record is completely after the current time or end time, not visible.
        if (record.startTime > this.graphDataSource.currentTime || record.startTime > this.graphDataSource.endTime)
            return false;

        return true;
    }
};
