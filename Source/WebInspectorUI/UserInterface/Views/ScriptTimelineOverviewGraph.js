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

WI.ScriptTimelineOverviewGraph = class ScriptTimelineOverviewGraph extends WI.TimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        super(timelineOverview);

        this.element.classList.add("script");

        this._scriptTimeline = timeline;
        this._scriptTimeline.addEventListener(WI.Timeline.Event.RecordAdded, this._scriptTimelineRecordAdded, this);

        this.reset();
    }

    // Public

    reset()
    {
        super.reset();

        this._recordsForTarget = new Map;

        this._recordRows = [];
        this._nextRecordRowIndex = 0;
        this._recordRowForTarget = new Map;
        this._recordBarsForTarget = new Map;

        this.element.removeChildren();
    }

    // Protected

    layout()
    {
        super.layout();

        if (this.hidden)
            return;

        let secondsPerPixel = this.timelineOverview.secondsPerPixel;
        let recordBarIndex = 0;

        function createBar(rowElement, recordBars, records, renderMode)
        {
            let timelineRecordBar = recordBars[recordBarIndex];
            if (!timelineRecordBar)
                timelineRecordBar = recordBars[recordBarIndex] = new WI.TimelineRecordBar(this, records, renderMode);
            else {
                timelineRecordBar.renderMode = renderMode;
                timelineRecordBar.records = records;
            }
            timelineRecordBar.refresh(this);
            if (!timelineRecordBar.element.parentNode)
                rowElement.appendChild(timelineRecordBar.element);
            ++recordBarIndex;
        };

        for (let [target, [gcRecords, nonGCRecords]] of this._recordsForTarget) {
            let rowElement = this._recordRowForTarget.getOrInsertComputed(target, () => {
                if (this._recordRows.length < 5) {
                    let rowElement = this.element.appendChild(document.createElement("div"));
                    rowElement.classList.add("graph-row");
                    this._recordRows.push(rowElement);
                }
                return this._recordRows[this._nextRecordRowIndex++ % this._recordRows.length];
            });

            let recordBars = this._recordBarsForTarget.getOrInsert(target, []);
            recordBarIndex = 0;

            let boundCreateBar = createBar.bind(this, rowElement, recordBars);
            WI.TimelineRecordBar.createCombinedBars(nonGCRecords, secondsPerPixel, this, boundCreateBar);
            WI.TimelineRecordBar.createCombinedBars(gcRecords, secondsPerPixel, this, boundCreateBar);

            // Remove the remaining unused TimelineRecordBars.
            for (; recordBarIndex < recordBars.length; ++recordBarIndex) {
                recordBars[recordBarIndex].records = null;
                recordBars[recordBarIndex].element.remove();
            }
        }
    }

    updateSelectedRecord()
    {
        super.updateSelectedRecord();

        for (let recordBars of this._recordBarsForTarget.values()) {
            for (let recordBar of recordBars) {
                if (recordBar.records.includes(this.selectedRecord)) {
                    this.selectedRecordBar = recordBar;
                    return;
                }
            }
        }

        this.selectedRecordBar = null;
    }

    // Private

    _scriptTimelineRecordAdded(event)
    {
        let {record} = event.data;
        console.assert(record instanceof WI.ScriptTimelineRecord);

        let [gcRecords, nonGCRecords] = this._recordsForTarget.getOrInsert(record.target, [[], []]);
        let records = record.isGarbageCollection() ? gcRecords : nonGCRecords;
        records.push(record);

        this.needsLayout();
    }
};
