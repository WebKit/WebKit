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

WebInspector.RunLoopTimelineRecord = class RunLoopTimelineRecord extends WebInspector.TimelineRecord
{
    constructor(startTime, endTime, children)
    {
        super(WebInspector.TimelineRecord.Type.RunLoop, startTime, endTime);

        this._children = children || [];
        this._durationByRecordType = new Map;
        this._durationRemainder = NaN;
    }

    // Public

    get children()
    {
        return this._children.slice();
    }

    get durationRemainder()
    {
        if (!isNaN(this._durationRemainder))
            return this._durationRemainder;

        this._durationRemainder = this.duration;
        for (var recordType in WebInspector.TimelineRecord.Type)
            this._durationRemainder -= this.durationForRecords(WebInspector.TimelineRecord.Type[recordType]);

        return this._durationRemainder;
    }

    durationForRecords(recordType)
    {
        if (this._durationByRecordType.has(recordType))
            return this._durationByRecordType.get(recordType);

        var duration = this._children.reduce(function(previousValue, currentValue) {
            if (currentValue.type === recordType) {
                var currentDuration = currentValue.duration;
                if (currentValue.usesActiveStartTime)
                    currentDuration -= currentValue.inactiveDuration;
                return previousValue + currentDuration;
            }
            return previousValue;
        }, 0);

        this._durationByRecordType.set(recordType, duration);
        return duration;
    }
};

WebInspector.RunLoopTimelineRecord.TypeIdentifier = "runloop-timeline-record";
