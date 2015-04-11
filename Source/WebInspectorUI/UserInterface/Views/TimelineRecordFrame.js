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

WebInspector.TimelineRecordFrame = function(graphDataSource, records)
{
    // FIXME: Convert this to a WebInspector.Object subclass, and call super().
    // WebInspector.Object.call(this);

    this._element = document.createElement("div");
    this._element.classList.add(WebInspector.TimelineRecordFrame.StyleClassName);

    this._graphDataSource = graphDataSource;
    this.records = records || [];
};

// FIXME: Move to a WebInspector.Object subclass and we can remove this.
WebInspector.Object.deprecatedAddConstructorFunctions(WebInspector.TimelineRecordFrame);

WebInspector.TimelineRecordFrame.StyleClassName = "timeline-record-frame";
WebInspector.TimelineRecordFrame.SixtyFpsFrameBudget = 0.0166;
WebInspector.TimelineRecordFrame.MaximumCombinedWidthPixels = 12;
WebInspector.TimelineRecordFrame.MinimumWidthPixels = 4;
WebInspector.TimelineRecordFrame.MinimumMarginPixels = 1;

WebInspector.TimelineRecordFrame.createCombinedFrames = function(records, secondsPerPixel, graphDataSource, createFrameCallback)
{
    if (!records.length)
        return;

    var startTime = graphDataSource.startTime;
    var currentTime = graphDataSource.currentTime;
    var endTime = graphDataSource.endTime;

    var visibleRecords = [];

    for (var record of records) {
        if (isNaN(record.startTime))
            continue;

        // If this frame is completely before the bounds of the graph, skip this record.
        if (record.endTime < startTime)
            continue;

        // If this record is completely after the current time or end time, break out now.
        // Records are sorted, so all records after this will be beyond the current or end time too.
        if (record.startTime > currentTime || record.startTime > endTime)
            break;

        visibleRecords.push(record);
    }

    if (!visibleRecords.length)
        return;

    var maximumCombinedFrameDuration = secondsPerPixel * WebInspector.TimelineRecordFrame.MaximumCombinedWidthPixels;
    var minimumFrameDuration = secondsPerPixel * WebInspector.TimelineRecordFrame.MinimumWidthPixels;
    var minimumMargin = secondsPerPixel * WebInspector.TimelineRecordFrame.MinimumMarginPixels;

    var activeStartTime = NaN;
    var activeEndTime = NaN;
    var activeRecords = [];

    function createFrameFromActiveRecords()
    {
        createFrameCallback(activeRecords);
        activeRecords = [];
        activeStartTime = NaN;
        activeEndTime = NaN;
    }

    for (var record of visibleRecords) {
        // Check if the previous record is far enough away to create the frame.
        if (!isNaN(activeStartTime) && (activeStartTime + Math.max(activeEndTime - activeStartTime, minimumFrameDuration) + minimumMargin <= record.startTime))
            createFrameFromActiveRecords();

        // Check if active records exceeds the maximum combined frame width.
        if (!isNaN(activeStartTime) && (activeEndTime - activeStartTime) > maximumCombinedFrameDuration)
            createFrameFromActiveRecords();

        // If this is a new frame, peg the start time.
        if (isNaN(activeStartTime))
            activeStartTime = record.startTime;

        // Update the end time to be the maximum we encounter. endTime might be NaN, so "|| 0" to prevent Math.max from returning NaN.
        if (!isNaN(record.endTime))
            activeEndTime = Math.max(activeEndTime || 0, record.endTime);

        activeRecords.push(record);
    }

    // Create the active frame for the last record if needed.
    if (!isNaN(activeStartTime))
        createFrameCallback(activeRecords);
};

