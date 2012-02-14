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
 * @extends {WebInspector.Object}
 */
WebInspector.DebuggerPresentationModel = function()
{
    // FIXME: apply formatter from outside as a generic mapping.
    this._formatter = new WebInspector.ScriptFormatter();
    this._rawSourceCodes = [];
    this._rawSourceCodeForScriptId = {};
    this._rawSourceCodeForURL = {};
    this._rawSourceCodeForDocumentURL = {};
    this._presentationCallFrames = [];

    this._breakpointManager = new WebInspector.BreakpointManager(WebInspector.settings.breakpoints, this._breakpointAdded.bind(this), this._breakpointRemoved.bind(this), WebInspector.debuggerModel);

    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, this._failedToParseScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerPaused, this._debuggerPaused, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerResumed, this._debuggerResumed, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.GlobalObjectCleared, this._debuggerReset, this);

    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.MessageAdded, this._consoleMessageAdded, this);
    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.ConsoleCleared, this._consoleCleared, this);

    new WebInspector.DebuggerPresentationModelResourceBinding(this);
}

WebInspector.DebuggerPresentationModel.Events = {
    UISourceCodeAdded: "source-file-added",
    UISourceCodeReplaced: "source-file-replaced",
    UISourceCodeRemoved: "source-file-removed",
    ConsoleMessageAdded: "console-message-added",
    ConsoleMessagesCleared: "console-messages-cleared",
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed",
    DebuggerPaused: "debugger-paused",
    DebuggerResumed: "debugger-resumed",
    DebuggerReset: "debugger-reset",
    CallFrameSelected: "call-frame-selected",
    ConsoleCommandEvaluatedInSelectedCallFrame: "console-command-evaluated-in-selected-call-frame",
    ExecutionLineChanged: "execution-line-changed"
}

