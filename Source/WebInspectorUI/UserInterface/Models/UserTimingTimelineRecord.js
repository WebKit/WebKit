/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.UserTimingTimelineRecord = class UserTimingTimelineRecord extends WI.TimelineRecord
{
    constructor(eventType, label, startTime, stackTrace, sourceCodeLocation, {endTime, target, detail} = {})
    {
        console.assert(Object.values(WI.UserTimingTimelineRecord.EventType).includes(eventType));

        super(WI.TimelineRecord.Type.UserTiming, startTime, endTime, stackTrace, sourceCodeLocation);

        this._eventType = eventType;
        this._label = label || null;

        if (detail) {
            if ((detail.objectId && target) || detail.type !== "object" || detail.value === null)
                this._detail = WI.RemoteObject.fromPayload(detail, target || null);
            else if (detail.preview)
                this._detail = WI.ObjectPreview.fromPayload(detail.preview);
        }
        this._detail ||= null;
    }

    // Import / Export

    static fromJSON(json)
    {
        let {eventType, label, startTime, endTime, stackTrace, sourceCodeLocation, detail} = json;
        return new WI.UserTimingTimelineRecord(eventType, label, startTime, stackTrace, sourceCodeLocation, {endTime, detail});
    }

    toJSON()
    {
        // FIXME: stackTrace
        // FIXME: sourceCodeLocation

        let json = {
            type: this.type,
            eventType: this._eventType,
            label: this._label,
            startTime: this._startTime,
        };
        if (!isNaN(this._endTime))
            json.endTime = this._endTime;
        if (this._detail instanceof WI.RemoteObject && (this._detail.type !== "object" || this._detail.value !== "null"))
            json.detail = this._detail.exportData();
        else if (this._detail instanceof WI.ObjectPreview)
            json.detail = {preview: this._detail.exportData()};
        return json;
    }

    // Public

    get eventType() { return this._eventType; }
    get label() { return this._label; }
    get detail() { return this._detail; }

    get updatesDynamically()
    {
        return this._eventType === WI.UserTimingTimelineRecord.EventType.ConsoleTime && isNaN(this._endTime);
    }

    finish(endTime)
    {
        console.assert(this._eventType === WI.UserTimingTimelineRecord.EventType.ConsoleTime, this);
        console.assert(isNaN(this._endTime), this);
        console.assert(!isNaN(endTime), endTime);

        this._endTime = endTime;

        this.dispatchEventToListeners(WI.TimelineRecord.Event.Updated);
    }

    // Protected

    saveIdentityToCookie(cookie)
    {
        super.saveIdentityToCookie(cookie);

        cookie["user-timing-timeline-record-label"] = this._label;
        cookie["user-timing-timeline-record-event-type"] = this._eventType;
    }
};

WI.UserTimingTimelineRecord.EventType = {
    PerformanceMeasure: "performance-measure",
    ConsoleTime: "console-time",
};

WI.UserTimingTimelineRecord.TypeIdentifier = "user-timing-timeline-record";
