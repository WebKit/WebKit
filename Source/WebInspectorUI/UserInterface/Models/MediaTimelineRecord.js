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

WI.MediaTimelineRecord = class MediaTimelineRecord extends WI.TimelineRecord
{
    constructor(eventType, timestamp, {domNode, domEvent, isLowPower} = {})
    {
        console.assert(Object.values(WI.MediaTimelineRecord.EventType).includes(eventType));

        super(WI.TimelineRecord.Type.Media, timestamp, timestamp);

        this._eventType = eventType;
        this._domNode = domNode || null;
        this._domEvent = domEvent || null;
        this._isLowPower = isLowPower || false;
    }

    // Public

    get eventType() { return this._eventType; }
    get domNode() { return this._domNode; }
    get domEvent() { return this._domEvent; }
    get isLowPower() { return this._isLowPower; }

    get displayName()
    {
        if (this._eventType === WI.MediaTimelineRecord.EventType.DOMEvent && this._domEvent) {
            let eventName = this._domEvent.eventName;
            if (eventName === "webkitfullscreenchange" && this._domEvent.data)
                return this._domEvent.data.enabled ? WI.UIString("Entered Full-Screen Mode") : WI.UIString("Exited Full-Screen Mode");
            return eventName;
        }

        if (this._eventType === WI.MediaTimelineRecord.EventType.LowPower)
            return this._isLowPower ? WI.UIString("Entered Low-Power Mode") : WI.UIString("Exited Low-Power Mode");

        if (this._domNode)
            return this._domNode.displayName;

        console.error("Unknown media record event type: ", this._eventType, this);
        return WI.UIString("Media Event");
    }

    saveIdentityToCookie(cookie)
    {
        super.saveIdentityToCookie(cookie);

        cookie["media-timeline-record-event-type"] = this._eventType;
        if (this._domNode)
            cookie["media-timeline-record-dom-node"] = this._domNode.path();
        if (this._domEvent)
            cookie["media-timeline-record-dom-event"] = this._domEvent.eventName;
        if (this._isLowPower || (this._domEvent && this._domEvent.data && this._domEvent.data.enabled))
            cookie["media-timeline-record-active"] = true;
    }
};

WI.MediaTimelineRecord.EventType = {
    DOMEvent: "dom-event",
    LowPower: "low-power",
};
