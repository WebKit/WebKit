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
    // FIXME: apply formatter from outside as a generic mapping.
    this._formatter = new WebInspector.ScriptFormatter();
    this._sourceFiles = {};
    this._messages = [];
    // FIXME: move this to RawSourceCode when it's not re-created in pretty-print mode.
    this._sourceMappingListeners = [];

    this._presentationCallFrames = [];
    this._selectedCallFrameIndex = 0;

    this._breakpointManager = new WebInspector.BreakpointManager(WebInspector.settings.breakpoints, this._breakpointAdded.bind(this), this._breakpointRemoved.bind(this), WebInspector.debuggerModel);

    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, this._failedToParseScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerPaused, this._debuggerPaused, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerResumed, this._debuggerResumed, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.Reset, this._debuggerReset, this);

    WebInspector.console.addEventListener(WebInspector.ConsoleView.Events.MessageAdded, this._consoleMessageAdded, this);
    WebInspector.console.addEventListener(WebInspector.ConsoleView.Events.ConsoleCleared, this._consoleCleared, this);

    new WebInspector.DebuggerPresentationModelResourceBinding(this);
}

WebInspector.DebuggerPresentationModel.Events = {
    SourceFileAdded: "source-file-added",
    SourceFileReplaced: "source-file-replaced",
    ConsoleMessageAdded: "console-message-added",
    ConsoleMessagesCleared: "console-messages-cleared",
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed",
    DebuggerPaused: "debugger-paused",
    DebuggerResumed: "debugger-resumed",
    CallFrameSelected: "call-frame-selected"
}

