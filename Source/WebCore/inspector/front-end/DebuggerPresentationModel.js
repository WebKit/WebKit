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
    this._sourceFiles = {};
    this._messages = [];
    this._anchors = [];
    this._breakpointsByDebuggerId = {};
    this._breakpointsWithoutSourceFile = {};

    this._presentationCallFrames = [];
    this._selectedCallFrameIndex = 0;

    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerWasEnabled, this._debuggerWasEnabled, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, this._failedToParseScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointResolved, this._breakpointResolved, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerPaused, this._debuggerPaused, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerResumed, this._debuggerResumed, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.Reset, this._debuggerReset, this);

    WebInspector.console.addEventListener(WebInspector.ConsoleView.Events.MessageAdded, this._consoleMessageAdded, this);
    WebInspector.console.addEventListener(WebInspector.ConsoleView.Events.ConsoleCleared, this._consoleCleared, this);

    new WebInspector.DebuggerPresentationModelResourceBinding(this);
}

WebInspector.DebuggerPresentationModel.Events = {
    SourceFileAdded: "source-file-added",
    SourceFileChanged: "source-file-changed",
    ConsoleMessageAdded: "console-message-added",
    ConsoleMessagesCleared: "console-messages-cleared",
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed",
    DebuggerPaused: "debugger-paused",
    DebuggerResumed: "debugger-resumed",
    CallFrameSelected: "call-frame-selected"
}

