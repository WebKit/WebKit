/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @param {WebInspector.Workspace} workspace
 */
WebInspector.ScriptSnippetModel = function(workspace)
{
    this._workspace = workspace;
    this._uiSourceCodeForScriptId = {};
    this._scriptForUISourceCode = new Map();
    this._snippetJavaScriptSourceForSnippetId = {};
    this._snippetIdForJavaScriptSource = new Map();
    
    this._snippetStorage = new WebInspector.SnippetStorage("script", "Script snippet #");
    this._lastSnippetEvaluationIndexSetting = WebInspector.settings.createSetting("lastSnippetEvaluationIndex", 0);
    this._snippetScriptMapping = new WebInspector.SnippetScriptMapping(this);
    this._workspace.addEventListener(WebInspector.Workspace.Events.ProjectWillReset, this._projectWillReset, this);
    this._workspace.addEventListener(WebInspector.Workspace.Events.ProjectDidReset, this._projectDidReset, this);
    this._loadSnippets();
}

WebInspector.ScriptSnippetModel.snippetSourceURLPrefix = "snippets:///";

WebInspector.ScriptSnippetModel.prototype = {
    /**
     * @return {WebInspector.SnippetScriptMapping}
     */
    get scriptMapping()
    {
        return this._snippetScriptMapping;
    },

    _loadSnippets: function()
    {
        var snippets = this._snippetStorage.snippets();
        for (var i = 0; i < snippets.length; ++i)
            this._addScriptSnippet(snippets[i]);
    },

    /**
     * @return {WebInspector.SnippetJavaScriptSource}
     */
    createScriptSnippet: function()
    {
        var snippet = this._snippetStorage.createSnippet();
        return this._addScriptSnippet(snippet);
    },

    /**
     * @param {WebInspector.Snippet} snippet
     * @return {WebInspector.SnippetJavaScriptSource}
     */
    _addScriptSnippet: function(snippet)
    {
        var snippetJavaScriptSource = new WebInspector.SnippetJavaScriptSource(snippet.name, new WebInspector.SnippetContentProvider(snippet), this);
        snippetJavaScriptSource.hasDivergedFromVM = true;
        this._snippetIdForJavaScriptSource.put(snippetJavaScriptSource, snippet.id);
        snippetJavaScriptSource.setSourceMapping(this._snippetScriptMapping); 
        this._snippetJavaScriptSourceForSnippetId[snippet.id] = snippetJavaScriptSource;
        this._workspace.project().addUISourceCode(snippetJavaScriptSource);
        return snippetJavaScriptSource;
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     */
    deleteScriptSnippet: function(snippetJavaScriptSource)
    {
        var snippetId = this._snippetIdForJavaScriptSource.get(snippetJavaScriptSource);
        var snippet = this._snippetStorage.snippetForId(snippetId);
        this._snippetStorage.deleteSnippet(snippet);
        this._removeBreakpoints(snippetJavaScriptSource);
        this._releaseSnippetScript(snippetJavaScriptSource);
        delete this._snippetJavaScriptSourceForSnippetId[snippet.id];
        this._snippetIdForJavaScriptSource.remove(snippetJavaScriptSource);
        this._workspace.project().removeUISourceCode(snippetJavaScriptSource);
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @param {string} newName
     */
    renameScriptSnippet: function(snippetJavaScriptSource, newName)
    {
        var breakpointLocations = this._removeBreakpoints(snippetJavaScriptSource);
        var snippetId = this._snippetIdForJavaScriptSource.get(snippetJavaScriptSource);
        var snippet = this._snippetStorage.snippetForId(snippetId);
        if (!snippet || !newName || snippet.name === newName)
            return;
        snippet.name = newName;
        snippetJavaScriptSource.urlChanged(snippet.name);
        this._restoreBreakpoints(snippetJavaScriptSource, breakpointLocations);
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @param {string} newContent
     */
    _setScriptSnippetContent: function(snippetJavaScriptSource, newContent)
    {
        var snippetId = this._snippetIdForJavaScriptSource.get(snippetJavaScriptSource);
        var snippet = this._snippetStorage.snippetForId(snippetId);
        snippet.content = newContent;
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     */
    _scriptSnippetEdited: function(snippetJavaScriptSource)
    {
        var script = this._scriptForUISourceCode.get(snippetJavaScriptSource);
        if (!script)
            return;
        
        var breakpointLocations = this._removeBreakpoints(snippetJavaScriptSource);
        var uiSourceCode = this._releaseSnippetScript(snippetJavaScriptSource);
        this._restoreBreakpoints(snippetJavaScriptSource, breakpointLocations);
        if (uiSourceCode)
            this._restoreBreakpoints(uiSourceCode, breakpointLocations);
    },

    /**
     * @param {string} snippetId
     * @return {number}
     */
    _nextEvaluationIndex: function(snippetId)
    {
        var evaluationIndex = this._lastSnippetEvaluationIndexSetting.get() + 1;
        this._lastSnippetEvaluationIndexSetting.set(evaluationIndex);
        return evaluationIndex;
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     */
    evaluateScriptSnippet: function(snippetJavaScriptSource)
    {
        var breakpointLocations = this._removeBreakpoints(snippetJavaScriptSource);
        this._releaseSnippetScript(snippetJavaScriptSource);
        this._restoreBreakpoints(snippetJavaScriptSource, breakpointLocations);
        var snippetId = this._snippetIdForJavaScriptSource.get(snippetJavaScriptSource);
        var evaluationIndex = this._nextEvaluationIndex(snippetId);
        snippetJavaScriptSource._evaluationIndex = evaluationIndex;
        var evaluationUrl = this._evaluationSourceURL(snippetJavaScriptSource);

        var expression = snippetJavaScriptSource.workingCopy();
        
        // In order to stop on the breakpoints during the snippet evaluation we need to compile and run it separately.
        // If separate compilation and execution is not supported by the port we fall back to evaluation in console.
        // In case we don't need that since debugger is already paused.
        // We do the same when we are stopped on the call frame  since debugger is already paused and can not stop on breakpoint anymore.
        if (WebInspector.debuggerModel.selectedCallFrame() || !Capabilities.separateScriptCompilationAndExecutionEnabled) {
            expression = snippetJavaScriptSource.workingCopy() + "\n//@ sourceURL=" + evaluationUrl + "\n";
            WebInspector.evaluateInConsole(expression, true);
            return;
        }
        
        WebInspector.showConsole();
        DebuggerAgent.compileScript(expression, evaluationUrl, compileCallback.bind(this));

        /**
         * @param {?string} error
         * @param {string=} scriptId
         * @param {string=} syntaxErrorMessage
         */
        function compileCallback(error, scriptId, syntaxErrorMessage)
        {
            if (!snippetJavaScriptSource || snippetJavaScriptSource._evaluationIndex !== evaluationIndex)
                return;

            if (error) {
                console.error(error);
                return;
            }

            if (!scriptId) {
                var consoleMessage = WebInspector.ConsoleMessage.create(
                        WebInspector.ConsoleMessage.MessageSource.JS,
                        WebInspector.ConsoleMessage.MessageLevel.Error,
                        syntaxErrorMessage || "");
                WebInspector.console.addMessage(consoleMessage);
                return;
            }

            var breakpointLocations = this._removeBreakpoints(snippetJavaScriptSource);
            this._restoreBreakpoints(snippetJavaScriptSource, breakpointLocations);

            this._runScript(scriptId);
        }
    },

    /**
     * @param {DebuggerAgent.ScriptId} scriptId
     */
    _runScript: function(scriptId)
    {
        var currentExecutionContext = WebInspector.runtimeModel.currentExecutionContext();
        DebuggerAgent.runScript(scriptId, currentExecutionContext ? currentExecutionContext.id : undefined, "console", false, runCallback.bind(this));

        /**
         * @param {?string} error
         * @param {?RuntimeAgent.RemoteObject} result
         * @param {boolean=} wasThrown
         */
        function runCallback(error, result, wasThrown)
        {
            if (error) {
                console.error(error);
                return;
            }

            this._printRunScriptResult(result, wasThrown);
        }
    },

    /**
     * @param {?RuntimeAgent.RemoteObject} result
     * @param {boolean=} wasThrown
     */
    _printRunScriptResult: function(result, wasThrown)
    {
        var level = (wasThrown ? WebInspector.ConsoleMessage.MessageLevel.Error : WebInspector.ConsoleMessage.MessageLevel.Log);
        var message = WebInspector.ConsoleMessage.create(WebInspector.ConsoleMessage.MessageSource.JS, level, "", undefined, undefined, undefined, undefined, [result]);
        WebInspector.console.addMessage(message)
    },

    /**
     * @param {WebInspector.DebuggerModel.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    _rawLocationToUILocation: function(rawLocation)
    {
        var uiSourceCode = this._uiSourceCodeForScriptId[rawLocation.scriptId];
        return new WebInspector.UILocation(uiSourceCode, rawLocation.lineNumber, rawLocation.columnNumber || 0);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.DebuggerModel.Location}
     */
    _uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var script = this._scriptForUISourceCode.get(uiSourceCode);
        if (!script)
            return null;

        return WebInspector.debuggerModel.createRawLocation(script, lineNumber, columnNumber);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    _releasedUISourceCodes: function()
    {
        var result = [];
        for (var scriptId in this._uiSourceCodeForScriptId) {
            var uiSourceCode = this._uiSourceCodeForScriptId[scriptId];
            if (!uiSourceCode.isSnippet)
                result.push(uiSourceCode);
        }
        return result;
    },

    /**
     * @param {WebInspector.Script} script
     */
    _addScript: function(script)
    {
        var snippetId = this._snippetIdForSourceURL(script.sourceURL);
        var snippetJavaScriptSource = this._snippetJavaScriptSourceForSnippetId[snippetId];
        
        if (!snippetJavaScriptSource || this._evaluationSourceURL(snippetJavaScriptSource) !== script.sourceURL) {
            this._createUISourceCodeForScript(script);
            return;
        }
        
        console.assert(!this._scriptForUISourceCode.get(snippetJavaScriptSource));
        this._uiSourceCodeForScriptId[script.scriptId] = snippetJavaScriptSource;
        this._scriptForUISourceCode.put(snippetJavaScriptSource, script);
        delete snippetJavaScriptSource.hasDivergedFromVM;
        script.setSourceMapping(this._snippetScriptMapping);
    },

    /**
     * @param {WebInspector.Script} script
     * @return {WebInspector.UISourceCode} uiSourceCode
     */
    _createUISourceCodeForScript: function(script)
    {
        var uiSourceCode = new WebInspector.JavaScriptSource(script.sourceURL, script, false);
        uiSourceCode.setSourceMapping(this._snippetScriptMapping);
        // FIXME: Should be added to workspace as temporary.
        uiSourceCode.isTemporary = true;
        uiSourceCode.isSnippetEvaluation = true;
        this._uiSourceCodeForScriptId[script.scriptId] = uiSourceCode;
        this._scriptForUISourceCode.put(uiSourceCode, script);
        script.setSourceMapping(this._snippetScriptMapping);
        return uiSourceCode;
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @return {Array.<Object>}
     */
    _removeBreakpoints: function(snippetJavaScriptSource)
    {
        var breakpointLocations = WebInspector.breakpointManager.breakpointLocationsForUISourceCode(snippetJavaScriptSource);
        for (var i = 0; i < breakpointLocations.length; ++i)
            breakpointLocations[i].breakpoint.remove();
        return breakpointLocations;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {Array.<Object>} breakpointLocations
     */
    _restoreBreakpoints: function(uiSourceCode, breakpointLocations)
    {
        for (var i = 0; i < breakpointLocations.length; ++i) {
            var uiLocation = breakpointLocations[i].uiLocation;
            var breakpoint = breakpointLocations[i].breakpoint;
            WebInspector.breakpointManager.setBreakpoint(uiSourceCode, uiLocation.lineNumber, breakpoint.condition(), breakpoint.enabled());
        }
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @return {WebInspector.UISourceCode}
     */
    _releaseSnippetScript: function(snippetJavaScriptSource)
    {
        var script = this._scriptForUISourceCode.get(snippetJavaScriptSource);
        if (!script)
            return null;

        snippetJavaScriptSource.isDivergingFromVM = true;
        snippetJavaScriptSource.hasDivergedFromVM = true;
        delete this._uiSourceCodeForScriptId[script.scriptId];
        this._scriptForUISourceCode.remove(snippetJavaScriptSource);
        delete snippetJavaScriptSource._evaluationIndex;
        var uiSourceCode = this._createUISourceCodeForScript(script);
        delete snippetJavaScriptSource.isDivergingFromVM;
        return uiSourceCode;
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @return {string}
     */
    _evaluationSourceURL: function(snippetJavaScriptSource)
    {
        var snippetPrefix = WebInspector.ScriptSnippetModel.snippetSourceURLPrefix;
        var evaluationSuffix = "_" + snippetJavaScriptSource._evaluationIndex;
        var snippetId = this._snippetIdForJavaScriptSource.get(snippetJavaScriptSource);
        return snippetPrefix + snippetId + evaluationSuffix;
    },

    /**
     * @param {string} sourceURL
     * @return {string|null}
     */
    _snippetIdForSourceURL: function(sourceURL)
    {
        var snippetPrefix = WebInspector.ScriptSnippetModel.snippetSourceURLPrefix;
        if (!sourceURL.startsWith(snippetPrefix))
            return null;
        var splittedURL = sourceURL.substring(snippetPrefix.length).split("_");
        var snippetId = splittedURL[0];
        return snippetId;
    },

    _projectWillReset: function()
    {
        var removedUISourceCodes = this._releasedUISourceCodes();
        this._uiSourceCodeForScriptId = {};
        this._scriptForUISourceCode = new Map();
        this._snippetJavaScriptSourceForSnippetId = {};
        this._snippetIdForJavaScriptSource = new Map();
    },

    _projectDidReset: function()
    {
        this._loadSnippets();
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @extends {WebInspector.JavaScriptSource}
 * @param {string} snippetName
 * @param {WebInspector.ContentProvider} contentProvider
 * @param {WebInspector.ScriptSnippetModel} scriptSnippetModel
 */
WebInspector.SnippetJavaScriptSource = function(snippetName, contentProvider, scriptSnippetModel)
{
    WebInspector.JavaScriptSource.call(this, snippetName, contentProvider, true);
    this._scriptSnippetModel = scriptSnippetModel;
    this.isSnippet = true;
}

WebInspector.SnippetJavaScriptSource.prototype = {
    /**
     * @param {function(?string)} callback
     */
    workingCopyCommitted: function(callback)
    {  
        this._scriptSnippetModel._setScriptSnippetContent(this, this.workingCopy());
        callback(null);
    },

    workingCopyChanged: function()
    {  
        this._scriptSnippetModel._scriptSnippetEdited(this);
    },

    __proto__: WebInspector.JavaScriptSource.prototype
}

/**
 * @constructor
 * @implements {WebInspector.SourceMapping}
 * @param {WebInspector.ScriptSnippetModel} scriptSnippetModel
 */
WebInspector.SnippetScriptMapping = function(scriptSnippetModel)
{
    this._scriptSnippetModel = scriptSnippetModel;
}

WebInspector.SnippetScriptMapping.prototype = {
    /**
     * @param {WebInspector.RawLocation} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var debuggerModelLocation = /** @type {WebInspector.DebuggerModel.Location} */ rawLocation;
        return this._scriptSnippetModel._rawLocationToUILocation(debuggerModelLocation);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.DebuggerModel.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        return this._scriptSnippetModel._uiLocationToRawLocation(uiSourceCode, lineNumber, columnNumber);
    },

    /**
     * @param {string} sourceURL
     * @return {string|null}
     */
    snippetIdForSourceURL: function(sourceURL)
    {
        return this._scriptSnippetModel._snippetIdForSourceURL(sourceURL);
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        this._scriptSnippetModel._addScript(script);
    }
}

/**
 * @constructor
 * @extends {WebInspector.StaticContentProvider}
 * @param {WebInspector.Snippet} snippet
 */
WebInspector.SnippetContentProvider = function(snippet)
{
    WebInspector.StaticContentProvider.call(this, WebInspector.resourceTypes.Script, snippet.content);
}

WebInspector.SnippetContentProvider.prototype = {
    __proto__: WebInspector.StaticContentProvider.prototype
}

/**
 * @type {?WebInspector.ScriptSnippetModel}
 */
WebInspector.scriptSnippetModel = null;
