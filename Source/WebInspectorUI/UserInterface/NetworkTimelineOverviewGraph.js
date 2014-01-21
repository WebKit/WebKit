
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
WebInspector.NetworkTimelineOverviewGraph.MinimumBarPaddingPixels = 5;

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

        function updateElementPosition(element, newPosition, property)
        {
            newPosition *= 100;
            newPosition = newPosition.toFixed(2);

            var currentPosition = parseFloat(element.style[property]).toFixed(2);
            if (currentPosition !== newPosition)
                element.style[property] = newPosition + "%";
        }

        function createBar(barElementCache, rowElement, barStartTime, barEndTime, inactive)
        {
            if (barStartTime > currentTime)
                return;

            var barElement = barElementCache.shift();
            if (!barElement) {
                barElement = document.createElement("div");
                barElement.classList.add(WebInspector.NetworkTimelineOverviewGraph.BarStyleClassName);
            }

            barElement.classList.toggle(WebInspector.NetworkTimelineOverviewGraph.InactiveBarStyleClassName, inactive);

            if (barEndTime >= currentTime) {
                barEndTime = currentTime;
                barElement.classList.add(WebInspector.NetworkTimelineOverviewGraph.UnfinishedStyleClassName);
            } else
                barElement.classList.remove(WebInspector.NetworkTimelineOverviewGraph.UnfinishedStyleClassName);

            if (inactive) {
                var newBarRightPosition = 1 - ((barEndTime - startTime) / duration);
                updateElementPosition(barElement, newBarRightPosition, "right");
                barElement.style.removeProperty("left");
            } else {
                var newBarLeftPosition = (barStartTime - startTime) / duration;
                updateElementPosition(barElement, newBarLeftPosition, "left");
                barElement.style.removeProperty("right");
            }

            var newBarWidth = ((barEndTime - barStartTime) / duration);
            updateElementPosition(barElement, newBarWidth, "width");

            if (!barElement.parendNode)
                rowElement.appendChild(barElement);
        }

        for (var rowRecords of this._timelineRecordGridRows) {
            var rowElement = rowRecords.__rowElement;
            if (!rowElement) {
                rowElement = rowRecords.__rowElement = document.createElement("div");
                rowElement.className = WebInspector.NetworkTimelineOverviewGraph.GraphRowStyleClassName;
                this.element.appendChild(rowElement);
            }

            if (!rowRecords.length)
                continue;

            // Save the current bar elements to reuse.
            var barElementCache = Array.prototype.slice.call(rowElement.childNodes);

            var inactiveStartTime = NaN;
            var inactiveEndTime = NaN;
            var activeStartTime = NaN;
            var activeEndTime = NaN;

            for (var record of rowRecords) {
                if (isNaN(record.startTime))
                    continue;

                // If this bar is completely before the bounds of the graph, skip this record.
                if (record.endTime < startTime)
                    continue;

                // If this record is completely after the current time or end time, break out now.
                // Records are sorted, so all records after this will be beyond the current or end time too.
                if (record.startTime > currentTime || record.startTime > endTime)
                    break;

                // Check if the previous record is far enough away to create the inactive bar.
                if (!isNaN(inactiveStartTime) && inactiveEndTime + (secondsPerPixel * WebInspector.NetworkTimelineOverviewGraph.MinimumBarPaddingPixels) <= record.startTime) {
                    createBar.call(this, barElementCache, rowElement, inactiveStartTime, inactiveEndTime, true);
                    inactiveStartTime = NaN;
                    inactiveEndTime = NaN;
                }

                // If this is a new bar, peg the start time.
                if (isNaN(inactiveStartTime))
                    inactiveStartTime = record.startTime;

                // Update the end time to be the maximum we encounter. inactiveEndTime might be NaN, so "|| 0" to prevent Math.max from returning NaN.
                inactiveEndTime = Math.max(inactiveEndTime || 0, record.activeStartTime);

                // Check if the previous record is far enough away to create the active bar. We also create it now if the current record has no active state time.
                if (!isNaN(activeStartTime) && (activeEndTime + (secondsPerPixel * WebInspector.NetworkTimelineOverviewGraph.MinimumBarPaddingPixels) <= record.activeStartTime || isNaN(record.activeStartTime))) {
                    if (!isNaN(activeEndTime)) {
                        createBar.call(this, barElementCache, rowElement, activeStartTime, activeEndTime);
                        activeStartTime = NaN;
                        activeEndTime = NaN;
                    }
                }

                if (isNaN(record.activeStartTime))
                    continue;

                // If this is a new bar, peg the start time.
                if (isNaN(activeStartTime))
                    activeStartTime = record.activeStartTime;

                // Update the end time to be the maximum we encounter. activeEndTime might be NaN, so "|| 0" to prevent Math.max from returning NaN.
                if (!isNaN(record.endTime))
                    activeEndTime = Math.max(activeEndTime || 0, record.endTime);
            }

            // Create the inactive bar for the last record if needed.
            if (!isNaN(inactiveStartTime))
                createBar.call(this, barElementCache, rowElement, inactiveStartTime, inactiveEndTime || currentTime, true);

            // Create the active bar for the last record if needed.
            if (!isNaN(activeStartTime))
                createBar.call(this, barElementCache, rowElement, activeStartTime, activeEndTime || currentTime);

            // Remove any unused bar elements.
            for (var barElement of barElementCache)
                barElement.remove();
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
