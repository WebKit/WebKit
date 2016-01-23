/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

WebInspector.TimelineRecordBar = class TimelineRecordBar extends WebInspector.Object
{
    constructor(records, renderMode)
    {
        super();

        this._element = document.createElement("div");
        this._element.classList.add("timeline-record-bar");

        this.renderMode = renderMode;
        this.records = records;
    }

    static createCombinedBars(records, secondsPerPixel, graphDataSource, createBarCallback)
    {
        if (!records.length)
            return;

        var startTime = graphDataSource.startTime;
        var currentTime = graphDataSource.currentTime;
        var endTime = graphDataSource.endTime;

        var visibleRecords = [];
        var usesActiveStartTime = false;
        var lastRecordType = null;

        // FIXME: Do a binary search for records that fall inside start and current time.

        for (var record of records) {
            if (isNaN(record.startTime))
                continue;

            // If this bar is completely before the bounds of the graph, skip this record.
            if (record.endTime < startTime)
                continue;

            // If this record is completely after the current time or end time, break out now.
            // Records are sorted, so all records after this will be beyond the current or end time too.
            if (record.startTime > currentTime || record.startTime > endTime)
                break;

            if (record.usesActiveStartTime)
                usesActiveStartTime = true;

            // If one record uses active time the rest are assumed to use it.
            console.assert(record.usesActiveStartTime === usesActiveStartTime);

            // Only a single record type is supported right now.
            console.assert(!lastRecordType || record.type === lastRecordType);

            visibleRecords.push(record);

            lastRecordType = record.type;
        }

        if (!visibleRecords.length)
            return;

        if (visibleRecords.length === 1) {
            createBarCallback(visibleRecords, WebInspector.TimelineRecordBar.RenderMode.Normal);
            return;
        }

        function compareByActiveStartTime(a, b)
        {
            return a.activeStartTime - b.activeStartTime;
        }

        var minimumDuration = secondsPerPixel * WebInspector.TimelineRecordBar.MinimumWidthPixels;
        var minimumMargin = secondsPerPixel * WebInspector.TimelineRecordBar.MinimumMarginPixels;

        if (usesActiveStartTime) {
            var inactiveStartTime = NaN;
            var inactiveEndTime = NaN;
            var inactiveRecords = [];

            for (var record of visibleRecords) {
                // Check if the previous record is far enough away to create the inactive bar.
                if (!isNaN(inactiveStartTime) && inactiveStartTime + Math.max(inactiveEndTime - inactiveStartTime, minimumDuration) + minimumMargin <= record.startTime) {
                    createBarCallback(inactiveRecords, WebInspector.TimelineRecordBar.RenderMode.InactiveOnly);
                    inactiveRecords = [];
                    inactiveStartTime = NaN;
                    inactiveEndTime = NaN;
                }

                // If this is a new bar, peg the start time.
                if (isNaN(inactiveStartTime))
                    inactiveStartTime = record.startTime;

                // Update the end time to be the maximum we encounter. inactiveEndTime might be NaN, so "|| 0" to prevent Math.max from returning NaN.
                inactiveEndTime = Math.max(inactiveEndTime || 0, record.activeStartTime);

                inactiveRecords.push(record);
            }

            // Create the inactive bar for the last record if needed.
            if (!isNaN(inactiveStartTime))
                createBarCallback(inactiveRecords, WebInspector.TimelineRecordBar.RenderMode.InactiveOnly);

            visibleRecords.sort(compareByActiveStartTime);
        }

        var activeStartTime = NaN;
        var activeEndTime = NaN;
        var activeRecords = [];

        var startTimeProperty = usesActiveStartTime ? "activeStartTime" : "startTime";

        for (var record of visibleRecords) {
            var startTime = record[startTimeProperty];

            // Check if the previous record is far enough away to create the active bar. We also create it now if the current record has no active state time.
            if (!isNaN(activeStartTime) && (activeStartTime + Math.max(activeEndTime - activeStartTime, minimumDuration) + minimumMargin <= startTime
                || (isNaN(startTime) && !isNaN(activeEndTime)))) {
                createBarCallback(activeRecords, WebInspector.TimelineRecordBar.RenderMode.ActiveOnly);
                activeRecords = [];
                activeStartTime = NaN;
                activeEndTime = NaN;
            }

            if (isNaN(startTime))
                continue;

            // If this is a new bar, peg the start time.
            if (isNaN(activeStartTime))
                activeStartTime = startTime;

            // Update the end time to be the maximum we encounter. activeEndTime might be NaN, so "|| 0" to prevent Math.max from returning NaN.
            if (!isNaN(record.endTime))
                activeEndTime = Math.max(activeEndTime || 0, record.endTime);

            activeRecords.push(record);
        }

        // Create the active bar for the last record if needed.
        if (!isNaN(activeStartTime))
            createBarCallback(activeRecords, WebInspector.TimelineRecordBar.RenderMode.ActiveOnly);
    }

    // Public

    get element()
    {
        return this._element;
    }

    get renderMode()
    {
        return this._renderMode;
    }

    set renderMode(renderMode)
    {
        this._renderMode = renderMode || WebInspector.TimelineRecordBar.RenderMode.Normal;
    }

    get records()
    {
        return this._records;
    }

    set records(records)
    {
        if (this._records && this._records.length) {
            this._element.classList.remove(this._records[0].type);
            if (this._records[0].eventType)
                this._element.classList.remove(this._records[0].eventType);
        }

        records = records || [];

        console.assert(records instanceof Array, "records should be an array");

        this._records = records;

        // Assume all records are the same type.
        if (this._records.length) {
            this._element.classList.add(this._records[0].type);
            // Although all records may not have the same event type, the first record is
            // sufficient to determine the correct style for the record bar.
            if (this._records[0].eventType)
                this._element.classList.add(this._records[0].eventType);
        }
    }

    refresh(graphDataSource)
    {
        if (!this._records || !this._records.length)
            return false;

        var firstRecord = this._records[0];
        var barStartTime = firstRecord.startTime;

        // If this bar has no time info, return early.
        if (isNaN(barStartTime))
            return false;

        var graphStartTime = graphDataSource.startTime;
        var graphEndTime = graphDataSource.endTime;
        var graphCurrentTime = graphDataSource.currentTime;

        var barEndTime = this._records.reduce(function(previousValue, currentValue) { return Math.max(previousValue, currentValue.endTime); }, 0);

        // If this bar is completely after the current time, return early.
        if (barStartTime > graphCurrentTime)
            return false;

        // If this bar is completely before or after the bounds of the graph, return early.
        if (barEndTime < graphStartTime || barStartTime > graphEndTime)
            return false;

        var barUnfinished = isNaN(barEndTime) || barEndTime >= graphCurrentTime;
        if (barUnfinished)
            barEndTime = graphCurrentTime;

        var graphDuration = graphEndTime - graphStartTime;

        this._element.classList.toggle("unfinished", barUnfinished);

        var newBarLeftPosition = (barStartTime - graphStartTime) / graphDuration;
        this._updateElementPosition(this._element, newBarLeftPosition, "left");

        var newBarWidth = ((barEndTime - graphStartTime) / graphDuration) - newBarLeftPosition;
        this._updateElementPosition(this._element, newBarWidth, "width");

        if (!this._activeBarElement && this._renderMode !== WebInspector.TimelineRecordBar.RenderMode.InactiveOnly) {
            this._activeBarElement = document.createElement("div");
            this._activeBarElement.classList.add("segment");
        }

        if (!firstRecord.usesActiveStartTime) {
            this._element.classList.remove("has-inactive-segment");

            if (this._inactiveBarElement)
                this._inactiveBarElement.remove();

            if (this._renderMode === WebInspector.TimelineRecordBar.RenderMode.InactiveOnly) {
                if (this._activeBarElement)
                    this._activeBarElement.remove();

                return false;
            }

            // If this TimelineRecordBar is reused and had an inactive bar previously, clean it up.
            this._activeBarElement.style.removeProperty("left");
            this._activeBarElement.style.removeProperty("width");

            if (!this._activeBarElement.parentNode)
                this._element.appendChild(this._activeBarElement);

            return true;
        }

        this._element.classList.add("has-inactive-segment");

        // Find the earliest active start time for active only rendering, and the latest for the other modes.
        // This matches the values that TimelineRecordBar.createCombinedBars uses when combining.
        if (this._renderMode === WebInspector.TimelineRecordBar.RenderMode.ActiveOnly)
            var barActiveStartTime = this._records.reduce(function(previousValue, currentValue) { return Math.min(previousValue, currentValue.activeStartTime); }, Infinity);
        else
            var barActiveStartTime = this._records.reduce(function(previousValue, currentValue) { return Math.max(previousValue, currentValue.activeStartTime); }, 0);

        var barDuration = barEndTime - barStartTime;

        var inactiveUnfinished = isNaN(barActiveStartTime) || barActiveStartTime >= graphCurrentTime;
        this._element.classList.toggle("unfinished", inactiveUnfinished);

        if (inactiveUnfinished)
            barActiveStartTime = graphCurrentTime;

        var middlePercentage = (barActiveStartTime - barStartTime) / barDuration;

        if (this._renderMode !== WebInspector.TimelineRecordBar.RenderMode.ActiveOnly) {
            if (!this._inactiveBarElement) {
                this._inactiveBarElement = document.createElement("div");
                this._inactiveBarElement.classList.add("segment");
                this._inactiveBarElement.classList.add("inactive");
            }

            this._updateElementPosition(this._inactiveBarElement, 1 - middlePercentage, "right");
            this._updateElementPosition(this._inactiveBarElement, middlePercentage, "width");

            if (!this._inactiveBarElement.parentNode)
                this._element.insertBefore(this._inactiveBarElement, this._element.firstChild);
        }

        if (!inactiveUnfinished && this._renderMode !== WebInspector.TimelineRecordBar.RenderMode.InactiveOnly) {
            this._updateElementPosition(this._activeBarElement, middlePercentage, "left");
            this._updateElementPosition(this._activeBarElement, 1 - middlePercentage, "width");

            if (!this._activeBarElement.parentNode)
                this._element.appendChild(this._activeBarElement);
        } else if (this._activeBarElement)
            this._activeBarElement.remove();

        return true;
    }

    // Private

    _updateElementPosition(element, newPosition, property)
    {
        newPosition *= 100;
        newPosition = newPosition.toFixed(2);

        var currentPosition = parseFloat(element.style[property]).toFixed(2);
        if (currentPosition !== newPosition)
            element.style[property] = newPosition + "%";
    }
};

WebInspector.TimelineRecordBar.MinimumWidthPixels = 4;
WebInspector.TimelineRecordBar.MinimumMarginPixels = 1;

WebInspector.TimelineRecordBar.RenderMode = {
    Normal: "timeline-record-bar-normal-render-mode",
    InactiveOnly: "timeline-record-bar-inactive-only-render-mode",
    ActiveOnly: "timeline-record-bar-active-only-render-mode"
};
