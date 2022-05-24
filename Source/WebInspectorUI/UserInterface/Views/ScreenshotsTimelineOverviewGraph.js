/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

WI.ScreenshotsTimelineOverviewGraph = class ScreenshotsTimelineOverviewGraph extends WI.TimelineOverviewGraph
{
    constructor(timeline, timelineOverview)
    {
        console.assert(timeline instanceof WI.Timeline);
        console.assert(timeline.type === WI.TimelineRecord.Type.Screenshots, timeline);

        super(timelineOverview);

        this.element.classList.add("screenshots");

        this._screenshotsTimeline = timeline;

        this._lastSelectedRecordInLayout = null;
    }

    // Protected

    get height()
    {
        return 60;
    }

    layout()
    {
        super.layout();

        if (this.hidden)
            return;

        this.element.removeChildren();

        let secondsPerPixel = this.timelineOverview.secondsPerPixel;

        this._lastSelectedRecordInLayout = this.selectedRecord;

        for (let record of this._visibleRecords()) {
            let imageElement = this.element.appendChild(document.createElement("img"));
            imageElement.src = record.imageData;
            if (record === this.selectedRecord)
                imageElement.classList.add("selected");

            imageElement.height = this.height;
            let widthScale = this.height / record.height;
            let pixelWidth = record.width * widthScale;
            imageElement.width = pixelWidth;

            imageElement.style.left = (record.startTime - this.startTime) / secondsPerPixel + "px";
            imageElement.addEventListener("click", (event) => {
                // Ensure that the container "click" listener added by `WI.TimelineOverview` isn't called.
                event.__timelineRecordClickEventHandled = true;

                this.selectedRecord = record;
            });
        }
    }

    updateSelectedRecord()
    {
        super.updateSelectedRecord();

        if (this._lastSelectedRecordInLayout !== this.selectedRecord) {
            // Since we don't have the exact element to re-style with a selected appearance
            // we trigger another layout to re-layout the graph and provide additional
            // styles for the column for the selected record.
            this.needsLayout();
        }
    }

    // Private

    _visibleRecords()
    {
        let visibleEndTime = Math.min(this.endTime, this.currentTime);

        let records = this._screenshotsTimeline.records;
        let visibleRecords = [];

        for (let i = 0; i < records.length; ++i) {
            let record = records[i];
            if (!(record.startTime >= this.startTime || record.startTime <= visibleEndTime))
                continue;
                
            if (!visibleRecords.length && i)
                visibleRecords.push(records[i - 1]);
                
            visibleRecords.push(record);
        }

        return visibleRecords;
    }
};
