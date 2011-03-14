/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

WebInspector.DebuggerPresentationModel = function()
{
    this._breakpoints = {};
    this._sourceLocationToBreakpointId = {};

    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, this._failedToParseScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ScriptSourceChanged, this._scriptSourceChanged, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointAdded, this._breakpointAdded, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointRemoved, this._breakpointRemoved, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointResolved, this._breakpointResolved, this);
}

WebInspector.DebuggerPresentationModel.Events = {
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed"
}

WebInspector.DebuggerPresentationModel.prototype = {
    _parsedScriptSource: function(event)
    {
        var script = event.data;
        if (script.sourceURL)
            this._revealHiddenBreakpoints(script.sourceURL);
    },

    _failedToParseScriptSource: function(event)
    {
        var script = event.data;
        if (script.sourceURL)
            this._revealHiddenBreakpoints(script.sourceURL);
    },

    _revealHiddenBreakpoints: function(url)
    {
        for (var id in this._breakpoints) {
            var breakpoint = this._breakpoints[id];
            if (breakpoint._hidden && breakpoint.url === url) {
                delete breakpoint._hidden;
                this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, breakpoint);
            }
        }
    },

    _scriptSourceChanged: function(event)
    {
        var sourceID = event.data.sourceID;
        var oldSource = event.data.oldSource;
        var script = WebInspector.debuggerModel.scriptForSourceID(sourceID);

        // Clear and re-create breakpoints according to text diff.
        var diff = Array.diff(oldSource.split("\n"), script.source.split("\n"));
        var breakpoints = WebInspector.debuggerModel.breakpoints;
        for (var id in breakpoints) {
            var breakpoint = breakpoints[id];
            if (breakpoint.url) {
                if (breakpoint.url !== script.sourceURL)
                    continue;
            } else {
                if (breakpoint.sourceID !== sourceID)
                    continue;
            }
            WebInspector.debuggerModel.removeBreakpoint(breakpoint.id);
            var lineNumber = breakpoint.lineNumber;
            var newLineNumber = diff.left[lineNumber].row;
            if (newLineNumber === undefined) {
                for (var i = lineNumber - 1; i >= 0; --i) {
                    if (diff.left[i].row === undefined)
                        continue;
                    var shiftedLineNumber = diff.left[i].row + lineNumber - i;
                    if (shiftedLineNumber < diff.right.length) {
                        var originalLineNumber = diff.right[shiftedLineNumber].row;
                        if (originalLineNumber === lineNumber || originalLineNumber === undefined)
                            newLineNumber = shiftedLineNumber;
                    }
                    break;
                }
            }
            if (newLineNumber === undefined)
                continue;
            if (breakpoint.url)
                WebInspector.debuggerModel.setBreakpoint(breakpoint.url, newLineNumber, breakpoint.columnNumber, breakpoint.condition, breakpoint.enabled);
            else
                WebInspector.debuggerModel.setBreakpointBySourceId(script.sourceID, newLineNumber, breakpoint.columnNumber, breakpoint.condition, breakpoint.enabled);
        }
    },

    continueToLine: function(sourceFileId, lineNumber)
    {
        var location = this._sourceLocationToActualLocation(sourceFileId, lineNumber);
        if (location.sourceID)
            WebInspector.debuggerModel.continueToLocation(location.sourceID, location.lineNumber, location.columnNumber);
    },

    breakpointsForSourceFileId: function(sourceFileId)
    {
        var breakpoints = [];
        for (var id in this._breakpoints) {
            var breakpoint = this._breakpoints[id];
            if (!breakpoint._hidden && breakpoint.sourceFileId === sourceFileId)
                breakpoints.push(breakpoint);
        }
        return breakpoints;
    },

    setBreakpoint: function(sourceFileId, lineNumber, condition, enabled)
    {
        var location = this._sourceLocationToActualLocation(sourceFileId, lineNumber);
        if (location.url)
            WebInspector.debuggerModel.setBreakpoint(location.url, location.lineNumber, location.columnNumber, condition, enabled);
        else
            WebInspector.debuggerModel.setBreakpointBySourceId(location.sourceID, location.lineNumber, location.columnNumber, condition, enabled);
    },

    setBreakpointEnabled: function(sourceFileId, lineNumber, enabled)
    {
        var encodedSourceLocation = this._encodeSourceLocation(sourceFileId, lineNumber);
        var breakpointId = this._sourceLocationToBreakpointId[encodedSourceLocation];
        if (!breakpointId)
            return;
        var breakpoint = this._breakpoints[breakpointId];
        WebInspector.debuggerModel.updateBreakpoint(breakpointId, breakpoint.condition, enabled);
    },

    updateBreakpoint: function(sourceFileId, lineNumber, condition, enabled)
    {
        var encodedSourceLocation = this._encodeSourceLocation(sourceFileId, lineNumber);
        var breakpointId = this._sourceLocationToBreakpointId[encodedSourceLocation];
        if (breakpointId)
            WebInspector.debuggerModel.updateBreakpoint(breakpointId, condition, enabled);
    },

    removeBreakpoint: function(sourceFileId, lineNumber)
    {
        var encodedSourceLocation = this._encodeSourceLocation(sourceFileId, lineNumber);
        var breakpointId = this._sourceLocationToBreakpointId[encodedSourceLocation];
        if (breakpointId)
            WebInspector.debuggerModel.removeBreakpoint(breakpointId);
    },

    findBreakpoint: function(sourceFileId, lineNumber)
    {
        var encodedSourceLocation = this._encodeSourceLocation(sourceFileId, lineNumber);
        var breakpointId = this._sourceLocationToBreakpointId[encodedSourceLocation];
        if (breakpointId)
            return this._breakpoints[breakpointId];
    },

    _breakpointAdded: function(event)
    {
        var breakpoint = event.data;
        var location = breakpoint.locations.length ? breakpoint.locations[0] : breakpoint;
        var sourceLocation = this._actualLocationToSourceLocation(breakpoint.url || breakpoint.sourceID, location.lineNumber, location.columnNumber);

        var encodedSourceLocation = this._encodeSourceLocation(sourceLocation.sourceFileId, sourceLocation.lineNumber);
        if (encodedSourceLocation in this._sourceLocationToBreakpointId) {
            // We can't show more than one breakpoint on a single source frame line. Remove newly added breakpoint.
            WebInspector.debuggerModel.removeBreakpoint(breakpoint.id);
            return;
        }

        var presentationBreakpoint = {
            sourceFileId: sourceLocation.sourceFileId,
            lineNumber: sourceLocation.lineNumber,
            url: breakpoint.url,
            resolved: !!breakpoint.locations.length,
            condition: breakpoint.condition,
            enabled: breakpoint.enabled
        };
        if (location.sourceID) {
            var script = WebInspector.debuggerModel.scriptForSourceID(location.sourceID);
            presentationBreakpoint.loadSnippet = script.sourceLine.bind(script, location.lineNumber);
        }

        this._sourceLocationToBreakpointId[encodedSourceLocation] = breakpoint.id;
        this._breakpoints[breakpoint.id] = presentationBreakpoint;

        if (!WebInspector.debuggerModel.scriptsForURL(breakpoint.url).length)
            presentationBreakpoint._hidden = true;
        else
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, presentationBreakpoint);
    },

    _breakpointRemoved: function(event)
    {
        var breakpointId = event.data;
        var breakpoint = this._breakpoints[breakpointId];
        if (!breakpoint)
            return;
        var encodedSourceLocation = this._encodeSourceLocation(breakpoint.sourceFileId, breakpoint.lineNumber);
        delete this._breakpoints[breakpointId];
        delete this._sourceLocationToBreakpointId[encodedSourceLocation];
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointRemoved, breakpoint);
    },

    _breakpointResolved: function(event)
    {
        var breakpoint = event.data;
        this._breakpointRemoved({ data: breakpoint.id });
        this._breakpointAdded({ data: breakpoint });
    },

    _encodeSourceLocation: function(sourceFileId, lineNumber)
    {
        return sourceFileId + ":" + lineNumber;
    },

    set selectedCallFrame(callFrame)
    {
        this._selectedCallFrame = callFrame;
        if (!callFrame)
            return;
        var script = WebInspector.debuggerModel.scriptForSourceID(callFrame.sourceID);
        callFrame.sourceLocation = this._actualLocationToSourceLocation(script.sourceURL || script.sourceID, callFrame.line, callFrame.column);
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.CallFrameSelected, callFrame);
    },

    get selectedCallFrame()
    {
        return this._selectedCallFrame;
    },

    _actualLocationToSourceLocation: function(sourceID, lineNumber, columnNumber)
    {
        // TODO: use source mapping to obtain source location.
        return { sourceFileId: sourceID, lineNumber: lineNumber, columnNumber: columnNumber };
    },

    _sourceLocationToActualLocation: function(sourceFileId, lineNumber)
    {
        // TODO: use source mapping to obtain actual location.
        function filter(script)
        {
            return (script.sourceURL || script.sourceID) === sourceFileId;
        }
        var script = WebInspector.debuggerModel.queryScripts(filter)[0];
        return { url: script.sourceURL, sourceID: script.sourceID, lineNumber: lineNumber, columnNumber: 0 };
    },

    reset: function()
    {
        for (var id in this._breakpoints) {
            var breakpoint = this._breakpoints[id];
            breakpoint._hidden = true;
        }
        this._sourceLocationToBreakpointId = {};
    }
}

WebInspector.DebuggerPresentationModel.prototype.__proto__ = WebInspector.Object.prototype;