WebInspector.DebuggerPresentationModel.prototype = {
    _debuggerWasEnabled: function()
    {
        if (this._breakpointsRestored)
            return;
        this._restoreBreakpointsFromSettings();
        this._breakpointsRestored = true;
    },

    sourceFile: function(sourceFileId)
    {
        return this._sourceFiles[sourceFileId];
    },

    sourceFileForScriptURL: function(scriptURL)
    {
        return this._sourceFiles[this._createSourceFileId(scriptURL)];
    },

    _scriptLocationToUILocation: function(sourceURL, sourceId, lineNumber, columnNumber, callback)
    {
        var sourceFile = this._sourceFileForScript(sourceURL, sourceId);
        var scriptLocation = { lineNumber: lineNumber, columnNumber: columnNumber };

        function didRequestSourceMapping(mapping)
        {
            var lineNumber = mapping.scriptLocationToSourceLine(scriptLocation);
            callback(sourceFile.id, lineNumber);
        }
        sourceFile.requestSourceMapping(didRequestSourceMapping);
    },

    _uiLocationToScriptLocation: function(sourceFileId, lineNumber, callback)
    {
        function didRequestSourceMapping(mapping)
        {
            callback(mapping.sourceLineToScriptLocation(lineNumber));
        }
        this._sourceFiles[sourceFileId].requestSourceMapping(didRequestSourceMapping.bind(this));
    },

    requestSourceFileContent: function(sourceFileId, callback)
    {
        this._sourceFiles[sourceFileId].requestContent(callback);
    },

    registerAnchor: function(sourceURL, sourceId, lineNumber, columnNumber, updateHandler)
    {
        var anchor = { sourceURL: sourceURL, sourceId: sourceId, lineNumber: lineNumber, columnNumber: columnNumber, updateHandler: updateHandler };
        this._anchors.push(anchor);
        this._updateAnchor(anchor);
    },

    _updateAnchor: function(anchor)
    {
        var sourceFile = this._sourceFileForScript(anchor.sourceURL, anchor.sourceId);
        if (!sourceFile)
            return;

        this._scriptLocationToUILocation(anchor.sourceURL, anchor.sourceId, anchor.lineNumber, anchor.columnNumber, anchor.updateHandler);
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
        var sourceFileId = this._createSourceFileId(script.sourceURL, script.sourceId);
        var sourceFile = this._sourceFiles[sourceFileId];
        if (sourceFile) {
            sourceFile.addScript(script);
            return;
        }

        function contentChanged(sourceFile)
        {
            if (this._sourceFiles[sourceFileId] === sourceFile)
                this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.SourceFileChanged, this._sourceFiles[sourceFileId]);
        }
        if (!this._formatSourceFiles)
            sourceFile = new WebInspector.SourceFile(sourceFileId, script, contentChanged.bind(this));
        else
            sourceFile = new WebInspector.FormattedSourceFile(sourceFileId, script, contentChanged.bind(this), this._formatter());
        this._sourceFiles[sourceFileId] = sourceFile;

        this._restoreBreakpoints(sourceFile);

        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.SourceFileAdded, sourceFile);
    },

    _restoreBreakpoints: function(sourceFile)
    {
        var pendingBreakpoints = this._breakpointsWithoutSourceFile[sourceFile.id];
        if (!pendingBreakpoints)
            return;
        for (var lineNumber in pendingBreakpoints) {
            var breakpointData = pendingBreakpoints[lineNumber];
            if ("debuggerId" in breakpointData) {
                var breakpoint = new WebInspector.PresentationBreakpoint(sourceFile, breakpointData.lineNumber, breakpointData.condition, breakpointData.enabled);
                this._bindDebuggerId(breakpoint, breakpointData.debuggerId);
                this._breakpointAdded(breakpoint);
            } else
                this.setBreakpoint(sourceFile.id, breakpointData.lineNumber, breakpointData.condition, breakpointData.enabled, true);
        }
        delete this._breakpointsWithoutSourceFile[sourceFile.id];
    },

    canEditScriptSource: function(sourceFileId)
    {
        if (!Preferences.canEditScriptSource || this._formatSourceFiles)
            return false;
        var script = this._scriptForSourceFileId(sourceFileId);
        return !script.lineOffset && !script.columnOffset;
    },

    setScriptSource: function(sourceFileId, newSource, callback)
    {
        var script = this._scriptForSourceFileId(sourceFileId);
        var sourceFile = this._sourceFiles[sourceFileId];

        function didEditScriptSource(oldSource, error)
        {
            if (!error) {
                sourceFile.content = newSource;

                var resource = WebInspector.resourceForURL(sourceFile.url);
                if (resource)
                    resource.addRevision(newSource);
            }

            callback(error);

            if (!error && WebInspector.debuggerModel.callFrames)
                this._debuggerPaused();
        }

        var oldSource = sourceFile.requestContent(didReceiveSource.bind(this));
        function didReceiveSource(oldSource)
        {
            WebInspector.debuggerModel.setScriptSource(script.sourceId, newSource, didEditScriptSource.bind(this, oldSource));
        }
    },

    _updateBreakpointsAfterLiveEdit: function(sourceFileId, oldSource, newSource)
    {
        var sourceFile = this._sourceFiles[sourceFileId];

        // Clear and re-create breakpoints according to text diff.
        var diff = Array.diff(oldSource.split("\n"), newSource.split("\n"));
        for (var lineNumber in sourceFile.breakpoints) {
            var breakpoint = sourceFile.breakpoints[lineNumber];

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
    },

    setFormatSourceFiles: function(formatSourceFiles)
    {
        if (this._formatSourceFiles === formatSourceFiles)
            return;

        this._formatSourceFiles = formatSourceFiles;

        for (var id in this._sourceFiles) {
            var sourceFile = this._sourceFiles[id];
            for (var line in sourceFile.breakpoints)
                this._removeBreakpointFromDebugger(sourceFile.breakpoints[line]);
        }

        for (var id in this._breakpointsWithoutSourceFile) {
            var breakpoints = this._breakpointsWithoutSourceFile[id];
            for (var lineNumber in breakpoints)
                this._removeBreakpointFromDebugger(breakpoints[lineNumber]);
        }

        var messages = this._messages;
        this._reset();

        var scripts = WebInspector.debuggerModel.scripts;
        for (var id in scripts)
            this._addScript(scripts[id]);

        for (var i = 0; i < messages.length; ++i)
            this._addConsoleMessage(messages[i]);

        for (var i = 0; i < this._anchors.length; ++i)
            this._updateAnchor(this._anchors[i]);

        if (WebInspector.debuggerModel.callFrames)
            this._debuggerPaused();
    },

    _formatter: function()
    {
        if (!this._scriptFormatter)
            this._scriptFormatter = new WebInspector.ScriptFormatter();
        return this._scriptFormatter;
    },

    _consoleMessageAdded: function(event)
    {
        var message = event.data;
        if (message.url && message.isErrorOrWarning() && message.message)
            this._addConsoleMessage(message);
    },

    _addConsoleMessage: function(message)
    {
        this._messages.push(message);

        var sourceFile = this._sourceFileForScript(message.url);
        if (!sourceFile)
            return;

        function didGetUILocation(sourceFileId, lineNumber)
        {
            var presentationMessage = {};
            presentationMessage.sourceFileId = sourceFileId;
            presentationMessage.lineNumber = lineNumber;
            presentationMessage.originalMessage = message;
            sourceFile.messages.push(presentationMessage);
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.ConsoleMessageAdded, presentationMessage);
        }
        // FIXME(62725): stack trace line/column numbers are one-based.
        var lineNumber = message.stackTrace ? message.stackTrace[0].lineNumber - 1 : message.line - 1;
        var columnNumber = message.stackTrace ? message.stackTrace[0].columnNumber - 1 : 0;
        this._scriptLocationToUILocation(message.url, null, lineNumber, columnNumber, didGetUILocation.bind(this));
    },

    _consoleCleared: function()
    {
        this._messages = [];
        for (var id in this._sourceFiles)
            this._sourceFiles[id].messages = [];
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.ConsoleMessagesCleared);
    },

    continueToLine: function(sourceFileId, lineNumber)
    {
        function didGetScriptLocation(location)
        {
            WebInspector.debuggerModel.continueToLocation(location);
        }
        this._uiLocationToScriptLocation(sourceFileId, lineNumber, didGetScriptLocation);
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

    setBreakpoint: function(sourceFileId, lineNumber, condition, enabled, dontSaveBreakpoints)
    {
        var sourceFile = this._sourceFiles[sourceFileId];
        if (!sourceFile)
            return;

        var breakpoint = new WebInspector.PresentationBreakpoint(sourceFile, lineNumber, condition, enabled);
        if (!enabled) {
            this._breakpointAdded(breakpoint);
            if (!dontSaveBreakpoints)
                this._saveBreakpoints();
            return;
        }

        function callback()
        {
            this._breakpointAdded(breakpoint);
            if (!dontSaveBreakpoints)
                this._saveBreakpoints();
        }
        this._setBreakpointInDebugger(breakpoint, callback.bind(this));
    },

    _setBreakpointInDebugger: function(breakpoint, callback)
    {
        function didSetBreakpoint(breakpointId, locations)
        {
            if (!breakpointId)
                return;

            this._bindDebuggerId(breakpoint, breakpointId);
            breakpoint.location = locations[0];
            callback();
        }

        function didGetScriptLocation(location)
        {
            var script = WebInspector.debuggerModel.scriptForSourceID(location.sourceId);
            if (script.sourceURL)
                WebInspector.debuggerModel.setBreakpoint(script.sourceURL, location.lineNumber, location.columnNumber, breakpoint.condition, didSetBreakpoint.bind(this));
            else {
                location.sourceId = script.sourceId;
                WebInspector.debuggerModel.setBreakpointBySourceId(location, breakpoint.condition, didSetBreakpoint.bind(this));
            }
        }
        this._uiLocationToScriptLocation(breakpoint.sourceFile.id, breakpoint.lineNumber, didGetScriptLocation.bind(this));
    },

    _removeBreakpointFromDebugger: function(breakpoint, callback)
    {
        if ("debuggerId" in breakpoint) {
            WebInspector.debuggerModel.removeBreakpoint(breakpoint.debuggerId);
            this._unbindDebuggerId(breakpoint);
        }

        if (callback)
            callback();
    },

    _bindDebuggerId: function(breakpoint, debuggerId)
    {
        breakpoint.debuggerId = debuggerId;
        this._breakpointsByDebuggerId[debuggerId] = breakpoint;
    },

    _unbindDebuggerId: function(breakpoint)
    {
        delete this._breakpointsByDebuggerId[breakpoint.debuggerId];
        delete breakpoint.debuggerId;
    },

    setBreakpointEnabled: function(sourceFileId, lineNumber, enabled)
    {
        var breakpoint = this.findBreakpoint(sourceFileId, lineNumber);
        if (!breakpoint)
            return;

        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointRemoved, breakpoint);

        breakpoint.enabled = enabled;

        function afterUpdate()
        {
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, breakpoint);
            this._saveBreakpoints();
        }

        if (!enabled)
            this._removeBreakpointFromDebugger(breakpoint, afterUpdate.call(this));
        else
            this._setBreakpointInDebugger(breakpoint, afterUpdate.bind(this));
    },

    updateBreakpoint: function(sourceFileId, lineNumber, condition, enabled)
    {
        this.removeBreakpoint(sourceFileId, lineNumber);
        this.setBreakpoint(sourceFileId, lineNumber, condition, enabled);
    },

    removeBreakpoint: function(sourceFileId, lineNumber)
    {
        var breakpoint = this.findBreakpoint(sourceFileId, lineNumber);
        if (!breakpoint)
            return;

        function callback()
        {
            this._breakpointRemoved(breakpoint);
            this._saveBreakpoints();
        }
        this._removeBreakpointFromDebugger(breakpoint, callback.bind(this));
    },

    findBreakpoint: function(sourceFileId, lineNumber)
    {
        var sourceFile = this.sourceFile(sourceFileId);
        if (sourceFile)
            return sourceFile.breakpoints[lineNumber];
    },

    _breakpointAdded: function(breakpoint)
    {
        var sourceFile = breakpoint.sourceFile;
        if (!sourceFile)
            return;

        function updateSourceFileBreakpointsAndDispatchEvent()
        {
            var existingBreakpoint = this.findBreakpoint(sourceFile.id, breakpoint.lineNumber);
            if (existingBreakpoint) {
                // We can't show more than one breakpoint on a single source file line.
                this._removeBreakpointFromDebugger(breakpoint);
                return;
            }
            sourceFile.breakpoints[breakpoint.lineNumber] = breakpoint;
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, breakpoint);
        }

        function didGetUILocation(sourceFileId, lineNumber)
        {
            breakpoint.lineNumber = lineNumber;
            updateSourceFileBreakpointsAndDispatchEvent.call(this);
        }
        // Refine line number based on resolved location.
        if (breakpoint.location)
            this._scriptLocationToUILocation(null, breakpoint.location.sourceId, breakpoint.location.lineNumber, breakpoint.location.columnNumber, didGetUILocation.bind(this));
        else
            updateSourceFileBreakpointsAndDispatchEvent.call(this);
    },

    _breakpointRemoved: function(breakpoint)
    {
        var sourceFile = breakpoint.sourceFile;
        if (sourceFile.breakpoints[breakpoint.lineNumber] === breakpoint) {
            // There can already be a newer breakpoint;
            delete sourceFile.breakpoints[breakpoint.lineNumber];
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointRemoved, breakpoint);
        }
    },

    _breakpointResolved: function(event)
    {
        var debuggerId = event.data.breakpointId;
        if (!(debuggerId in this._breakpointsByDebuggerId))
            return;
        var breakpoint = this._breakpointsByDebuggerId[debuggerId];

        this._breakpointRemoved(breakpoint);
        breakpoint.location = event.data.location;
        this._breakpointAdded(breakpoint);
    },

    _restoreBreakpointsFromSettings: function()
    {
        var breakpoints = WebInspector.settings.breakpoints.get();
        for (var i = 0; i < breakpoints.length; ++i) {
            var breakpointData = breakpoints[i];
            var sourceFileId = breakpointData.sourceFileId;
            if (!sourceFileId)
                continue;
            var sourceFile = this._sourceFiles[sourceFileId];
            if (sourceFile) {
                this.setBreakpoint(sourceFileId, breakpointData.lineNumber, breakpointData.condition, breakpointData.enabled);
                continue;
            }

            // Add breakpoint once source file becomes available.
            var pendingBreakpoints = this._breakpointsWithoutSourceFile[sourceFileId];
            if (!pendingBreakpoints) {
                pendingBreakpoints = {};
                this._breakpointsWithoutSourceFile[sourceFileId] = pendingBreakpoints;
            }
            pendingBreakpoints[breakpointData.lineNumber] = breakpointData;
        }
    },

    _saveBreakpoints: function()
    {
        var serializedBreakpoints = [];

        // Store added breakpoints.
        for (var sourceFileId in this._sourceFiles) {
            var sourceFile = this._sourceFiles[sourceFileId];
            if (!sourceFile.url)
                continue;

            for (var lineNumber in sourceFile.breakpoints)
                serializedBreakpoints.push(sourceFile.breakpoints[lineNumber].serialize());
        }

        // Store not added breakpoints.
        for (var sourceFileId in this._breakpointsWithoutSourceFile) {
            var breakpoints = this._breakpointsWithoutSourceFile[sourceFileId];
            for (var lineNumber in breakpoints)
                serializedBreakpoints.push(breakpoints[lineNumber]);
        }

        // Sanitize debugger ids.
        for (var i = 0; i < serializedBreakpoints.length; ++i) {
            var breakpoint = serializedBreakpoints[i];
            var breakpointCopy = {};
            for (var property in breakpoint) {
                if (property !== "debuggerId")
                    breakpointCopy[property] = breakpoint[property];
            }
            serializedBreakpoints[i] = breakpointCopy;
        }

        WebInspector.settings.breakpoints.set(serializedBreakpoints);
    },

    _debuggerPaused: function()
    {
        var callFrames = WebInspector.debuggerModel.callFrames;
        this._presentationCallFrames = [];
        for (var i = 0; i < callFrames.length; ++i) {
            var callFrame = callFrames[i];
            var sourceFile;
            var script = WebInspector.debuggerModel.scriptForSourceID(callFrame.location.sourceId);
            if (script)
                sourceFile = this._sourceFileForScript(script.sourceURL, script.sourceId);
            this._presentationCallFrames.push(new WebInspector.PresenationCallFrame(callFrame, i, this, sourceFile));
        }
        var details = WebInspector.debuggerModel.debuggerPausedDetails;
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.DebuggerPaused, { callFrames: this._presentationCallFrames, details: details });

        this.selectedCallFrame = this._presentationCallFrames[this._selectedCallFrameIndex];
    },

    _debuggerResumed: function()
    {
        this._presentationCallFrames = [];
        this._selectedCallFrameIndex = 0;
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.DebuggerResumed);
    },

    set selectedCallFrame(callFrame)
    {
        this._selectedCallFrameIndex = callFrame.index;
        callFrame.select();
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.CallFrameSelected, callFrame);
    },

    get selectedCallFrame()
    {
        return this._presentationCallFrames[this._selectedCallFrameIndex];
    },

    _sourceFileForScript: function(sourceURL, sourceId)
    {
        if (!sourceURL)
            sourceURL = WebInspector.debuggerModel.scriptForSourceID(sourceId).sourceURL;
        return this._sourceFiles[this._createSourceFileId(sourceURL, sourceId)];
    },

    _scriptForSourceFileId: function(sourceFileId)
    {
        function filter(script)
        {
            return this._createSourceFileId(script.sourceURL, script.sourceId) === sourceFileId;
        }
        return WebInspector.debuggerModel.queryScripts(filter.bind(this))[0];
    },

    _createSourceFileId: function(sourceURL, sourceId)
    {
        var prefix = this._formatSourceFiles ? "deobfuscated:" : "";
        return prefix + (sourceURL || sourceId);
    },

    _reset: function()
    {
        for (var id in this._sourceFiles) {
            var sourceFile = this._sourceFiles[id];
            for (var line in sourceFile.breakpoints) {
                var breakpoints = this._breakpointsWithoutSourceFile[sourceFile.id];
                if (!breakpoints) {
                    breakpoints = {};
                    this._breakpointsWithoutSourceFile[sourceFile.id] = breakpoints;
                }
                breakpoints[line] = sourceFile.breakpoints[line].serialize();
            }
        }

        this._sourceFiles = {};
        this._messages = [];
        this._breakpointsByDebuggerId = {};
    },

    _debuggerReset: function()
    {
        this._reset();
        this._anchors = [];
        this._presentationCallFrames = [];
        this._selectedCallFrameIndex = 0;
    }
}

