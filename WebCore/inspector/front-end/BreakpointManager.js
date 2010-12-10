/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.BreakpointManager = function()
{
    this._nativeBreakpoints = {};

    WebInspector.debuggerModel.addEventListener("native-breakpoint-hit", this._nativeBreakpointHit, this);
    WebInspector.debuggerModel.addEventListener("debugger-resumed", this._debuggerResumed, this);
}

WebInspector.BreakpointManager.NativeBreakpointTypes = {
    DOM: "DOM",
    EventListener: "EventListener",
    XHR: "XHR"
}

WebInspector.BreakpointManager.prototype = {
    createDOMBreakpoint: function(nodeId, type, disabled)
    {
        var node = WebInspector.domAgent.nodeForId(nodeId);
        if (!node)
            return;

        var breakpointId = this._createDOMBreakpointId(nodeId, type);
        if (breakpointId in this._nativeBreakpoints)
            return;

        var breakpoint = new WebInspector.DOMBreakpoint(this, breakpointId, !disabled, node, type);
        this._nativeBreakpoints[breakpointId] = breakpoint;
        this._updateNativeBreakpointsInSettings();
        this.dispatchEventToListeners("dom-breakpoint-added", breakpoint);
        return breakpoint;
    },

    createEventListenerBreakpoint: function(eventName)
    {
        var breakpointId = this._createEventListenerBreakpointId(eventName);
        if (breakpointId in this._nativeBreakpoints)
            return;

        var breakpoint = new WebInspector.EventListenerBreakpoint(this, breakpointId, true, eventName);
        this._nativeBreakpoints[breakpointId] = breakpoint;
        this._updateNativeBreakpointsInSettings();
        this.dispatchEventToListeners("event-listener-breakpoint-added", { breakpoint: breakpoint, eventName: eventName });
        return breakpoint;
    },

    createXHRBreakpoint: function(url, disabled)
    {
        var breakpointId = this._createXHRBreakpointId(url);
        if (breakpointId in this._nativeBreakpoints)
            return;

        var breakpoint = new WebInspector.XHRBreakpoint(this, breakpointId, !disabled, url);
        this._nativeBreakpoints[breakpointId] = breakpoint;
        this._updateNativeBreakpointsInSettings();
        this.dispatchEventToListeners("xhr-breakpoint-added", breakpoint);
        return breakpoint;
    },

    _setNativeBreakpointEnabled: function(breakpointId, enabled)
    {
        var breakpoint = this._nativeBreakpoints[breakpointId];

        if (enabled)
            breakpoint._enable();
        else
            breakpoint._disable();

        breakpoint._enabled = enabled;
        this._updateNativeBreakpointsInSettings();
        breakpoint.dispatchEventToListeners("enable-changed");
    },

    _removeNativeBreakpoint: function(breakpointId)
    {
        var breakpoint = this._nativeBreakpoints[breakpointId];

        if (breakpoint.enabled)
            breakpoint._disable();

        delete this._nativeBreakpoints[breakpointId];
        this._updateNativeBreakpointsInSettings();
        breakpoint.dispatchEventToListeners("removed");
    },

    _updateNativeBreakpointsInSettings: function()
    {
        var breakpoints = [];
        for (var id in this._nativeBreakpoints) {
            var breakpoint = this._nativeBreakpoints[id];
            breakpoints.push(breakpoint._serializeToJSON());
        }
        WebInspector.settings.nativeBreakpoints = breakpoints;
    },

    _nativeBreakpointHit: function(event)
    {
        var eventData = event.data;

        var breakpointId;
        if (eventData.breakpointType === WebInspector.BreakpointManager.NativeBreakpointTypes.DOM)
            breakpointId = this._createDOMBreakpointId(eventData.nodeId, eventData.type);
        else if (eventData.breakpointType === WebInspector.BreakpointManager.NativeBreakpointTypes.EventListener)
            breakpointId = this._createEventListenerBreakpointId(eventData.eventName);
        else if (eventData.breakpointType === WebInspector.BreakpointManager.NativeBreakpointTypes.XHR)
            breakpointId = this._createXHRBreakpointId(eventData.breakpointURL);

        var breakpoint = this._nativeBreakpoints[breakpointId];
        if (!breakpoint)
            return;

        breakpoint.hit = true;
        this._lastHitBreakpoint = breakpoint;
        this.dispatchEventToListeners("native-breakpoint-hit", { breakpoint: breakpoint, eventData: eventData });
    },

    _debuggerResumed: function(event)
    {
        if (!this._lastHitBreakpoint)
            return;
        this._lastHitBreakpoint.hit = false;
        delete this._lastHitBreakpoint;
    },

    restoreBreakpoints: function()
    {
        var breakpoints = this._persistentBreakpoints();
        this._domBreakpoints = [];
        for (var i = 0; i < breakpoints.length; ++i) {
            if (breakpoints[i].type === WebInspector.BreakpointManager.NativeBreakpointTypes.DOM)
                this._domBreakpoints.push(breakpoints[i]);
            else if (breakpoints[i].type === WebInspector.BreakpointManager.NativeBreakpointTypes.EventListener)
                this.createEventListenerBreakpoint(breakpoints[i].condition.eventName);
            else if (breakpoints[i].type === WebInspector.BreakpointManager.NativeBreakpointTypes.XHR)
                this.createXHRBreakpoint(breakpoints[i].condition.url, !breakpoints[i].enabled);
        }
    },

    restoreDOMBreakpoints: function()
    {
        function didPushNodeByPathToFrontend(path, nodeId)
        {
            pathToNodeId[path] = nodeId;
            pendingCalls -= 1;
            if (pendingCalls)
                return;
            for (var i = 0; i < breakpoints.length; ++i) {
                if (breakpoints[i].type !== WebInspector.BreakpointManager.NativeBreakpointTypes.DOM)
                    continue;
                var breakpoint = breakpoints[i];
                var nodeId = pathToNodeId[breakpoint.condition.path];
                if (nodeId)
                    this.createDOMBreakpoint(nodeId, breakpoint.condition.type, !breakpoint.enabled);
            }
        }

        var breakpoints = this._domBreakpoints;
        var pathToNodeId = {};
        var pendingCalls = 0;
        for (var i = 0; i < breakpoints.length; ++i) {
            if (breakpoints[i].type !== WebInspector.BreakpointManager.NativeBreakpointTypes.DOM)
                continue;
            var path = breakpoints[i].condition.path;
            if (path in pathToNodeId)
                continue;
            pathToNodeId[path] = 0;
            pendingCalls += 1;
            InspectorBackend.pushNodeByPathToFrontend(path, didPushNodeByPathToFrontend.bind(this, path));
        }
    },

    _persistentBreakpoints: function()
    {
        var result = [];
        var breakpoints = WebInspector.settings.nativeBreakpoints;
        if (breakpoints instanceof Array) {
            for (var i = 0; i < breakpoints.length; ++i) {
                var breakpoint = breakpoints[i];
                if ("type" in breakpoint && "condition" in breakpoint)
                    result.push(breakpoint)
            }
        }
        return result;
    },

    _createDOMBreakpointId: function(nodeId, type)
    {
        return "dom:" + nodeId + ":" + type;
    },

    _createEventListenerBreakpointId: function(eventName)
    {
        return "eventListner:" + eventName;
    },

    _createXHRBreakpointId: function(url)
    {
        return "xhr:" + url;
    },

    reset: function()
    {
        this._nativeBreakpoints = {};
    }
}

