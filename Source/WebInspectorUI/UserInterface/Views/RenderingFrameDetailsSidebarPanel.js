/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.RenderingFrameDetailsSidebarPanel = class RenderingFrameDetailsSidebarPanel extends WebInspector.DetailsSidebarPanel
{
    constructor()
    {
        super("rendering-frame-details", WebInspector.UIString("Frames"), WebInspector.UIString("Frames"), "Images/NavigationItemDoughnutChart.svg");

        this._frameRangeRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Frames"));
        this._timeRangeRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Time"));
        var rangeGroup = new WebInspector.DetailsSectionGroup([this._frameRangeRow, this._timeRangeRow]);

        function formatChartValue(value)
        {
            return Number.secondsToString(value);
        }

        this._chartRow = new WebInspector.ChartDetailsSectionRow(formatChartValue);
        var chartGroup = new WebInspector.DetailsSectionGroup([this._chartRow]);

        var rangeSection = new WebInspector.DetailsSection("rendering-frames-range", WebInspector.UIString("Selected Range"), [rangeGroup]);
        var chartSection = new WebInspector.DetailsSection("rendering-frames-chart", WebInspector.UIString("Chart"), [chartGroup]);
        var detailsSection = new WebInspector.DetailsSection("rendering-frames-details", WebInspector.UIString("Details"), [this._chartRow.legendGroup]);

        this.contentElement.appendChild(rangeSection.element);
        this.contentElement.appendChild(chartSection.element);
        this.contentElement.appendChild(detailsSection.element);

        this._renderingFrameTimeline = null;
        this._startTime = 0;
        this._endTime = 0;

        this._emptyValuePlaceholderString = "\u2014";

        this._chartColors = {
            layout: "rgb(212, 108, 108)",
            script: "rgb(153, 113, 185)",
            other: "rgb(221, 221, 221)",
            idle: "rgb(255, 255, 255)"
        };
    }

    // Public

    inspect(objects)
    {
        return !!this._renderingFrameTimeline;
    }

    updateRangeSelection(startTime, endTime)
    {
        if (this._startTime === startTime && this._endTime === endTime)
            return;

        this._startTime = startTime || 0;
        this._endTime = endTime || 0;

        this.needsRefresh();
    }

    get renderingFrameTimeline()
    {
        return this._renderingFrameTimeline;
    }

    set renderingFrameTimeline(renderingFrameTimeline)
    {
        if (renderingFrameTimeline === this._renderingFrameTimeline)
            return;

        if (this._renderingFrameTimeline) {
            this._renderingFrameTimeline.removeEventListener(WebInspector.Timeline.Event.RecordAdded, this._recordAdded, this);
            this._renderingFrameTimeline.removeEventListener(WebInspector.Timeline.Event.Reset, this._timelineReset, this);
        }

        this._renderingFrameTimeline = renderingFrameTimeline;

        if (this._renderingFrameTimeline) {
            this._renderingFrameTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._recordAdded, this);
            this._renderingFrameTimeline.addEventListener(WebInspector.Timeline.Event.Reset, this._timelineReset, this);
        }

        this.needsRefresh();
    }

    refresh()
    {
        var records = this._getSelectedRecords();
        if (!records.length) {
            this._resetAll();
            return;
        }

        var firstRecord = records[0];
        var lastRecord = records.lastValue;

        this._frameRangeRow.label = records.length > 1 ? WebInspector.UIString("Frames") : WebInspector.UIString("Frame");
        this._frameRangeRow.value = records.length > 1 ? WebInspector.UIString("%d – %d").format(firstRecord.frameNumber, lastRecord.frameNumber) : firstRecord.frameNumber;
        this._timeRangeRow.value = WebInspector.UIString("%s – %s").format(Number.secondsToString(firstRecord.startTime), Number.secondsToString(lastRecord.endTime));

        function durationForRecordType(type)
        {
            return records.reduce(function(previousValue, currentValue) {
                return previousValue + (type ? currentValue.durationForRecords(type) : currentValue.durationRemainder);
            }, 0);
        }

        var totalTime = lastRecord.endTime - firstRecord.startTime;
        var layoutTime = durationForRecordType(WebInspector.TimelineRecord.Type.Layout);
        var scriptTime = durationForRecordType(WebInspector.TimelineRecord.Type.Script);
        var otherTime = durationForRecordType();
        var idleTime = totalTime - layoutTime - scriptTime - otherTime;

        this._chartRow.clearChart();

        this._chartRow.addChartValue(WebInspector.UIString("Layout"), layoutTime, this._chartColors.layout);
        this._chartRow.addChartValue(WebInspector.UIString("Script"), scriptTime, this._chartColors.script);
        this._chartRow.addChartValue(WebInspector.UIString("Other"), otherTime, this._chartColors.other);
        this._chartRow.addChartValue(WebInspector.UIString("Idle"), idleTime, this._chartColors.idle);

        this._chartRow.innerLabel = Number.secondsToString(totalTime)
    }

    // Private

    _resetAll()
    {
        this._frameRangeRow.value = this._emptyValuePlaceholderString;
        this._timeRangeRow.value = this._emptyValuePlaceholderString;

        this._chartRow.clearChart();
    }

    _recordAdded(event)
    {
        this.needsRefresh();
    }

    _timelineReset(event)
    {
        this.needsRefresh();
    }

    _getSelectedRecords()
    {
        if (!this._renderingFrameTimeline)
            return [];

        var records = [];
        for (var record of this._renderingFrameTimeline.records) {
            console.assert(record instanceof WebInspector.RenderingFrameTimelineRecord);
            // If this frame is completely before the bounds of the graph, skip this record.
            if (record.endTime < this._startTime)
                continue;

            // If this record is completely after the end time, break out now.
            // Records are sorted, so all records after this will be beyond the end time too.
            if (record.startTime > this._endTime)
                break;

            records.push(record);
        }

        return records;
    }
};
