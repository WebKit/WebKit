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

WebInspector.LayoutTimelineOverviewGraph = function(recording)
{
    WebInspector.TimelineOverviewGraph.call(this, recording);

    this.element.classList.add(WebInspector.LayoutTimelineOverviewGraph.StyleClassName);

    this._layoutTimeline = recording.timelines.get(WebInspector.TimelineRecord.Type.Layout);
    this._layoutTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._layoutTimelineRecordAdded, this);

    this._timelineRecordBars = [];

    this.reset();
};

WebInspector.LayoutTimelineOverviewGraph.StyleClassName = "layout";

WebInspector.LayoutTimelineOverviewGraph.prototype = {
    constructor: WebInspector.LayoutTimelineOverviewGraph,
    __proto__: WebInspector.TimelineOverviewGraph.prototype,

    // Public

    reset: function()
    {
        WebInspector.TimelineOverviewGraph.prototype.reset.call(this);

        this._timelineRecordBarMap = new Map;

        this.element.removeChildren();
    },

    updateLayout: function()
    {
        WebInspector.TimelineOverviewGraph.prototype.updateLayout.call(this);

        var startTime = this.startTime;
        var currentTime = this.currentTime;
        var endTime = this.endTime;
        var duration = (endTime - startTime);

        var visibleWidth = this.element.offsetWidth;
        var secondsPerPixel = duration / visibleWidth;
        var recordBarIndex = 0;
        var barRecords = [];

        function createBar(barRecords)
        {
            var timelineRecordBar = this._timelineRecordBars[recordBarIndex];
            if (!timelineRecordBar)
                timelineRecordBar = this._timelineRecordBars[recordBarIndex] = new WebInspector.TimelineRecordBar;
            timelineRecordBar.records = barRecords;
            timelineRecordBar.refresh(this);
            if (!timelineRecordBar.element.parentNode)
                this.element.appendChild(timelineRecordBar.element);
            ++recordBarIndex;
        }

        for (var record of this._layoutTimeline.records) {
            // If this bar is completely before the bounds of the graph, skip this record.
            if (record.endTime < startTime)
                continue;

            // If this record is completely after the current time or end time, break out now.
            // Records are sorted, so all records after this will be beyond the current or end time too.
            if (record.startTime > currentTime || record.startTime > endTime)
                break;

            // Check if the previous record is a different type or far enough away to create the bar.
            if (barRecords.length && WebInspector.TimelineRecordBar.recordsCannotBeCombined(barRecords, record, secondsPerPixel)) {
                createBar.call(this, barRecords);
                barRecords = [];
            }

            barRecords.push(record);
        }

        // Create the bar for the last record if needed.
        if (barRecords.length)
            createBar.call(this, barRecords);

        // Remove the remaining unused TimelineRecordBars.
        for (; recordBarIndex < this._timelineRecordBars.length; ++recordBarIndex) {
            this._timelineRecordBars[recordBarIndex].records = null;
            this._timelineRecordBars[recordBarIndex].element.remove();
        }
    },

    // Private

    _layoutTimelineRecordAdded: function(event)
    {
        this.needsLayout();
    }
};
