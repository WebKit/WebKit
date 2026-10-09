/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.CPUTimelineOverviewGraph = class CPUTimelineOverviewGraph extends WI.TimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        console.assert(timeline instanceof WI.Timeline);
        console.assert(timeline.type === WI.TimelineRecord.Type.CPU, timeline);

        super(timelineOverview);

        this.element.classList.add("cpu");

        this._cpuTimeline = timeline;
        this._cpuTimeline.addEventListener(WI.Timeline.Event.RecordAdded, this._cpuTimelineRecordAdded, this);

        this.element.addEventListener("click", this._handleChartClick.bind(this));

        this._legendElement = this.element.appendChild(document.createElement("div"));
        this._legendElement.classList.add("legend");

        this._columnGeometries = [];
        this._lastSelectedRecordInLayout = null;

        this.reset();

        for (let record of this._cpuTimeline.records)
            this._processRecord(record);
    }

    // Protected

    get height()
    {
        return 60;
    }

    reset()
    {
        super.reset();

        this._maxUsage = 0;
        this._cachedMaxUsage = undefined;
        this._columnGeometries = [];
        this._lastSelectedRecordInLayout = null;

        this._updateLegend();
    }

    layout()
    {
        super.layout();

        if (this.hidden)
            return;

        this._updateLegend();
        this._columnGeometries = [];
        this._lastSelectedRecordInLayout = this.selectedRecord;

        let graphWidth = this.timelineOverview.scrollContainerWidth;
        if (isNaN(graphWidth)) {
            this.clearCanvas();
            return;
        }

        this.updateCanvas();
    }

    drawCanvas(context)
    {
        let graphStartTime = this.startTime;
        let visibleEndTime = Math.min(this.endTime, this.currentTime);
        let secondsPerPixel = this.timelineOverview.secondsPerPixel;
        let maxCapacity = Math.max(20, this._maxUsage * 1.05); // Add 5% for padding.

        function xScale(time) {
            return (time - graphStartTime) / secondsPerPixel;
        }

        let height = this.canvasHeight;
        function yScale(size) {
            return (size / maxCapacity) * height;
        }

        let visibleRecords = this._cpuTimeline.recordsInTimeRange(graphStartTime, visibleEndTime, {
            includeRecordBeforeStart: true,
        });
        if (!visibleRecords.length)
            return;

        const minimumDisplayHeight = 4;

        for (let record of visibleRecords) {
            let w = (record.endTime - record.startTime) / secondsPerPixel;
            let x = xScale(record.startTime);
            let heights = {
                mainThread: Math.max(minimumDisplayHeight, yScale(record.mainThreadUsage)),
                workerThreads: Math.max(minimumDisplayHeight, yScale(record.mainThreadUsage + record.workerThreadUsage)),
                otherThreads: Math.max(minimumDisplayHeight, yScale(record.usage)),
            };
            this._columnGeometries.push({x, width: w, heights, selected: record === this.selectedRecord});
        }

        this._drawColumns(context);
    }

    updateSelectedRecord()
    {
        super.updateSelectedRecord();

        if (this._lastSelectedRecordInLayout !== this.selectedRecord) {
            // Since we don't have the exact element to re-style with a selected appearance
            // we trigger another layout to re-layout the graph and provide additional
            // styles for the column for the selected record.
            this.needsLayout();
        }
    }

    // Private

    _updateLegend()
    {
        if (this._cachedMaxUsage === this._maxUsage)
            return;

        this._cachedMaxUsage = this._maxUsage;

        if (!this._maxUsage) {
            this._legendElement.hidden = true;
            this._legendElement.textContent = "";
        } else {
            this._legendElement.hidden = false;
            this._legendElement.textContent = WI.UIString("Maximum CPU Usage: %s").format(Number.percentageString(this._maxUsage / 100));
        }
    }

    _drawColumns(context)
    {
        let selectedFillStyle = this.cssVariableValue("--selected-background-color");
        let selectedStrokeStyle = this.cssVariableValue("--selected-background-color-active");

        this._drawColumnSection(context, "otherThreads", "workerThreads", selectedFillStyle, selectedStrokeStyle);
        this._drawColumnSection(context, "workerThreads", "mainThread", selectedFillStyle, selectedStrokeStyle);
        this._drawColumnSection(context, "mainThread", null, selectedFillStyle, selectedStrokeStyle);
    }

    _drawColumnSection(context, sectionName, foregroundSectionName, selectedFillStyle, selectedStrokeStyle)
    {
        let {fill, stroke} = WI.CPUTimelineOverviewGraph._sectionStyles[sectionName];
        let canvasHeight = this.canvasHeight;

        context.beginPath();
        let selectedColumnGeometry = null;
        for (let columnGeometry of this._columnGeometries) {
            if (columnGeometry.selected) {
                selectedColumnGeometry = columnGeometry;
                continue;
            }

            let {x, width, heights} = columnGeometry;
            if (foregroundSectionName && heights[sectionName] === heights[foregroundSectionName])
                continue;

            let height = heights[sectionName];
            context.rect(x, canvasHeight - height, width, height);
        }
        context.fillStyle = fill;
        context.fill();
        context.strokeStyle = stroke;
        context.stroke();

        if (!selectedColumnGeometry)
            return;

        let {x, width, heights} = selectedColumnGeometry;
        if (foregroundSectionName && heights[sectionName] === heights[foregroundSectionName])
            return;

        let height = heights[sectionName];
        context.beginPath();
        context.rect(x, canvasHeight - height, width, height);
        context.save();
        context.fillStyle = selectedFillStyle;
        context.globalAlpha = 0.5;
        context.fill();
        context.strokeStyle = selectedStrokeStyle;
        context.globalAlpha = 0.8;
        context.stroke();
        context.restore();
    }

    _graphPositionForMouseEvent(event)
    {
        let position = this.canvasPositionForEvent(event);
        let canvasHeight = this.canvasHeight;
        for (let columnGeometry of this._columnGeometries) {
            if (position.x >= columnGeometry.x && position.x <= columnGeometry.x + columnGeometry.width && position.y >= canvasHeight - columnGeometry.heights.otherThreads && position.y <= canvasHeight)
                return position.x;
        }

        return NaN;
    }

    _handleChartClick(event)
    {
        let position = this._graphPositionForMouseEvent(event);
        if (isNaN(position))
            return;

        let secondsPerPixel = this.timelineOverview.secondsPerPixel;
        let graphClickTime = position * secondsPerPixel;
        let graphStartTime = this.startTime;

        let clickTime = graphStartTime + graphClickTime;
        let record = this._cpuTimeline.closestRecordTo(clickTime);
        if (!record)
            return;

        // Ensure that the container "click" listener added by `WI.TimelineOverview` isn't called.
        event.__timelineRecordClickEventHandled = true;

        this.selectedRecord = record;
        this.needsLayout();
    }

    _cpuTimelineRecordAdded(event)
    {
        let cpuTimelineRecord = event.data.record;

        this._processRecord(cpuTimelineRecord);

        this.needsLayout();
    }

    _processRecord(cpuTimelineRecord)
    {
        this._maxUsage = Math.max(this._maxUsage, cpuTimelineRecord.usage);
    }
};

WI.CPUTimelineOverviewGraph._sectionStyles = {
    mainThread: {
        fill: "hsl(118, 43%, 55%)", // Keep this in sync with `--cpu-main-thread-fill-color`.
        stroke: "hsl(118, 33%, 42%)", // Keep this in sync with `--cpu-main-thread-stroke-color`.
    },
    workerThreads: {
        fill: "hsl(59, 79%, 62%)", // Keep this in sync with `--cpu-worker-thread-fill-color`.
        stroke: "hsl(59, 79%, 37%)", // Keep this in sync with `--cpu-worker-thread-stroke-color`.
    },
    otherThreads: {
        fill: "hsl(81, 80%, 50%)", // Keep this in sync with `--cpu-other-thread-fill-color`.
        stroke: "hsl(81, 80%, 30%)", // Keep this in sync with `--cpu-other-thread-stroke-color`.
    },
};