WebInspector.BreakpointManager.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.NativeBreakpoint = function(manager, id, enabled)
{
    this._manager = manager;
    this._id = id;
    this._enabled = enabled;
    this._hit = false;
}

WebInspector.NativeBreakpoint.prototype = {
    get enabled()
    {
        return this._enabled;
    },

    set enabled(enabled)
    {
        this._manager._setNativeBreakpointEnabled(this._id, enabled);
    },

    get hit()
    {
        return this._hit;
    },

    set hit(hit)
    {
        this._hit = hit;
        this.dispatchEventToListeners("hit-state-changed");
    },

    remove: function()
    {
        this._manager._removeNativeBreakpoint(this._id);
        this._onRemove();
    },

    _compare: function(x, y)
    {
        if (x !== y)
            return x < y ? -1 : 1;
        return 0;
    },

    _onRemove: function()
    {
    }
}

WebInspector.NativeBreakpoint.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.DOMBreakpoint = function(manager, id, enabled, node, type)
{
    WebInspector.NativeBreakpoint.call(this, manager, id, enabled);
    this._node = node;
    this._nodeId = node.id;
    this._path = node.path();
    this._type = type;
    if (enabled)
        this._enable();

    node.breakpoints[this._type] = this;
}

WebInspector.DOMBreakpoint.prototype = {
    compareTo: function(other)
    {
        return this._compare(this._type, other._type);
    },

    populateLabelElement: function(element)
    {
        // FIXME: this should belong to the view, not the manager.
        var linkifiedNode = WebInspector.panels.elements.linkifyNodeById(this._nodeId);
        linkifiedNode.addStyleClass("monospace");
        element.appendChild(linkifiedNode);
        var description = document.createElement("div");
        description.className = "source-text";
        description.textContent = WebInspector.domBreakpointTypeLabel(this._type);
        element.appendChild(description);
    },

    populateStatusMessageElement: function(element, eventData)
    {
        var substitutions = [WebInspector.domBreakpointTypeLabel(this._type), WebInspector.panels.elements.linkifyNodeById(this._nodeId)];
        var formatters = {
            s: function(substitution)
            {
                return substitution;
            }
        };
        function append(a, b)
        {
            if (typeof b === "string")
                b = document.createTextNode(b);
            element.appendChild(b);
        }
        if (this._type === WebInspector.DOMBreakpointTypes.SubtreeModified) {
            var targetNode = WebInspector.panels.elements.linkifyNodeById(eventData.targetNodeId);
            if (eventData.insertion) {
                if (eventData.targetNodeId !== this._nodeId)
                    WebInspector.formatLocalized("Paused on a \"%s\" breakpoint set on %s, because a new child was added to its descendant %s.", substitutions.concat(targetNode), formatters, "", append);
                else
                    WebInspector.formatLocalized("Paused on a \"%s\" breakpoint set on %s, because a new child was added to that node.", substitutions, formatters, "", append);
            } else
                WebInspector.formatLocalized("Paused on a \"%s\" breakpoint set on %s, because its descendant %s was removed.", substitutions.concat(targetNode), formatters, "", append);
        } else
            WebInspector.formatLocalized("Paused on a \"%s\" breakpoint set on %s.", substitutions, formatters, "", append);
    },

    _enable: function()
    {
        InspectorBackend.setDOMBreakpoint(this._nodeId, this._type);
    },

    _disable: function()
    {
        InspectorBackend.removeDOMBreakpoint(this._nodeId, this._type);
    },

    _serializeToJSON: function()
    {
        var type = WebInspector.BreakpointManager.NativeBreakpointTypes.DOM;
        return { type: type, enabled: this._enabled, condition: { path: this._path, type: this._type } };
    },

    _onRemove: function()
    {
        delete this._node.breakpoints[this._type];
    }
}

