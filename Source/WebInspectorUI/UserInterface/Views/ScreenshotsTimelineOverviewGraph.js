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
        this.element.addEventListener("click", this._handleClick.bind(this));

        this._screenshotsTimeline = timeline;

        this._imageGeometries = [];
        this._lastSelectedRecordInLayout = null;

        this.reset();
    }

    // Protected

    get height()
    {
        return 60;
    }

    reset()
    {
        super.reset();

        this._imageForRecord = new WeakMap;
        this._imageGeometries = [];
        this._lastSelectedRecordInLayout = null;
    }

    layout()
    {
        super.layout();

        if (this.hidden)
            return;

        let secondsPerPixel = this.timelineOverview.secondsPerPixel;
        let imageItems = [];
        let selectedImageItem = null;

        for (let record of this._visibleRecords()) {
            let image = this._imageForRecord.getOrInsertComputed(record, () => {
                let image = new Image;
                image.addEventListener("load", () => {
                    this.needsLayout();
                }, {once: true});
                image.src = record.imageData;
                return image;
            });

            if (!image.complete || !image.naturalWidth || !image.naturalHeight)
                continue;

            let height = this.canvasHeight;
            let imageItem = {
                record,
                image,
                x: (record.startTime - this.startTime) / secondsPerPixel,
                width: image.naturalWidth / image.naturalHeight * height,
                height,
            };

            if (record === this.selectedRecord)
                selectedImageItem = imageItem;
            else
                imageItems.push(imageItem);
        }

        if (selectedImageItem)
            imageItems.push(selectedImageItem);

        this._imageGeometries = imageItems;
        this._lastSelectedRecordInLayout = this.selectedRecord;

        this.updateCanvas();
    }

    updateSelectedRecord()
    {
        super.updateSelectedRecord();

        if (this._lastSelectedRecordInLayout !== this.selectedRecord) {
            // Since we don't have the exact element to re-style with a selected appearance
            // we trigger another layout to re-layout the graph and provide additional
            // styles for the image for the selected record.
            this.needsLayout();
        }
    }

    drawCanvas(context)
    {
        let borderColor = this.cssVariableValue("--border-color");
        let selectedBorderColor = this.cssVariableValue("--glyph-color-active");

        for (let item of this._imageGeometries) {
            this.drawCanvasImage(context, item.image, item.x, 0, item.width, item.height);

            context.strokeStyle = item.record === this.selectedRecord ? selectedBorderColor : borderColor;
            context.lineWidth = 1;
            context.strokeRect(item.x + 0.5, 0.5, item.width - 1, item.height - 1);
        }
    }

    // Private

    _handleClick(event)
    {
        let {x, y} = this.canvasPositionForEvent(event);
        let item = null;
        for (let i = this._imageGeometries.length - 1; i >= 0; --i) {
            let candidate = this._imageGeometries[i];
            if (x >= candidate.x && x <= candidate.x + candidate.width && y >= 0 && y <= candidate.height) {
                item = candidate;
                break;
            }
        }

        if (!item)
            return;

        // Ensure that the container "click" listener added by `WI.TimelineOverview` isn't called.
        event.__timelineRecordClickEventHandled = true;

        this.selectedRecord = item.record;
    }

    _visibleRecords()
    {
        let visibleEndTime = Math.min(this.endTime, this.currentTime);
        return this._screenshotsTimeline.recordsInTimeRange(this.startTime, visibleEndTime, {includeRecordBeforeStart: true});
    }
};