WebInspector.DebuggerPresentationModel.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.PresentationBreakpoint = function(sourceFile, lineNumber, condition, enabled)
{
    this.sourceFile = sourceFile;
    this.sourceFileId = sourceFile.id;
    this.lineNumber = lineNumber;
    this.condition = condition;
    this.enabled = enabled;
}

WebInspector.PresentationBreakpoint.prototype = {
    get url()
    {
        return this.sourceFile.url;
    },

    get resolved()
    {
        return !!this.location;
    },

    loadSnippet: function(callback)
    {
        function didRequestContent(mimeType, content)
        {
            var lineEndings = content.lineEndings();
            var snippet = "";
            if (this.lineNumber < lineEndings.length)
                snippet = content.substring(lineEndings[this.lineNumber - 1], lineEndings[this.lineNumber]);
            callback(snippet);
        }
        if (!this.sourceFile) {
            callback(WebInspector.UIString("N/A"));
            return;
        }
        this.sourceFile.requestContent(didRequestContent.bind(this));
    },

    serialize: function()
    {
        var serializedBreakpoint = {};
        serializedBreakpoint.sourceFileId = this.sourceFile.id;
        serializedBreakpoint.lineNumber = this.lineNumber;
        serializedBreakpoint.condition = this.condition;
        serializedBreakpoint.enabled = this.enabled;
        if ("debuggerId" in this)
            serializedBreakpoint.debuggerId = this.debuggerId;
        return serializedBreakpoint;
    }
}

