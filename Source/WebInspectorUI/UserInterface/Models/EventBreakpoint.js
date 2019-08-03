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

WI.EventBreakpoint = class EventBreakpoint extends WI.Object
{
    constructor(type, {eventName, eventListener, disabled} = {})
    {
        super();

        console.assert(Object.values(WI.EventBreakpoint.Type).includes(type), type);

        this._type = type;
        this._eventName = eventName || null;
        this._eventListener = eventListener || null;
        this._disabled = disabled || false;
    }

    // Static

    static deserialize(serializedInfo)
    {
        return new WI.EventBreakpoint(serializedInfo.type, {
            eventName: serializedInfo.eventName,
            disabled: !!serializedInfo.disabled,
        });
    }

    // Public

    get type() { return this._type; }
    get eventName() { return this._eventName; }
    get eventListener() { return this._eventListener; }

    get disabled()
    {
        return this._disabled;
    }

    set disabled(disabled)
    {
        if (this._disabled === disabled)
            return;

        this._disabled = disabled;

        this.dispatchEventToListeners(WI.EventBreakpoint.Event.DisabledStateChanged);
    }

    saveIdentityToCookie(cookie)
    {
        cookie["event-breakpoint-type"] = this._type;
        if (this._eventName)
            cookie["event-breakpoint-event-name"] = this._eventName;
        if (this._eventListener)
            cookie["event-breakpoint-event-listener"] = this._eventListener.eventListenerId;
        if (this._disabled)
            cookie["event-breakpoint-disabled"] = this._disabled;
    }

    toJSON(key)
    {
        let json = {
            type: this._type,
        };
        if (this._eventName)
            json.eventName = this._eventName;
        if (this._disabled)
            json.disabled = true;
        if (key === WI.ObjectStore.toJSONSymbol)
            json[WI.objectStores.eventBreakpoints.keyPath] = this._type + (this._eventName ? ":" + this._eventName : "");
        return json;
    }
};

WI.EventBreakpoint.Type = {
    AnimationFrame: "animation-frame",
    Interval: "interval",
    Listener: "listener",
    Timeout: "timeout",

    // COMPATIBILITY (iOS 13): DOMDebugger.EventBreakpointTypes.Timer was removed.
    Timer: "timer",
};

WI.EventBreakpoint.Event = {
    DisabledStateChanged: "event-breakpoint-disabled-state-changed",
};
