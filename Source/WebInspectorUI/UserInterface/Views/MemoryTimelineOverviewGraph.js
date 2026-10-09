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

WI.MemoryTimelineOverviewGraph = class MemoryTimelineOverviewGraph extends WI.TimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        console.assert(timeline instanceof WI.MemoryTimeline);

        super(timelineOverview);

        this.element.classList.add("memory");

        this._memoryTimeline = timeline;
        this._memoryTimeline.addEventListener(WI.Timeline.Event.RecordAdded, this._memoryTimelineRecordAdded, this);
        this._memoryTimeline.addEventListener(WI.MemoryTimeline.Event.MemoryPressureEventAdded, this._memoryTimelineMemoryPressureEventAdded, this);

        this._didInitializeCategories = false;
        this._categoryTypes = [];

        this._legendElement = this.element.appendChild(document.createElement("div"));
        this._legendElement.classList.add("legend");

        this.reset();

        for (let record of this._memoryTimeline.records)
            this._processRecord(record);
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
        this._cachedMaxSize = undefined;

        this._updateLegend();
    }

    layout()
    {
        super.layout();

        if (this.hidden)
            return;

        this._updateLegend();

        let graphWidth = this.timelineOverview.scrollContainerWidth;
        if (isNaN(graphWidth)) {
            this.clearCanvas();
            return;
        }

        this.updateCanvas();
    }

    drawCanvas(context)
    {
        if (!this._didInitializeCategories)
            return;

        let graphStartTime = this.startTime;
        let visibleEndTime = Math.min(this.endTime, this.currentTime);

        let secondsPerPixel = this.timelineOverview.secondsPerPixel;
        let maxCapacity = this._maxSize * 1.05; // Add 5% for padding.

        function xScale(time) {
            return (time - graphStartTime) / secondsPerPixel;
        }

        let height = this.canvasHeight;
        function yScale(size) {
            return height - ((size / maxCapacity) * height);
        }

        let visibleMemoryPressureEvents = this._visibleMemoryPressureEvents(graphStartTime, visibleEndTime);

        let discontinuities = this.timelineOverview.discontinuitiesInTimeRange(graphStartTime, visibleEndTime);

        let visibleRecords = this._memoryTimeline.recordsInTimeRange(graphStartTime, visibleEndTime, {
            includeRecordBeforeStart: !discontinuities.length || discontinuities[0].startTime > graphStartTime,
            includeRecordAfterEnd: true,
        });
        if (!visibleRecords.length) {
            this._drawMemoryPressureEvents(context, visibleMemoryPressureEvents, xScale);
            return;
        }

        function categoryYValuesForRecord(record) {
            let size = 0;
            let yForCategoryType = {};
            for (let category of record.categories) {
                size += category.size;
                yForCategoryType[category.type] = yScale(size);
            }
            return yForCategoryType;
        }

        let points = [];

        // Extend the first record to the start so it doesn't look like we originate at zero size.
        if (visibleRecords[0] === this._memoryTimeline.records[0] && (!discontinuities.length || discontinuities[0].startTime > visibleRecords[0].startTime))
            points.push({x: 0, yForCategoryType: categoryYValuesForRecord(visibleRecords[0])});

        function insertDiscontinuity(previousRecord, startDiscontinuity, endDiscontinuity, nextRecord)
        {
            console.assert(previousRecord || nextRecord, previousRecord, nextRecord);

            let xStart = xScale(previousRecord ? previousRecord.endTime : startDiscontinuity.startTime);
            let xEnd = xScale(endDiscontinuity.endTime);

            // Extend the previous record to the start of the discontinuity.
            if (previousRecord)
                points.push({x: xStart, yForCategoryType: categoryYValuesForRecord(previousRecord)});

            let zeroYForCategoryType = {};
            for (let category of (previousRecord || nextRecord).categories)
                zeroYForCategoryType[category.type] = yScale(0);
            points.push({x: xStart, yForCategoryType: zeroYForCategoryType});

            if (nextRecord) {
                points.push({x: xEnd, yForCategoryType: zeroYForCategoryType});
                points.push({x: xEnd, yForCategoryType: categoryYValuesForRecord(nextRecord)});
            } else {
                // Extend the discontinuity to the visible end time to prevent
                // drawing artifacts when the next record arrives.
                points.push({x: xScale(visibleEndTime), yForCategoryType: zeroYForCategoryType});
            }
        }

        // Points for visible records.
        let previousRecord = null;
        for (let record of visibleRecords) {
            if (discontinuities.length && discontinuities[0].endTime <= record.startTime) {
                let startDiscontinuity = discontinuities.shift();
                let endDiscontinuity = startDiscontinuity;
                while (discontinuities.length && discontinuities[0].endTime <= record.startTime)
                    endDiscontinuity = discontinuities.shift();
                insertDiscontinuity(previousRecord, startDiscontinuity, endDiscontinuity, record);
            }

            let x = xScale(record.startTime);
            points.push({x, yForCategoryType: categoryYValuesForRecord(record)});

            previousRecord = record;
        }

        if (discontinuities.length)
            insertDiscontinuity(previousRecord, discontinuities[0], discontinuities[0], null);
        else {
            // Extend the last value to current / end time.
            let lastRecord = visibleRecords.lastValue;
            if (lastRecord.startTime <= visibleEndTime) {
                let x = Math.floor(xScale(lastRecord.endTime));
                points.push({x, yForCategoryType: categoryYValuesForRecord(lastRecord)});
            }
        }

        this._drawAreas(context, points);
        this._drawMemoryPressureEvents(context, visibleMemoryPressureEvents, xScale);
    }

    // Private

    _drawAreas(context, points)
    {
        let canvasHeight = this.canvasHeight;
        let lastX = points.length ? points.lastValue.x : 0;

        for (let categoryType of this._categoryTypes) {
            context.beginPath();
            context.moveTo(0, canvasHeight);
            for (let point of points)
                context.lineTo(point.x, point.yForCategoryType[categoryType]);
            context.lineTo(lastX, canvasHeight);
            context.closePath();

            let {fill, stroke} = WI.MemoryTimelineOverviewGraph._categoryStyles[categoryType];
            context.fillStyle = fill;
            context.fill();
            context.strokeStyle = stroke;
            context.stroke();
        }
    }

    _drawMemoryPressureEvents(context, events, xScale)
    {
        let canvasHeight = this.canvasHeight;
        context.fillStyle = "black";
        for (let event of events)
            context.fillRect(xScale(event.timestamp), 0, 1, canvasHeight);
    }

    _updateLegend()
    {
        if (this._cachedMaxSize === this._maxSize)
            return;

        this._cachedMaxSize = this._maxSize;

        if (!this._maxSize) {
            this._legendElement.hidden = true;
            this._legendElement.textContent = "";
        } else {
            this._legendElement.hidden = false;
            this._legendElement.textContent = WI.UIString("Maximum Size: %s").format(Number.bytesToString(this._maxSize));
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

    _memoryTimelineRecordAdded(event)
    {
        let memoryTimelineRecord = event.data.record;
        console.assert(memoryTimelineRecord instanceof WI.MemoryTimelineRecord);

        this._processRecord(memoryTimelineRecord);

        this.needsLayout();
    }

    _processRecord(memoryTimelineRecord)
    {
        this._maxSize = Math.max(this._maxSize, memoryTimelineRecord.totalSize);

        if (!this._didInitializeCategories) {
            this._didInitializeCategories = true;
            this._categoryTypes = memoryTimelineRecord.categories.map((category) => category.type).reverse();
        }
    }

    _memoryTimelineMemoryPressureEventAdded(event)
    {
        this.needsLayout();
    }
};

WI.MemoryTimelineOverviewGraph._categoryStyles = {
    [WI.MemoryCategory.Type.JavaScript]: {
        fill: "hsl(269, 65%, 75%)", // Keep this in sync with `--memory-javascript-fill-color`.
        stroke: "hsl(269, 33%, 50%)", // Keep this in sync with `--memory-javascript-stroke-color`.
    },
    [WI.MemoryCategory.Type.Images]: {
        fill: "hsl(0, 65%, 75%)", // Keep this in sync with `--memory-images-fill-color`.
        stroke: "hsl(0, 54%, 50%)", // Keep this in sync with `--memory-images-stroke-color`.
    },
    [WI.MemoryCategory.Type.Layers]: {
        fill: "hsl(76, 49%, 75%)", // Keep this in sync with `--memory-layers-fill-color`.
        stroke: "hsl(79, 45%, 50%)", // Keep this in sync with `--memory-layers-stroke-color`.
    },
    [WI.MemoryCategory.Type.Page]: {
        fill: "hsl(22, 60%, 70%)", // Keep this in sync with `--memory-page-fill-color`.
        stroke: "hsl(22, 40%, 50%)", // Keep this in sync with `--memory-page-stroke-color`.
    },
};