WebInspector.DebuggerPresentationModel.prototype = {
    /**
     * @param {WebInspector.DebuggerPresentationModel.LinkifierFormatter=} formatter
     */
    createLinkifier: function(formatter)
    {
        return new WebInspector.DebuggerPresentationModel.Linkifier(this, formatter);
    },

    /**
     * @param {WebInspector.PresentationCallFrame} callFrame
     * @return {WebInspector.DebuggerPresentationModel.CallFramePlacard}
     */
    createPlacard: function(callFrame)
    {
        return new WebInspector.DebuggerPresentationModel.CallFramePlacard(callFrame);
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {?WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var rawSourceCode = this._rawSourceCodeForScriptId[rawLocation.scriptId];
        if (!rawSourceCode.sourceMapping)
            return null;
        return rawSourceCode.sourceMapping.rawLocationToUILocation(rawLocation);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _parsedScriptSource: function(event)
    {
        var script = /** @type {WebInspector.Script} */ event.data;
        this._addScript(script);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _failedToParseScriptSource: function(event)
    {
        var script = /** @type {WebInspector.Script} */ event.data;
        this._addScript(script);
    },

    /**
     * @param {WebInspector.Script} script
     */
    _addScript: function(script)
    {
        var resource = null;
        var isInlineScript = false;
        if (script.isInlineScript()) {
            resource = WebInspector.networkManager.inflightResourceForURL(script.sourceURL) || WebInspector.resourceForURL(script.sourceURL);
            if (resource && resource.type === WebInspector.Resource.Type.Document) {
                isInlineScript = true;
                var rawSourceCode = this._rawSourceCodeForDocumentURL[script.sourceURL];
                if (rawSourceCode) {
                    rawSourceCode.addScript(script);
                    this._bindScriptToRawSourceCode(script, rawSourceCode);
                    return;
                }
            }
        }

        var compilerSourceMapping = null;
        if (WebInspector.settings.sourceMapsEnabled.get() && script.sourceMapURL)
            compilerSourceMapping = new WebInspector.ClosureCompilerSourceMapping(script.sourceMapURL, script.sourceURL);

        var rawSourceCode = new WebInspector.RawSourceCode(script.scriptId, script, resource, this._formatter, this._formatSource, compilerSourceMapping);
        this._rawSourceCodes.push(rawSourceCode);
        this._bindScriptToRawSourceCode(script, rawSourceCode);

        if (isInlineScript)
            this._rawSourceCodeForDocumentURL[script.sourceURL] = rawSourceCode;

        if (rawSourceCode.sourceMapping)
            this._updateSourceMapping(rawSourceCode, null);
        rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._sourceMappingUpdated, this);
    },

    /**
     * @param {WebInspector.Script} script
     * @param {WebInspector.RawSourceCode} rawSourceCode
     */
    _bindScriptToRawSourceCode: function(script, rawSourceCode)
    {
        this._rawSourceCodeForScriptId[script.scriptId] = rawSourceCode;
        this._rawSourceCodeForURL[script.sourceURL] = rawSourceCode;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _sourceMappingUpdated: function(event)
    {
        var rawSourceCode = /** @type {WebInspector.RawSourceCode} */ event.target;
        var oldSourceMapping = /** @type {WebInspector.RawSourceCode.SourceMapping} */ event.data["oldSourceMapping"];
        this._updateSourceMapping(rawSourceCode, oldSourceMapping);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function()
    {
        var result = [];
        for (var i = 0; i < this._rawSourceCodes.length; ++i) {
            if (!this._rawSourceCodes[i].sourceMapping)
                continue;
            var uiSourceCodeList = this._rawSourceCodes[i].sourceMapping.uiSourceCodeList();
            for (var j = 0; j < uiSourceCodeList.length; ++j)
                result.push(uiSourceCodeList[j]);
        }
        return result;
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @param {WebInspector.RawSourceCode.SourceMapping} oldSourceMapping
     */
    _updateSourceMapping: function(rawSourceCode, oldSourceMapping)
    {
        if (oldSourceMapping) {
            var oldUISourceCodeList = oldSourceMapping.uiSourceCodeList();
            for (var i = 0; i < oldUISourceCodeList.length; ++i) {
                var breakpoints = this._breakpointManager.breakpointsForUISourceCode(oldUISourceCodeList[i]);
                for (var lineNumber in breakpoints) {
                    var breakpoint = breakpoints[lineNumber];
                    this._breakpointRemoved(breakpoint);
                    delete breakpoint.uiSourceCode;
                }
            }
        }

        this._restoreBreakpoints(rawSourceCode);
        this._restoreConsoleMessages(rawSourceCode);

        if (!oldSourceMapping) {
            var uiSourceCodeList = rawSourceCode.sourceMapping.uiSourceCodeList();
            for (var i = 0; i < uiSourceCodeList.length; ++i)
                this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeAdded, uiSourceCodeList[i]);
        } else {
            var eventData = { uiSourceCodeList: rawSourceCode.sourceMapping.uiSourceCodeList(), oldUISourceCodeList: oldSourceMapping.uiSourceCodeList() };
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeReplaced, eventData);
        }
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     */
    _restoreBreakpoints: function(rawSourceCode)
    {
        var uiSourceCodeList = rawSourceCode.sourceMapping.uiSourceCodeList();
        for (var i = 0; i < uiSourceCodeList.length; ++i) {
            var uiSourceCode = uiSourceCodeList[i];
            this._breakpointManager.uiSourceCodeAdded(uiSourceCode);
            var breakpoints = this._breakpointManager.breakpointsForUISourceCode(uiSourceCode);
            for (var lineNumber in breakpoints)
                this._breakpointAdded(breakpoints[lineNumber]);
        }

    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     */
    _restoreConsoleMessages: function(rawSourceCode)
    {
        var messages = rawSourceCode.messages;
        for (var i = 0; i < messages.length; ++i)
            messages[i]._presentationMessage = this._createPresentationMessage(messages[i], rawSourceCode.sourceMapping);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {boolean}
     */
    canEditScriptSource: function(uiSourceCode)
    {
        if (!WebInspector.debuggerModel.canSetScriptSource() || this._formatSource)
            return false;
        var rawSourceCode = uiSourceCode.rawSourceCode;
        var script = this._scriptForRawSourceCode(rawSourceCode);
        return script && !script.lineOffset && !script.columnOffset;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} newSource
     * @param {function(?Protocol.Error)} callback
     */
    setScriptSource: function(uiSourceCode, newSource, callback)
    {
        var rawSourceCode = uiSourceCode.rawSourceCode;
        var script = this._scriptForRawSourceCode(rawSourceCode);

        /**
         * @this {WebInspector.DebuggerPresentationModel}
         * @param {?Protocol.Error} error
         */
        function didEditScriptSource(error)
        {
            callback(error);
            if (error)
                return;

            var resource = WebInspector.resourceForURL(rawSourceCode.url);
            if (resource)
                resource.addRevision(newSource);

            uiSourceCode.contentChanged(newSource);

            if (WebInspector.debuggerModel.callFrames)
                this._debuggerPaused();
        }
        WebInspector.debuggerModel.setScriptSource(script.scriptId, newSource, didEditScriptSource.bind(this));
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} oldSource
     * @param {string} newSource
     */
    _updateBreakpointsAfterLiveEdit: function(uiSourceCode, oldSource, newSource)
    {
        var breakpoints = this._breakpointManager.breakpointsForUISourceCode(uiSourceCode);

        // Clear and re-create breakpoints according to text diff.
        var diff = Array.diff(oldSource.split("\n"), newSource.split("\n"));
        for (var lineNumber in breakpoints) {
            var breakpoint = breakpoints[lineNumber];

            this.removeBreakpoint(uiSourceCode, parseInt(lineNumber, 10));

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

    /**
     * @param {boolean} formatSource
     */
    setFormatSource: function(formatSource)
    {
        if (this._formatSource === formatSource)
            return;

        this._formatSource = formatSource;
        this._breakpointManager.reset();
        for (var i = 0; i < this._rawSourceCodes.length; ++i)
            this._rawSourceCodes[i].setFormatted(this._formatSource);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _consoleMessageAdded: function(event)
    {
        var message = /** @type {WebInspector.ConsoleMessage} */ event.data;
        if (!message.url || !message.isErrorOrWarning())
            return;

        var rawSourceCode = this._rawSourceCodeForScriptWithURL(message.url);
        if (!rawSourceCode)
            return;

        rawSourceCode.messages.push(message);
        if (rawSourceCode.sourceMapping) {
            message._presentationMessage = this._createPresentationMessage(message, rawSourceCode.sourceMapping);
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.ConsoleMessageAdded, message._presentationMessage);
        }
    },

    /**
     * @param {WebInspector.ConsoleMessage} message
     * @param {WebInspector.RawSourceCode.SourceMapping} sourceMapping
     * @return {WebInspector.PresentationConsoleMessage}
     */
    _createPresentationMessage: function(message, sourceMapping)
    {
        // FIXME(62725): stack trace line/column numbers are one-based.
        var lineNumber = message.stackTrace ? message.stackTrace[0].lineNumber - 1 : message.line - 1;
        var columnNumber = message.stackTrace ? message.stackTrace[0].columnNumber - 1 : 0;
        var uiLocation = sourceMapping.rawLocationToUILocation(/** @type {DebuggerAgent.Location} */ { lineNumber: lineNumber, columnNumber: columnNumber });
        var presentationMessage = new WebInspector.PresentationConsoleMessage(uiLocation.uiSourceCode, uiLocation.lineNumber, message);
        return presentationMessage;
    },

    _consoleCleared: function()
    {
        for (var i = 0; i < this._rawSourceCodes.length; ++i)
            this._rawSourceCodes[i].messages = [];
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.ConsoleMessagesCleared);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     */
    continueToLine: function(uiSourceCode, lineNumber)
    {
        // FIXME: use RawSourceCode.uiLocationToRawLocation.
        var rawLocation = uiSourceCode.rawSourceCode.sourceMapping.uiLocationToRawLocation(uiSourceCode, lineNumber, 0);
        WebInspector.debuggerModel.continueToLocation(rawLocation);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {Array.<WebInspector.Breakpoint>}
     */
    breakpointsForUISourceCode: function(uiSourceCode)
    {
        var breakpointsMap = this._breakpointManager.breakpointsForUISourceCode(uiSourceCode);
        var breakpointsList = [];
        for (var lineNumber in breakpointsMap)
            breakpointsList.push(breakpointsMap[lineNumber]);
        return breakpointsList;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {Array.<WebInspector.ConsoleMessage>}
     */
    messagesForUISourceCode: function(uiSourceCode)
    {
        var rawSourceCode = uiSourceCode.rawSourceCode;
        var messages = [];
        for (var i = 0; i < rawSourceCode.messages.length; ++i)
            messages.push(rawSourceCode.messages[i]._presentationMessage);
        return messages;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     */
    setBreakpoint: function(uiSourceCode, lineNumber, condition, enabled)
    {
        this._breakpointManager.setBreakpoint(uiSourceCode, lineNumber, condition, enabled);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {boolean} enabled
     */
    setBreakpointEnabled: function(uiSourceCode, lineNumber, enabled)
    {
        var breakpoint = this.findBreakpoint(uiSourceCode, lineNumber);
        if (!breakpoint)
            return;
        this._breakpointManager.removeBreakpoint(uiSourceCode, lineNumber);
        this._breakpointManager.setBreakpoint(uiSourceCode, lineNumber, breakpoint.condition, enabled);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     */
    updateBreakpoint: function(uiSourceCode, lineNumber, condition, enabled)
    {
        this._breakpointManager.removeBreakpoint(uiSourceCode, lineNumber);
        this._breakpointManager.setBreakpoint(uiSourceCode, lineNumber, condition, enabled);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     */
    removeBreakpoint: function(uiSourceCode, lineNumber)
    {
        this._breakpointManager.removeBreakpoint(uiSourceCode, lineNumber);
    },

    /**
     */
    removeAllBreakpoints: function()
    {
        this._breakpointManager.removeAllBreakpoints();
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @return {WebInspector.Breakpoint|undefined}
     */
    findBreakpoint: function(uiSourceCode, lineNumber)
    {
        return this._breakpointManager.breakpointsForUISourceCode(uiSourceCode)[lineNumber];
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     */
    _breakpointAdded: function(breakpoint)
    {
        if (breakpoint.uiSourceCode)
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointAdded, breakpoint);
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     */
    _breakpointRemoved: function(breakpoint)
    {
        if (breakpoint.uiSourceCode)
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.BreakpointRemoved, breakpoint);
    },

    _debuggerPaused: function()
    {
        var callFrames = WebInspector.debuggerModel.callFrames;
        this._presentationCallFrames = [];
        for (var i = 0; i < callFrames.length; ++i) {
            var callFrame = callFrames[i];
            var script = WebInspector.debuggerModel.scriptForSourceID(callFrame.location.scriptId);
            if (!script)
                continue;
            var rawSourceCode = this._rawSourceCodeForScript(script);
            this._presentationCallFrames.push(new WebInspector.PresentationCallFrame(callFrame, i, this, rawSourceCode));
        }
        var details = WebInspector.debuggerModel.debuggerPausedDetails;
        this.selectedCallFrame = this._presentationCallFrames[0];
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.DebuggerPaused, { callFrames: this._presentationCallFrames, details: details });
    },

    _debuggerResumed: function()
    {
        this._presentationCallFrames = [];
        this.selectedCallFrame = null;
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.DebuggerResumed);
    },

    get paused()
    {
        return !!WebInspector.debuggerModel.debuggerPausedDetails;
    },

    set selectedCallFrame(callFrame)
    {
        if (this._selectedCallFrame)
            this._selectedCallFrame.rawSourceCode.removeEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._dispatchExecutionLineChanged, this);
        this._selectedCallFrame = callFrame;
        if (!this._selectedCallFrame)
            return;

        this._selectedCallFrame.rawSourceCode.forceUpdateSourceMapping();
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.CallFrameSelected, callFrame);

        this._selectedCallFrame.rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._dispatchExecutionLineChanged, this);
    },

    get selectedCallFrame()
    {
        return this._selectedCallFrame;
    },

    /**
     * @param {function(?WebInspector.RemoteObject, boolean, RuntimeAgent.RemoteObject=)} callback
     */
    evaluateInSelectedCallFrame: function(code, objectGroup, includeCommandLineAPI, returnByValue, callback)
    {
        /**
         * @param {?RuntimeAgent.RemoteObject} result
         * @param {boolean} wasThrown
         */
        function didEvaluate(result, wasThrown)
        {
            if (returnByValue)
                callback(null, wasThrown, wasThrown ? null : result);
            else
                callback(WebInspector.RemoteObject.fromPayload(result), wasThrown);

            if (objectGroup === "console")
                this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.ConsoleCommandEvaluatedInSelectedCallFrame);
        }

        this.selectedCallFrame.evaluate(code, objectGroup, includeCommandLineAPI, returnByValue, didEvaluate.bind(this));
    },

    /**
     * @param {function(Object)} callback
     */
    getSelectedCallFrameVariables: function(callback)
    {
        var result = { this: true };

        var selectedCallFrame = this.selectedCallFrame;
        if (!selectedCallFrame)
            callback(result);

        var pendingRequests = 0;

        function propertiesCollected(properties)
        {
            for (var i = 0; properties && i < properties.length; ++i)
                result[properties[i].name] = true;
            if (--pendingRequests == 0)
                callback(result);
        }

        for (var i = 0; i < selectedCallFrame.scopeChain.length; ++i) {
            var scope = selectedCallFrame.scopeChain[i];
            var object = WebInspector.RemoteObject.fromPayload(scope.object);
            pendingRequests++;
            object.getAllProperties(propertiesCollected);
        }
    },

    /**
     * @param {WebInspector.Event} event
     */
    _dispatchExecutionLineChanged: function(event)
    {
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.ExecutionLineChanged, this.executionLineLocation);
    },

    /**
     * @type {WebInspector.UILocation}
     */
    get executionLineLocation()
    {
        if (!this._selectedCallFrame.rawSourceCode.sourceMapping)
            return;

        var rawLocation = this._selectedCallFrame._callFrame.location;
        var uiLocation = this._selectedCallFrame.rawSourceCode.sourceMapping.rawLocationToUILocation(rawLocation);
        return uiLocation;
    },

    /**
     * @param {string} sourceURL
     */
    _rawSourceCodeForScriptWithURL: function(sourceURL)
    {
        return this._rawSourceCodeForURL[sourceURL];
    },

    /**
     * @param {WebInspector.Script} script
     */
    _rawSourceCodeForScript: function(script)
    {
        return this._rawSourceCodeForScriptId[script.scriptId];
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     */
    _scriptForRawSourceCode: function(rawSourceCode)
    {
        /**
         * @this {WebInspector.DebuggerPresentationModel}
         * @param {WebInspector.Script} script
         * @return {boolean}
         */
        function filter(script)
        {
            return script.scriptId === rawSourceCode.id;
        }
        return WebInspector.debuggerModel.queryScripts(filter.bind(this))[0];
    },

    _debuggerReset: function()
    {
        for (var i = 0; i < this._rawSourceCodes.length; ++i) {
            var rawSourceCode = this._rawSourceCodes[i];
            if (rawSourceCode.sourceMapping) {
                var uiSourceCodeList = rawSourceCode.sourceMapping.uiSourceCodeList();
                for (var j = 0; j < uiSourceCodeList.length; ++j)
                    this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeRemoved, uiSourceCodeList[j]);
            }
            rawSourceCode.removeAllListeners();
        }
        this._rawSourceCodes = [];
        this._rawSourceCodeForScriptId = {};
        this._rawSourceCodeForURL = {};
        this._rawSourceCodeForDocumentURL = {};
        this._presentationCallFrames = [];
        this._selectedCallFrame = null;
        this._breakpointManager.debuggerReset();
        this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.DebuggerReset);
    }
}

WebInspector.DebuggerPresentationModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {WebInspector.UISourceCode} uiSourceCode
 * @param {number} lineNumber
 * @param {WebInspector.ConsoleMessage} originalMessage
 */
WebInspector.PresentationConsoleMessage = function(uiSourceCode, lineNumber, originalMessage)
{
    this.uiSourceCode = uiSourceCode;
    this.lineNumber = lineNumber;
    this.originalMessage = originalMessage;
}

/**
 * @constructor
 * @param {DebuggerAgent.CallFrame} callFrame
 * @param {number} index
 * @param {WebInspector.DebuggerPresentationModel} model
 * @param {WebInspector.RawSourceCode} rawSourceCode
 */
WebInspector.PresentationCallFrame = function(callFrame, index, model, rawSourceCode)
{
    this._callFrame = callFrame;
    this._index = index;
    this._model = model;
    this._rawSourceCode = rawSourceCode;
}

WebInspector.PresentationCallFrame.prototype = {
    /**
     * @return {string}
     */
    get type()
    {
        return this._callFrame.type;
    },

    /**
     * @return {Array.<DebuggerAgent.Scope>}
     */
    get scopeChain()
    {
        return this._callFrame.scopeChain;
    },

    /**
     * @return {RuntimeAgent.RemoteObject}
     */
    get this()
    {
        return this._callFrame.this;
    },

    /**
     * @return {number}
     */
    get index()
    {
        return this._index;
    },

    /**
     * @return {WebInspector.RawSourceCode}
     */
    get rawSourceCode()
    {
        return this._rawSourceCode;
    },

    /**
     * @param {string} code
     * @param {string} objectGroup
     * @param {boolean} includeCommandLineAPI
     * @param {boolean} returnByValue
     * @param {function(?RuntimeAgent.RemoteObject, boolean=)=} callback
     */
    evaluate: function(code, objectGroup, includeCommandLineAPI, returnByValue, callback)
    {
        /**
         * @this {WebInspector.PresentationCallFrame}
         * @param {?Protocol.Error} error
         * @param {RuntimeAgent.RemoteObject} result
         * @param {boolean=} wasThrown
         */
        function didEvaluateOnCallFrame(error, result, wasThrown)
        {
            if (error) {
                console.error(error);
                callback(null, false);
                return;
            }
            callback(result, wasThrown);
        }
        DebuggerAgent.evaluateOnCallFrame(this._callFrame.callFrameId, code, objectGroup, includeCommandLineAPI, returnByValue, didEvaluateOnCallFrame.bind(this));
    },

    /**
     * @param {function(WebInspector.UILocation)} callback
     */
    uiLocation: function(callback)
    {
        function sourceMappingReady()
        {
            this._rawSourceCode.removeEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, sourceMappingReady, this);
            callback(this._rawSourceCode.sourceMapping.rawLocationToUILocation(this._callFrame.location));
        }
        if (this._rawSourceCode.sourceMapping)
            sourceMappingReady.call(this);
        else
            this._rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, sourceMappingReady, this);
    }
}

/**
 * @constructor
 * @extends {WebInspector.Placard}
 * @param {WebInspector.PresentationCallFrame} callFrame
 */
WebInspector.DebuggerPresentationModel.CallFramePlacard = function(callFrame)
{
    WebInspector.Placard.call(this, callFrame._callFrame.functionName || WebInspector.UIString("(anonymous function)"), "");
    this._callFrame = callFrame;
    var rawSourceCode = callFrame._rawSourceCode;
    if (rawSourceCode.sourceMapping)
        this._update();
    rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._update, this);
}

WebInspector.DebuggerPresentationModel.CallFramePlacard.prototype = {
    discard: function()
    {
        this._callFrame._rawSourceCode.removeEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._update, this);
    },

    _update: function()
    {
        var rawSourceCode = this._callFrame._rawSourceCode;
        var uiLocation = rawSourceCode.sourceMapping.rawLocationToUILocation(this._callFrame._callFrame.location);
        this.subtitle = WebInspector.displayNameForURL(uiLocation.uiSourceCode.url) + ":" + (uiLocation.lineNumber + 1);
    }
}

WebInspector.DebuggerPresentationModel.CallFramePlacard.prototype.__proto__ = WebInspector.Placard.prototype;

/**
 * @constructor
 * @implements {WebInspector.ResourceDomainModelBinding}
 * @param {WebInspector.DebuggerPresentationModel} model
 */
WebInspector.DebuggerPresentationModelResourceBinding = function(model)
{
    this._presentationModel = model;
    WebInspector.Resource.registerDomainModelBinding(WebInspector.Resource.Type.Script, this);
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype = {
    /**
     * @param {WebInspector.Resource} resource
     */
    canSetContent: function(resource)
    {
        var rawSourceCode = this._presentationModel._rawSourceCodeForScriptWithURL(resource.url)
        if (!rawSourceCode)
            return false;
        return this._presentationModel.canEditScriptSource(rawSourceCode.sourceMapping.uiSourceCodeList()[0]);
    },

    /**
     * @param {WebInspector.Resource} resource
     * @param {string} content
     * @param {boolean} majorChange
     * @param {function(?string)} userCallback
     */
    setContent: function(resource, content, majorChange, userCallback)
    {
        if (!majorChange)
            return;

        var rawSourceCode = this._presentationModel._rawSourceCodeForScriptWithURL(resource.url);
        if (!rawSourceCode) {
            userCallback("Resource is not editable");
            return;
        }

        resource.requestContent(this._setContentWithInitialContent.bind(this, rawSourceCode.sourceMapping.uiSourceCodeList()[0], content, userCallback));
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} content
     * @param {function(?string)} userCallback
     * @param {?string} oldContent
     * @param {?string} oldContentEncoded
     */
    _setContentWithInitialContent: function(uiSourceCode, content, userCallback, oldContent, oldContentEncoded)
    {
        /**
         * @this {WebInspector.DebuggerPresentationModelResourceBinding}
         * @param {?string} error
         */
        function callback(error)
        {
            if (userCallback)
                userCallback(error);
            if (!error)
                this._presentationModel._updateBreakpointsAfterLiveEdit(uiSourceCode, oldContent || "", content);
        }
        this._presentationModel.setScriptSource(uiSourceCode, content, callback.bind(this));
    }
}

/**
 * @interface
 */
WebInspector.DebuggerPresentationModel.LinkifierFormatter = function()
{
}

WebInspector.DebuggerPresentationModel.LinkifierFormatter.prototype = {
    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @param {Element} anchor
     */
    formatRawSourceCodeAnchor: function(rawSourceCode, anchor) { },
}

/**
 * @constructor
 * @implements {WebInspector.DebuggerPresentationModel.LinkifierFormatter}
 * @param {number=} maxLength
 */
WebInspector.DebuggerPresentationModel.DefaultLinkifierFormatter = function(maxLength)
{
    this._maxLength = maxLength;
}

WebInspector.DebuggerPresentationModel.DefaultLinkifierFormatter.prototype = {
    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @param {Element} anchor
     */
    formatRawSourceCodeAnchor: function(rawSourceCode, anchor)
    {
        var uiLocation = rawSourceCode.sourceMapping.rawLocationToUILocation(anchor.rawLocation);

        anchor.textContent = WebInspector.formatLinkText(uiLocation.uiSourceCode.url, uiLocation.lineNumber);

        var text = WebInspector.formatLinkText(uiLocation.uiSourceCode.url, uiLocation.lineNumber);
        if (this._maxLength)
            text = text.trimMiddle(this._maxLength);
        anchor.textContent = text;
    }
}

WebInspector.DebuggerPresentationModel.DefaultLinkifierFormatter.prototype.__proto__ = WebInspector.DebuggerPresentationModel.LinkifierFormatter.prototype;

/**
 * @constructor
 * @param {WebInspector.DebuggerPresentationModel} model
 * @param {WebInspector.DebuggerPresentationModel.LinkifierFormatter=} formatter
 */
WebInspector.DebuggerPresentationModel.Linkifier = function(model, formatter)
{
    this._model = model;
    this._formatter = formatter || new WebInspector.DebuggerPresentationModel.DefaultLinkifierFormatter();
    this._anchorsForRawSourceCode = {};
}

WebInspector.DebuggerPresentationModel.Linkifier.prototype = {
    /**
     * @param {string} sourceURL
     * @param {number} lineNumber
     * @param {number=} columnNumber
     * @param {string=} classes
     */
    linkifyLocation: function(sourceURL, lineNumber, columnNumber, classes)
    {
        var rawSourceCode = this._model._rawSourceCodeForScriptWithURL(sourceURL);
        if (!rawSourceCode)
            return WebInspector.linkifyResourceAsNode(sourceURL, lineNumber, classes);

        return this.linkifyRawSourceCode(rawSourceCode, lineNumber, columnNumber, classes);
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @param {number=} lineNumber
     * @param {number=} columnNumber
     * @param {string=} classes
     */
    linkifyRawSourceCode: function(rawSourceCode, lineNumber, columnNumber, classes)
    {
        var anchor = WebInspector.linkifyURLAsNode(rawSourceCode.url, "", classes, false);
        anchor.rawLocation = { lineNumber: lineNumber, columnNumber: columnNumber };

        var anchors = this._anchorsForRawSourceCode[rawSourceCode.id];
        if (!anchors) {
            anchors = [];
            this._anchorsForRawSourceCode[rawSourceCode.id] = anchors;
            rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._updateSourceAnchors, this);
        }

        if (rawSourceCode.sourceMapping)
            this._updateAnchor(rawSourceCode, anchor);
        anchors.push(anchor);
        return anchor;
    },

    linkifyFunctionLocation: function(functionLocation, classes)
    {
        var rawSourceCode = this._model._rawSourceCodeForScriptId[functionLocation.scriptId];
        if (!rawSourceCode)
            return undefined;
        return this.linkifyRawSourceCode(rawSourceCode, functionLocation.lineNumber, functionLocation.columnNumber, classes);
    },

    reset: function()
    {
        for (var id in this._anchorsForRawSourceCode) {
            if (this._model._rawSourceCodeForScriptId[id]) // In case of navigation the list of rawSourceCodes is empty.
                this._model._rawSourceCodeForScriptId[id].removeEventListener(WebInspector.RawSourceCode.Events.SourceMappingUpdated, this._updateSourceAnchors, this);
        }
        this._anchorsForRawSourceCode = {};
    },

    /**
     * @param {WebInspector.Event} event
     */
    _updateSourceAnchors: function(event)
    {
        var rawSourceCode = /** @type {WebInspector.RawSourceCode} */ event.target;
        var anchors = this._anchorsForRawSourceCode[rawSourceCode.id];
        for (var i = 0; i < anchors.length; ++i)
            this._updateAnchor(rawSourceCode, anchors[i]);
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @param {Element} anchor
     */
    _updateAnchor: function(rawSourceCode, anchor)
    {
        var uiLocation = rawSourceCode.sourceMapping.rawLocationToUILocation(anchor.rawLocation);
        anchor.preferredPanel = "scripts";
        anchor.uiSourceCode = uiLocation.uiSourceCode;
        anchor.lineNumber = uiLocation.lineNumber;

        this._formatter.formatRawSourceCodeAnchor(rawSourceCode, anchor);
    }
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype.__proto__ = WebInspector.ResourceDomainModelBinding.prototype;

/**
 * @type {?WebInspector.DebuggerPresentationModel}
 */
WebInspector.debuggerPresentationModel = null;
