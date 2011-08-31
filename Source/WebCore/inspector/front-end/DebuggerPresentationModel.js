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

/**
 * @constructor
 */
WebInspector.DebuggerPresentationModel = function()
{
    // FIXME: apply formatter from outside as a generic mapping.
    this._formatter = new WebInspector.ScriptFormatter();
    this._rawSourceCode = {};
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

    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.MessageAdded, this._consoleMessageAdded, this);
    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.ConsoleCleared, this._consoleCleared, this);

    new WebInspector.DebuggerPresentationModelResourceBinding(this);
}

WebInspector.DebuggerPresentationModel.Events = {
    UISourceCodeAdded: "source-file-added",
    UISourceCodeReplaced: "source-file-replaced",
    ConsoleMessageAdded: "console-message-added",
    ConsoleMessagesCleared: "console-messages-cleared",
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed",
    DebuggerPaused: "debugger-paused",
    DebuggerResumed: "debugger-resumed",
    CallFrameSelected: "call-frame-selected"
}

WebInspector.DebuggerPresentationModel.prototype = {
    _scriptLocationToUILocation: function(sourceURL, scriptId, lineNumber, columnNumber, callback)
    {
        var rawSourceCode = this._rawSourceCodeForScript(sourceURL, scriptId);

        function didCreateSourceMapping()
        {
            var uiLocation = rawSourceCode.rawLocationToUILocation({ lineNumber: lineNumber, columnNumber: columnNumber });
            callback(uiLocation.uiSourceCode, uiLocation.lineNumber);
        }
        // FIXME: force source formatting if needed. This will go away once formatting
        // is fully encapsulated in RawSourceCode class.
        rawSourceCode.createSourceMappingIfNeeded(didCreateSourceMapping);
    },

    _uiLocationToScriptLocation: function(uiSourceCode, lineNumber, callback)
    {
        var rawSourceCode = uiSourceCode.rawSourceCode;

        function didCreateSourceMapping()
        {
            var rawLocation = rawSourceCode.uiLocationToRawLocation(lineNumber, 0);
            callback(rawLocation);
        }
        // FIXME: force source formatting if needed. This will go away once formatting
        // is fully encapsulated in RawSourceCode class.
        rawSourceCode.createSourceMappingIfNeeded(didCreateSourceMapping);
    },

    addSourceMappingListener: function(sourceURL, scriptId, listener)
    {
        this._sourceMappingListeners.push(listener);
    },

    removeSourceMappingListener: function(sourceURL, scriptId, listener)
    {
        // FIXME: implement this.
    },

    linkifyLocation: function(sourceURL, lineNumber, columnNumber, classes)
    {
        var linkText = WebInspector.formatLinkText(sourceURL, lineNumber);
        var anchor = WebInspector.linkifyURLAsNode(sourceURL, linkText, classes, false);

        var rawSourceCode = this._rawSourceCodeForScript(sourceURL);
        if (!rawSourceCode) {
            anchor.setAttribute("preferred_panel", "resources");
            anchor.setAttribute("line_number", lineNumber);
            return anchor;
        }

        function updateAnchor()
        {
            function didGetLocation(uiSourceCode, lineNumber)
            {
                anchor.textContent = WebInspector.formatLinkText(uiSourceCode.url, lineNumber);
                anchor.setAttribute("preferred_panel", "scripts");
                anchor.uiSourceCode = uiSourceCode;
                anchor.lineNumber = lineNumber;
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
        var rawSourceCodeId = this._createRawSourceCodeId(script.sourceURL, script.scriptId);
        var rawSourceCode = this._rawSourceCode[rawSourceCodeId];
        if (rawSourceCode) {
            rawSourceCode.addScript(script);
            return;
        }

        rawSourceCode = new WebInspector.RawSourceCode(rawSourceCodeId, script, this._formatter, this._formatSource);
        this._rawSourceCode[rawSourceCodeId] = rawSourceCode;
        rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.UISourceCodeReplaced, this._uiSourceCodeReplaced, this);

        function didCreateSourceMapping()
        {
            this._breakpointManager.uiSourceCodeAdded(rawSourceCode.uiSourceCode);
            var breakpoints = this._breakpointManager.breakpointsForUISourceCode(rawSourceCode.uiSourceCode);
            for (var lineNumber in breakpoints)
                this._breakpointAdded(breakpoints[lineNumber]);
        }
        // FIXME: force source formatting if needed. This will go away once formatting
        // is fully encapsulated in RawSourceCode class.
        rawSourceCode.createSourceMappingIfNeeded(didCreateSourceMapping.bind(this));

        var uiSourceCode = rawSourceCode.uiSourceCode;
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeAdded, uiSourceCode);
    },

    _uiSourceCodeReplaced: function(event)
    {
        // FIXME: restore breakpoints in new source code (currently we just recreate everything when switching to pretty-print mode).
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeReplaced, event.data);
    },

    canEditScriptSource: function(uiSourceCode)
    {
        if (!Preferences.canEditScriptSource || this._formatSource)
            return false;
        var rawSourceCode = uiSourceCode.rawSourceCode;
        var script = this._scriptForRawSourceCode(rawSourceCode);
        return !script.lineOffset && !script.columnOffset;
    },

    setScriptSource: function(uiSourceCode, newSource, callback)
    {
        var rawSourceCode = uiSourceCode.rawSourceCode;
        var script = this._scriptForRawSourceCode(rawSourceCode);

        function didEditScriptSource(error)
        {
            if (!error) {
                rawSourceCode.content = newSource;

                var resource = WebInspector.resourceForURL(rawSourceCode.url);
                if (resource)
                    resource.addRevision(newSource);
            }

            callback(error);

            if (!error && WebInspector.debuggerModel.callFrames)
                this._debuggerPaused();
        }
        WebInspector.debuggerModel.setScriptSource(script.scriptId, newSource, didEditScriptSource.bind(this));
    },

    _updateBreakpointsAfterLiveEdit: function(uiSourceCode, oldSource, newSource)
    {
        var breakpoints = this._breakpointManager.breakpointsForUISourceCode(uiSourceCode);

        // Clear and re-create breakpoints according to text diff.
        var diff = Array.diff(oldSource.split("\n"), newSource.split("\n"));
        for (var lineNumber in breakpoints) {
            var breakpoint = breakpoints[lineNumber];

            this.removeBreakpoint(uiSourceCode, lineNumber);

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
                this.setBreakpoint(uiSourceCode, newLineNumber, breakpoint.condition, breakpoint.enabled);
        }
    },

    setFormatSource: function(formatSource)
    {
        if (this._formatSource === formatSource)
            return;

        this._formatSource = formatSource;

        this._breakpointManager.reset();
        this._rawSourceCode = {};
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

        var rawSourceCode = this._rawSourceCodeForScript(message.url);
        if (!rawSourceCode)
            return;

        function didGetUILocation(uiSourceCode, lineNumber)
        {
            var presentationMessage = {};
            presentationMessage.uiSourceCode = uiSourceCode;
            presentationMessage.lineNumber = lineNumber;
            presentationMessage.originalMessage = message;
            uiSourceCode.messages.push(presentationMessage);
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
        for (var id in this._rawSourceCode)
            this._rawSourceCode[id].messages = [];
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.ConsoleMessagesCleared);
    },

    continueToLine: function(uiSourceCode, lineNumber)
    {
        function didGetScriptLocation(location)
        {
            WebInspector.debuggerModel.continueToLocation(location);
        }
        this._uiLocationToScriptLocation(uiSourceCode, lineNumber, didGetScriptLocation);
    },

    breakpointsForUISourceCode: function(uiSourceCode)
    {
        var breakpointsMap = this._breakpointManager.breakpointsForUISourceCode(uiSourceCode);
        var breakpointsList = [];
        for (var lineNumber in breakpointsMap)
            breakpointsList.push(breakpointsMap[lineNumber]);
        return breakpointsList;
    },

    setBreakpoint: function(uiSourceCode, lineNumber, condition, enabled)
    {
        this._breakpointManager.setBreakpoint(uiSourceCode, lineNumber, condition, enabled);
    },

    setBreakpointEnabled: function(uiSourceCode, lineNumber, enabled)
    {
        var breakpoint = this.findBreakpoint(uiSourceCode, lineNumber);
        if (!breakpoint)
            return;
        this._breakpointManager.removeBreakpoint(uiSourceCode, lineNumber);
        this._breakpointManager.setBreakpoint(uiSourceCode, lineNumber, breakpoint.condition, enabled);
    },

    updateBreakpoint: function(uiSourceCode, lineNumber, condition, enabled)
    {
        this._breakpointManager.removeBreakpoint(uiSourceCode, lineNumber);
        this._breakpointManager.setBreakpoint(uiSourceCode, lineNumber, condition, enabled);
    },

    removeBreakpoint: function(uiSourceCode, lineNumber)
    {
        this._breakpointManager.removeBreakpoint(uiSourceCode, lineNumber);
    },

    findBreakpoint: function(uiSourceCode, lineNumber)
    {
        return this._breakpointManager.breakpointsForUISourceCode(uiSourceCode)[lineNumber];
    },

    _breakpointAdded: function(breakpoint)
    {
        if (!breakpoint.uiSourceCode)
            return;
        var presentationBreakpoint = new WebInspector.PresentationBreakpoint(breakpoint.uiSourceCode, breakpoint.lineNumber, breakpoint.condition, breakpoint.enabled);
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, presentationBreakpoint);
    },

    _breakpointRemoved: function(breakpoint)
    {
        if (!breakpoint.uiSourceCode)
            return;
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointRemoved, { uiSourceCode: breakpoint.uiSourceCode, lineNumber: breakpoint.lineNumber });
    },

    _debuggerPaused: function()
    {
        var callFrames = WebInspector.debuggerModel.callFrames;
        this._presentationCallFrames = [];
        for (var i = 0; i < callFrames.length; ++i) {
            var callFrame = callFrames[i];
            var rawSourceCode;
            var script = WebInspector.debuggerModel.scriptForSourceID(callFrame.location.scriptId);
            if (script)
                rawSourceCode = this._rawSourceCodeForScript(script.sourceURL, script.scriptId);
            this._presentationCallFrames.push(new WebInspector.PresenationCallFrame(callFrame, i, this, rawSourceCode));
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

    _rawSourceCodeForScript: function(sourceURL, scriptId)
    {
        if (!sourceURL) {
            var script = WebInspector.debuggerModel.scriptForSourceID(scriptId);
            if (!script)
                return;
            sourceURL = script.sourceURL;
        }
        return this._rawSourceCode[this._createRawSourceCodeId(sourceURL, scriptId)];
    },

    _scriptForRawSourceCode: function(rawSourceCode)
    {
        function filter(script)
        {
            return this._createRawSourceCodeId(script.sourceURL, script.scriptId) === rawSourceCode.id;
        }
        return WebInspector.debuggerModel.queryScripts(filter.bind(this))[0];
    },

    _createRawSourceCodeId: function(sourceURL, scriptId)
    {
        var prefix = this._formatSource ? "deobfuscated:" : "";
        return prefix + (sourceURL || scriptId);
    },

    _debuggerReset: function()
    {
        this._rawSourceCode = {};
        this._messages = [];
        this._sourceMappingListeners = [];
        this._presentationCallFrames = [];
        this._selectedCallFrameIndex = 0;
        this._breakpointManager.debuggerReset();
    }
}

WebInspector.DebuggerPresentationModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 */
WebInspector.PresentationBreakpoint = function(uiSourceCode, lineNumber, condition, enabled)
{
    this.uiSourceCode = uiSourceCode;
    this.lineNumber = lineNumber;
    this.condition = condition;
    this.enabled = enabled;
}

WebInspector.PresentationBreakpoint.prototype = {
    get url()
    {
        return this.uiSourceCode.url;
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
        this.uiSourceCode.requestContent(didRequestContent.bind(this));
    }
}

/**
 * @constructor
 */
WebInspector.PresenationCallFrame = function(callFrame, index, model, uiSourceCode)
{
    this._callFrame = callFrame;
    this._index = index;
    this._model = model;
    this._uiSourceCode = uiSourceCode;
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
        if (this._uiSourceCode)
            return this._uiSourceCode.url;
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
        if (this._uiSourceCode)
            this._uiSourceCode.forceLoadContent(this._script);
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

/**
 * @constructor
 * @extends {WebInspector.ResourceDomainModelBinding}
 */
WebInspector.DebuggerPresentationModelResourceBinding = function(model)
{
    this._presentationModel = model;
    WebInspector.Resource.registerDomainModelBinding(WebInspector.Resource.Type.Script, this);
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype = {
    canSetContent: function(resource)
    {
        var rawSourceCode = this._presentationModel._rawSourceCodeForScript(resource.url)
        if (!rawSourceCode)
            return false;
        return this._presentationModel.canEditScriptSource(rawSourceCode.id);
    },

    setContent: function(resource, content, majorChange, userCallback)
    {
        if (!majorChange)
            return;

        var rawSourceCode = this._presentationModel._rawSourceCodeForScript(resource.url);
        if (!rawSourceCode) {
            userCallback("Resource is not editable");
            return;
        }

        resource.requestContent(this._setContentWithInitialContent.bind(this, rawSourceCode, content, userCallback));
    },

    _setContentWithInitialContent: function(rawSourceCode, content, userCallback, oldContent)
    {
        function callback(error)
        {
            if (userCallback)
                userCallback(error);
            if (!error) {
                this._presentationModel._updateBreakpointsAfterLiveEdit(rawSourceCode.id, oldContent, content);
                rawSourceCode.reload();
            }
        }
        this._presentationModel.setScriptSource(rawSourceCode.id, content, callback.bind(this));
    }
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype.__proto__ = WebInspector.ResourceDomainModelBinding.prototype;

/**
 * @type {?WebInspector.DebuggerPresentationModel}
 */
WebInspector.debuggerPresentationModel = null;
