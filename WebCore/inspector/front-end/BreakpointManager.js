/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

WebInspector.BreakpointManager = function()
{
    this._breakpoints = {};
    this._xhrBreakpoints = {};
}

WebInspector.BreakpointManager.prototype = {
    setOneTimeBreakpoint: function(sourceID, line)
    {
        var breakpoint = new WebInspector.Breakpoint(this, sourceID, undefined, line, true, undefined);
        if (this._breakpoints[breakpoint.id])
            return;
        if (this._oneTimeBreakpoint)
            InspectorBackend.removeBreakpoint(this._oneTimeBreakpoint.sourceID, this._oneTimeBreakpoint.line);
        this._oneTimeBreakpoint = breakpoint;
        // FIXME(40669): one time breakpoint will be persisted in inspector settings if not hit.
        this._setBreakpointOnBackend(breakpoint, true);
    },

    removeOneTimeBreakpoint: function()
    {
        if (this._oneTimeBreakpoint) {
            InspectorBackend.removeBreakpoint(this._oneTimeBreakpoint.sourceID, this._oneTimeBreakpoint.line);
            delete this._oneTimeBreakpoint;
        }
    },

    setBreakpoint: function(sourceID, url, line, enabled, condition)
    {
        var breakpoint = this._setBreakpoint(sourceID, url, line, enabled, condition);
        if (breakpoint)
            this._setBreakpointOnBackend(breakpoint);
    },

    restoredBreakpoint: function(sourceID, url, line, enabled, condition)
    {
        this._setBreakpoint(sourceID, url, line, enabled, condition);
    },

    breakpointsForSourceID: function(sourceID)
    {
        var breakpoints = [];
        for (var id in this._breakpoints) {
            if (this._breakpoints[id].sourceID === sourceID)
                breakpoints.push(this._breakpoints[id]);
        }
        return breakpoints;
    },

    breakpointsForURL: function(url)
    {
        var breakpoints = [];
        for (var id in this._breakpoints) {
            if (this._breakpoints[id].url === url)
                breakpoints.push(this._breakpoints[id]);
        }
        return breakpoints;
    },

    reset: function()
    {
        this._breakpoints = {};
        delete this._oneTimeBreakpoint;
    },

    _setBreakpoint: function(sourceID, url, line, enabled, condition)
    {
        var breakpoint = new WebInspector.Breakpoint(this, sourceID, url, line, enabled, condition);
        if (this._breakpoints[breakpoint.id])
            return;
        if (this._oneTimeBreakpoint && (this._oneTimeBreakpoint.id == breakpoint.id))
            delete this._oneTimeBreakpoint;
        this._breakpoints[breakpoint.id] = breakpoint;
        breakpoint.addEventListener("removed", this._breakpointRemoved, this);
        this.dispatchEventToListeners("breakpoint-added", breakpoint);
        return breakpoint;
    },

    _breakpointRemoved: function(event)
    {
        delete this._breakpoints[event.target.id];
    },

    _setBreakpointOnBackend: function(breakpoint, isOneTime)
    {
        function didSetBreakpoint(success, line)
        {
            if (success && line == breakpoint.line)
                return;
            if (isOneTime) {
                if (success)
                    this._oneTimeBreakpoint.line = line;
                else
                    delete this._oneTimeBreakpoint;
            } else {
                breakpoint.remove();
                if (success)
                    this._setBreakpoint(breakpoint.sourceID, breakpoint.url, line, breakpoint.enabled, breakpoint.condition);
            }
        }
        InspectorBackend.setBreakpoint(breakpoint.sourceID, breakpoint.line, breakpoint.enabled, breakpoint.condition, didSetBreakpoint.bind(this));
    },

    createXHRBreakpoint: function(url)
    {
        if (url in this._xhrBreakpoints)
            return;
        this._xhrBreakpoints[url] = true;

        var breakpoint = new WebInspector.XHRBreakpoint(url);
        breakpoint.addEventListener("removed", this._xhrBreakpointRemoved.bind(this));
        this.dispatchEventToListeners("xhr-breakpoint-added", breakpoint);
    },

    _xhrBreakpointRemoved: function(event)
    {
        delete this._xhrBreakpoints[event.target.url];
    }
}

WebInspector.BreakpointManager.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.Breakpoint = function(breakpointManager, sourceID, url, line, enabled, condition)
{
    this.url = url;
    this.line = line;
    this.sourceID = sourceID;
    this._enabled = enabled;
    this._condition = condition || "";
    this._sourceText = "";
    this._breakpointManager = breakpointManager;
}

WebInspector.Breakpoint.prototype = {
    get enabled()
    {
        return this._enabled;
    },

    set enabled(x)
    {
        if (this._enabled === x)
            return;

        this._enabled = x;
        this._breakpointManager._setBreakpointOnBackend(this);
        this.dispatchEventToListeners("enable-changed");
    },

    get sourceText()
    {
        return this._sourceText;
    },

    set sourceText(text)
    {
        this._sourceText = text;
        this.dispatchEventToListeners("text-changed");
    },

    get id()
    {
        return this.sourceID + ":" + this.line;
    },

    get condition()
    {
        return this._condition;
    },

    set condition(c)
    {
        c = c || "";
        if (this._condition === c)
            return;

        this._condition = c;
        if (this.enabled)
            this._breakpointManager._setBreakpointOnBackend(this);
        this.dispatchEventToListeners("condition-changed");
    },

    compareTo: function(other)
    {
        if (this.url != other.url)
            return this.url < other.url ? -1 : 1;
        if (this.line != other.line)
            return this.line < other.line ? -1 : 1;
        return 0;
    },

    remove: function()
    {
        InspectorBackend.removeBreakpoint(this.sourceID, this.line);
        this.dispatchEventToListeners("removed");
        this.removeAllListeners();
        delete this._breakpointManager;
    }
}

WebInspector.Breakpoint.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.XHRBreakpoint = function(url)
{
    this._url = url;
    this._locked = false;
    this.enabled = true;
}

WebInspector.XHRBreakpoint.prototype = {
    get enabled()
    {
        return "_id" in this;
    },

    set enabled(enabled)
    {
        if (this._locked)
            return;
        if (this.enabled === enabled)
            return;
        if (enabled)
            this._setOnBackend();
        else
            this._removeFromBackend();
    },

    get url()
    {
        return this._url;
    },

    formatLabel: function()
    {
        var label = "";
        if (!this.url.length)
            label = WebInspector.UIString("Any XHR");
        else
            label = WebInspector.UIString("URL contains \"%s\"", this.url);
        return label;
    },

    compareTo: function(other)
    {
        if (this.url != other.url)
            return this.url < other.url ? -1 : 1;
        return 0;
    },

    remove: function()
    {
        if (this._locked)
            return;
        if (this.enabled)
            this._removeFromBackend();
        this.dispatchEventToListeners("removed");
    },

    _setOnBackend: function()
    {
        this._locked = true;
        var data = { type: "XHR", condition: { url: this.url } };
        InspectorBackend.setNativeBreakpoint(data, didSet.bind(this));

        function didSet(breakpointId)
        {
            this._locked = false;
            this._id = breakpointId;
            this.dispatchEventToListeners("enable-changed");
        }
    },

    _removeFromBackend: function()
    {
        InspectorBackend.removeNativeBreakpoint(this._id);
        delete this._id;
        this.dispatchEventToListeners("enable-changed");
    }
}

WebInspector.XHRBreakpoint.prototype.__proto__ = WebInspector.Object.prototype;
