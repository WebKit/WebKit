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

WebInspector.DebuggerModel = function()
{
    this._breakpoints = {};
    InspectorBackend.registerDomainDispatcher("Debugger", this);
}

WebInspector.DebuggerModel.prototype = {
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

    breakpointRestored: function(sourceID, url, line, enabled, condition)
    {
        this._setBreakpoint(sourceID, url, line, enabled, condition);
    },

    queryBreakpoints: function(filter)
    {
        var breakpoints = [];
        for (var id in this._breakpoints) {
           var breakpoint = this._breakpoints[id];
           if (filter(breakpoint))
               breakpoints.push(breakpoint);
        }
        return breakpoints;
    },

    findBreakpoint: function(sourceID, lineNumber)
    {
        var breakpointId = WebInspector.Breakpoint.jsBreakpointId(sourceID, lineNumber);
        return this._breakpoints[breakpointId];
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

    pausedScript: function(details)
    {
        this.dispatchEventToListeners("debugger-paused", details.callFrames);

        if (details.eventType === WebInspector.DebuggerEventTypes.JavaScriptPause)
            return;
        if (details.eventType === WebInspector.DebuggerEventTypes.NativeBreakpoint) {
            this.dispatchEventToListeners("native-breakpoint-hit", details.eventData);
            return;
        }

        var breakpointId = WebInspector.Breakpoint.jsBreakpointId(details.callFrames[0].sourceID, details.callFrames[0].line);
        var breakpoint = this._breakpoints[breakpointId];
        if (!breakpoint)
            return;
        breakpoint.hit = true;
        this._lastHitBreakpoint = breakpoint;
        this.dispatchEventToListeners("script-breakpoint-hit", breakpoint);
    },

    resumedScript: function()
    {
        this.dispatchEventToListeners("debugger-resumed");

        if (!this._lastHitBreakpoint)
            return;
        this._lastHitBreakpoint.hit = false;
        delete this._lastHitBreakpoint;
    },

    attachDebuggerWhenShown: function()
    {
        WebInspector.panels.scripts.attachDebuggerWhenShown();
    },

    debuggerWasEnabled: function()
    {
        WebInspector.panels.scripts.debuggerWasEnabled();
    },

    debuggerWasDisabled: function()
    {
        WebInspector.panels.scripts.debuggerWasDisabled();
    },

    parsedScriptSource: function(sourceID, sourceURL, source, startingLine, scriptWorldType)
    {
        WebInspector.panels.scripts.addScript(sourceID, sourceURL, source, startingLine, undefined, undefined, scriptWorldType);
    },

    failedToParseScriptSource: function(sourceURL, source, startingLine, errorLine, errorMessage)
    {
        WebInspector.panels.scripts.addScript(null, sourceURL, source, startingLine, errorLine, errorMessage);
    }
}

WebInspector.DebuggerModel.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.DebuggerEventTypes = {
    JavaScriptPause: 0,
    JavaScriptBreakpoint: 1,
    NativeBreakpoint: 2
};