WebInspector.DOMBreakpoint.prototype.__proto__ = WebInspector.NativeBreakpoint.prototype;

WebInspector.EventListenerBreakpoint = function(manager, id, enabled, eventName)
{
    WebInspector.NativeBreakpoint.call(this, manager, id, enabled);
    this._eventName = eventName;
    if (enabled)
        this._enable();
}

WebInspector.EventListenerBreakpoint.eventNameForUI = function(eventName)
{
    if (!WebInspector.EventListenerBreakpoint._eventNamesForUI) {
        WebInspector.EventListenerBreakpoint._eventNamesForUI = {
            "instrumentation:setTimer": WebInspector.UIString("Set Timer"),
            "instrumentation:clearTimer": WebInspector.UIString("Clear Timer"),
            "instrumentation:timerFired": WebInspector.UIString("Timer Fired")
        };
    }
    return WebInspector.EventListenerBreakpoint._eventNamesForUI[eventName] || eventName.substring(eventName.indexOf(":") + 1);
}

WebInspector.EventListenerBreakpoint.prototype = {
    compareTo: function(other)
    {
        return this._compare(this._eventName, other._eventName);
    },

    populateLabelElement: function(element)
    {
        element.appendChild(document.createTextNode(this._uiEventName()));
    },

    populateStatusMessageElement: function(element, eventData)
    {
        var status = WebInspector.UIString("Paused on a \"%s\" Event Listener.", this._uiEventName());
        element.appendChild(document.createTextNode(status));
    },

    _uiEventName: function()
    {
        return WebInspector.EventListenerBreakpoint.eventNameForUI(this._eventName);
    },

    _enable: function()
    {
        InspectorBackend.setEventListenerBreakpoint(this._eventName);
    },

    _disable: function()
    {
        InspectorBackend.removeEventListenerBreakpoint(this._eventName);
    },

    _serializeToJSON: function()
    {
        var type = WebInspector.BreakpointManager.NativeBreakpointTypes.EventListener;
        return { type: type, enabled: this._enabled, condition: { eventName: this._eventName } };
    }
}

