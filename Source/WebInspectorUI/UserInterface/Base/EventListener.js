/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2013, 2014 University of Washington. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.EventListener = class EventListener
{
    constructor(thisObject, fireOnce)
    {
        this._thisObject = thisObject;
        this._emitter = null;
        this._callback = null;
        this._fireOnce = fireOnce;
    }

    // Public

    connect(emitter, type, callback, usesCapture)
    {
        console.assert(!this._emitter && !this._callback, "EventListener already bound to a callback.", this);
        console.assert(emitter, `Missing event emitter for event: ${type}.`);
        console.assert(type, "Missing event type.");
        console.assert(callback, `Missing callback for event: ${type}.`);
        var emitterIsValid = emitter && (emitter instanceof WI.Object || emitter instanceof Node || (typeof emitter.addEventListener === "function"));
        console.assert(emitterIsValid, "Event emitter ", emitter, ` (type: ${type}) is null or does not implement Node or WI.Object.`);

        if (!emitterIsValid || !type || !callback)
            return;

        this._emitter = emitter;
        this._type = type;
        this._usesCapture = !!usesCapture;

        if (emitter instanceof Node)
            callback = callback.bind(this._thisObject);

        if (this._fireOnce) {
            var listener = this;
            this._callback = function() {
                listener.disconnect();
                callback.apply(this, arguments);
            };
        } else
            this._callback = callback;

        if (this._emitter instanceof Node)
            this._emitter.addEventListener(this._type, this._callback, this._usesCapture);
        else
            this._emitter.addEventListener(this._type, this._callback, this._thisObject);
    }

    disconnect()
    {
        console.assert(this._emitter && this._callback, "EventListener is not bound to a callback.", this);

        if (!this._emitter || !this._callback)
            return;

        if (this._emitter instanceof Node)
            this._emitter.removeEventListener(this._type, this._callback, this._usesCapture);
        else
            this._emitter.removeEventListener(this._type, this._callback, this._thisObject);

        if (this._fireOnce)
            delete this._thisObject;
        delete this._emitter;
        delete this._type;
        delete this._callback;
    }
};
