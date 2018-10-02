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

        this._domBreakpointsSetting = new WI.Setting("dom-breakpoints", []);
        this._domBreakpointURLMap = new Map;
        this._domBreakpointFrameIdentifierMap = new Map;

        this._eventBreakpointSetting = new WI.Setting("event-breakpoints", []);
        this._eventBreakpoints = [];

        this._xhrBreakpointsSetting = new WI.Setting("xhr-breakpoints", []);
        this._xhrBreakpoints = [];
        this._allRequestsBreakpointEnabledSetting = new WI.Setting("break-on-all-requests", false);

        this._allRequestsBreakpoint = new WI.XHRBreakpoint(null, null, !this._allRequestsBreakpointEnabledSetting.value);

        WI.DOMBreakpoint.addEventListener(WI.DOMBreakpoint.Event.DisabledStateDidChange, this._domBreakpointDisabledStateDidChange, this);
        WI.EventBreakpoint.addEventListener(WI.EventBreakpoint.Event.DisabledStateDidChange, this._eventBreakpointDisabledStateDidChange, this);
        WI.XHRBreakpoint.addEventListener(WI.XHRBreakpoint.Event.DisabledStateDidChange, this._xhrBreakpointDisabledStateDidChange, this);

        WI.domManager.addEventListener(WI.DOMManager.Event.NodeRemoved, this._nodeRemoved, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.NodeInserted, this._nodeInserted, this);

        WI.networkManager.addEventListener(WI.NetworkManager.Event.MainFrameDidChange, this._mainFrameDidChange, this);

        WI.Frame.addEventListener(WI.Frame.Event.ChildFrameWasRemoved, this._childFrameWasRemoved, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        if (this.supported) {
            this._restoringBreakpoints = true;

            for (let cookie of this._domBreakpointsSetting.value) {
                let breakpoint = new WI.DOMBreakpoint(cookie, cookie.type, cookie.disabled);
                this.addDOMBreakpoint(breakpoint);
            }

            for (let payload of this._eventBreakpointSetting.value)
                this.addEventBreakpoint(WI.EventBreakpoint.fromPayload(payload));

            for (let cookie of this._xhrBreakpointsSetting.value) {
                let breakpoint = new WI.XHRBreakpoint(cookie.type, cookie.url, cookie.disabled);
                this.addXHRBreakpoint(breakpoint);
            }

            this._restoringBreakpoints = false;
            this._speculativelyResolveBreakpoints();

            if (!this._allRequestsBreakpoint.disabled)
                this._updateXHRBreakpoint(this._allRequestsBreakpoint);
        }
    }

    // Static

    static supportsEventBreakpoints()
    {
        return DOMDebuggerAgent.setEventBreakpoint && DOMDebuggerAgent.removeEventBreakpoint;
    }

    // Public

    get supported()
    {
        return !!window.DOMDebuggerAgent;
    }

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
            if (domBreakpointNodeIdentifierMap) {
                for (let breakpoints of domBreakpointNodeIdentifierMap.values())
                    resolvedBreakpoints = resolvedBreakpoints.concat(breakpoints);
            }

            frames.push(...frame.childFrameCollection);
        }

        return resolvedBreakpoints;
    }

    get eventBreakpoints() { return this._eventBreakpoints; }

    get xhrBreakpoints() { return this._xhrBreakpoints; }

    isBreakpointSpecial(breakpoint)
    {
        return breakpoint === this._allRequestsBreakpoint;
    }

    domBreakpointsForNode(node)
    {
        console.assert(node instanceof WI.DOMNode);

        if (!node)
            return [];

        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(node.frameIdentifier);
        if (!domBreakpointNodeIdentifierMap)
            return [];

        let breakpoints = domBreakpointNodeIdentifierMap.get(node.id);
        return breakpoints ? breakpoints.slice() : [];
    }

    addDOMBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.DOMBreakpoint);
        if (!breakpoint || !breakpoint.url)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointAdded, {breakpoint});
            return;
        }

        let breakpoints = this._domBreakpointURLMap.get(breakpoint.url);
        if (!breakpoints) {
            breakpoints = [breakpoint];
            this._domBreakpointURLMap.set(breakpoint.url, breakpoints);
        } else
            breakpoints.push(breakpoint);

        if (breakpoint.domNodeIdentifier)
            this._resolveDOMBreakpoint(breakpoint, breakpoint.domNodeIdentifier);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointAdded, {breakpoint});

        this._saveDOMBreakpoints();
    }

    removeDOMBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.DOMBreakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            breakpoint.disabled = true;
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, {breakpoint});
            return;
        }

        let nodeIdentifier = breakpoint.domNodeIdentifier;
        console.assert(nodeIdentifier, "Cannot remove unresolved DOM breakpoint.");
        if (!nodeIdentifier)
            return;

        this._detachDOMBreakpoint(breakpoint);

        let urlBreakpoints = this._domBreakpointURLMap.get(breakpoint.url);
        urlBreakpoints.remove(breakpoint, true);

        if (!breakpoint.disabled)
            DOMDebuggerAgent.removeDOMBreakpoint(nodeIdentifier, breakpoint.type);

        if (!urlBreakpoints.length)
            this._domBreakpointURLMap.delete(breakpoint.url);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, {breakpoint});

        breakpoint.domNodeIdentifier = null;

        this._saveDOMBreakpoints();
    }

    removeDOMBreakpointsForNode(node)
    {
        this._restoringBreakpoints = true;

        this.domBreakpointsForNode(node).forEach(this.removeDOMBreakpoint, this);

        this._restoringBreakpoints = false;
        this._saveDOMBreakpoints();
    }

    eventBreakpointForTypeAndEventName(type, eventName)
    {
        return this._eventBreakpoints.find((breakpoint) => breakpoint.type === type && breakpoint.eventName === eventName) || null;
    }

    addEventBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.EventBreakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointAdded, {breakpoint});
            return;
        }

        if (this.eventBreakpointForTypeAndEventName(breakpoint.type, breakpoint.eventName))
            return;

        this._eventBreakpoints.push(breakpoint);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointAdded, {breakpoint});

        this._resolveEventBreakpoint(breakpoint);
        this._saveEventBreakpoints();
    }

    removeEventBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.EventBreakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            breakpoint.disabled = true;
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, {breakpoint});
            return;
        }

        if (!this._eventBreakpoints.includes(breakpoint))
            return;

        this._eventBreakpoints.remove(breakpoint);

        this._saveEventBreakpoints();
        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, {breakpoint});

        if (breakpoint.disabled)
            return;

        function breakpointRemoved(error) {
            if (error)
                console.error(error);
        }

        // Compatibility (iOS 12): DOMDebuggerAgent.removeEventBreakpoint did not exist.
        if (!WI.DOMDebuggerManager.supportsEventBreakpoints()) {
            console.assert(breakpoint.type === WI.EventBreakpoint.Type.Listener);
            DOMDebuggerAgent.removeEventListenerBreakpoint(breakpoint.eventName, breakpointRemoved);
            return;
        }

        DOMDebuggerAgent.removeEventBreakpoint(breakpoint.type, breakpoint.eventName, breakpointRemoved);
    }

    xhrBreakpointForURL(url)
    {
        return this._xhrBreakpoints.find((breakpoint) => breakpoint.url === url) || null;
    }

    addXHRBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.XHRBreakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.XHRBreakpointAdded, {breakpoint});
            return;
        }

        console.assert(!this._xhrBreakpoints.includes(breakpoint), "Already added XHR breakpoint.", breakpoint);
        if (this._xhrBreakpoints.includes(breakpoint))
            return;

        if (this._xhrBreakpoints.some((entry) => entry.type === breakpoint.type && entry.url === breakpoint.url))
            return;

        this._xhrBreakpoints.push(breakpoint);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.XHRBreakpointAdded, {breakpoint});

        this._resolveXHRBreakpoint(breakpoint);
        this._saveXHRBreakpoints();
    }

    removeXHRBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.XHRBreakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            breakpoint.disabled = true;
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.XHRBreakpointRemoved, {breakpoint});
            return;
        }

        if (!this._xhrBreakpoints.includes(breakpoint))
            return;

        this._xhrBreakpoints.remove(breakpoint, true);

        this._saveXHRBreakpoints();
        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.XHRBreakpointRemoved, {breakpoint});

        if (breakpoint.disabled)
            return;

        DOMDebuggerAgent.removeXHRBreakpoint(breakpoint.url, (error) => {
            if (error)
                console.error(error);
        });
    }

    // Private

    _detachDOMBreakpoint(breakpoint)
    {
        let nodeIdentifier = breakpoint.domNodeIdentifier;
        let node = WI.domManager.nodeForId(nodeIdentifier);
        console.assert(node, "Missing DOM node for breakpoint.", breakpoint);
        if (!node)
            return;

        let frameIdentifier = node.frameIdentifier;
        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(frameIdentifier);
        console.assert(domBreakpointNodeIdentifierMap, "Missing DOM breakpoints for node parent frame.", node);
        if (!domBreakpointNodeIdentifierMap)
            return;

        let breakpoints = domBreakpointNodeIdentifierMap.get(nodeIdentifier);
        console.assert(breakpoints, "Missing DOM breakpoints for node.", node);
        if (!breakpoints)
            return;

        breakpoints.remove(breakpoint, true);

        if (breakpoints.length)
            return;

        domBreakpointNodeIdentifierMap.delete(nodeIdentifier);

        if (!domBreakpointNodeIdentifierMap.size)
            this._domBreakpointFrameIdentifierMap.delete(frameIdentifier);
    }

    _detachBreakpointsForFrame(frame)
    {
        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(frame.id);
        if (!domBreakpointNodeIdentifierMap)
            return;

        this._domBreakpointFrameIdentifierMap.delete(frame.id);

        for (let breakpoints of domBreakpointNodeIdentifierMap.values()) {
            for (let breakpoint of breakpoints)
                breakpoint.domNodeIdentifier = null;
        }
    }

    _speculativelyResolveBreakpoints()
    {
        let mainFrame = WI.networkManager.mainFrame;
        if (!mainFrame)
            return;

        let domBreakpoints = this._domBreakpointURLMap.get(mainFrame.url);
        if (domBreakpoints) {
            for (let breakpoint of domBreakpoints) {
                if (breakpoint.domNodeIdentifier)
                    continue;

                WI.domManager.pushNodeByPathToFrontend(breakpoint.path, (nodeIdentifier) => {
                    if (!nodeIdentifier)
                        return;

                    this._resolveDOMBreakpoint(breakpoint, nodeIdentifier);
                });
            }
        }

        for (let breakpoint of this._eventBreakpoints)
            this._resolveEventBreakpoint(breakpoint);

        for (let breakpoint of this._xhrBreakpoints)
            this._resolveXHRBreakpoint(breakpoint);
    }

    _resolveDOMBreakpoint(breakpoint, nodeIdentifier)
    {
        let node = WI.domManager.nodeForId(nodeIdentifier);
        console.assert(node, "Missing DOM node for nodeIdentifier.", nodeIdentifier);
        if (!node)
            return;

        let frameIdentifier = node.frameIdentifier;
        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(frameIdentifier);
        if (!domBreakpointNodeIdentifierMap) {
            domBreakpointNodeIdentifierMap = new Map;
            this._domBreakpointFrameIdentifierMap.set(frameIdentifier, domBreakpointNodeIdentifierMap);
        }

        let breakpoints = domBreakpointNodeIdentifierMap.get(nodeIdentifier);
        if (breakpoints)
            breakpoints.push(breakpoint);
        else
            domBreakpointNodeIdentifierMap.set(nodeIdentifier, [breakpoint]);

        breakpoint.domNodeIdentifier = nodeIdentifier;

        this._updateDOMBreakpoint(breakpoint);
    }

    _updateDOMBreakpoint(breakpoint)
    {
        let nodeIdentifier = breakpoint.domNodeIdentifier;
        if (!nodeIdentifier)
            return;

        function breakpointUpdated(error)
        {
            if (error)
                console.error(error);
        }

        if (breakpoint.disabled)
            DOMDebuggerAgent.removeDOMBreakpoint(nodeIdentifier, breakpoint.type, breakpointUpdated);
        else
            DOMDebuggerAgent.setDOMBreakpoint(nodeIdentifier, breakpoint.type, breakpointUpdated);
    }

    _updateEventBreakpoint(breakpoint, callback)
    {
        function breakpointUpdated(error)
        {
            if (error)
                console.error(error);

            if (callback)
                callback(error);
        }

        // Compatibility (iOS 12): DOMDebuggerAgent.removeEventBreakpoint did not exist.
        if (!WI.DOMDebuggerManager.supportsEventBreakpoints()) {
            console.assert(breakpoint.type === WI.EventBreakpoint.Type.Listener);
            if (breakpoint.disabled)
                DOMDebuggerAgent.removeEventListenerBreakpoint(breakpoint.eventName, breakpointUpdated);
            else
                DOMDebuggerAgent.setEventListenerBreakpoint(breakpoint.eventName, breakpointUpdated);
            return;
        }

        if (breakpoint.disabled)
            DOMDebuggerAgent.removeEventBreakpoint(breakpoint.type, breakpoint.eventName, breakpointUpdated);
        else
            DOMDebuggerAgent.setEventBreakpoint(breakpoint.type, breakpoint.eventName, breakpointUpdated);
    }

    _updateXHRBreakpoint(breakpoint, callback)
    {
        function breakpointUpdated(error)
        {
            if (error)
                console.error(error);

            if (callback && typeof callback === "function")
                callback(error);
        }

        if (breakpoint.disabled)
            DOMDebuggerAgent.removeXHRBreakpoint(breakpoint.url, breakpointUpdated);
        else {
            let isRegex = breakpoint.type === WI.XHRBreakpoint.Type.RegularExpression;
            DOMDebuggerAgent.setXHRBreakpoint(breakpoint.url, isRegex, breakpointUpdated);
        }
    }

    _resolveEventBreakpoint(breakpoint)
    {
        if (breakpoint.disabled)
            return;

        this._updateEventBreakpoint(breakpoint, () => {
            breakpoint.dispatchEventToListeners(WI.EventBreakpoint.Event.ResolvedStateDidChange);
        });
    }

    _resolveXHRBreakpoint(breakpoint)
    {
        if (breakpoint.disabled)
            return;

        this._updateXHRBreakpoint(breakpoint, () => {
            breakpoint.dispatchEventToListeners(WI.XHRBreakpoint.Event.ResolvedStateDidChange);
        });
    }

    _saveDOMBreakpoints()
    {
        if (this._restoringBreakpoints)
            return;

        let breakpointsToSave = [];
        for (let breakpoints of this._domBreakpointURLMap.values())
            breakpointsToSave = breakpointsToSave.concat(breakpoints);

        this._domBreakpointsSetting.value = breakpointsToSave.map((breakpoint) => breakpoint.serializableInfo);
    }

    _saveEventBreakpoints()
    {
        if (this._restoringBreakpoints)
            return;

        this._eventBreakpointSetting.value = this._eventBreakpoints.map((breakpoint) => breakpoint.serializableInfo);
    }

    _saveXHRBreakpoints()
    {
        if (this._restoringBreakpoints)
            return;

        this._xhrBreakpointsSetting.value = this._xhrBreakpoints.map((breakpoint) => breakpoint.serializableInfo);
    }

    _domBreakpointDisabledStateDidChange(event)
    {
        let breakpoint = event.target;
        this._updateDOMBreakpoint(breakpoint);
        this._saveDOMBreakpoints();
    }

    _eventBreakpointDisabledStateDidChange(event)
    {
        let breakpoint = event.target;
        this._updateEventBreakpoint(breakpoint);
        this._saveEventBreakpoints();
    }

    _xhrBreakpointDisabledStateDidChange(event)
    {
        let breakpoint = event.target;

        if (breakpoint === this._allRequestsBreakpoint)
            this._allRequestsBreakpointEnabledSetting.value = !breakpoint.disabled;

        this._updateXHRBreakpoint(breakpoint);
        this._saveXHRBreakpoints();
    }

    _childFrameWasRemoved(event)
    {
        let frame = event.data.childFrame;
        this._detachBreakpointsForFrame(frame);
    }

    _mainFrameDidChange()
    {
        this._speculativelyResolveBreakpoints();
    }

    _mainResourceDidChange(event)
    {
        let frame = event.target;
        if (frame.isMainFrame()) {
            for (let breakpoints of this._domBreakpointURLMap.values())
                breakpoints.forEach((breakpoint) => { breakpoint.domNodeIdentifier = null; });

            this._domBreakpointFrameIdentifierMap.clear();
        } else
            this._detachBreakpointsForFrame(frame);

        this._speculativelyResolveBreakpoints();
    }

    _nodeInserted(event)
    {
        let node = event.data.node;
        if (node.nodeType() !== Node.ELEMENT_NODE || !node.ownerDocument)
            return;

        let url = node.ownerDocument.documentURL;
        let breakpoints = this._domBreakpointURLMap.get(url);
        if (!breakpoints)
            return;

        for (let breakpoint of breakpoints) {
            if (breakpoint.domNodeIdentifier)
                continue;

            if (breakpoint.path !== node.path())
                continue;

            this._resolveDOMBreakpoint(breakpoint, node.id);
        }
    }

    _nodeRemoved(event)
    {
        let node = event.data.node;
        if (node.nodeType() !== Node.ELEMENT_NODE || !node.ownerDocument)
            return;

        let domBreakpointNodeIdentifierMap = this._domBreakpointFrameIdentifierMap.get(node.frameIdentifier);
        if (!domBreakpointNodeIdentifierMap)
            return;

        let breakpoints = domBreakpointNodeIdentifierMap.get(node.id);
        if (!breakpoints)
            return;

        domBreakpointNodeIdentifierMap.delete(node.id);

        if (!domBreakpointNodeIdentifierMap.size)
            this._domBreakpointFrameIdentifierMap.delete(node.frameIdentifier);

        for (let breakpoint of breakpoints)
            breakpoint.domNodeIdentifier = null;
    }
};

WI.DOMDebuggerManager.Event = {
    DOMBreakpointAdded: "dom-debugger-manager-dom-breakpoint-added",
    DOMBreakpointRemoved: "dom-debugger-manager-dom-breakpoint-removed",
    EventBreakpointAdded: "dom-debugger-manager-event-breakpoint-added",
    EventBreakpointRemoved: "dom-debugger-manager-event-breakpoint-removed",
    XHRBreakpointAdded: "dom-debugger-manager-xhr-breakpoint-added",
    XHRBreakpointRemoved: "dom-debugger-manager-xhr-breakpoint-removed",
};
