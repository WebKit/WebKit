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

WI.ScriptTimelineOverviewGraph = class ScriptTimelineOverviewGraph extends WI.RecordBarTimelineOverviewGraph
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

        this._recordRowCount = 0;
        this._nextRecordRowIndex = 0;
        this._recordRowForTarget = new Map;
    }

    // Protected

    layout()
    {
        super.layout();

        if (this.hidden)
            return;

        let graphWidth = this.timelineOverview.scrollContainerWidth;
        if (isNaN(graphWidth)) {
            this.clearCanvas();
            return;
        }

        for (let target of this._recordsForTarget.keys()) {
            if (this._recordRowForTarget.has(target))
                continue;

            if (this._recordRowCount < 5)
                ++this._recordRowCount;
            this._recordRowForTarget.set(target, this._nextRecordRowIndex++ % this._recordRowCount);
        }

        this.beginRecordBarLayout();
        if (this._recordRowCount) {
            let {height, margin} = WI.ScriptTimelineOverviewGraph._dimensionsForRowCount[this._recordRowCount];
            for (let [target, [gcRecords, nonGCRecords]] of this._recordsForTarget) {
                let rowIndex = this._recordRowForTarget.get(target);
                let y = margin + rowIndex * (height + margin);
                this.addRecordBars(nonGCRecords, y, height, 2, true);
                this.addRecordBars(gcRecords, y, height, 2, true);
            }
        }
        this.updateCanvas();
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

WI.ScriptTimelineOverviewGraph._dimensionsForRowCount = {
    1: {height: 21, margin: 7},
    2: {height: 12, margin: 4},
    3: {height: 8, margin: 3},
    4: {height: 6.5, margin: 2},
    5: {height: 6, margin: 1},
};
