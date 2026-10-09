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

WI.LayoutTimelineOverviewGraph = class LayoutTimelineOverviewGraph extends WI.RecordBarTimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        super(timelineOverview);

        this.element.classList.add("layout-overview");

        this._layoutTimeline = timeline;
        this._layoutTimeline.addEventListener(WI.Timeline.Event.RecordAdded, this._layoutTimelineRecordAdded, this);

        this.reset();

        for (let record of this._layoutTimeline.records)
            this._processRecord(record);
    }

    // Public

    reset()
    {
        super.reset();

        this._timelineLayoutRecords = [];
        this._timelinePaintRecords = [];
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

        this.beginRecordBarLayout();
        this.addRecordBars(this._timelineLayoutRecords, 4, 12, 2, true);
        this.addRecordBars(this._timelinePaintRecords, 20, 12, 2, true);
        this.updateCanvas();
    }

    // Private

    _layoutTimelineRecordAdded(event)
    {
        let layoutTimelineRecord = event.data.record;
        console.assert(layoutTimelineRecord instanceof WI.LayoutTimelineRecord);

        this._processRecord(layoutTimelineRecord);

        this.needsLayout();
    }

    _processRecord(layoutTimelineRecord)
    {
        if (layoutTimelineRecord.eventType === WI.LayoutTimelineRecord.EventType.Paint || layoutTimelineRecord.eventType === WI.LayoutTimelineRecord.EventType.Composite)
            this._timelinePaintRecords.push(layoutTimelineRecord);
        else
            this._timelineLayoutRecords.push(layoutTimelineRecord);
    }
};
