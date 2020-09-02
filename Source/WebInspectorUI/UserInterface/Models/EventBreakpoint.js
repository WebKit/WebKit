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

WI.EventBreakpoint = class EventBreakpoint extends WI.Breakpoint
{
    constructor(type, {eventName, eventListener, disabled, actions, condition, ignoreCount, autoContinue} = {})
    {
        // COMPATIBILITY (iOS 13): DOMDebugger.EventBreakpointTypes.Timer was removed.
        if (type === "timer") {
            switch (eventName) {
            case "setInterval":
                type = WI.EventBreakpoint.Type.Interval;
                break;

            case "setTimeout":
                type = WI.EventBreakpoint.Type.Timeout;
                break;
            }
        }

        console.assert(Object.values(WI.EventBreakpoint.Type).includes(type), type);
        console.assert(!eventName || type === WI.EventBreakpoint.Type.Listener, eventName);
        console.assert(!eventListener || type === WI.EventBreakpoint.Type.Listener, eventListener);

        super({disabled, condition, actions, ignoreCount, autoContinue});

        this._type = type;
        this._eventName = eventName || null;
        this._eventListener = eventListener || null;
    }

    // Static

    static get supportsEditing()
    {
        if (this._eventListener) {
            // COMPATIBILITY (iOS 14): DOM.setBreakpointForEventListener did not have an "options" parameter yet.
            return InspectorBackend.hasCommand("DOM.setBreakpointForEventListener", "options");
        }

        // COMPATIBILITY (iOS 14): DOMDebugger.setEventBreakpoint did not have an "options" parameter yet.
        return InspectorBackend.hasCommand("DOMDebugger.setEventBreakpoint", "options");
    }

    static fromJSON(json)
    {
        return new WI.EventBreakpoint(json.type, {
            eventName: json.eventName,
            disabled: json.disabled,
            condition: json.condition,
            actions: json.actions?.map((actionJSON) => WI.BreakpointAction.fromJSON(actionJSON)) || [],
            ignoreCount: json.ignoreCount,
            autoContinue: json.autoContinue,
        });
    }

    // Public

    get type() { return this._type; }
    get eventName() { return this._eventName; }
    get eventListener() { return this._eventListener; }

    get displayName()
    {
        switch (this) {
        case WI.domDebuggerManager.allAnimationFramesBreakpoint:
            return WI.repeatedUIString.allAnimationFrames();

        case WI.domDebuggerManager.allIntervalsBreakpoint:
            return WI.repeatedUIString.allIntervals();

        case WI.domDebuggerManager.allListenersBreakpoint:
            return WI.repeatedUIString.allEvents();

        case WI.domDebuggerManager.allTimeoutsBreakpoint:
            return WI.repeatedUIString.allTimeouts();
        }

        return this._eventName;
    }

    get special()
    {
        switch (this) {
        case WI.domDebuggerManager.allAnimationFramesBreakpoint:
        case WI.domDebuggerManager.allIntervalsBreakpoint:
        case WI.domDebuggerManager.allListenersBreakpoint:
        case WI.domDebuggerManager.allTimeoutsBreakpoint:
            return true;
        }

        return super.special;
    }

    get editable()
    {
        return WI.EventBreakpoint.editable || super.editable;
    }

    remove()
    {
        super.remove();

        if (this._eventListener)
            WI.domManager.removeBreakpointForEventListener(this._eventListener);
        else
            WI.domDebuggerManager.removeEventBreakpoint(this);
    }

    saveIdentityToCookie(cookie)
    {
        cookie["event-breakpoint-type"] = this._type;
        if (this._eventName)
            cookie["event-breakpoint-event-name"] = this._eventName;
        if (this._eventListener)
            cookie["event-breakpoint-event-listener"] = this._eventListener.eventListenerId;
    }

    toJSON(key)
    {
        let json = super.toJSON(key);
        json.type = this._type;
        if (this._eventName)
            json.eventName = this._eventName;
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
};

WI.EventBreakpoint.ReferencePage = WI.ReferencePage.EventBreakpoints;
