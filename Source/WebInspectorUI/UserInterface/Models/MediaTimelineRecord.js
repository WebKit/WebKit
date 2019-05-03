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
    constructor(eventType, timestamp, {domNode, domEvent, isPowerEfficient} = {})
    {
        console.assert(Object.values(WI.MediaTimelineRecord.EventType).includes(eventType));

        super(WI.TimelineRecord.Type.Media, timestamp, timestamp);

        this._eventType = eventType;
        this._domNode = domNode || null;
        this._domEvent = domEvent || null;
        this._isPowerEfficient = isPowerEfficient || false;
    }

    // Import / Export

    static fromJSON(json)
    {
        let {eventType, timestamp} = json;

        // COMPATIBILITY (iOS 12.2): isLowPower was renamed to isPowerEfficient.
        if ("isLowPower" in json && !("isPowerEfficient" in json))
            json.isPowerEfficient = json.isLowPower;

        return new WI.MediaTimelineRecord(eventType, timestamp, json);
    }

    toJSON()
    {
        // FIXME: DOMNode

        // Don't include the DOMEvent's originator.
        let domEvent = this._domEvent;
        if (domEvent && domEvent.originator) {
            domEvent = Object.shallowCopy(domEvent);
            domEvent.originator = undefined;
        }

        return {
            type: this.type,
            eventType: this._eventType,
            timestamp: this.startTime,
            domEvent,
            isPowerEfficient: this._isPowerEfficient,
        };
    }

    // Public

    get eventType() { return this._eventType; }
    get domNode() { return this._domNode; }
    get domEvent() { return this._domEvent; }
    get isPowerEfficient() { return this._isPowerEfficient; }

    get displayName()
    {
        if (this._eventType === WI.MediaTimelineRecord.EventType.DOMEvent && this._domEvent) {
            let eventName = this._domEvent.eventName;
            if (eventName === "webkitfullscreenchange" && this._domEvent.data)
                return this._domEvent.data.enabled ? WI.UIString("Entered Full-Screen Mode") : WI.UIString("Exited Full-Screen Mode");
            return eventName;
        }

        if (this._eventType === MediaTimelineRecord.EventType.PowerEfficientPlaybackStateChanged)
            return this._isPowerEfficient ? WI.UIString("Power Efficient Playback Started") : WI.UIString("Power Efficient Playback Stopped");

        if (this._domNode)
            return this._domNode.displayName;

        console.error("Unknown media record event type: ", this._eventType, this);
        return WI.UIString("Media Event");
    }

    saveIdentityToCookie(cookie)
    {
        super.saveIdentityToCookie(cookie);

        cookie["media-timeline-record-event-type"] = this._eventType;
        if (this._eventType === MediaTimelineRecord.EventType.PowerEfficientPlaybackStateChanged)
            cookie["media-timeline-record-power-efficient-playback"] = this._isPowerEfficient;
        if (this._domNode)
            cookie["media-timeline-record-dom-node"] = this._domNode.path();
        if (this._domEvent) {
            cookie["media-timeline-record-dom-event"] = this._domEvent.eventName;
            if (this._domEvent.data && this._domEvent.data.enabled)
                cookie["media-timeline-record-dom-event-active"] = true;
        }
    }
};

WI.MediaTimelineRecord.EventType = {
    DOMEvent: "dom-event",
    PowerEfficientPlaybackStateChanged: "power-efficient-playback-state-changed",
};
