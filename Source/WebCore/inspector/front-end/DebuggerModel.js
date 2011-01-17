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
    this._paused = false;
    this._breakpoints = {};
    this._sourceIDAndLineToBreakpointId = {};
    this._scripts = {};

    InspectorBackend.registerDomainDispatcher("Debugger", new WebInspector.DebuggerDispatcher(this));
}

WebInspector.DebuggerModel.Events = {
    DebuggerPaused: "debugger-paused",
    DebuggerResumed: "debugger-resumed",
    ParsedScriptSource: "parsed-script-source",
    FailedToParseScriptSource: "failed-to-parse-script-source",
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed"
}

WebInspector.DebuggerModel.prototype = {
    continueToLine: function(sourceID, lineNumber)
    {
        function didSetBreakpoint(breakpointId, actualLineNumber)
        {
            if (!breakpointId)
                return;
            if (this.findBreakpoint(sourceID, actualLineNumber)) {
                InspectorBackend.removeBreakpoint(breakpointId);
                return;
            }
            if ("_continueToLineBreakpointId" in this)
                InspectorBackend.removeBreakpoint(this._continueToLineBreakpointId);
            this._continueToLineBreakpointId = breakpointId;
        }
        InspectorBackend.setBreakpoint(sourceID, lineNumber, "", true, didSetBreakpoint.bind(this));
        if (this._paused)
            InspectorBackend.resume();
    },

    setBreakpoint: function(sourceID, lineNumber, enabled, condition)
    {
        function didSetBreakpoint(breakpointId, actualLineNumber)
        {
            if (breakpointId)
                this._breakpointSetOnBackend(breakpointId, sourceID, actualLineNumber, condition, enabled, lineNumber, false);
        }
        InspectorBackend.setBreakpoint(sourceID, lineNumber, condition, enabled, didSetBreakpoint.bind(this));
    },

    removeBreakpoint: function(breakpointId)
    {
        InspectorBackend.removeBreakpoint(breakpointId);
        var breakpoint = this._breakpoints[breakpointId];
        delete this._breakpoints[breakpointId];
        delete this._sourceIDAndLineToBreakpointId[this._encodeSourceIDAndLine(breakpoint.sourceID, breakpoint.line)];
        this.dispatchEventToListeners(WebInspector.DebuggerModel.Events.BreakpointRemoved, breakpointId);
    },

    _breakpointSetOnBackend: function(breakpointId, sourceID, lineNumber, condition, enabled, originalLineNumber, restored)
    {
        var sourceIDAndLine = this._encodeSourceIDAndLine(sourceID, lineNumber);
        if (sourceIDAndLine in this._sourceIDAndLineToBreakpointId) {
            InspectorBackend.removeBreakpoint(breakpointId);
            return;
        }

        var url = this._scripts[sourceID].sourceURL;
        var breakpoint = new WebInspector.Breakpoint(this, breakpointId, sourceID, url, lineNumber, enabled, condition);
        breakpoint.restored = restored;
        breakpoint.originalLineNumber = originalLineNumber;
        this._breakpoints[breakpointId] = breakpoint;
        this._sourceIDAndLineToBreakpointId[sourceIDAndLine] = breakpointId;
        this.dispatchEventToListeners(WebInspector.DebuggerModel.Events.BreakpointAdded, breakpoint);
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
        var sourceIDAndLine = this._encodeSourceIDAndLine(sourceID, lineNumber);
        var breakpointId = this._sourceIDAndLineToBreakpointId[sourceIDAndLine];
        return this._breakpoints[breakpointId];
    },

    _encodeSourceIDAndLine: function(sourceID, lineNumber)
    {
        return sourceID + ":" + lineNumber;
    },

    reset: function()
    {
        this._paused = false;
        this._breakpoints = {};
        delete this._oneTimeBreakpoint;
        this._sourceIDAndLineToBreakpointId = {};
        this._scripts = {};
    },

    scriptForSourceID: function(sourceID)
    {
        return this._scripts[sourceID];
    },

    scriptsForURL: function(url)
    {
        return this.queryScripts(function(s) { return s.sourceURL === url; });
    },

    queryScripts: function(filter)
    {
        var scripts = [];
        for (var sourceID in this._scripts) {
            var script = this._scripts[sourceID];
            if (filter(script))
                scripts.push(script);
        }
        return scripts;
    },

    _pausedScript: function(details)
    {
        this._paused = true;
        if ("_continueToLineBreakpointId" in this) {
            InspectorBackend.removeBreakpoint(this._continueToLineBreakpointId);
            delete this._continueToLineBreakpointId;
        }
        this.dispatchEventToListeners(WebInspector.DebuggerModel.Events.DebuggerPaused, details);

        if (details.eventType === WebInspector.DebuggerEventTypes.JavaScriptPause || details.eventType === WebInspector.DebuggerEventTypes.NativeBreakpoint)
            return;

        var breakpoint = this.findBreakpoint(details.callFrames[0].sourceID, details.callFrames[0].line);
        if (!breakpoint)
            return;
        breakpoint.hit = true;
        this._lastHitBreakpoint = breakpoint;
    },

    _resumedScript: function()
    {
        this._paused = false;
        this.dispatchEventToListeners(WebInspector.DebuggerModel.Events.DebuggerResumed);

        if (!this._lastHitBreakpoint)
            return;
        this._lastHitBreakpoint.hit = false;
        delete this._lastHitBreakpoint;
    },

    _parsedScriptSource: function(sourceID, sourceURL, lineOffset, columnOffset, scriptWorldType)
    {
        var script = new WebInspector.Script(sourceID, sourceURL, "", lineOffset, columnOffset, undefined, undefined, scriptWorldType);
        this._scripts[sourceID] = script;
        this.dispatchEventToListeners(WebInspector.DebuggerModel.Events.ParsedScriptSource, sourceID);
    },

    _failedToParseScriptSource: function(sourceURL, source, startingLine, errorLine, errorMessage)
    {
        var script = new WebInspector.Script(null, sourceURL, source, startingLine, errorLine, errorMessage, undefined);
        this.dispatchEventToListeners(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, script);
    }
}

