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

WebInspector.TimelineRecordBar = function(records)
{
    WebInspector.Object.call(this);

    this._element = document.createElement("div");
    this._element.classList.add(WebInspector.TimelineRecordBar.StyleClassName);

    this._activeBarElement = document.createElement("div");
    this._activeBarElement.classList.add(WebInspector.TimelineRecordBar.BarSegmentStyleClassName);
    this._element.appendChild(this._activeBarElement);

    this.records = records;
};

WebInspector.Object.addConstructorFunctions(WebInspector.TimelineRecordBar);

WebInspector.TimelineRecordBar.StyleClassName = "timeline-record-bar";
WebInspector.TimelineRecordBar.BarSegmentStyleClassName = "segment";
WebInspector.TimelineRecordBar.InactiveStyleClassName = "inactive";
WebInspector.TimelineRecordBar.UnfinishedStyleClassName = "unfinished";
WebInspector.TimelineRecordBar.HasInactiveSegmentStyleClassName = "has-inactive-segment";
WebInspector.TimelineRecordBar.MinimumWidthPixels = 4;
WebInspector.TimelineRecordBar.MinimumMarginPixels = 1;

WebInspector.TimelineRecordBar.recordsCannotBeCombined = function(records, candidateRecord, secondsPerPixel)
{
    console.assert(records instanceof Array || records instanceof WebInspector.TimelineRecord);
    console.assert(candidateRecord instanceof WebInspector.TimelineRecord);

    if (records instanceof WebInspector.TimelineRecord)
        records = [records];

    if (!records.length)
        return true;

    if (candidateRecord.usesActiveStartTime)
        return true;

    var lastRecord = records.lastValue;
    if (lastRecord.usesActiveStartTime)
        return true;

    if (lastRecord.type !== candidateRecord.type)
        return true;

    if (isNaN(candidateRecord.startTime))
        return true;

    var firstRecord = records[0];
    var totalDuration = lastRecord.endTime - firstRecord.startTime;

    var minimumMargin = WebInspector.TimelineRecordBar.MinimumMarginPixels * secondsPerPixel;
    var minimumDuration = WebInspector.TimelineRecordBar.MinimumWidthPixels * secondsPerPixel;

    return firstRecord.startTime + Math.max(minimumDuration, totalDuration) + minimumMargin <= candidateRecord.startTime;
};

WebInspector.TimelineRecordBar.prototype = {
    constructor: WebInspector.TimelineRecordBar,
    __proto__: WebInspector.Object.prototype,

    // Public

    get element()
    {
        return this._element;
    },

    get records()
    {
        return this._records;
    },

    set records(records)
    {
        if (this._records && this._records.length)
            this._element.classList.remove(this._records[0].type);

        records = records || [];

        if (!(records instanceof Array))
            records = [records];

        this._records = records;

        // Combining multiple record bars is not supported with records that have inactive time.
        console.assert(this._records.length <= 1 || !this._records[0].usesActiveStartTime);

        // Inactive time is only supported with one record.
        if (this._records.length === 1 && this._records[0].usesActiveStartTime) {
            if (!this._inactiveBarElement) {
                this._inactiveBarElement = document.createElement("div");
                this._inactiveBarElement.classList.add(WebInspector.TimelineRecordBar.BarSegmentStyleClassName);
                this._inactiveBarElement.classList.add(WebInspector.TimelineRecordBar.InactiveStyleClassName);
                this._element.classList.add(WebInspector.TimelineRecordBar.HasInactiveSegmentStyleClassName);
                this._element.insertBefore(this._inactiveBarElement, this._element.firstChild);
            }
        } else if (this._inactiveBarElement) {
            this._element.classList.remove(WebInspector.TimelineRecordBar.HasInactiveSegmentStyleClassName);
            this._inactiveBarElement.remove();
            delete this._inactiveBarElement;
        }

        // Assume all records are the same type.
        if (this._records.length)
            this._element.classList.add(this._records[0].type);
    },

    refresh: function(graphDataSource)
    {
        if (!this._records || !this._records.length)
            return;

        var firstRecord = this._records[0];
        var barStartTime = firstRecord.startTime;

        // If this bar has no time info, return early.
        if (isNaN(barStartTime))
            return false;

        var graphStartTime = graphDataSource.startTime;
        var graphEndTime = graphDataSource.endTime;
        var graphCurrentTime = graphDataSource.currentTime;

        var lastRecord = this._records.lastValue;
        var barEndTime = lastRecord.endTime;

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

        this._element.classList.toggle(WebInspector.TimelineRecordBar.UnfinishedStyleClassName, barUnfinished);

        var newBarLeftPosition = (barStartTime - graphStartTime) / graphDuration;
        this._updateElementPosition(this._element, newBarLeftPosition, "left");

        var newBarWidth = ((barEndTime - graphStartTime) / graphDuration) - newBarLeftPosition;
        this._updateElementPosition(this._element, newBarWidth, "width");

        if (!this._inactiveBarElement) {
            // If this TimelineRecordBar is reused and had an inactive bar previously,
            // we might need to remove some styles and add the active element back.
            this._activeBarElement.style.removeProperty("left");
            this._activeBarElement.style.removeProperty("width");
            if (!this._activeBarElement.parentNode)
                this._element.appendChild(this._activeBarElement);
            return true;
        }

        console.assert(firstRecord === lastRecord);

        var barActiveStartTime = Math.max(barStartTime, Math.min(firstRecord.activeStartTime, barEndTime));
        var barDuration = barEndTime - barStartTime;

        var inactiveUnfinished = isNaN(barActiveStartTime) || barActiveStartTime >= graphCurrentTime;
        this._element.classList.toggle(WebInspector.TimelineRecordBar.UnfinishedStyleClassName, inactiveUnfinished);

        if (inactiveUnfinished)
            barActiveStartTime = graphCurrentTime;

        var middlePercentage = (barActiveStartTime - barStartTime) / barDuration;

        this._updateElementPosition(this._inactiveBarElement, 1 - middlePercentage, "right");
        this._updateElementPosition(this._inactiveBarElement, middlePercentage, "width");

        if (!inactiveUnfinished) {
            if (!this._activeBarElement.parentNode)
                this._element.appendChild(this._activeBarElement);

            this._updateElementPosition(this._activeBarElement, middlePercentage, "left");
            this._updateElementPosition(this._activeBarElement, 1 - middlePercentage, "width");
        } else
            this._activeBarElement.remove();

        return true;
    },

    // Private

    _updateElementPosition: function(element, newPosition, property)
    {
        newPosition *= 100;
        newPosition = newPosition.toFixed(2);

        var currentPosition = parseFloat(element.style[property]).toFixed(2);
        if (currentPosition !== newPosition)
            element.style[property] = newPosition + "%";
    }
};
