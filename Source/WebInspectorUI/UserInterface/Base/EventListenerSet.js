/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This class supports adding and removing many listeners at once.
// Add DOM or Inspector event listeners to the set using `register()`.
// Use `install()` and `uninstall()` to enable or disable all listeners
// in the set at once.
WebInspector.EventListenerSet = function(defaultThisObject, name)
{
    this.name = name;
    this._defaultThisObject = defaultThisObject;

    this._listeners = [];
    this._installed = false;
}

WebInspector.EventListenerSet.prototype = {
    register: function(emitter, type, listener, thisObject, useCapture)
    {
        console.assert(listener, "Missing listener for event: " + type);
        console.assert(emitter, "Missing event emitter for event: " + type);
        console.assert(emitter instanceof WebInspector.Object || emitter instanceof Node || (typeof emitter.addEventListener === "function"), "Event emitter", emitter, " (type:" + type + ") does not implement Node or WebInspector.Object!");

        if (emitter instanceof Node)
            listener = listener.bind(thisObject || this._defaultThisObject);

        this._listeners.push({emitter: emitter, type: type, listener: listener, thisObject: thisObject, useCapture: useCapture});
    },

    unregister: function()
    {
        if (this._installed)
            this.uninstall();
        this._listeners = [];
    },

    install: function()
    {
        console.assert(!this._installed, "Already installed listener group: " + this.name);

        this._installed = true;

        for (var listenerData of this._listeners) {
            if (listenerData.emitter instanceof Node)
                listenerData.emitter.addEventListener(listenerData.type, listenerData.listener, listenerData.useCapture);
            else
                listenerData.emitter.addEventListener(listenerData.type, listenerData.listener, listenerData.thisObject || this._defaultThisObject);
        }
    },

    uninstall: function(unregisterListeners)
    {
        console.assert(this._installed, "Trying to uninstall listener group " + this.name + ", but it isn't installed.");

        this._installed = false;

        for (var listenerData of this._listeners) {
            if (listenerData.emitter instanceof Node)
                listenerData.emitter.removeEventListener(listenerData.type, listenerData.listener, listenerData.useCapture);
            else
                listenerData.emitter.removeEventListener(listenerData.type, listenerData.listener, listenerData.thisObject || this._defaultThisObject);
        }

        if (unregisterListeners) {
            this._listeners = [];
            delete this._defaultThisObject;
        }
    },
}