WebInspector.DebuggerModel.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.DebuggerEventTypes = {
    JavaScriptPause: 0,
    JavaScriptBreakpoint: 1,
    NativeBreakpoint: 2
};

WebInspector.DebuggerDispatcher = function(debuggerModel)
{
    this._debuggerModel = debuggerModel;
}

WebInspector.DebuggerDispatcher.prototype = {
    pausedScript: function(details)
    {
        this._debuggerModel._pausedScript(details);
    },

    resumedScript: function()
    {
        this._debuggerModel._resumedScript();
    },

    debuggerWasEnabled: function()
    {
        WebInspector.panels.scripts.debuggerWasEnabled();
    },

    debuggerWasDisabled: function()
    {
        WebInspector.panels.scripts.debuggerWasDisabled();
    },

    parsedScriptSource: function(sourceID, sourceURL, lineOffset, columnOffset, scriptWorldType)
    {
        this._debuggerModel._parsedScriptSource(sourceID, sourceURL, lineOffset, columnOffset, scriptWorldType);
    },

    failedToParseScriptSource: function(sourceURL, source, startingLine, errorLine, errorMessage)
    {
        this._debuggerModel._failedToParseScriptSource(sourceURL, source, startingLine, errorLine, errorMessage);
    },

    breakpointResolved: function(breakpointId, sourceID, lineNumber, condition, enabled, originalLineNumber)
    {
        this._debuggerModel._breakpointSetOnBackend(breakpointId, sourceID, lineNumber, condition, enabled, originalLineNumber, true);
    },

    didCreateWorker: function()
    {
        var workersPane = WebInspector.panels.scripts.sidebarPanes.workers;
        workersPane.addWorker.apply(workersPane, arguments);
    },

    didDestroyWorker: function()
    {
        var workersPane = WebInspector.panels.scripts.sidebarPanes.workers;
        workersPane.removeWorker.apply(workersPane, arguments);
    }
}
