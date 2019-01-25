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

        let size = new WI.Size(0, this.height);
        this._chart = new WI.ColumnChart(size);
        this.element.appendChild(this._chart.element);

        this._legendElement = this.element.appendChild(document.createElement("div"));
        this._legendElement.classList.add("legend");

        this.reset();
    }

    // Protected

    get height()
    {
        return 72;
    }

    reset()
    {
        super.reset();

        this._maxUsage = 0;
        this._cachedMaxUsage = undefined;

        this._updateLegend();
        this._chart.clear();
        this._chart.needsLayout();
    }

    layout()
    {
        if (!this.visible)
            return;

        this._updateLegend();
        this._chart.clear();

        let graphWidth = this.timelineOverview.scrollContainerWidth;
        if (isNaN(graphWidth))
            return;

        if (this._chart.size.width !== graphWidth || this._chart.size.height !== this.height)
            this._chart.size = new WI.Size(graphWidth, this.height);

        let graphStartTime = this.startTime;
        let visibleEndTime = Math.min(this.endTime, this.currentTime);
        let secondsPerPixel = this.timelineOverview.secondsPerPixel;
        let maxCapacity = Math.max(20, this._maxUsage * 1.05); // Add 5% for padding.

        // 500ms. This matches the ResourceUsageThread sampling frequency in the backend.
        const samplingRatePerSecond = 0.5;

        function xScale(time) {
            return (time - graphStartTime) / secondsPerPixel;
        }

        let height = this.height;
        function yScale(size) {
            return (size / maxCapacity) * height;
        }

        const includeRecordBeforeStart = true;
        let visibleRecords = this._cpuTimeline.recordsInTimeRange(graphStartTime, visibleEndTime + (samplingRatePerSecond / 2), includeRecordBeforeStart);
        if (!visibleRecords.length)
            return;

        function yScaleForRecord(record) {
            return yScale(record.usage);
        }

        let intervalWidth = (samplingRatePerSecond / secondsPerPixel);
        const minimumDisplayHeight = 2;

        // Bars for each record.
        for (let record of visibleRecords) {
            let w = intervalWidth;
            let h = Math.max(minimumDisplayHeight, yScale(record.usage));
            let x = xScale(record.startTime - (samplingRatePerSecond / 2));
            let y = height - h;
            this._chart.addBar(x, y, w, h);
        }

        this._chart.updateLayout();
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

    _cpuTimelineRecordAdded(event)
    {
        let cpuTimelineRecord = event.data.record;

        this._maxUsage = Math.max(this._maxUsage, cpuTimelineRecord.usage);

        this.needsLayout();
    }
};
