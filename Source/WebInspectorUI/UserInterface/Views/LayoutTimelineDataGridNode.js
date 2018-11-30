/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.LayoutTimelineDataGridNode = class LayoutTimelineDataGridNode extends WI.TimelineDataGridNode
{
    constructor(layoutTimelineRecord, baseStartTime)
    {
        super(false, null);

        this._record = layoutTimelineRecord;
        this._baseStartTime = baseStartTime || 0;
    }

    // Public

    get records()
    {
        return [this._record];
    }

    get data()
    {
        if (!this._cachedData) {
            this._cachedData = {
                type: this._record.eventType,
                name: this.displayName(),
                width: this._record.width,
                height: this._record.height,
                area: this._record.width * this._record.height,
                startTime: this._record.startTime,
                totalTime: this._record.duration,
                location: this._record.initiatorCallFrame,
            };
        }

        return this._cachedData;
    }

    createCellContent(columnIdentifier, cell)
    {
        var value = this.data[columnIdentifier];

        switch (columnIdentifier) {
        case "name":
            cell.classList.add(...this.iconClassNames());
            return value;

        case "width":
        case "height":
            return isNaN(value) ? emDash : WI.UIString("%dpx").format(value);

        case "area":
            return isNaN(value) ? emDash : WI.UIString("%dpx\u00B2").format(value);

        case "startTime":
            return isNaN(value) ? emDash : Number.secondsToString(value - this._baseStartTime, true);

        case "totalTime":
            return isNaN(value) ? emDash : Number.secondsToString(value, true);
        }

        return super.createCellContent(columnIdentifier, cell);
    }
};
