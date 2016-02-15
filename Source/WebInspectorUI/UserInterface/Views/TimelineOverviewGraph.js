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

WebInspector.TimelineOverviewGraph = class TimelineOverviewGraph extends WebInspector.View
{
    constructor(timelineOverview)
    {
        super();

        this.element.classList.add("timeline-overview-graph");

        this._zeroTime = 0;
        this._startTime = 0;
        this._endTime = 5;
        this._currentTime = 0;
        this._timelineOverview = timelineOverview;
        this._selectedRecord = null;
        this._selectedRecordChanged = false;
        this._scheduledSelectedRecordLayoutUpdateIdentifier = undefined;
        this._visible = true;
    }

    // Public

    static createForTimeline(timeline, timelineOverview)
    {
        console.assert(timeline instanceof WebInspector.Timeline, timeline);
        console.assert(timelineOverview instanceof WebInspector.TimelineOverview, timelineOverview);

        var timelineType = timeline.type;
        if (timelineType === WebInspector.TimelineRecord.Type.Network)
            return new WebInspector.NetworkTimelineOverviewGraph(timeline, timelineOverview);

        if (timelineType === WebInspector.TimelineRecord.Type.Layout)
            return new WebInspector.LayoutTimelineOverviewGraph(timeline, timelineOverview);

        if (timelineType === WebInspector.TimelineRecord.Type.Script)
            return new WebInspector.ScriptTimelineOverviewGraph(timeline, timelineOverview);

        if (timelineType === WebInspector.TimelineRecord.Type.RenderingFrame)
            return new WebInspector.RenderingFrameTimelineOverviewGraph(timeline, timelineOverview);

        if (timelineType === WebInspector.TimelineRecord.Type.Memory)
            return new WebInspector.MemoryTimelineOverviewGraph(timeline, timelineOverview);

        throw new Error("Can't make a graph for an unknown timeline.");
    }

    get zeroTime()
    {
        return this._zeroTime;
    }

    set zeroTime(x)
    {
        x = x || 0;

        if (this._zeroTime === x)
            return;

        this._zeroTime = x;

        this.needsLayout();
    }

    get startTime()
    {
        return this._startTime;
    }

    set startTime(x)
    {
        x = x || 0;

        if (this._startTime === x)
            return;

        this._startTime = x;

        this.needsLayout();
    }

    get endTime()
    {
        return this._endTime;
    }

    set endTime(x)
    {
        x = x || 0;

        if (this._endTime === x)
            return;

        this._endTime = x;

        this.needsLayout();
    }

    get currentTime()
    {
        return this._currentTime;
    }

    set currentTime(x)
    {
        x = x || 0;

        if (this._currentTime === x)
            return;

        let oldCurrentTime = this._currentTime;

        this._currentTime = x;

        if ((this._startTime <= oldCurrentTime && oldCurrentTime <= this._endTime) || (this._startTime <= this._currentTime && this._currentTime <= this._endTime))
            this.needsLayout();
    }

    get timelineOverview()
    {
        return this._timelineOverview;
    }

    get visible()
    {
        return this._visible;
    }

    get selectedRecord()
    {
        return this._selectedRecord;
    }

    set selectedRecord(x)
    {
        if (this._selectedRecord === x)
            return;

        this._selectedRecord = x;
        this._selectedRecordChanged = true;

        this._needsSelectedRecordLayout();
    }

    get height()
    {
        // Overridden by sub-classes if needed.
        return 36;
    }

    shown()
    {
        if (this._visible)
            return;

        this._visible = true;
        this.element.classList.toggle("hidden", !this._visible);
        this.updateLayout();
    }

    hidden()
    {
        if (!this._visible)
            return;

        this._visible = false;
        this.element.classList.toggle("hidden", !this._visible);
    }

    reset()
    {
        // Implemented by sub-classes if needed.
    }

    recordWasFiltered(record, filtered)
    {
        // Implemented by sub-classes if needed.
    }

    needsLayout()
    {
        // FIXME: needsLayout can be removed once <https://webkit.org/b/150741> is fixed.
        if (!this._visible)
            return;

        super.needsLayout();
    }

    // Protected

    dispatchSelectedRecordChangedEvent()
    {
        if (!this._selectedRecordChanged)
            return;

        this._selectedRecordChanged = false;

        this.dispatchEventToListeners(WebInspector.TimelineOverviewGraph.Event.RecordSelected, {record: this.selectedRecord});
    }

    updateSelectedRecord()
    {
        // Implemented by sub-classes if needed.
    }

    // Private

    _needsSelectedRecordLayout()
    {
        // If layout is scheduled, abort since the selected record will be updated when layout happens.
        if (this.layoutPending)
            return;

        if (this._scheduledSelectedRecordLayoutUpdateIdentifier)
            return;

        this._scheduledSelectedRecordLayoutUpdateIdentifier = requestAnimationFrame(function() {
            this._scheduledSelectedRecordLayoutUpdateIdentifier = undefined;
            this.updateSelectedRecord();
        }.bind(this));
    }
};

WebInspector.TimelineOverviewGraph.Event = {
    RecordSelected: "timeline-overview-graph-record-selected"
};
