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

WI.MediaPlayer = class MediaPlayer extends WI.Object
{
    constructor(identifier, originUrl, contentType, duration, engine)
    {
        super();

        console.assert(identifier);
        
        this._identifier = identifier;
        this._originUrl = originUrl;
        this._contentType = contentType;
        this._duration = duration;
        this._engine = engine;
        this._mediaType = 'video';

        this._events = [];
    }

    // Static

    static fromPayload(payload)
    {
        return new WI.MediaPlayer(
            payload.playerId,
            payload.originUrl,
            payload.contentType,
            payload.duration,
            payload.engine);
    }

    static parseMediaTime(time)
    {
        if (typeof time.value == 'number')
            return new Date(+time.value * 1000).toISOString().substring(11, 23);
        
        if (time.invalid || !time.value)
            return 'Invalid Time.'

        return time.value;
    }

    // Public

    get identifier() { return this._identifier; }
    get originUrl() { return this._originUrl; }
    get contentType() { return this._contentType; }
    get duration() { return this._duration; }
    get engine() { return this._engine; }
    get mediaType() { return this._mediaType; }

    get events() { return this._events; }

    properties()
    {
        let props = {};
        for (let key of WI.MediaPlayer.Properties)
            props[key] = this[key];
        return props;
    }

    updateFromPayload(payload)
    {
        this._originUrl = payload.originUrl;
        this._contentType = payload.contentType;
        this._duration = payload.duration;
        this._engine = payload.engine;

        this.dispatchEventToListeners(WI.MediaPlayer.Event.PlayerDidUpdate);
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WI.MediaPlayer.PlayerIdentifierCookieKey] = this._identifier;
    }

    requestNode()
    {
        if (!this._requestNodePromise) {
            this._requestNodePromise = new Promise((resolve, reject) => {
                WI.domManager.ensureDocument();

                let target = WI.assumingMainTarget();
                target.MediaAgent.requestNode(this._identifier, (error, nodeId) => {
                    if (error) {
                        resolve(null);
                        return;
                    }

                    this._domNode = WI.domManager.nodeForId(nodeId);
                    if (!this._domNode) {
                        resolve(null);
                        return;
                    }

                    resolve(this._domNode);
                });
            });
        }
        return this._requestNodePromise;
    }

    play(callback)
    {
        let target = WI.assumingMainTarget();
        target.MediaAgent.play(this._identifier, callback);
    }

    pause(callback)
    {
        let target = WI.assumingMainTarget();
        target.MediaAgent.pause(this._identifier, callback);
    }

    pushEvent({event, data})
    {
        switch (event) {
            case "play":
                this.dispatchEventToListeners(WI.MediaPlayer.Event.PlayerDidPlay);
                break;
            case "pause":
                this.dispatchEventToListeners(WI.MediaPlayer.Event.PlayerDidPause);
                break;
        }
        const eventObject = {"time": new Date(), "playerEvent": event, "playerData": data};
        this._events.push(eventObject);
        this.dispatchEventToListeners(WI.MediaPlayer.Event.EventsDidChange, eventObject);
    }
};

WI.MediaPlayer.Properties = [
    "identifier",
    "originUrl",
    "contentType",
    "duration",
    "engine"
];

WI.MediaPlayer.PlayerIdentifierCookieKey = "media-player-identifier";

WI.MediaPlayer.Event = {
    PlayerDidUpdate: "media-player-did-update",
    EventsDidChange: "media-player-events-did-change",
    PlayerDidPlay: "media-player-did-play",
    PlayerDidPause: "media-player-did-pause",
};
