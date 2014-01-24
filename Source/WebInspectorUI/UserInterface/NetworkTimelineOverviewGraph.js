
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

WebInspector.NetworkTimelineOverviewGraph = function(recording)
{
    WebInspector.TimelineOverviewGraph.call(this, recording);

    this.element.classList.add(WebInspector.NetworkTimelineOverviewGraph.StyleClassName);

    var networkTimeline = recording.timelines.get(WebInspector.TimelineRecord.Type.Network);
    networkTimeline.addEventListener(WebInspector.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);
    networkTimeline.addEventListener(WebInspector.Timeline.Event.TimesUpdated, this.needsLayout, this);

    this.reset();
};

WebInspector.NetworkTimelineOverviewGraph.StyleClassName = "network";
WebInspector.NetworkTimelineOverviewGraph.GraphRowStyleClassName = "graph-row";
WebInspector.NetworkTimelineOverviewGraph.BarStyleClassName = "bar";
WebInspector.NetworkTimelineOverviewGraph.InactiveBarStyleClassName = "inactive";
WebInspector.NetworkTimelineOverviewGraph.UnfinishedStyleClassName = "unfinished";
WebInspector.NetworkTimelineOverviewGraph.MaximumRowCount = 6;

WebInspector.NetworkTimelineOverviewGraph.prototype = {
    constructor: WebInspector.NetworkTimelineOverviewGraph,
    __proto__: WebInspector.TimelineOverviewGraph.prototype,

    // Public

    reset: function()
    {
        WebInspector.TimelineOverviewGraph.prototype.reset.call(this);

        this._nextDumpRow = 0;
        this._timelineRecordGridRows = [];

        for (var i = 0; i < WebInspector.NetworkTimelineOverviewGraph.MaximumRowCount; ++i)
            this._timelineRecordGridRows.push([]);

        this.element.removeChildren();

        for (var rowRecords of this._timelineRecordGridRows) {
            rowRecords.__element = document.createElement("div");
            rowRecords.__element.className = WebInspector.NetworkTimelineOverviewGraph.GraphRowStyleClassName;
            this.element.appendChild(rowRecords.__element);

            rowRecords.__recordBars = [];
        }
    },

    updateLayout: function()
    {
        WebInspector.TimelineOverviewGraph.prototype.updateLayout.call(this);

        var visibleWidth = this.element.offsetWidth;
        var secondsPerPixel = (this.endTime - this.startTime) / visibleWidth;

        var recordBarIndex = 0;

        function createBar(rowElement, rowRecordBars, records, renderMode)
        {
            var timelineRecordBar = rowRecordBars[recordBarIndex];
            if (!timelineRecordBar)
                timelineRecordBar = rowRecordBars[recordBarIndex] = new WebInspector.TimelineRecordBar;
            timelineRecordBar.renderMode = renderMode;
            timelineRecordBar.records = records;
            timelineRecordBar.refresh(this);
            if (!timelineRecordBar.element.parentNode)
                rowElement.appendChild(timelineRecordBar.element);
            ++recordBarIndex;
        }

        for (var rowRecords of this._timelineRecordGridRows) {
            var rowElement = rowRecords.__element;
            var rowRecordBars = rowRecords.__recordBars;

            recordBarIndex = 0;

            WebInspector.TimelineRecordBar.createCombinedBars(rowRecords, secondsPerPixel, this, createBar.bind(this, rowElement, rowRecordBars));

            // Remove the remaining unused TimelineRecordBars.
            for (; recordBarIndex < rowRecordBars.length; ++recordBarIndex) {
                rowRecordBars[recordBarIndex].records = null;
                rowRecordBars[recordBarIndex].element.remove();
            }
        }
    },

    // Private

    _networkTimelineRecordAdded: function(event)
    {
        var resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WebInspector.ResourceTimelineRecord);

        function compareByStartTime(a, b)
        {
            return a.startTime - b.startTime;
        }

        // Try to find a row that has room and does not overlap a previous record.
        var foundRowForRecord = false;
        for (var i = 0; i < this._timelineRecordGridRows.length; ++i) {
            var rowRecords = this._timelineRecordGridRows[i];
            var lastRecord = rowRecords.lastValue;

            if (!lastRecord || lastRecord.endTime + WebInspector.NetworkTimelineOverviewGraph.MinimumBarPaddingTime <= resourceTimelineRecord.startTime) {
                insertObjectIntoSortedArray(resourceTimelineRecord, rowRecords, compareByStartTime);
                this._nextDumpRow = i + 1;
                foundRowForRecord = true;
                break;
            }
        }

        if (!foundRowForRecord) {
            // Try to find a row that does not overlap a previous record's active time, but it can overlap the inactive time.
            for (var i = 0; i < this._timelineRecordGridRows.length; ++i) {
                var rowRecords = this._timelineRecordGridRows[i];
                var lastRecord = rowRecords.lastValue;
                console.assert(lastRecord);

                if (lastRecord.activeStartTime + WebInspector.NetworkTimelineOverviewGraph.MinimumBarPaddingTime <= resourceTimelineRecord.startTime) {
                    insertObjectIntoSortedArray(resourceTimelineRecord, rowRecords, compareByStartTime);
                    this._nextDumpRow = i + 1;
                    foundRowForRecord = true;
                    break;
                }
            }
        }

        // We didn't find a empty spot, so dump into the designated dump row.
        if (!foundRowForRecord) {
            if (this._nextDumpRow >= WebInspector.NetworkTimelineOverviewGraph.MaximumRowCount)
                this._nextDumpRow = 0;
            insertObjectIntoSortedArray(resourceTimelineRecord, this._timelineRecordGridRows[this._nextDumpRow++], compareByStartTime);
        }

        this.needsLayout();
    }
};