WebInspector.PresenationCallFrame = function(callFrame, index, model, sourceFile)
{
    this._callFrame = callFrame;
    this._index = index;
    this._model = model;
    this._sourceFile = sourceFile;
    this._script = WebInspector.debuggerModel.scriptForSourceID(callFrame.location.sourceId);
}

WebInspector.PresenationCallFrame.prototype = {
    get functionName()
    {
        return this._callFrame.functionName;
    },

    get type()
    {
        return this._callFrame.type;
    },

    get isInternalScript()
    {
        return !this._script;
    },

    get url()
    {
        if (this._sourceFile)
            return this._sourceFile.url;
    },

    get scopeChain()
    {
        return this._callFrame.scopeChain;
    },

    get this()
    {
        return this._callFrame.this;
    },

    get index()
    {
        return this._index;
    },

    select: function()
    {
        if (this._sourceFile)
            this._sourceFile.forceLoadContent(this._script);
    },

    evaluate: function(code, objectGroup, includeCommandLineAPI, callback)
    {
        function didEvaluateOnCallFrame(error, result, wasThrown)
        {
            callback(WebInspector.RemoteObject.fromPayload(result), wasThrown);
        }
        DebuggerAgent.evaluateOnCallFrame(this._callFrame.id, code, objectGroup, includeCommandLineAPI, didEvaluateOnCallFrame.bind(this));
    },

    sourceLine: function(callback)
    {
        var location = this._callFrame.location;
        this._model._scriptLocationToUILocation(null, location.sourceId, location.lineNumber, location.columnNumber, callback);
    }
}

