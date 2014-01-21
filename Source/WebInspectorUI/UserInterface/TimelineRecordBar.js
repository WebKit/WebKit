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

WebInspector.TimelineRecordBar = function(record)
{
    WebInspector.Object.call(this);

    console.assert(record);

    this._record = record;

    this._element = document.createElement("div");
    this._element.classList.add(WebInspector.TimelineRecordBar.StyleClassName);
    this._element.classList.add(record.type);

    if (record.usesActiveStartTime) {
        this._inactiveBarElement = document.createElement("div");
        this._inactiveBarElement.classList.add(WebInspector.TimelineRecordBar.BarSegmentStyleClassName);
        this._inactiveBarElement.classList.add(WebInspector.TimelineRecordBar.InactiveStyleClassName);
        this._element.classList.add(WebInspector.TimelineRecordBar.HasInactiveSegmentStyleClassName);
    }

    this._activeBarElement = document.createElement("div");
    this._activeBarElement.classList.add(WebInspector.TimelineRecordBar.BarSegmentStyleClassName);

    if (this._inactiveBarElement)
        this._element.appendChild(this._inactiveBarElement);
    this._element.appendChild(this._activeBarElement);
};

WebInspector.Object.addConstructorFunctions(WebInspector.TimelineRecordBar);

WebInspector.TimelineRecordBar.StyleClassName = "timeline-record-bar";
WebInspector.TimelineRecordBar.BarSegmentStyleClassName = "segment";
WebInspector.TimelineRecordBar.InactiveStyleClassName = "inactive";
WebInspector.TimelineRecordBar.UnfinishedStyleClassName = "unfinished";
WebInspector.TimelineRecordBar.HasInactiveSegmentStyleClassName = "has-inactive-segment";

WebInspector.TimelineRecordBar.prototype = {
    constructor: WebInspector.TimelineRecordBar,
    __proto__: WebInspector.Object.prototype,

    // Public

    get element()
    {
        return this._element;
    },

    refresh: function(graphDataSource)
    {
        var barStartTime = this._record.startTime;

        // If this bar has no time info, return early.
        if (isNaN(barStartTime))
            return false;

        var graphStartTime = graphDataSource.startTime;
        var graphEndTime = graphDataSource.endTime;
        var graphCurrentTime = graphDataSource.currentTime;

        var barEndTime = this._record.endTime;

        // If this bar is completly after the current time, return early.
        if (barStartTime > graphCurrentTime)
            return false;

        // If this bar is completly before or after the bounds of the graph, return early.
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

        if (!this._record.usesActiveStartTime)
            return true;

        var barActiveStartTime = Math.max(barStartTime, Math.min(this._record.activeStartTime, barEndTime));
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
