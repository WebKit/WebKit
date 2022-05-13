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

WI.ScreenshotsTimelineRecord = class ScreenshotsTimelineRecord extends WI.TimelineRecord
{
    constructor(timestamp, imageData, width, height)
    {
        console.assert(timestamp);
        console.assert(imageData && typeof imageData === "string", imageData);
        console.assert(width);
        console.assert(height);

        // Pass the startTime as the endTime since this record type has no duration.
        super(WI.TimelineRecord.Type.Screenshots, timestamp, timestamp);

        this._imageData = imageData;
        this._width = width;
        this._height = height;
    }

    // Import / Export

    static async fromJSON(json)
    {
        return new WI.ScreenshotsTimelineRecord(json.timestamp, json.imageData, json.width, json.height);
    }

    toJSON()
    {
        return {
            type: this.type,
            timestamp: this.startTime,
            imageData: this._imageData,
            width: this._width,
            height: this._height,
        };
    }

    // Public

    get imageData() { return this._imageData; }
    get width() { return this._width; }
    get height() { return this._height; }
};
