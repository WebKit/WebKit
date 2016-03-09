/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.MemoryTimelineOverviewGraph = class MemoryTimelineOverviewGraph extends WebInspector.TimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        super(timelineOverview);

        this.element.classList.add("memory");

        console.assert(timeline instanceof WebInspector.MemoryTimeline);

        this._memoryTimeline = timeline;
        this._memoryTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._memoryTimelineRecordAdded, this);
        this._memoryTimeline.addEventListener(WebInspector.MemoryTimeline.Event.MemoryPressureEventAdded, this._memoryTimelineMemoryPressureEventAdded, this)

        this._didInitializeCategories = false;

        let size = new WebInspector.Size(0, this.height);
        this._chart = new WebInspector.StackedLineChart(size);
        this.element.appendChild(this._chart.element);

        this._legendElement = this.element.appendChild(document.createElement("div"));
        this._legendElement.classList.add("legend");

        this._memoryPressureMarkersContainerElement = this.element.appendChild(document.createElement("div"));
        this._memoryPressureMarkersContainerElement.classList.add("memory-pressure-markers-container");
        this._memoryPressureMarkerElements = [];

        this.reset();
    }

    // Protected

    get height()
    {
        return 108;
    }

    reset()
    {
        super.reset();

        this._maxSize = 0;

        this._updateLegend();
        this._chart.clear();
        this._chart.needsLayout();

        this._memoryPressureMarkersContainerElement.removeChildren();
        this._memoryPressureMarkerElements = [];
    }

    layout()
    {
        this._updateLegend();
        this._chart.clear();

        if (!this._didInitializeCategories)
            return;

        let graphWidth = this.timelineOverview.scrollContainerWidth;
        if (isNaN(graphWidth))
            return;

        if (this._chart.size.width !== graphWidth || this._chart.size.height !== this.height)
            this._chart.size = new WebInspector.Size(graphWidth, this.height);

        let graphStartTime = this.startTime;
        let graphEndTime = this.endTime;
        let graphCurrentTime = this.currentTime;
        let visibleEndTime = Math.min(this.endTime, this.currentTime);

        let secondsPerPixel = this.timelineOverview.secondsPerPixel;
        let maxCapacity = this._maxSize * 1.05; // Add 5% for padding.

        function xScale(time) {
            return (time - graphStartTime) / secondsPerPixel;
        }

        let height = this.height;
        function yScale(size) {
            return height - ((size / maxCapacity) * height);
        }

        let visibleMemoryPressureEventMarkers = this._visibleMemoryPressureEvents(graphStartTime, visibleEndTime);

        // Reuse existing marker elements.
        for (let i = 0; i < visibleMemoryPressureEventMarkers.length; ++i) {
            let markerElement = this._memoryPressureMarkerElements[i];
            if (!markerElement) {
                markerElement = this._memoryPressureMarkersContainerElement.appendChild(document.createElement("div"));
                markerElement.classList.add("memory-pressure-event");
                this._memoryPressureMarkerElements[i] = markerElement;
            }

            let memoryPressureEvent = visibleMemoryPressureEventMarkers[i];
            markerElement.style.left = xScale(memoryPressureEvent.timestamp) + "px";
        }

        // Remove excess marker elements.
        let excess = this._memoryPressureMarkerElements.length - visibleMemoryPressureEventMarkers.length;
        if (excess) {
            let elementsToRemove = this._memoryPressureMarkerElements.splice(visibleMemoryPressureEventMarkers.length);
            for (let element of elementsToRemove)
                element.remove();
        }

        let visibleRecords = this._visibleRecords(graphStartTime, visibleEndTime);
        if (!visibleRecords.length)
            return;

        function pointSetForRecord(record) {
            let size = 0;
            let ys = [];
            for (let i = 0; i < record.categories.length; ++i) {
                size += record.categories[i].size;
                ys[i] = yScale(size);
            }
            return ys;
        }

        // Extend the first record to the start so it doesn't look like we originate at zero size.
        if (visibleRecords[0] === this._memoryTimeline.records[0])
            this._chart.addPointSet(0, pointSetForRecord(visibleRecords[0]));

        // Points for visible records.
        for (let record of visibleRecords) {
            let x = xScale(record.startTime);
            this._chart.addPointSet(x, pointSetForRecord(record));
        }

        // Extend the last value to current / end time.
        let lastRecord = visibleRecords.lastValue;
        let x = Math.floor(xScale(visibleEndTime));
        this._chart.addPointSet(x, pointSetForRecord(lastRecord));

        this._chart.updateLayout();
    }

    // Private

    _updateLegend()
    {
        if (!this._maxSize) {
            this._legendElement.hidden = true;
            this._legendElement.textContent = "";
        } else {
            this._legendElement.hidden = false;
            this._legendElement.textContent = WebInspector.UIString("Maximum Size: %s").format(Number.bytesToString(this._maxSize));
        }
    }

    _visibleMemoryPressureEvents(startTime, endTime)
    {
        let events = this._memoryTimeline.memoryPressureEvents;
        if (!events.length)
            return [];

        let lowerIndex = events.lowerBound(startTime, (time, event) => time - event.timestamp);
        let upperIndex = events.upperBound(endTime, (time, event) => time - event.timestamp);
        return events.slice(lowerIndex, upperIndex);
    }

    _visibleRecords(startTime, endTime)
    {
        let records = this._memoryTimeline.records;
        let lowerIndex = records.lowerBound(startTime, (time, record) => time - record.timestamp);
        let upperIndex = records.upperBound(endTime, (time, record) => time - record.timestamp);

        // Include the record right before the start time in case it extends into this range.
        if (lowerIndex > 0)
            lowerIndex--;
        // FIXME: <https://webkit.org/b/153759> Web Inspector: Memory Timelines should better extend to future data
        // if (upperIndex !== records.length)
        //     upperIndex++;

        return records.slice(lowerIndex, upperIndex);
    }

    _memoryTimelineRecordAdded(event)
    {
        let memoryTimelineRecord = event.data.record;

        this._maxSize = Math.max(this._maxSize, memoryTimelineRecord.totalSize);

        if (!this._didInitializeCategories) {
            this._didInitializeCategories = true;
            let types = [];
            for (let category of memoryTimelineRecord.categories)
                types.push(category.type);
            this._chart.initializeSections(types);
        }

        this.needsLayout();
    }

    _memoryTimelineMemoryPressureEventAdded(event)
    {
        this.needsLayout();
    }
};
