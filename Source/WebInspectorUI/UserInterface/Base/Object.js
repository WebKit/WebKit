/*
 * Copyright (C) 2008, 2013 Apple Inc. All Rights Reserved.
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

WebInspector.Object = class WebInspectorObject
{
    constructor()
    {
        this._listeners = null;
    }

    // Static

    static addEventListener(eventType, listener, thisObject)
    {
        thisObject = thisObject || null;

        console.assert(eventType, "Object.addEventListener: invalid event type ", eventType, "(listener: ", listener, "thisObject: ", thisObject, ")");
        if (!eventType)
            return;

        console.assert(listener, "Object.addEventListener: invalid listener ", listener, "(event type: ", eventType, "thisObject: ", thisObject, ")");
        if (!listener)
            return;

        if (!this._listeners)
            this._listeners = new Map();

        let listenersTable = this._listeners.get(eventType);
        if (!listenersTable) {
            listenersTable = new ListMultimap();
            this._listeners.set(eventType, listenersTable);
        }

        listenersTable.add(thisObject, listener);
    }

    static singleFireEventListener(eventType, listener, thisObject)
    {
        let wrappedCallback = () => {
            this.removeEventListener(eventType, wrappedCallback, null);
            listener.apply(thisObject, arguments);
        };

        this.addEventListener(eventType, wrappedCallback, null);
        return wrappedCallback;
    }

    static removeEventListener(eventType, listener, thisObject)
    {
        eventType = eventType || null;
        listener = listener || null;
        thisObject = thisObject || null;

        if (!this._listeners)
            return;

        if (thisObject && !eventType) {
            this._listeners.forEach(function(listenersTable) {
                let listenerPairs = listenersTable.toArray();
                for (let i = 0, length = listenerPairs.length; i < length; ++i) {
                    let existingThisObject = listenerPairs[i][0];
                    if (existingThisObject === thisObject)
                        listenersTable.deleteAll(existingThisObject);
                }
            });

            return;
        }

        let listenersTable = this._listeners.get(eventType);
        if (!listenersTable || listenersTable.size === 0)
            return;

        let didDelete = listenersTable.delete(thisObject, listener);
        console.assert(didDelete, "removeEventListener cannot remove " + eventType.toString() + " because it doesn't exist.");
    }

    // Only used by tests.
    static hasEventListeners(eventType)
    {
        if (!this._listeners)
            return false;

        let listenersTable = this._listeners.get(eventType);
        return listenersTable && listenersTable.size > 0;
    }

    // This should only be used within regression tests to detect leaks.
    static retainedObjectsWithPrototype(proto)
    {
        let results = new Set;

        if (this._listeners) {
            this._listeners.forEach(function(listenersTable, eventType) {
                listenersTable.forEach(function(pair) {
                    let thisObject = pair[0];
                    if (thisObject instanceof proto)
                        results.add(thisObject);
                });
            });
        }

        return results;
    }

    // Public

    addEventListener() { return WebInspector.Object.addEventListener.apply(this, arguments); }
    singleFireEventListener() { return WebInspector.Object.singleFireEventListener.apply(this, arguments); }
    removeEventListener() { return WebInspector.Object.removeEventListener.apply(this, arguments); }
    hasEventListeners() { return WebInspector.Object.hasEventListeners.apply(this, arguments); }
    retainedObjectsWithPrototype() { return WebInspector.Object.retainedObjectsWithPrototype.apply(this, arguments); }

    dispatchEventToListeners(eventType, eventData)
    {
        let event = new WebInspector.Event(this, eventType, eventData);

        function dispatch(object)
        {
            if (!object || !object._listeners || event._stoppedPropagation)
                return;

            if (!(object._listeners instanceof Map)) {
                console.error("object._listeners should be a Map but it isn't.\n`object` is most likely a WebInspector.EventListenerSet.");
                return;
            }

            let listenersTable = object._listeners.get(eventType);
            if (!listenersTable)
                return;

            // Make a copy with slice so mutations during the loop doesn't affect us.
            let listeners = listenersTable.toArray();

            // Iterate over the listeners and call them. Stop if stopPropagation is called.
            for (let i = 0, length = listeners.length; i < length; ++i) {
                let [thisObject, listener] = listeners[i];
                listener.call(thisObject, event);
                if (event._stoppedPropagation)
                    break;
            }
        }

        // Dispatch to listeners of this specific object.
        dispatch(this);

        // Allow propagation again so listeners on the constructor always have a crack at the event.
        event._stoppedPropagation = false;

        // Dispatch to listeners on all constructors up the prototype chain, including the immediate constructor.
        let constructor = this.constructor;
        while (constructor) {
            dispatch(constructor);

            if (!constructor.prototype.__proto__)
                break;

            constructor = constructor.prototype.__proto__.constructor;
        }

        return event.defaultPrevented;
    }
};

WebInspector.Event = class Event
{
    constructor(target, type, data)
    {
        this.target = target;
        this.type = type;
        this.data = data;
        this.defaultPrevented = false;
        this._stoppedPropagation = false;
    }

    stopPropagation()
    {
        this._stoppedPropagation = true;
    }

    preventDefault()
    {
        this.defaultPrevented = true;
    }
};

WebInspector.notifications = new WebInspector.Object;

WebInspector.Notification = {
    GlobalModifierKeysDidChange: "global-modifiers-did-change",
    PageArchiveStarted: "page-archive-started",
    PageArchiveEnded: "page-archive-ended",
    ExtraDomainsActivated: "extra-domains-activated",
    TabTypesChanged: "tab-types-changed",
    DebugUIEnabledDidChange: "debug-ui-enabled-did-change",
};
