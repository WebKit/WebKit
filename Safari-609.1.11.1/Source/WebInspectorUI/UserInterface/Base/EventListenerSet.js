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

// This class supports adding and removing many listeners at once.
// Add DOM or Inspector event listeners to the set using `register()`.
// Use `install()` and `uninstall()` to enable or disable all listeners
// in the set at once.

WI.EventListenerSet = class EventListenerSet
{
    constructor(defaultThisObject, name)
    {
        this.name = name;
        this._defaultThisObject = defaultThisObject;

        this._listeners = [];
        this._installed = false;
    }

    // Public

    register(emitter, type, callback, thisObject, usesCapture)
    {
        console.assert(emitter, `Missing event emitter for event: ${type}.`);
        console.assert(type, "Missing event type.");
        console.assert(callback, `Missing callback for event: ${type}.`);
        var emitterIsValid = emitter && (emitter instanceof WI.Object || emitter instanceof Node || (typeof emitter.addEventListener === "function"));
        console.assert(emitterIsValid, "Event emitter ", emitter, ` (type: ${type}) is null or does not implement Node or WI.Object.`);

        if (!emitterIsValid || !type || !callback)
            return;

        this._listeners.push({listener: new WI.EventListener(thisObject || this._defaultThisObject), emitter, type, callback, usesCapture});
    }

    unregister()
    {
        if (this._installed)
            this.uninstall();
        this._listeners = [];
    }

    install()
    {
        console.assert(!this._installed, "Already installed listener group: " + this.name);
        if (this._installed)
            return;

        this._installed = true;

        for (var data of this._listeners)
            data.listener.connect(data.emitter, data.type, data.callback, data.usesCapture);
    }

    uninstall(unregisterListeners)
    {
        console.assert(this._installed, "Trying to uninstall listener group " + this.name + ", but it isn't installed.");
        if (!this._installed)
            return;

        this._installed = false;

        for (var data of this._listeners)
            data.listener.disconnect();

        if (unregisterListeners)
            this._listeners = [];
    }
};
