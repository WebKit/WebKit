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

        this._urlBreakpointsSetting = new WI.Setting("url-breakpoints", WI.Setting.migrateValue("xhr-breakpoints") || []);
        this._urlBreakpoints = [];
        this._allRequestsBreakpointEnabledSetting = new WI.Setting("break-on-all-requests", false);

        this._allRequestsBreakpoint = new WI.URLBreakpoint(WI.URLBreakpoint.Type.Text, "", {
            disabled: !this._allRequestsBreakpointEnabledSetting.value,
        });

        WI.DOMBreakpoint.addEventListener(WI.DOMBreakpoint.Event.DisabledStateChanged, this._handleDOMBreakpointDisabledStateChanged, this);
        WI.EventBreakpoint.addEventListener(WI.EventBreakpoint.Event.DisabledStateChanged, this._handleEventBreakpointDisabledStateChanged, this);
        WI.URLBreakpoint.addEventListener(WI.URLBreakpoint.Event.DisabledStateChanged, this._handleURLBreakpointDisabledStateChanged, this);

        WI.domManager.addEventListener(WI.DOMManager.Event.NodeRemoved, this._nodeRemoved, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.NodeInserted, this._nodeInserted, this);

        WI.networkManager.addEventListener(WI.NetworkManager.Event.MainFrameDidChange, this._mainFrameDidChange, this);

        WI.Frame.addEventListener(WI.Frame.Event.ChildFrameWasRemoved, this._childFrameWasRemoved, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        if (this.supported) {
            this._restoringBreakpoints = true;

            for (let serializedInfo of this._domBreakpointsSetting.value)
                this.addDOMBreakpoint(WI.DOMBreakpoint.deserialize(serializedInfo));

            for (let serializedInfo of this._eventBreakpointSetting.value)
                this.addEventBreakpoint(WI.EventBreakpoint.deserialize(serializedInfo));

            for (let serializedInfo of this._urlBreakpointsSetting.value)
                this.addURLBreakpoint(WI.URLBreakpoint.deserialize(serializedInfo));

            this._restoringBreakpoints = false;
            this._speculativelyResolveBreakpoints();

            if (!this._allRequestsBreakpoint.disabled)
                this._updateURLBreakpoint(this._allRequestsBreakpoint);
        }
    }

    // Static

    static supportsEventBreakpoints()
    {
        return DOMDebuggerAgent.setEventBreakpoint && DOMDebuggerAgent.removeEventBreakpoint;
    }

    static supportsURLBreakpoints()
    {
        return InspectorBackend.domains.DOMDebugger.setURLBreakpoint && InspectorBackend.domains.DOMDebugger.removeURLBreakpoint;
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

    get urlBreakpoints() { return this._urlBreakpoints; }

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

    urlBreakpointForURL(url)
    {
        return this._urlBreakpoints.find((breakpoint) => breakpoint.url === url) || null;
    }

    addURLBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.URLBreakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.URLBreakpointAdded, {breakpoint});
            return;
        }

        console.assert(!this._urlBreakpoints.includes(breakpoint), "Already added URL breakpoint.", breakpoint);
        if (this._urlBreakpoints.includes(breakpoint))
            return;

        if (this._urlBreakpoints.some((entry) => entry.type === breakpoint.type && entry.url === breakpoint.url))
            return;

        this._urlBreakpoints.push(breakpoint);

        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.URLBreakpointAdded, {breakpoint});

        this._resolveURLBreakpoint(breakpoint);
        this._saveURLBreakpoints();
    }

    removeURLBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.URLBreakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            breakpoint.disabled = true;
            this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.URLBreakpointRemoved, {breakpoint});
            return;
        }

        if (!this._urlBreakpoints.includes(breakpoint))
            return;

        this._urlBreakpoints.remove(breakpoint, true);

        this._saveURLBreakpoints();
        this.dispatchEventToListeners(WI.DOMDebuggerManager.Event.URLBreakpointRemoved, {breakpoint});

        if (breakpoint.disabled)
            return;

        // Compatibility (iOS 12.1): DOMDebuggerAgent.removeURLBreakpoint did not exist.
        if (WI.DOMDebuggerManager.supportsURLBreakpoints()) {
            DOMDebuggerAgent.removeURLBreakpoint(breakpoint.url, (error) => {
                if (error)
                    console.error(error);
            });
        } else {
            DOMDebuggerAgent.removeXHRBreakpoint(breakpoint.url, (error) => {
                if (error)
                    console.error(error);
            });
        }
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

        for (let breakpoint of this._urlBreakpoints)
            this._resolveURLBreakpoint(breakpoint);
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

    _updateEventBreakpoint(breakpoint)
    {
        // Compatibility (iOS 12): DOMDebuggerAgent.removeEventBreakpoint did not exist.
        if (!WI.DOMDebuggerManager.supportsEventBreakpoints()) {
            console.assert(breakpoint.type === WI.EventBreakpoint.Type.Listener);
            if (breakpoint.disabled)
                DOMDebuggerAgent.removeEventListenerBreakpoint(breakpoint.eventName);
            else
                DOMDebuggerAgent.setEventListenerBreakpoint(breakpoint.eventName);
            return;
        }

        if (breakpoint.disabled)
            DOMDebuggerAgent.removeEventBreakpoint(breakpoint.type, breakpoint.eventName);
        else
            DOMDebuggerAgent.setEventBreakpoint(breakpoint.type, breakpoint.eventName);
    }

    _updateURLBreakpoint(breakpoint)
    {
        // Compatibility (iOS 12.1): DOMDebuggerAgent.removeURLBreakpoint did not exist.
        if (!WI.DOMDebuggerManager.supportsURLBreakpoints()) {
            if (breakpoint.disabled)
                DOMDebuggerAgent.removeXHRBreakpoint(breakpoint.url);
            else {
                let isRegex = breakpoint.type === WI.URLBreakpoint.Type.RegularExpression;
                DebuggerAgent.setXHRBreakpoint(breakpoint.url, isRegex);
            }
            return;
        }

        if (breakpoint.disabled)
            DOMDebuggerAgent.removeURLBreakpoint(breakpoint.url);
        else {
            let isRegex = breakpoint.type === WI.URLBreakpoint.Type.RegularExpression;
            DOMDebuggerAgent.setURLBreakpoint(breakpoint.url, isRegex);
        }
    }

    _resolveEventBreakpoint(breakpoint)
    {
        if (breakpoint.disabled)
            return;

        this._updateEventBreakpoint(breakpoint);
    }

    _resolveURLBreakpoint(breakpoint)
    {
        if (breakpoint.disabled)
            return;

        this._updateURLBreakpoint(breakpoint);
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

    _saveURLBreakpoints()
    {
        if (this._restoringBreakpoints)
            return;

        this._urlBreakpointsSetting.value = this._urlBreakpoints.map((breakpoint) => breakpoint.serializableInfo);
    }

    _handleDOMBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;
        this._updateDOMBreakpoint(breakpoint);
        this._saveDOMBreakpoints();
    }

    _handleEventBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;
        this._updateEventBreakpoint(breakpoint);
        this._saveEventBreakpoints();
    }

    _handleURLBreakpointDisabledStateChanged(event)
    {
        let breakpoint = event.target;

        if (breakpoint === this._allRequestsBreakpoint)
            this._allRequestsBreakpointEnabledSetting.value = !breakpoint.disabled;

        this._updateURLBreakpoint(breakpoint);
        this._saveURLBreakpoints();
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
    URLBreakpointAdded: "dom-debugger-manager-url-breakpoint-added",
    URLBreakpointRemoved: "dom-debugger-manager-url-breakpoint-removed",
};
