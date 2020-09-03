/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.DOMDebuggerManager = class DOMDebuggerManager extends WI.Object
{
    constructor()
    {
        super();

        this._domBreakpointURLMap = new Multimap;
        this._domBreakpointFrameIdentifierMap = new Map;

        this._listenerBreakpoints = [];
        this._allAnimationFramesBreakpoint = null;
        this._allIntervalsBreakpoint = null;
        this._allListenersBreakpoint = null;
        this._allTimeoutsBreakpoint = null;

        this._urlBreakpoints = [];
        this._allRequestsBreakpoint = null;

        WI.DOMBreakpoint.addEventListener(WI.Breakpoint.Event.DisabledStateDidChange, this._handleDOMBreakpointDisabledStateChanged, this);

        WI.EventBreakpoint.addEventListener(WI.Breakpoint.Event.DisabledStateDidChange, this._handleEventBreakpointDisabledStateChanged, this);
        WI.EventBreakpoint.addEventListener(WI.Breakpoint.Event.ConditionDidChange, this._handleEventBreakpointEditablePropertyChanged, this);
        WI.EventBreakpoint.addEventListener(WI.Breakpoint.Event.IgnoreCountDidChange, this._handleEventBreakpointEditablePropertyChanged, this);
        WI.EventBreakpoint.addEventListener(WI.Breakpoint.Event.AutoContinueDidChange, this._handleEventBreakpointEditablePropertyChanged, this);
        WI.EventBreakpoint.addEventListener(WI.Breakpoint.Event.ActionsDidChange, this._handleEventBreakpointActionsChanged, this);

        WI.URLBreakpoint.addEventListener(WI.Breakpoint.Event.DisabledStateDidChange, this._handleURLBreakpointDisabledStateChanged, this);
        WI.URLBreakpoint.addEventListener(WI.Breakpoint.Event.ConditionDidChange, this._handleURLBreakpointEditablePropertyChanged, this);
        WI.URLBreakpoint.addEventListener(WI.Breakpoint.Event.IgnoreCountDidChange, this._handleURLBreakpointEditablePropertyChanged, this);
        WI.URLBreakpoint.addEventListener(WI.Breakpoint.Event.AutoContinueDidChange, this._handleURLBreakpointEditablePropertyChanged, this);
        WI.URLBreakpoint.addEventListener(WI.Breakpoint.Event.ActionsDidChange, this._handleURLBreakpointActionsChanged, this);

        WI.domManager.addEventListener(WI.DOMManager.Event.NodeRemoved, this._nodeRemoved, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.NodeInserted, this._nodeInserted, this);

        WI.networkManager.addEventListener(WI.NetworkManager.Event.MainFrameDidChange, this._mainFrameDidChange, this);

        WI.Frame.addEventListener(WI.Frame.Event.ChildFrameWasRemoved, this._childFrameWasRemoved, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        let loadBreakpoints = (constructor, objectStore, oldSettings, callback) => {
            WI.Target.registerInitializationPromise((async () => {
                for (let key of oldSettings) {
                    let existingSerializedBreakpoints = WI.Setting.migrateValue(key);
                    if (existingSerializedBreakpoints) {
                        for (let existingSerializedBreakpoint of existingSerializedBreakpoints)
                            await objectStore.putObject(constructor.fromJSON(existingSerializedBreakpoint));
                    }
                }

                let serializedBreakpoints = await objectStore.getAll();

                this._restoringBreakpoints = true;
                for (let serializedBreakpoint of serializedBreakpoints) {
                    let breakpoint = constructor.fromJSON(serializedBreakpoint);

                    const key = null;
                    objectStore.associateObject(breakpoint, key, serializedBreakpoint);

                    callback(breakpoint);
                }
                this._restoringBreakpoints = false;
            })());
        };

        function loadLegacySpecialBreakpoint(shownSettingsKey, enabledSettingsKey, callback) {
            if (!WI.Setting.migrateValue(shownSettingsKey))
                return;

            return callback({
                disabled: !WI.Setting.migrateValue(enabledSettingsKey),
            });
        }

        if (DOMDebuggerManager.supportsDOMBreakpoints()) {
            loadBreakpoints(WI.DOMBreakpoint, WI.objectStores.domBreakpoints, ["dom-breakpoints"], (breakpoint) => {
                this.addDOMBreakpoint(breakpoint);
            });
        }

        if (DOMDebuggerManager.supportsEventBreakpoints() || DOMDebuggerManager.supportsEventListenerBreakpoints()) {
            loadBreakpoints(WI.EventBreakpoint, WI.objectStores.eventBreakpoints, ["event-breakpoints"], (breakpoint) => {
                this.addEventBreakpoint(breakpoint);
            });

            this._allAnimationFramesBreakpoint ??= loadLegacySpecialBreakpoint("show-all-animation-frames-breakpoint", "break-on-all-animation-frames", (options) => new WI.EventBreakpoint(WI.EventBreakpoint.Type.AnimationFrame, options));
            this._allIntervalsBreakpoint ??= loadLegacySpecialBreakpoint("show-all-inteverals-breakpoint", "break-on-all-intervals", (options) => new WI.EventBreakpoint(WI.EventBreakpoint.Type.Interval, options));
            this._allListenersBreakpoint ??= loadLegacySpecialBreakpoint("show-all-listeners-breakpoint", "break-on-all-listeners", (options) => new WI.EventBreakpoint(WI.EventBreakpoint.Type.Listener, options));
            this._allTimeoutsBreakpoint ??= loadLegacySpecialBreakpoint("show-all-timeouts-breakpoint", "break-on-all-timeouts", (options) => new WI.EventBreakpoint(WI.EventBreakpoint.Type.Timeout, options));
        }

        if (DOMDebuggerManager.supportsURLBreakpoints() || DOMDebuggerManager.supportsXHRBreakpoints()) {
            loadBreakpoints(WI.URLBreakpoint, WI.objectStores.urlBreakpoints, ["xhr-breakpoints", "url-breakpoints"], (breakpoint) => {
                this.addURLBreakpoint(breakpoint);
            });

            this._allRequestsBreakpoint ??= loadLegacySpecialBreakpoint("show-all-requests-breakpoint", "break-on-all-requests", (options) => new WI.URLBreakpoint(WI.URLBreakpoint.Type.Text, "", options));
        }
    }

    // Target

    initializeTarget(target)
    {
        if (target.hasDomain("DOMDebugger")) {
            this._restoringBreakpoints = true;

            if (target === WI.assumingMainTarget() && target.mainResource)
                this._speculativelyResolveDOMBreakpointsForURL(target.mainResource.url);

            if (this._allAnimationFramesBreakpoint && !this._allAnimationFramesBreakpoint.disabled)
                this._setEventBreakpoint(this._allAnimationFramesBreakpoint, target);

            if (this._allIntervalsBreakpoint && !this._allIntervalsBreakpoint.disabled)
                this._setEventBreakpoint(this._allIntervalsBreakpoint, target);

            if (this._allListenersBreakpoint && !this._allListenersBreakpoint.disabled)
                this._setEventBreakpoint(this._allListenersBreakpoint, target);

            if (this._allTimeoutsBreakpoint && !this._allTimeoutsBreakpoint.disabled)
                this._setEventBreakpoint(this._allTimeoutsBreakpoint, target);

            if (this._allRequestsBreakpoint)
                this._setURLBreakpoint(this._allRequestsBreakpoint, target);

            for (let breakpoint of this._listenerBreakpoints) {
                if (!breakpoint.disabled)
                    this._setEventBreakpoint(breakpoint, target);
            }

            for (let breakpoint of this._urlBreakpoints) {
                if (!breakpoint.disabled)
                    this._setURLBreakpoint(breakpoint, target);
            }

            this._restoringBreakpoints = false;
        }
    }

    // Static

    static supportsDOMBreakpoints()
    {
        return InspectorBackend.hasCommand("DOMDebugger.setDOMBreakpoint")
            && InspectorBackend.hasCommand("DOMDebugger.removeDOMBreakpoint");
    }

    static supportsEventBreakpoints()
    {
        // COMPATIBILITY (iOS 13): DOMDebugger.setEventBreakpoint and DOMDebugger.removeEventBreakpoint did not exist yet.
        return InspectorBackend.hasCommand("DOMDebugger.setEventBreakpoint")
            && InspectorBackend.hasCommand("DOMDebugger.removeEventBreakpoint");
    }

    static supportsEventListenerBreakpoints()
    {
        // COMPATIBILITY (iOS 12.2): Replaced by DOMDebugger.setEventBreakpoint and DOMDebugger.removeEventBreakpoint.
        return InspectorBackend.hasCommand("DOMDebugger.setEventListenerBreakpoint")
            && InspectorBackend.hasCommand("DOMDebugger.removeEventListenerBreakpoint");
    }

    static supportsURLBreakpoints()
    {
        // COMPATIBILITY (iOS 13): DOMDebugger.setURLBreakpoint and DOMDebugger.removeURLBreakpoint did not exist yet.
        return InspectorBackend.hasCommand("DOMDebugger.setURLBreakpoint")
            && InspectorBackend.hasCommand("DOMDebugger.removeURLBreakpoint");
    }

    static supportsXHRBreakpoints()
    {
        // COMPATIBILITY (iOS 13): Replaced by DOMDebugger.setURLBreakpoint and DOMDebugger.removeURLBreakpoint.
        return InspectorBackend.hasCommand("DOMDebugger.setXHRBreakpoint")
            && InspectorBackend.hasCommand("DOMDebugger.removeXHRBreakpoint");
    }

    static supportsAllListenersBreakpoint()
    {
        // COMPATIBILITY (iOS 13): DOMDebugger.EventBreakpointType.Interval and DOMDebugger.EventBreakpointType.Timeout did not exist yet.
        return DOMDebuggerManager.supportsEventBreakpoints()
            && InspectorBackend.Enum.DOMDebugger.EventBreakpointType.Interval
            && InspectorBackend.Enum.DOMDebugger.EventBreakpointType.Timeout;
    }

    // Public

    get supported()
    {
        return InspectorBackend.hasDomain("DOMDebugger");
    }

    get allAnimationFramesBreakpoint() { return this._allAnimationFramesBreakpoint; }
    get allIntervalsBreakpoint() { return this._allIntervalsBreakpoint; }
    get allListenersBreakpoint() { return this._allListenersBreakpoint; }
    get allTimeoutsBreakpoint() { return this._allTimeoutsBreakpoint; }
    get allRequestsBreakpoint() { return this._allRequestsBreakpoint; }

    get domBreakpoints()
    {
        let mainFrame = WI.networkManager.mainFrame;
        if (!mainFrame)
            return [];

        let resolvedBreakpoints = [];
        let frames = [mainFrame];
        while (frames.length) {
            let frame = frames.shift();

            let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(frame.id);
            if (domBreakpointNodeIdentifierMap)
                resolvedBreakpoints.pushAll(domBreakpointNodeIdentifierMap.values());

            frames.pushAll(frame.childFrameCollection);
        }

        return resolvedBreakpoints;
    }

    get listenerBreakpoints() { return this._listenerBreakpoints; }
    get urlBreakpoints() { return this._urlBreakpoints; }

    domBreakpointsForNode(node)
    {
        console.assert(node instanceof WI.DOMNode);

        if (!node || !node.frame)
            return [];

        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(node.frame.id);
        if (!domBreakpointNodeIdentifierMap)
            return [];

        let breakpoints = domBreakpointNodeIdentifierMap.get(node.id);
        return breakpoints ? Array.from(breakpoints) : [];
    }

    domBreakpointsInSubtree(node)
    {
        console.assert(node instanceof WI.DOMNode);

        let breakpoints = [];

        if (node.children) {
            let children = Array.from(node.children);
            while (children.length) {
                let child = children.pop();
                if (child.children)
                    children.pushAll(child.children);
                breakpoints.pushAll(this.domBreakpointsForNode(child));
            }
        }

        return breakpoints;
    }

    addDOMBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.DOMBreakpoint);
        if (!breakpoint || !breakpoint.url)
            return;

        if (breakpoint.special) {
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointAdded, {breakpoint});
            return;
        }

        this._domBreakpointURLMap.add(breakpoint.url, breakpoint);

        if (breakpoint.domNodeIdentifier)
            this._resolveDOMBreakpoint(breakpoint, breakpoint.domNodeIdentifier);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointAdded, {breakpoint});

        if (!this._restoringBreakpoints)
            WI.objectStores.domBreakpoints.putObject(breakpoint);
    }

    removeDOMBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.DOMBreakpoint);
        if (!breakpoint)
            return;

        if (breakpoint.special) {
            breakpoint.disabled = true;
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, {breakpoint});
            return;
        }

        this._detachDOMBreakpoint(breakpoint);

        this._domBreakpointURLMap.delete(breakpoint.url);

        if (!breakpoint.disabled) {
            // We should get the target associated with the nodeIdentifier of this breakpoint.
            let target = WI.assumingMainTarget();
            target.DOMDebuggerAgent.removeDOMBreakpoint(breakpoint.domNodeIdentifier, breakpoint.type);
        }

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, {breakpoint});

        breakpoint.domNodeIdentifier = null;

        if (!this._restoringBreakpoints)
            WI.objectStores.domBreakpoints.deleteObject(breakpoint);
    }

    removeDOMBreakpointsForNode(node)
    {
        this.domBreakpointsForNode(node).forEach(this.removeDOMBreakpoint, this);
    }

    listenerBreakpointForEventName(eventName)
    {
        if (DOMDebuggerManager.supportsAllListenersBreakpoint() && !this._allListenersBreakpoint.disabled)
            return this._allListenersBreakpoint;
        return this._listenerBreakpoints.find((breakpoint) => breakpoint.eventName === eventName) || null;
    }

    addEventBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.EventBreakpoint, breakpoint);
        if (!breakpoint)
            return false;

        console.assert(!breakpoint.special, breakpoint);

        switch (breakpoint.type) {
        case WI.EventBreakpoint.Type.AnimationFrame:
            console.assert(!this._allAnimationFramesBreakpoint, this._allAnimationFramesBreakpoint, breakpoint);
            this._allAnimationFramesBreakpoint = breakpoint;
            break;

        case WI.EventBreakpoint.Type.Interval:
            console.assert(!this._allIntervalsBreakpoint, this._allIntervalsBreakpoint, breakpoint);
            this._allIntervalsBreakpoint = breakpoint;
            break;

        case WI.EventBreakpoint.Type.Listener:
            if (breakpoint.eventName) {
                if (this._listenerBreakpoints.find((existing) => existing.eventName === breakpoint.eventName))
                    return false;

                this._listenerBreakpoints.push(breakpoint);
            } else {
                console.assert(!this._allListenersBreakpoint, this._allListenersBreakpoint, breakpoint);
                this._allListenersBreakpoint = breakpoint;
            }
            break;

        case WI.EventBreakpoint.Type.Timeout:
            console.assert(!this._allTimeoutsBreakpoint, this._allTimeoutsBreakpoint, breakpoint);
            this._allTimeoutsBreakpoint = breakpoint;
            break;
        }

        WI.debuggerManager.addProbesForBreakpoint(breakpoint);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointAdded, {breakpoint});

        if (!breakpoint.disabled) {
            for (let target of WI.targets)
                this._setEventBreakpoint(breakpoint, target);
        }

        if (!this._restoringBreakpoints)
            WI.objectStores.eventBreakpoints.putObject(breakpoint);

        return true;
    }

    removeEventBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.EventBreakpoint, breakpoint);
        if (!breakpoint)
            return;

        // Disable the breakpoint first, so removing actions doesn't re-add the breakpoint.
        breakpoint.disabled = true;
        breakpoint.clearActions();

        switch (breakpoint.type) {
        case WI.EventBreakpoint.Type.AnimationFrame:
            console.assert(this._allAnimationFramesBreakpoint, this._allAnimationFramesBreakpoint);
            this._allAnimationFramesBreakpoint = null;
            break;

        case WI.EventBreakpoint.Type.Interval:
            console.assert(this._allIntervalsBreakpoint, this._allIntervalsBreakpoint);
            this._allIntervalsBreakpoint = null;
            break;

        case WI.EventBreakpoint.Type.Listener:
            if (breakpoint.eventName) {
                console.assert(this._listenerBreakpoints.includes(breakpoint), breakpoint);
                if (!this._listenerBreakpoints.includes(breakpoint))
                    return;

                this._listenerBreakpoints.remove(breakpoint);
            } else {
                console.assert(this._allListenersBreakpoint, this._allListenersBreakpoint);
                this._allListenersBreakpoint = null;
            }
            break;

        case WI.EventBreakpoint.Type.Timeout:
            console.assert(this._allTimeoutsBreakpoint, this._allTimeoutsBreakpoint);
            this._allTimeoutsBreakpoint = null;
            break;
        }

        if (!this._restoringBreakpoints)
            WI.objectStores.eventBreakpoints.deleteObject(breakpoint);

        WI.debuggerManager.removeProbesForBreakpoint(breakpoint);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, {breakpoint});
    }

    urlBreakpointForURL(url)
    {
        return this._urlBreakpoints.find((breakpoint) => breakpoint.url === url) || null;
    }

    addURLBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.URLBreakpoint, breakpoint);
        if (!breakpoint)
            return false;

        console.assert(!breakpoint.special, breakpoint);
        if (breakpoint.url) {
            if (this._urlBreakpoints.some((entry) => entry.type === breakpoint.type && entry.url === breakpoint.url))
                return false;

            this._urlBreakpoints.push(breakpoint);
        } else {
            console.assert(!this._allRequestsBreakpoint, this._allRequestsBreakpoint, breakpoint);
            this._allRequestsBreakpoint = breakpoint;
        }

        WI.debuggerManager.addProbesForBreakpoint(breakpoint);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.URLBreakpointAdded, {breakpoint});

        if (!breakpoint.disabled) {
            for (let target of WI.targets)
                this._setURLBreakpoint(breakpoint, target);
        }

        if (!this._restoringBreakpoints)
            WI.objectStores.urlBreakpoints.putObject(breakpoint);

        return true;
    }

    removeURLBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.URLBreakpoint, breakpoint);
        if (!breakpoint)
            return;

        // Disable the breakpoint first, so removing actions doesn't re-add the breakpoint.
        breakpoint.disabled = true;
        breakpoint.clearActions();

        if (breakpoint.url) {
            console.assert(this._urlBreakpoints.includes(breakpoint), breakpoint);
            if (!this._urlBreakpoints.includes(breakpoint))
                return;

            this._urlBreakpoints.remove(breakpoint);
        } else {
            console.assert(this._allRequestsBreakpoint, this._allRequestsBreakpoint);
            this._allRequestsBreakpoint = null;
        }

        if (!this._restoringBreakpoints)
            WI.objectStores.urlBreakpoints.deleteObject(breakpoint);

        WI.debuggerManager.removeProbesForBreakpoint(breakpoint);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.URLBreakpointRemoved, {breakpoint});
    }

    // Private

    _detachDOMBreakpoint(breakpoint)
    {
        let nodeIdentifier = breakpoint.domNodeIdentifier;
        let node = WI.domManager.nodeForId(nodeIdentifier);
        console.assert(node, "Missing DOM node for breakpoint.", breakpoint);
        if (!node || !node.frame)
            return;

        let frameIdentifier = node.frame.id;
        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(frameIdentifier);
        console.assert(domBreakpointNodeIdentifierMap, "Missing DOM breakpoints for node parent frame.", node);
        if (!domBreakpointNodeIdentifierMap)
            return;

        domBreakpointNodeIdentifierMap.delete(nodeIdentifier, breakpoint);

        if (!domBreakpointNodeIdentifierMap.size)
            this._domBreakpointFrameIdentifierMap.delete(frameIdentifier);
    }

    _detachBreakpointsForFrame(frame)
    {
        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(frame.id);
        if (!domBreakpointNodeIdentifierMap)
            return;

        this._domBreakpointFrameIdentifierMap.delete(frame.id);

        for (let breakpoint of domBreakpointNodeIdentifierMap.values())
            breakpoint.domNodeIdentifier = null;
    }

    _speculativelyResolveDOMBreakpointsForURL(url)
    {
        let domBreakpoints = this._domBreakpointURLMap.get(url);
        if (!domBreakpoints)
            return;

        for (let breakpoint of domBreakpoints) {
            if (breakpoint.domNodeIdentifier)
                continue;

            WI.domManager.pushNodeByPathToFrontend(breakpoint.path, (nodeIdentifier) => {
                if (breakpoint.domNodeIdentifier) {
                    // This breakpoint may have been resolved by a node being inserted before this
                    // callback is invoked.  If so, the `nodeIdentifier` should match, so don't try
                    // to resolve it again as it would've already been resolved.
                    console.assert(breakpoint.domNodeIdentifier === nodeIdentifier);
                    return;
                }

                if (!nodeIdentifier)
                    return;

                this._restoringBreakpoints = true;
                this._resolveDOMBreakpoint(breakpoint, nodeIdentifier);
                this._restoringBreakpoints = false;
            });
        }
    }

    _resolveDOMBreakpoint(breakpoint, nodeIdentifier)
    {
        let node = WI.domManager.nodeForId(nodeIdentifier);
        console.assert(node, "Missing DOM node for nodeIdentifier.", nodeIdentifier);
        if (!node || !node.frame)
            return;

        let frameIdentifier = node.frame.id;
        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(frameIdentifier);
        if (!domBreakpointNodeIdentifierMap) {
            domBreakpointNodeIdentifierMap = new Multimap;
            this._domBreakpointFrameIdentifierMap.set(frameIdentifier, domBreakpointNodeIdentifierMap);
        }

        domBreakpointNodeIdentifierMap.add(nodeIdentifier, breakpoint);

        breakpoint.domNodeIdentifier = nodeIdentifier;

        if (!breakpoint.disabled) {
            // We should get the target associated with the nodeIdentifier of this breakpoint.
            let target = WI.assumingMainTarget();
            if (target)
                this._updateDOMBreakpoint(breakpoint, target);
        }
    }

    _updateDOMBreakpoint(breakpoint, target)
    {
        console.assert(target.type !== WI.TargetType.Worker, "Worker targets do not support DOM breakpoints");
        if (target.type === WI.TargetType.Worker)
            return;

        if (!target.hasCommand("DOMDebugger.setDOMBreakpoint") || !target.hasCommand("DOMDebugger.removeDOMBreakpoint"))
            return;

        if (!breakpoint.domNodeIdentifier)
            return;

        if (breakpoint.disabled)
            target.DOMDebuggerAgent.removeDOMBreakpoint(breakpoint.domNodeIdentifier, breakpoint.type);
        else {
            if (!this._restoringBreakpoints && !WI.debuggerManager.breakpointsDisabledTemporarily)
                WI.debuggerManager.breakpointsEnabled = true;

            target.DOMDebuggerAgent.setDOMBreakpoint(breakpoint.domNodeIdentifier, breakpoint.type);
        }
    }

    _commandArgumentsForEventBreakpoint(breakpoint)
    {
        let commandArguments = {};

        switch (breakpoint) {
        case this._allAnimationFramesBreakpoint:
            commandArguments.breakpointType = WI.EventBreakpoint.Type.AnimationFrame;
            if (!DOMDebuggerManager.supportsAllListenersBreakpoint())
                commandArguments.eventName = "requestAnimationFrame";
            break;

        case this._allIntervalsBreakpoint:
            if (DOMDebuggerManager.supportsAllListenersBreakpoint())
                commandArguments.breakpointType = WI.EventBreakpoint.Type.Interval;
            else {
                commandArguments.breakpointType = WI.EventBreakpoint.Type.Timer;
                commandArguments.eventName = "setInterval";
            }
            break;

        case this._allListenersBreakpoint:
            if (!DOMDebuggerManager.supportsAllListenersBreakpoint())
                return;

            commandArguments.breakpointType = WI.EventBreakpoint.Type.Listener;
            break;

        case this._allTimeoutsBreakpoint:
            if (DOMDebuggerManager.supportsAllListenersBreakpoint())
                commandArguments.breakpointType = WI.EventBreakpoint.Type.Timeout;
            else {
                commandArguments.breakpointType = WI.EventBreakpoint.Type.Timer;
                commandArguments.eventName = "setTimeout";
            }
            break;

        default:
            commandArguments.breakpointType = breakpoint.type;
            commandArguments.eventName = breakpoint.eventName;
            console.assert(commandArguments.eventName);
            break;
        }

        return commandArguments;
    }

    _setEventBreakpoint(breakpoint, target)
    {
        console.assert(!breakpoint.disabled, breakpoint);

        // Worker targets do not support `requestAnimationFrame` breakpoints.
        if (breakpoint === this._allAnimationFramesBreakpoint && target.type === WI.TargetType.Worker)
            return;

        // COMPATIBILITY (iOS 10.3): DOMDebugger.setEventListenerBreakpoint did not exist yet.
        // COMPATIBILITY (iOS 12.0): DOMDebugger.setEventListenerBreakpoint was replaced by DOMDebugger.setEventBreakpoint.
        if (target.hasCommand("DOMDebugger.setEventListenerBreakpoint")) {
            console.assert(breakpoint.type === WI.EventBreakpoint.Type.Listener);

            if (!this._restoringBreakpoints && !WI.debuggerManager.breakpointsDisabledTemporarily)
                WI.debuggerManager.breakpointsEnabled = true;

            target.DOMDebuggerAgent.setEventListenerBreakpoint(breakpoint.eventName);
            return;
        }

        // COMPATIBILITY (iOS 12.0): DOMDebugger.setEventBreakpoint did not exist yet.
        if (!target.hasCommand("DOMDebugger.setEventBreakpoint"))
            return;

        let commandArguments = this._commandArgumentsForEventBreakpoint(breakpoint);

        if (!this._restoringBreakpoints && !WI.debuggerManager.breakpointsDisabledTemporarily)
            WI.debuggerManager.breakpointsEnabled = true;

        commandArguments.options = breakpoint.optionsToProtocol();

        target.DOMDebuggerAgent.setEventBreakpoint.invoke(commandArguments);
    }

    _removeEventBreakpoint(breakpoint, target)
    {
        // Worker targets do not support `requestAnimationFrame` breakpoints.
        if (breakpoint === this._allAnimationFramesBreakpoint && target.type === WI.TargetType.Worker)
            return;

        // COMPATIBILITY (iOS 10.3): DOMDebugger.removeEventListenerBreakpoint did not exist yet.
        // COMPATIBILITY (iOS 12.0): DOMDebugger.removeEventListenerBreakpoint was replaced by DOMDebugger.removeEventBreakpoint.
        if (target.hasCommand("DOMDebugger.removeEventListenerBreakpoint")) {
            console.assert(breakpoint.type === WI.EventBreakpoint.Type.Listener);
            target.DOMDebuggerAgent.removeEventListenerBreakpoint(breakpoint.eventName);
            return;
        }

        // COMPATIBILITY (iOS 12.0): DOMDebugger.removeEventBreakpoint did not exist yet.
        if (!target.hasCommand("DOMDebugger.removeEventBreakpoint"))
            return;

        let commandArguments = this._commandArgumentsForEventBreakpoint(breakpoint);

        target.DOMDebuggerAgent.removeEventBreakpoint.invoke(commandArguments);
    }

    _setURLBreakpoint(breakpoint, target)
    {
        console.assert(!breakpoint.disabled, breakpoint);

        // COMPATIBILITY (iOS 10.3): DOMDebugger.setXHRBreakpoint did not exist yet.
        // COMPATIBILITY (iOS 12.2): DOMDebugger.setXHRBreakpoint was replaced by DOMDebugger.setURLBreakpoint.
        if (target.hasCommand("DOMDebugger.setXHRBreakpoint")) {
            if (!this._restoringBreakpoints && !WI.debuggerManager.breakpointsDisabledTemporarily)
                WI.debuggerManager.breakpointsEnabled = true;

            let isRegex = breakpoint.type === WI.URLBreakpoint.Type.RegularExpression;
            target.DOMDebuggerAgent.setXHRBreakpoint(breakpoint.url, isRegex);
            return;
        }

        // COMPATIBILITY (iOS 12.2): DOMDebugger.setURLBreakpoint did not exist yet.
        if (!target.hasCommand("DOMDebugger.setURLBreakpoint"))
            return;

        if (!this._restoringBreakpoints && !WI.debuggerManager.breakpointsDisabledTemporarily)
            WI.debuggerManager.breakpointsEnabled = true;

        target.DOMDebuggerAgent.setURLBreakpoint.invoke({
            url: breakpoint.url,
            isRegex: breakpoint.type === WI.URLBreakpoint.Type.RegularExpression,
            options: breakpoint.optionsToProtocol(),
        });
    }

    _removeURLBreakpoint(breakpoint, target)
    {
        // COMPATIBILITY (iOS 10.3): DOMDebugger.removeXHRBreakpoint did not exist yet.
        // COMPATIBILITY (iOS 12.2): DOMDebugger.removeXHRBreakpoint was replaced by DOMDebugger.setURLBreakpoint.
        if (target.hasCommand("DOMDebugger.removeXHRBreakpoint")) {
            target.DOMDebuggerAgent.removeXHRBreakpoint(breakpoint.url);
            return;
        }

        // COMPATIBILITY (iOS 12.2): DOMDebugger.removeURLBreakpoint did not exist yet.
        if (!target.hasCommand("DOMDebugger.removeURLBreakpoint"))
            return;

        target.DOMDebuggerAgent.removeURLBreakpoint.invoke({
            url: breakpoint.url,
            isRegex: breakpoint.type === WI.URLBreakpoint.Type.RegularExpression,
        });
    }

    _handleDOMBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;
        let target = WI.assumingMainTarget();
        if (target)
            this._updateDOMBreakpoint(breakpoint, target);

        if (!this._restoringBreakpoints)
            WI.objectStores.domBreakpoints.putObject(breakpoint);
    }

    _handleEventBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;

        // Specific event listener breakpoints are handled by `DOMManager`.
        if (breakpoint.eventListener)
            return;

        for (let target of WI.targets) {
            if (breakpoint.disabled)
                this._removeEventBreakpoint(breakpoint, target);
            else
                this._setEventBreakpoint(breakpoint, target);
        }

        if (!this._restoringBreakpoints)
            WI.objectStores.eventBreakpoints.putObject(breakpoint);
    }

    _handleEventBreakpointEditablePropertyChanged(event)
    {
        let breakpoint = event.target;

        // Specific event listener breakpoints are handled by `DOMManager`.
        if (breakpoint.eventListener)
            return;

        if (!this._restoringBreakpoints)
            WI.objectStores.eventBreakpoints.putObject(breakpoint);

        if (breakpoint.disabled)
            return;

        this._restoringBreakpoints = true;
        for (let target of WI.targets) {
            // Clear the old breakpoint from the backend before setting the new one.
            this._removeEventBreakpoint(breakpoint, target);
            this._setEventBreakpoint(breakpoint, target);
        }
        this._restoringBreakpoints = false;
    }

    _handleEventBreakpointActionsChanged(event)
    {
        let breakpoint = event.target;

        // Specific event listener breakpoints are handled by `DOMManager`.
        if (breakpoint.eventListener)
            return;

        this._handleEventBreakpointEditablePropertyChanged(event);

        WI.debuggerManager.updateProbesForBreakpoint(breakpoint);
    }

    _handleURLBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;

        for (let target of WI.targets) {
            if (breakpoint.disabled)
                this._removeURLBreakpoint(breakpoint, target);
            else
                this._setURLBreakpoint(breakpoint, target);
        }

        if (!this._restoringBreakpoints)
            WI.objectStores.urlBreakpoints.putObject(breakpoint);
    }

    _handleURLBreakpointEditablePropertyChanged(event)
    {
        let breakpoint = event.target;

        if (!this._restoringBreakpoints)
            WI.objectStores.urlBreakpoints.putObject(breakpoint);

        if (breakpoint.disabled)
            return;

        this._restoringBreakpoints = true;
        for (let target of WI.targets) {
            // Clear the old breakpoint from the backend before setting the new one.
            this._removeURLBreakpoint(breakpoint, target)
            this._setURLBreakpoint(breakpoint, target);
        }
        this._restoringBreakpoints = false;
    }

    _handleURLBreakpointActionsChanged(event)
    {
        let breakpoint = event.target;

        this._handleURLBreakpointEditablePropertyChanged(event);

        WI.debuggerManager.updateProbesForBreakpoint(breakpoint);
    }

    _childFrameWasRemoved(event)
    {
        let frame = event.data.childFrame;
        this._detachBreakpointsForFrame(frame);
    }

    _mainFrameDidChange(event)
    {
        this._speculativelyResolveDOMBreakpointsForURL(WI.networkManager.mainFrame.url);
    }

    _mainResourceDidChange(event)
    {
        let frame = event.target;
        if (frame.isMainFrame()) {
            for (let breakpoint of this._domBreakpointURLMap.values())
                breakpoint.domNodeIdentifier = null;

            this._domBreakpointFrameIdentifierMap.clear();
        } else
            this._detachBreakpointsForFrame(frame);

        this._speculativelyResolveDOMBreakpointsForURL(frame.url);
    }

    _nodeInserted(event)
    {
        let node = event.data.node;
        if (node.nodeType() !== Node.ELEMENT_NODE || !node.frame)
            return;

        let url = node.frame.url;
        let breakpoints = this._domBreakpointURLMap.get(url);
        if (!breakpoints)
            return;

        for (let breakpoint of breakpoints) {
            if (breakpoint.domNodeIdentifier)
                continue;

            if (breakpoint.path !== node.path())
                continue;

            this._restoringBreakpoints = true;
            this._resolveDOMBreakpoint(breakpoint, node.id);
            this._restoringBreakpoints = false;
        }
    }

    _nodeRemoved(event)
    {
        let node = event.data.node;
        if (node.nodeType() !== Node.ELEMENT_NODE || !node.frame)
            return;

        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(node.frame.id);
        if (!domBreakpointNodeIdentifierMap)
            return;

        let breakpoints = domBreakpointNodeIdentifierMap.get(node.id);
        if (!breakpoints)
            return;

        domBreakpointNodeIdentifierMap.delete(node.id);

        if (!domBreakpointNodeIdentifierMap.size)
            this._domBreakpointFrameIdentifierMap.delete(node.frame.id);

        for (let breakpoint of breakpoints)
            breakpoint.domNodeIdentifier = null;
    }
};

WI.DOMDebuggerManager.Event = {
    DOMBreakpointAdded: "dom-debugger-manager-dom-breakpoint-added",
    DOMBreakpointRemoved: "dom-debugger-manager-dom-breakpoint-removed",
    EventBreakpointAdded: "dom-debugger-manager-event-breakpoint-added",
    EventBreakpointRemoved: "dom-debugger-manager-event-breakpoint-removed",
    URLBreakpointAdded: "dom-debugger-manager-url-breakpoint-added",
    URLBreakpointRemoved: "dom-debugger-manager-url-breakpoint-removed",
};
