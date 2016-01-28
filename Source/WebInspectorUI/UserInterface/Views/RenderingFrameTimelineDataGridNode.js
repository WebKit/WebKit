/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.RenderingFrameTimelineDataGridNode = class RenderingFrameTimelineDataGridNode extends WebInspector.TimelineDataGridNode
{
    constructor(renderingFrameTimelineRecord, baseStartTime)
    {
        super(false, null);

        this._record = renderingFrameTimelineRecord;
        this._baseStartTime = baseStartTime || 0;
    }

    // Public

    get record()
    {
        return this._record;
    }

    get records()
    {
        return [this._record];
    }

    get data()
    {
        if (!this._cachedData) {
            var scriptTime = this._record.durationForTask(WebInspector.RenderingFrameTimelineRecord.TaskType.Script);
            var layoutTime = this._record.durationForTask(WebInspector.RenderingFrameTimelineRecord.TaskType.Layout);
            var paintTime = this._record.durationForTask(WebInspector.RenderingFrameTimelineRecord.TaskType.Paint);
            var otherTime = this._record.durationForTask(WebInspector.RenderingFrameTimelineRecord.TaskType.Other);
            this._cachedData = {
                startTime: this._record.startTime,
                totalTime: this._record.duration,
                scriptTime,
                layoutTime,
                paintTime,
                otherTime,
            };
        }

        return this._cachedData;
    }

    createCellContent(columnIdentifier, cell)
    {
        const emptyValuePlaceholderString = "\u2014";
        var value = this.data[columnIdentifier];

        switch (columnIdentifier) {
        case "startTime":
            return isNaN(value) ? emptyValuePlaceholderString : Number.secondsToString(value - this._baseStartTime, true);

        case "scriptTime":
        case "layoutTime":
        case "paintTime":
        case "otherTime":
        case "totalTime":
            return (isNaN(value) || value === 0) ? emptyValuePlaceholderString : Number.secondsToString(value, true);
        }

        return super.createCellContent(columnIdentifier, cell);
    }
};