WebInspector.EventListenerBreakpoint.prototype.__proto__ = WebInspector.NativeBreakpoint.prototype;

WebInspector.XHRBreakpoint = function(manager, id, enabled, url)
{
    WebInspector.NativeBreakpoint.call(this, manager, id, enabled);
    this._url = url;
    if (enabled)
        this._enable();
}

WebInspector.XHRBreakpoint.prototype = {
    compareTo: function(other)
    {
        return this._compare(this._url, other._url);
    },

    populateEditElement: function(element)
    {
        element.textContent = this._url;
    },

    populateLabelElement: function(element)
    {
        var label;
        if (!this._url.length)
            label = WebInspector.UIString("Any XHR");
        else
            label = WebInspector.UIString("URL contains \"%s\"", this._url);
        element.appendChild(document.createTextNode(label));
        element.addStyleClass("cursor-auto");
    },

    populateStatusMessageElement: function(element)
    {
        var status = WebInspector.UIString("Paused on a XMLHttpRequest.");
        element.appendChild(document.createTextNode(status));
    },

    _enable: function()
    {
        InspectorBackend.setXHRBreakpoint(this._url);
    },

    _disable: function()
    {
        InspectorBackend.removeXHRBreakpoint(this._url);
    },

    _serializeToJSON: function()
    {
        var type = WebInspector.BreakpointManager.NativeBreakpointTypes.XHR;
        return { type: type, enabled: this._enabled, condition: { url: this._url } };
    }
}

WebInspector.XHRBreakpoint.prototype.__proto__ = WebInspector.NativeBreakpoint.prototype;

WebInspector.DOMBreakpointTypes = {
    SubtreeModified: 0,
    AttributeModified: 1,
    NodeRemoved: 2
};

WebInspector.domBreakpointTypeLabel = function(type)
{
    if (!WebInspector._DOMBreakpointTypeLabels) {
        WebInspector._DOMBreakpointTypeLabels = {};
        WebInspector._DOMBreakpointTypeLabels[WebInspector.DOMBreakpointTypes.SubtreeModified] = WebInspector.UIString("Subtree Modified");
        WebInspector._DOMBreakpointTypeLabels[WebInspector.DOMBreakpointTypes.AttributeModified] = WebInspector.UIString("Attribute Modified");
        WebInspector._DOMBreakpointTypeLabels[WebInspector.DOMBreakpointTypes.NodeRemoved] = WebInspector.UIString("Node Removed");
    }
    return WebInspector._DOMBreakpointTypeLabels[type];
}

WebInspector.domBreakpointTypeContextMenuLabel = function(type)
{
    if (!WebInspector._DOMBreakpointTypeContextMenuLabels) {
        WebInspector._DOMBreakpointTypeContextMenuLabels = {};
        WebInspector._DOMBreakpointTypeContextMenuLabels[WebInspector.DOMBreakpointTypes.SubtreeModified] = WebInspector.UIString("Break on Subtree Modifications");
        WebInspector._DOMBreakpointTypeContextMenuLabels[WebInspector.DOMBreakpointTypes.AttributeModified] = WebInspector.UIString("Break on Attributes Modifications");
        WebInspector._DOMBreakpointTypeContextMenuLabels[WebInspector.DOMBreakpointTypes.NodeRemoved] = WebInspector.UIString("Break on Node Removal");
    }
    return WebInspector._DOMBreakpointTypeContextMenuLabels[type];
}
