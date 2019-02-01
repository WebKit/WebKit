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

WI.CPUTimelineView = class CPUTimelineView extends WI.TimelineView
{
    constructor(timeline, extraArguments)
    {
        console.assert(timeline.type === WI.TimelineRecord.Type.CPU, timeline);

        super(timeline, extraArguments);

        this._recording = extraArguments.recording;
        this._maxUsage = -Infinity;

        this.element.classList.add("cpu");

        let contentElement = this.element.appendChild(document.createElement("div"));
        contentElement.classList.add("content");

        // FIXME: Overview with charts.

        let detailsContainerElement = this._detailsContainerElement = contentElement.appendChild(document.createElement("div"));
        detailsContainerElement.classList.add("details");

        this._timelineRuler = new WI.TimelineRuler;
        this.addSubview(this._timelineRuler);
        detailsContainerElement.appendChild(this._timelineRuler.element);

        let detailsSubtitleElement = detailsContainerElement.appendChild(document.createElement("div"));
        detailsSubtitleElement.classList.add("subtitle");
        detailsSubtitleElement.textContent = WI.UIString("CPU Usage");

        this._cpuUsageView = new WI.CPUUsageView;
        this.addSubview(this._cpuUsageView);
        this._detailsContainerElement.appendChild(this._cpuUsageView.element);

        timeline.addEventListener(WI.Timeline.Event.RecordAdded, this._cpuTimelineRecordAdded, this);
    }

    // Public

    shown()
    {
        super.shown();

        this._timelineRuler.updateLayout(WI.View.LayoutReason.Resize);
    }

    closed()
    {
        console.assert(this.representedObject instanceof WI.Timeline);
        this.representedObject.removeEventListener(null, null, this);
    }

    reset()
    {
        super.reset();

        this._maxUsage = -Infinity;

        this._cpuUsageView.clear();
    }

    get scrollableElements()
    {
        return [this.element];
    }

    // Protected

    get showsFilterBar() { return false; }

    layout()
    {
        if (this.layoutReason === WI.View.LayoutReason.Resize)
            return;

        // Always update timeline ruler.
        this._timelineRuler.zeroTime = this.zeroTime;
        this._timelineRuler.startTime = this.startTime;
        this._timelineRuler.endTime = this.endTime;

        const cpuUsageViewHeight = 75; // Keep this in sync with .cpu-usage-view

        let graphStartTime = this.startTime;
        let graphEndTime = this.endTime;
        let secondsPerPixel = this._timelineRuler.secondsPerPixel;
        let visibleEndTime = Math.min(this.endTime, this.currentTime);

        let discontinuities = this._recording.discontinuitiesInTimeRange(graphStartTime, visibleEndTime);

        // Don't include the record before the graph start if the graph start is within a gap.
        let includeRecordBeforeStart = !discontinuities.length || discontinuities[0].startTime > graphStartTime;
        let visibleRecords = this.representedObject.recordsInTimeRange(graphStartTime, visibleEndTime, includeRecordBeforeStart);
        if (!visibleRecords.length)
            return;

        // Update total usage chart with the last record's data.
        let lastRecord = visibleRecords.lastValue;

        // FIXME: Left chart.
        // FIXME: Right chart.

        let dataPoints = [];
        let max = -Infinity;
        let min = Infinity;
        let average = 0;

        for (let record of visibleRecords) {
            let time = record.startTime;
            let usage = record.usage;

            if (discontinuities.length && discontinuities[0].endTime < time) {
                let startDiscontinuity = discontinuities.shift();
                let endDiscontinuity = startDiscontinuity;
                while (discontinuities.length && discontinuities[0].endTime < time)
                    endDiscontinuity = discontinuities.shift();
                dataPoints.push({time: startDiscontinuity.startTime, size: 0});
                dataPoints.push({time: endDiscontinuity.endTime, size: 0});
                dataPoints.push({time: endDiscontinuity.endTime, size: usage});
            }

            dataPoints.push({time, size: usage});
            max = Math.max(max, usage);
            min = Math.min(min, usage);
            average += usage;
        }

        average /= visibleRecords.length;

        // If the graph end time is inside a gap, the last data point should
        // only be extended to the start of the discontinuity.
        if (discontinuities.length)
            visibleEndTime = discontinuities[0].startTime;

        function layoutView(view, {dataPoints, min, max, average}) {
            if (min === Infinity)
                min = 0;
            if (max === -Infinity)
                max = 0;

            // Zoom in to the top of each graph to accentuate small changes.
            let graphMin = min * 0.95;
            let graphMax = (max * 1.05) - graphMin;

            function xScale(time) {
                return (time - graphStartTime) / secondsPerPixel;
            }

            let size = new WI.Size(xScale(graphEndTime), cpuUsageViewHeight);

            function yScale(value) {
                return size.height - (((value - graphMin) / graphMax) * size.height);
            }

            view.updateChart(dataPoints, size, visibleEndTime, min, max, average, xScale, yScale);
        }

        layoutView(this._cpuUsageView, {dataPoints, min, max, average});
    }

    // Private

    _cpuTimelineRecordAdded(event)
    {
        let cpuTimelineRecord = event.data.record;
        console.assert(cpuTimelineRecord instanceof WI.CPUTimelineRecord);

        this._maxUsage = Math.max(this._maxUsage, cpuTimelineRecord.usage);

        if (cpuTimelineRecord.startTime >= this.startTime && cpuTimelineRecord.endTime <= this.endTime)
            this.needsLayout();
    }
};
