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
    this.reset();

    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerWasEnabled, this._debuggerWasEnabled, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, this._failedToParseScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ScriptSourceChanged, this._scriptSourceChanged, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointResolved, this._breakpointResolved, this);
}

WebInspector.DebuggerPresentationModel.Events = {
    SourceFileAdded: "source-file-added",
    SourceFileChanged: "source-file-changed",
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed"
}

WebInspector.DebuggerPresentationModel.prototype = {
    _debuggerWasEnabled: function()
    {
        this._restoreBreakpoints();
    },

    sourceFile: function(sourceFileId)
    {
        return this._sourceFiles[sourceFileId];
    },

    requestSourceFileContent: function(sourceFileId, callback)
    {
        this._sourceFiles[sourceFileId].requestContent(callback);
    },

    _parsedScriptSource: function(event)
    {
        this._addScript(event.data);
    },

    _failedToParseScriptSource: function(event)
    {
        this._addScript(event.data);
    },

    _addScript: function(script)
    {
        var sourceFileId = script.sourceURL || script.sourceID;
        var sourceFile = this._sourceFiles[sourceFileId];
        if (sourceFile) {
            sourceFile.addScript(script);
            return;
        }

        function contentChanged(sourceFile)
        {
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.SourceFileChanged, this._sourceFiles[sourceFileId]);
        }
        sourceFile = new WebInspector.SourceFile(sourceFileId, script, contentChanged.bind(this));
        this._sourceFiles[sourceFileId] = sourceFile;

        var breakpoints = WebInspector.debuggerModel.breakpoints;
        for (var id in breakpoints) {
            if (!(id in this._presentationBreakpoints))
                this._breakpointAdded(breakpoints[id]);
        }

        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.SourceFileAdded, sourceFile);
    },

    _scriptSourceChanged: function(event)
    {
        var sourceID = event.data.sourceID;
        var oldSource = event.data.oldSource;
        var script = WebInspector.debuggerModel.scriptForSourceID(sourceID);

        var resource = WebInspector.resourceForURL(script.sourceURL);
        if (resource) {
            var revertHandle = WebInspector.debuggerModel.editScriptSource.bind(WebInspector.debuggerModel, script.sourceID, oldSource);
            resource.setContent(script.source, revertHandle);
        }

        var sourceFileId = script.sourceURL || script.sourceID;

        // Clear and re-create breakpoints according to text diff.
        var diff = Array.diff(oldSource.split("\n"), script.source.split("\n"));
        for (var id in this._presentationBreakpoints) {
            var breakpoint = this._presentationBreakpoints[id];
            if (breakpoint.sourceFileId !== sourceFileId)
                continue;
            var lineNumber = breakpoint.lineNumber;
            this.removeBreakpoint(sourceFileId, lineNumber);

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
            if (newLineNumber !== undefined)
                this.setBreakpoint(sourceFileId, newLineNumber, breakpoint.condition, breakpoint.enabled);
        }

        this._sourceFiles[sourceFileId].reload();
    },

    continueToLine: function(sourceFileId, lineNumber)
    {
        var location = this._sourceLocationToActualLocation(sourceFileId, lineNumber);
        if (location.sourceID)
            WebInspector.debuggerModel.continueToLocation(location.sourceID, location.lineNumber, location.columnNumber);
    },

    breakpointsForSourceFileId: function(sourceFileId)
    {
        var sourceFile = this.sourceFile(sourceFileId);
        if (!sourceFile)
            return [];
        var breakpoints = [];
        for (var lineNumber in sourceFile.breakpoints)
            breakpoints.push(sourceFile.breakpoints[lineNumber]);
        return breakpoints;
    },

    setBreakpoint: function(sourceFileId, lineNumber, condition, enabled)
    {
        function didSetBreakpoint(breakpoint)
        {
            if (breakpoint) {
                this._breakpointAdded(breakpoint);
                this._saveBreakpoints();
            }
        }
        var location = this._sourceLocationToActualLocation(sourceFileId, lineNumber);
        if (location.url)
            WebInspector.debuggerModel.setBreakpoint(location.url, location.lineNumber, location.columnNumber, condition, enabled, didSetBreakpoint.bind(this));
        else
            WebInspector.debuggerModel.setBreakpointBySourceId(location.sourceID, location.lineNumber, location.columnNumber, condition, enabled, didSetBreakpoint.bind(this));
    },

    setBreakpointEnabled: function(sourceFileId, lineNumber, enabled)
    {
        var breakpoint = this.removeBreakpoint(sourceFileId, lineNumber);
        this.setBreakpoint(sourceFileId, lineNumber, breakpoint.condition, enabled);
    },

    updateBreakpoint: function(sourceFileId, lineNumber, condition, enabled)
    {
        this.removeBreakpoint(sourceFileId, lineNumber);
        this.setBreakpoint(sourceFileId, lineNumber, condition, enabled);
    },

    removeBreakpoint: function(sourceFileId, lineNumber)
    {
        var breakpoint = this.findBreakpoint(sourceFileId, lineNumber);
        WebInspector.debuggerModel.removeBreakpoint(breakpoint._id);
        this._breakpointRemoved(breakpoint._id);
        this._saveBreakpoints();
        return breakpoint;
    },

    findBreakpoint: function(sourceFileId, lineNumber)
    {
        var sourceFile = this.sourceFile(sourceFileId);
        if (sourceFile)
            return sourceFile.breakpoints[lineNumber];
    },

    _breakpointAdded: function(breakpoint)
    {
        var location = breakpoint.locations.length ? breakpoint.locations[0] : breakpoint;
        var sourceLocation = this._actualLocationToSourceLocation(breakpoint.url, breakpoint.sourceID, location.lineNumber, location.columnNumber);
        if (!sourceLocation)
            return;

        if (this.findBreakpoint(sourceLocation.sourceFileId, sourceLocation.lineNumber)) {
            // We can't show more than one breakpoint on a single source file line.
            WebInspector.debuggerModel.removeBreakpoint(breakpoint.id);
            return;
        }

        var presentationBreakpoint = {
            sourceFileId: sourceLocation.sourceFileId,
            lineNumber: sourceLocation.lineNumber,
            url: breakpoint.url,
            resolved: !!breakpoint.locations.length,
            condition: breakpoint.condition,
            enabled: breakpoint.enabled,
            _id: breakpoint.id
        };
        if (location.sourceID) {
            var script = WebInspector.debuggerModel.scriptForSourceID(location.sourceID);
            presentationBreakpoint.loadSnippet = script.sourceLine.bind(script, location.lineNumber);
        }

        this._presentationBreakpoints[breakpoint.id] = presentationBreakpoint;
        var sourceFile = this.sourceFile(sourceLocation.sourceFileId);
        sourceFile.breakpoints[sourceLocation.lineNumber] = presentationBreakpoint;
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, presentationBreakpoint);
    },

    _breakpointRemoved: function(breakpointId)
    {
        var breakpoint = this._presentationBreakpoints[breakpointId];
        delete this._presentationBreakpoints[breakpointId];
        var sourceFile = this.sourceFile(breakpoint.sourceFileId);
        delete sourceFile.breakpoints[breakpoint.lineNumber];
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointRemoved, breakpoint);
    },

    _breakpointResolved: function(event)
    {
        var breakpoint = event.data;
        if (!(breakpoint.id in this._presentationBreakpoints))
            return;
        this._breakpointRemoved(breakpoint.id);
        this._breakpointAdded(breakpoint);
    },

    _restoreBreakpoints: function()
    {
        function didSetBreakpoint(breakpoint)
        {
            if (breakpoint)
                this._breakpointAdded(breakpoint);
        }
        var breakpoints = WebInspector.settings.breakpoints;
        for (var i = 0; i < breakpoints.length; ++i) {
            var breakpoint = breakpoints[i];
            WebInspector.debuggerModel.setBreakpoint(breakpoint.url, breakpoint.lineNumber, breakpoint.columnNumber, breakpoint.condition, breakpoint.enabled, didSetBreakpoint.bind(this));
        }
    },

    _saveBreakpoints: function()
    {
        var serializedBreakpoints = [];
        var breakpoints = WebInspector.debuggerModel.breakpoints;
        for (var id in breakpoints) {
            var breakpoint = breakpoints[id];
            if (!breakpoint.url)
                continue;
            var serializedBreakpoint = {};
            serializedBreakpoint.url = breakpoint.url;
            serializedBreakpoint.lineNumber = breakpoint.lineNumber;
            serializedBreakpoint.columnNumber = breakpoint.columnNumber;
            serializedBreakpoint.condition = breakpoint.condition;
            serializedBreakpoint.enabled = breakpoint.enabled;
            serializedBreakpoints.push(serializedBreakpoint);
        }
        WebInspector.settings.breakpoints = serializedBreakpoints;
    },

    set selectedCallFrame(callFrame)
    {
        this._selectedCallFrame = callFrame;
        if (!callFrame)
            return;

        var script = WebInspector.debuggerModel.scriptForSourceID(callFrame.sourceID);
        var sourceFile = this._sourceFiles[script.sourceURL || script.sourceID];
        if (sourceFile)
            sourceFile.forceLoadContent(script);
        callFrame.sourceLocation = this._actualLocationToSourceLocation(script.sourceURL, script.sourceID, callFrame.line, callFrame.column);
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.CallFrameSelected, callFrame);
    },

    get selectedCallFrame()
    {
        return this._selectedCallFrame;
    },

    _actualLocationToSourceLocation: function(scriptURL, scriptId, lineNumber, columnNumber)
    {
        // TODO: use source mapping to obtain source location.
        var sourceFile = this._sourceFiles[scriptURL || scriptId];
        if (sourceFile && !sourceFile._pending)
            return { sourceFileId: sourceFile.id, lineNumber: lineNumber, columnNumber: columnNumber };
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
        this._sourceFiles = {};
        this._presentationBreakpoints = {};
    }
}

WebInspector.DebuggerPresentationModel.prototype.__proto__ = WebInspector.Object.prototype;