WebInspector.DebuggerPresentationModel.prototype = {
    sourceFile: function(sourceFileId)
    {
        return this._sourceFiles[sourceFileId];
    },

    sourceFileForScriptURL: function(scriptURL)
    {
        return this._sourceFiles[this._createSourceFileId(scriptURL)];
    },

    _scriptLocationToUILocation: function(sourceURL, scriptId, lineNumber, columnNumber, callback)
    {
        var sourceFile = this._sourceFileForScript(sourceURL, scriptId);

        function didCreateSourceMapping()
        {
            var uiLocation = sourceFile.rawLocationToUILocation({ lineNumber: lineNumber, columnNumber: columnNumber });
            callback(uiLocation.sourceFile.id, uiLocation.lineNumber);
        }
        // FIXME: force source formatting if needed. This will go away once formatting
        // is fully encapsulated in RawSourceCode class.
        sourceFile.createSourceMappingIfNeeded(didCreateSourceMapping);
    },

    _uiLocationToScriptLocation: function(sourceFileId, lineNumber, callback)
    {
        var sourceFile = this._sourceFiles[sourceFileId];

        function didCreateSourceMapping()
        {
            var rawLocation = sourceFile.uiLocationToRawLocation(lineNumber, 0);
            callback(rawLocation);
        }
        // FIXME: force source formatting if needed. This will go away once formatting
        // is fully encapsulated in RawSourceCode class.
        sourceFile.createSourceMappingIfNeeded(didCreateSourceMapping);
    },

    requestSourceFileContent: function(sourceFileId, callback)
    {
        this._sourceFiles[sourceFileId].requestContent(callback);
    },

    addSourceMappingListener: function(sourceURL, sourceId, listener)
    {
        this._sourceMappingListeners.push(listener);
    },

    removeSourceMappingListener: function(sourceURL, sourceId, listener)
    {
        // FIXME: implement this.
    },

    linkifyLocation: function(sourceURL, lineNumber, columnNumber, classes)
    {
        var linkText = WebInspector.formatLinkText(sourceURL, lineNumber);
        var anchor = WebInspector.linkifyURLAsNode(sourceURL, linkText, classes, false);

        var sourceFile = this._sourceFileForScript(sourceURL);
        if (!sourceFile) {
            anchor.setAttribute("preferred_panel", "resources");
            anchor.setAttribute("line_number", lineNumber);
            return anchor;
        }

        function updateAnchor()
        {
            function didGetLocation(sourceFileId, lineNumber)
            {
                anchor.textContent = WebInspector.formatLinkText(sourceFile.url, lineNumber);
                anchor.setAttribute("preferred_panel", "scripts");
                anchor.setAttribute("source_file_id", sourceFileId);
                anchor.setAttribute("line_number", lineNumber);
            }
            this._scriptLocationToUILocation(sourceURL, null, lineNumber, columnNumber, didGetLocation.bind(this));
        }
        updateAnchor.call(this);
        this.addSourceMappingListener(sourceURL, null, updateAnchor.bind(this));
        return anchor;
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
        var sourceFileId = this._createSourceFileId(script.sourceURL, script.scriptId);
        var sourceFile = this._sourceFiles[sourceFileId];
        if (sourceFile) {
            sourceFile.addScript(script);
            return;
        }

        sourceFile = new WebInspector.RawSourceCode(sourceFileId, script, this._formatter);
        sourceFile.setFormatted(this._formatSourceFiles);
        this._sourceFiles[sourceFileId] = sourceFile;
        sourceFile.addEventListener(WebInspector.RawSourceCode.Events.UISourceCodeReplaced, this._uiSourceCodeReplaced, this);

        function didCreateSourceMapping()
        {
            this._breakpointManager.uiSourceCodeAdded(sourceFile);
            var breakpoints = this._breakpointManager.breakpointsForUISourceCode(sourceFileId);
            for (var lineNumber in breakpoints) {
                var breakpoint = breakpoints[lineNumber];
                this._breakpointAdded(breakpoint.uiSourceCodeId, breakpoint.lineNumber, breakpoint.condition, breakpoint.enabled);
            }
        }
        // FIXME: force source formatting if needed. This will go away once formatting
        // is fully encapsulated in RawSourceCode class.
        sourceFile.createSourceMappingIfNeeded(didCreateSourceMapping.bind(this));

        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.SourceFileAdded, sourceFile.uiSourceCode);
    },

    _uiSourceCodeReplaced: function(event)
    {
        var oldUISourceCode = event.data.oldSourceCode;
        var newUISourceCode = event.data.sourceCode;

        delete this._sourceFiles[oldUISourceCode.id];
        this._sourceFiles[newUISourceCode.id] = newUISourceCode;

        // FIXME: restore breakpoints in new source code (currently we just recreate everything when switching to pretty-print mode).
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.SourceFileReplaced, event.data);
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
            WebInspector.debuggerModel.setScriptSource(script.scriptId, newSource, didEditScriptSource.bind(this, oldSource));
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

        this._breakpointManager.reset();
        this._sourceFiles = {};
        var messages = this._messages;
        this._messages = [];

        var scripts = WebInspector.debuggerModel.scripts;
        for (var id in scripts)
            this._addScript(scripts[id]);

        for (var i = 0; i < messages.length; ++i)
            this._addConsoleMessage(messages[i]);

        // FIXME: move this to RawSourceCode.
        for (var i = 0; i < this._sourceMappingListeners.length; ++i)
            this._sourceMappingListeners[i]();

        if (WebInspector.debuggerModel.callFrames)
            this._debuggerPaused();
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
        var breakpointsMap = this._breakpointManager.breakpointsForUISourceCode(sourceFileId);
        var breakpointsList = [];
        for (var lineNumber in breakpointsMap)
            breakpointsList.push(breakpointsMap[lineNumber]);
        return breakpointsList;
    },

    setBreakpoint: function(sourceFileId, lineNumber, condition, enabled)
    {
        this._breakpointManager.setBreakpoint(this._sourceFiles[sourceFileId], lineNumber, condition, enabled);
    },

    setBreakpointEnabled: function(sourceFileId, lineNumber, enabled)
    {
        var breakpoint = this.findBreakpoint(sourceFileId, lineNumber);
        if (!breakpoint)
            return;
        this._breakpointManager.removeBreakpoint(sourceFileId, lineNumber);
        this._breakpointManager.setBreakpoint(this._sourceFiles[sourceFileId], lineNumber, breakpoint.condition, enabled);
    },

    updateBreakpoint: function(sourceFileId, lineNumber, condition, enabled)
    {
        this._breakpointManager.removeBreakpoint(sourceFileId, lineNumber);
        this._breakpointManager.setBreakpoint(this._sourceFiles[sourceFileId], lineNumber, condition, enabled);
    },

    removeBreakpoint: function(sourceFileId, lineNumber)
    {
        this._breakpointManager.removeBreakpoint(sourceFileId, lineNumber);
    },

    findBreakpoint: function(sourceFileId, lineNumber)
    {
        return this._breakpointManager.breakpointsForUISourceCode(sourceFileId)[lineNumber];
    },

    _breakpointAdded: function(sourceFileId, lineNumber, condition, enabled)
    {
        var sourceFile = this._sourceFiles[sourceFileId];
        if (!sourceFile)
            return;
        var presentationBreakpoint = new WebInspector.PresentationBreakpoint(sourceFile, lineNumber, condition, enabled);
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, presentationBreakpoint);
    },

    _breakpointRemoved: function(sourceFileId, lineNumber)
    {
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointRemoved, { sourceFileId: sourceFileId, lineNumber: lineNumber });
    },

    _debuggerPaused: function()
    {
        var callFrames = WebInspector.debuggerModel.callFrames;
        this._presentationCallFrames = [];
        for (var i = 0; i < callFrames.length; ++i) {
            var callFrame = callFrames[i];
            var sourceFile;
            var script = WebInspector.debuggerModel.scriptForSourceID(callFrame.location.scriptId);
            if (script)
                sourceFile = this._sourceFileForScript(script.sourceURL, script.scriptId);
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

    _sourceFileForScript: function(sourceURL, scriptId)
    {
        if (!sourceURL) {
            var script = WebInspector.debuggerModel.scriptForSourceID(scriptId);
            if (!script)
                return;
            sourceURL = script.sourceURL;
        }
        return this._sourceFiles[this._createSourceFileId(sourceURL, scriptId)];
    },

    _scriptForSourceFileId: function(sourceFileId)
    {
        function filter(script)
        {
            return this._createSourceFileId(script.sourceURL, script.scriptId) === sourceFileId;
        }
        return WebInspector.debuggerModel.queryScripts(filter.bind(this))[0];
    },

    _createSourceFileId: function(sourceURL, scriptId)
    {
        var prefix = this._formatSourceFiles ? "deobfuscated:" : "";
        return prefix + (sourceURL || scriptId);
    },

    _debuggerReset: function()
    {
        this._sourceFiles = {};
        this._messages = [];
        this._sourceMappingListeners = [];
        this._presentationCallFrames = [];
        this._selectedCallFrameIndex = 0;
        this._breakpointManager.debuggerReset();
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
    }
}

WebInspector.PresenationCallFrame = function(callFrame, index, model, sourceFile)
{
    this._callFrame = callFrame;
    this._index = index;
    this._model = model;
    this._sourceFile = sourceFile;
    this._script = WebInspector.debuggerModel.scriptForSourceID(callFrame.location.scriptId);
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

    evaluate: function(code, objectGroup, includeCommandLineAPI, returnByValue, callback)
    {
        function didEvaluateOnCallFrame(error, result, wasThrown)
        {
            if (error) {
                console.error(error);
                callback(null);
                return;
            }

            if (returnByValue && !wasThrown)
                callback(result, wasThrown);
            else
                callback(WebInspector.RemoteObject.fromPayload(result), wasThrown);
        }
        DebuggerAgent.evaluateOnCallFrame(this._callFrame.id, code, objectGroup, includeCommandLineAPI, returnByValue, didEvaluateOnCallFrame.bind(this));
    },

    sourceLine: function(callback)
    {
        var location = this._callFrame.location;
        if (!this.isInternalScript)
            this._model._scriptLocationToUILocation(null, location.scriptId, location.lineNumber, location.columnNumber, callback);
        else
            callback(undefined, location.lineNumber);
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
