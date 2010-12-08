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
    this._backendIdToBreakpoint = {};

    WebInspector.debuggerModel.addEventListener("native-breakpoint-hit", this._nativeBreakpointHit, this);
    WebInspector.debuggerModel.addEventListener("debugger-resumed", this._debuggerResumed, this);
}

WebInspector.BreakpointManager.prototype = {
    createDOMBreakpoint: function(nodeId, domEventType, disabled)
    {
        var frontendId = "dom:" + nodeId + ":" + domEventType;
        if (frontendId in this._nativeBreakpoints)
            return;

        var breakpoint = new WebInspector.DOMBreakpoint(this, frontendId, nodeId, domEventType);
        this._nativeBreakpoints[frontendId] = breakpoint;
        this.dispatchEventToListeners("dom-breakpoint-added", breakpoint);
        breakpoint.enabled = !disabled;
        return breakpoint;
    },

    createEventListenerBreakpoint: function(eventName)
    {
        var frontendId = eventName;
        if (frontendId in this._nativeBreakpoints)
            return;

        var breakpoint = new WebInspector.EventListenerBreakpoint(this, frontendId, eventName);
        this._nativeBreakpoints[frontendId] = breakpoint;
        this.dispatchEventToListeners("event-listener-breakpoint-added", { breakpoint: breakpoint, eventName: eventName });
        breakpoint.enabled = true;
        return breakpoint;
    },

    createXHRBreakpoint: function(url, disabled)
    {
        var frontendId = url;
        if (frontendId in this._nativeBreakpoints)
            return;

        var breakpoint = new WebInspector.XHRBreakpoint(this, frontendId, url);
        this._nativeBreakpoints[frontendId] = breakpoint;
        this.dispatchEventToListeners("xhr-breakpoint-added", breakpoint);
        breakpoint.enabled = !disabled
        return breakpoint;
    },

    findBreakpoint: function(backendBreakpointId)
    {
        return this._backendIdToBreakpoint[backendBreakpointId];
    },

    _removeNativeBreakpoint: function(breakpoint)
    {
        if (breakpoint._beingSetOnBackend)
            return;
        if (breakpoint.enabled)
            this._removeNativeBreakpointFromBackend(breakpoint);
        delete this._nativeBreakpoints[breakpoint._frontendId];
        this._updateNativeBreakpointsInSettings();
        breakpoint.dispatchEventToListeners("removed");
    },

    _setNativeBreakpointEnabled: function(breakpoint, enabled)
    {
        if (breakpoint._beingSetOnBackend)
            return;
        if (breakpoint.enabled === enabled)
            return;
        if (enabled)
            this._setNativeBreakpointOnBackend(breakpoint);
        else
            this._removeNativeBreakpointFromBackend(breakpoint);
    },

    _setNativeBreakpointOnBackend: function(breakpoint)
    {
        breakpoint._beingSetOnBackend = true;
        var data = { type: breakpoint._type, condition: breakpoint._condition };
        InspectorBackend.setNativeBreakpoint(data, didSetNativeBreakpoint.bind(this));

        function didSetNativeBreakpoint(backendBreakpointId)
        {
            breakpoint._beingSetOnBackend = false;
            if (backendBreakpointId !== "") {
                breakpoint._backendId = backendBreakpointId;
                this._backendIdToBreakpoint[backendBreakpointId] = breakpoint;
            }
            breakpoint.dispatchEventToListeners("enable-changed");
            this._updateNativeBreakpointsInSettings();
        }
    },

    _removeNativeBreakpointFromBackend: function(breakpoint)
    {
        InspectorBackend.removeNativeBreakpoint(breakpoint._backendId);
        delete this._backendIdToBreakpoint[breakpoint._backendId]
        delete breakpoint._backendId;
        breakpoint.dispatchEventToListeners("enable-changed");
        this._updateNativeBreakpointsInSettings();
    },

    _updateNativeBreakpointsInSettings: function()
    {
        var persistentBreakpoints = [];
        for (var id in this._nativeBreakpoints) {
            var breakpoint = this._nativeBreakpoints[id];
            if (breakpoint._persistentCondition)
                persistentBreakpoints.push({ type: breakpoint._type, enabled: breakpoint.enabled, condition: breakpoint._persistentCondition });
        }
        WebInspector.settings.nativeBreakpoints = persistentBreakpoints;
    },

    _nativeBreakpointHit: function(event)
    {
        var breakpointId = event.data.breakpointId;

        var breakpoint = this._backendIdToBreakpoint[breakpointId];
        if (!breakpoint)
            return;

        breakpoint.hit = true;
        breakpoint.dispatchEventToListeners("hit-state-changed");
        this._lastHitBreakpoint = breakpoint;
    },

    _debuggerResumed: function(event)
    {
        if (!this._lastHitBreakpoint)
            return;
        this._lastHitBreakpoint.hit = false;
        this._lastHitBreakpoint.dispatchEventToListeners("hit-state-changed");
        delete this._lastHitBreakpoint;
    },

    restoreBreakpoints: function()
    {
        var breakpoints = this._persistentBreakpoints();
        for (var i = 0; i < breakpoints.length; ++i) {
            if (breakpoints[i].type === "EventListener")
                this.createEventListenerBreakpoint(breakpoints[i].condition.eventName);
            else if (breakpoints[i].type === "XHR")
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
                var breakpoint = breakpoints[i];
                var nodeId = pathToNodeId[breakpoint.condition.path];
                if (nodeId)
                    this.createDOMBreakpoint(nodeId, breakpoint.condition.type, !breakpoint.enabled);
            }
        }

        var breakpoints = this._persistentBreakpoints();
        var pathToNodeId = {};
        var pendingCalls = 0;
        for (var i = 0; i < breakpoints.length; ++i) {
            if (breakpoints[i].type !== "DOM")
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

    reset: function()
    {
        this._nativeBreakpoints = {};
        this._backendIdToBreakpoint = {};
    }
}

WebInspector.BreakpointManager.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.NativeBreakpoint = function(manager, frontendId, type)
{
    this._manager = manager;
    this.__frontendId = frontendId;
    this.__type = type;
}

WebInspector.NativeBreakpoint.prototype = {
    get enabled()
    {
        return "_backendId" in this;
    },

    set enabled(enabled)
    {
        this._manager._setNativeBreakpointEnabled(this, enabled);
    },

    remove: function()
    {
        this._manager._removeNativeBreakpoint(this);
        this._onRemove();
    },

    get _frontendId()
    {
        return this.__frontendId;
    },

    get _type()
    {
        return this.__type;
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

WebInspector.DOMBreakpoint = function(manager, frontendId, nodeId, domEventType)
{
    WebInspector.NativeBreakpoint.call(this, manager, frontendId, "DOM");
    this._nodeId = nodeId;
    this._domEventType = domEventType;
    this._condition = { nodeId: this._nodeId, type: this._domEventType };

    var node = WebInspector.domAgent.nodeForId(this._nodeId);
    if (node) {
        node.breakpoints[this._domEventType] = this;
        this._persistentCondition = { path: node.path(), type: this._domEventType };
    }
}

WebInspector.DOMBreakpoint.prototype = {
    compareTo: function(other)
    {
        return this._compare(this._domEventType, other._domEventType);
    },

    populateLabelElement: function(element)
    {
        // FIXME: this should belong to the view, not the manager.
        var linkifiedNode = WebInspector.panels.elements.linkifyNodeById(this._nodeId);
        linkifiedNode.addStyleClass("monospace");
        element.appendChild(linkifiedNode);
        var description = document.createElement("div");
        description.className = "source-text";
        description.textContent = WebInspector.domBreakpointTypeLabel(this._domEventType);
        element.appendChild(description);
    },

    populateStatusMessageElement: function(element, eventData)
    {
        var substitutions = [WebInspector.domBreakpointTypeLabel(this._domEventType), WebInspector.panels.elements.linkifyNodeById(this._nodeId)];
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
        if (this._domEventType === WebInspector.DOMBreakpointTypes.SubtreeModified) {
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

    _onRemove: function()
    {
        var node = WebInspector.domAgent.nodeForId(this._nodeId);
        if (node)
            delete node.breakpoints[this._domEventType];
    }
}

WebInspector.DOMBreakpoint.prototype.__proto__ = WebInspector.NativeBreakpoint.prototype;

WebInspector.EventListenerBreakpoint = function(manager, frontendId, eventName)
{
    WebInspector.NativeBreakpoint.call(this, manager, frontendId, "EventListener");
    this._eventName = eventName;
    this._condition = { eventName: this._eventName };
    this._persistentCondition = this._condition;
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
    }
}

WebInspector.EventListenerBreakpoint.prototype.__proto__ = WebInspector.NativeBreakpoint.prototype;

WebInspector.XHRBreakpoint = function(manager, frontendId, url)
{
    WebInspector.NativeBreakpoint.call(this, manager, frontendId, "XHR");
    this._url = url;
    this._condition = { url: this._url };
    this._persistentCondition = this._condition;
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
