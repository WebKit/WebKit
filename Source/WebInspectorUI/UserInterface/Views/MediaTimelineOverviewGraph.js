
/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.MediaTimelineOverviewGraph = class MediaTimelineOverviewGraph extends WI.TimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        console.assert(timeline instanceof WI.Timeline);
        console.assert(timeline.type === WI.TimelineRecord.Type.Media);

        super(timelineOverview);

        this._timeline = timeline;
        this._recordBars = [];

        this.element.classList.add("media");

        this.reset();
    }

    // Public

    reset()
    {
        this.element.removeChildren();

        super.reset();
    }

    shown()
    {
        super.shown();

        this._timeline.addEventListener(WI.Timeline.Event.RecordAdded, this._handleRecordAdded, this);
    }

    hidden()
    {
        this._timeline.removeEventListener(null, null, this);

        super.hidden();
    }

    // Protected

    layout()
    {
        if (!this.visible)
            return;

        let recordBarIndex = 0;

        let createBar = (records, renderMode) => {
            let timelineRecordBar = this._recordBars[recordBarIndex];
            if (timelineRecordBar) {
                timelineRecordBar.renderMode = renderMode;
                timelineRecordBar.records = records;
            } else
                timelineRecordBar = this._recordBars[recordBarIndex] = new WI.TimelineRecordBar(this, records, renderMode);
            timelineRecordBar.refresh(this);
            if (!timelineRecordBar.element.parentNode)
                this.element.appendChild(timelineRecordBar.element);
            ++recordBarIndex;
        };

        WI.TimelineRecordBar.createCombinedBars(this._timeline.records, this.timelineOverview.secondsPerPixel, this, createBar);

        // Remove the remaining unused TimelineRecordBars.
        for (let i = recordBarIndex; i < this._recordBars.length; ++i) {
            this._recordBars[i].records = null;
            this._recordBars[i].element.remove();
        }
    }

    updateSelectedRecord()
    {
        super.updateSelectedRecord();

        for (let recordBar of this._recordBars) {
            if (recordBar.records.includes(this.selectedRecord)) {
                this.selectedRecordBar = recordBar;
                return;
            }
        }

        this.selectedRecordBar = null;
    }

    // Private

    _handleRecordAdded(event)
    {
        this.needsLayout();
    }
};

WI.MediaTimelineOverviewGraph.MaximumRowCount = 6;