WebInspector.DebuggerPresentationModelResourceBinding = function(model)
{
    this._presentationModel = model;
    WebInspector.Resource.registerDomainModelBinding(WebInspector.Resource.Type.Script, this);
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype = {
    canSetContent: function(resource)
    {
        var sourceFile = this._presentationModel._sourceFileForScript(resource.url)
        if (!sourceFile)
            return false;
        return this._presentationModel.canEditScriptSource(sourceFile.id);
    },

    setContent: function(resource, content, majorChange, userCallback)
    {
        if (!majorChange)
            return;

        var sourceFile = this._presentationModel._sourceFileForScript(resource.url);
        if (!sourceFile) {
            userCallback("Resource is not editable");
            return;
        }

        resource.requestContent(this._setContentWithInitialContent.bind(this, sourceFile, content, userCallback));
    },

    _setContentWithInitialContent: function(sourceFile, content, userCallback, oldContent)
    {
        function callback(error)
        {
            if (userCallback)
                userCallback(error);
            if (!error) {
                this._presentationModel._updateBreakpointsAfterLiveEdit(sourceFile.id, oldContent, content);
                sourceFile.reload();
            }
        }
        this._presentationModel.setScriptSource(sourceFile.id, content, callback.bind(this));
    }
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype.__proto__ = WebInspector.ResourceDomainModelBinding.prototype;