WebInspector.TimelineRecordFrame.prototype = {
    constructor: WebInspector.TimelineRecordFrame,
    __proto__: WebInspector.Object.prototype,

    // Public

    get element()
    {
        return this._element;
    },

    get duration()
    {
        if (this.records.length === 0)
            return 0;

        return this.records.lastValue.endTime - this.records[0].startTime;
    },

    get records()
    {
        return this._records;
    },

    set records(records)
    {
        records = records || [];

        if (!(records instanceof Array))
            records = [records];

        this._records = records;
    },

    refresh(graphDataSource)
    {
        if (!this.records || !this.records.length)
            return false;

        var firstRecord = this.records[0];
        var frameStartTime = firstRecord.startTime;

        // If this frame has no time info, return early.
        if (isNaN(frameStartTime))
            return false;

        var graphStartTime = graphDataSource.startTime;
        var graphEndTime = graphDataSource.endTime;
        var graphCurrentTime = graphDataSource.currentTime;

        // If this frame is completely after the current time, return early.
        if (frameStartTime > graphCurrentTime)
            return false;

        var frameEndTime = this.records.lastValue.endTime;

        // If this frame is completely before or after the bounds of the graph, return early.
        if (frameEndTime < graphStartTime || frameStartTime > graphEndTime)
            return false;

        var graphDuration = graphEndTime - graphStartTime;
        var recordLeftPosition = (frameStartTime - graphStartTime) / graphDuration;
        this._updateElementPosition(this._element, recordLeftPosition, "left");

        var recordWidth = (frameEndTime - frameStartTime) / graphDuration;
        this._updateElementPosition(this._element, recordWidth, "width");

        this._updateChildElements(graphDataSource);

        return true;
    },

    // Private

    _updateChildElements(graphDataSource)
    {
        this._element.removeChildren();

        console.assert(this.records.length);
        if (!this.records.length)
            return;

        if (graphDataSource.graphHeightSeconds === 0)
            return;

        // When combining multiple records into a frame, display the record with the longest duration rather than averaging.
        var displayRecord = this.records.reduce(function(previousValue, currentValue) {
            return currentValue.duration >= previousValue.duration ? currentValue : previousValue;
        });

        var frameElement = document.createElement("div");
        frameElement.classList.add("frame");
        this._element.appendChild(frameElement);

        var frameHeight = displayRecord.duration / graphDataSource.graphHeightSeconds;
        this._updateElementPosition(frameElement, frameHeight, "height");

        function createDurationElement(duration, recordType)
        {
            var element = document.createElement("div");
            this._updateElementPosition(element, duration / displayRecord.duration, "height");
            element.classList.add("duration");
            if (recordType)
                element.classList.add(recordType);
            return element;
        }

        if (displayRecord.durationRemainder > 0)
            frameElement.appendChild(createDurationElement.call(this, displayRecord.durationRemainder));

        for (var type in WebInspector.TimelineRecord.Type) {
            var recordType = WebInspector.TimelineRecord.Type[type];
            var duration = displayRecord.durationForRecords(recordType);
            if (duration === 0)
                continue;
            frameElement.appendChild(createDurationElement.call(this, duration, recordType));
        }

        // Add "includes-dropped" style class if multiple records are being combined in the frame,
        // and one of those records exceeds the 60 fps frame budget.
        if (this.records.length > 1 && displayRecord.duration > WebInspector.TimelineRecordFrame.SixtyFpsFrameBudget)
            frameElement.classList.add("includes-dropped");

        // If the display record is the last combined record and also exceeds the 60 fps budget,
        // add a "dropped" element to the right of the frame.
        var frameWidth = 1;
        var secondsPerPixel = this._graphDataSource.timelineOverview.secondsPerPixel;
        var minimumRecordDuration = secondsPerPixel * WebInspector.TimelineRecordFrame.MinimumWidthPixels * 2;    // Combine minimum widths of the frame element and dropped element.

        if (displayRecord === this.records.lastValue && displayRecord.duration > WebInspector.TimelineRecordFrame.SixtyFpsFrameBudget && displayRecord.duration >= minimumRecordDuration) {
            var overflowDuration = displayRecord.duration - WebInspector.TimelineRecordFrame.SixtyFpsFrameBudget;
            var droppedElementWidth = overflowDuration / displayRecord.duration;
            frameWidth -= droppedElementWidth;

            var droppedElement = document.createElement("div");
            droppedElement.className = "dropped";

            this._element.appendChild(droppedElement);

            this._updateElementPosition(droppedElement, frameHeight, "height");
            this._updateElementPosition(droppedElement, droppedElementWidth, "width");
        }

        this._updateElementPosition(frameElement, frameWidth, "width");
    },

    _updateElementPosition(element, newPosition, property)
    {
        newPosition *= 100;
        newPosition = newPosition.toFixed(2);

        var currentPosition = parseFloat(element.style[property]).toFixed(2);
        if (currentPosition !== newPosition)
            element.style[property] = newPosition + "%";
    }
};
