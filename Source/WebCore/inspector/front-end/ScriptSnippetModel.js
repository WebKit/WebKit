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
 */
WebInspector.ScriptSnippetModel = function()
{
    this._uiSourceCodeForScriptId = {};
    this._scriptForUISourceCode = new Map();
    this._snippetJavaScriptSourceForSnippetId = {};
    
    this._snippetStorage = new WebInspector.SnippetStorage("script", "Script snippet #");
    this._lastSnippetEvaluationIndexSetting = WebInspector.settings.createSetting("lastSnippetEvaluationIndex", 0);
    this._snippetScriptMapping = new WebInspector.SnippetScriptMapping(this);
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
        var snippetJavaScriptSource = new WebInspector.SnippetJavaScriptSource(snippet.id, snippet.name, new WebInspector.SnippetContentProvider(snippet), this);
        this._snippetJavaScriptSourceForSnippetId[snippet.id] = snippetJavaScriptSource;
        this._snippetScriptMapping._fireUISourceCodeAdded(snippetJavaScriptSource);
        return snippetJavaScriptSource;
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     */
    deleteScriptSnippet: function(snippetJavaScriptSource)
    {
        var snippet = this._snippetStorage.snippetForId(snippetJavaScriptSource.snippetId);
        this._snippetStorage.deleteSnippet(snippet);
        this._removeBreakpoints(snippetJavaScriptSource);
        this._releaseSnippetScript(snippetJavaScriptSource);
        delete this._snippetJavaScriptSourceForSnippetId[snippet.id];
        this._snippetScriptMapping._fireUISourceCodeRemoved(snippetJavaScriptSource);
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @param {string} newName
     */
    renameScriptSnippet: function(snippetJavaScriptSource, newName)
    {
        var snippet = this._snippetStorage.snippetForId(snippetJavaScriptSource.snippetId);
        if (!snippet || !newName || snippet.name === newName)
            return;
        snippet.name = newName;
        snippetJavaScriptSource.urlChanged(snippet.name);
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @return {boolean}
     */
    _isDivergedFromVM: function(snippetJavaScriptSource)
    {
        var script = this._scriptForUISourceCode.get(snippetJavaScriptSource);
        return !script;
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @param {string} newContent
     */
    _setScriptSnippetContent: function(snippetJavaScriptSource, newContent)
    {
        var snippet = this._snippetStorage.snippetForId(snippetJavaScriptSource.snippetId);
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
        this._releaseSnippetScript(snippetJavaScriptSource);
        this._restoreBreakpoints(snippetJavaScriptSource, breakpointLocations);
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
        var evaluationIndex = this._nextEvaluationIndex(snippetJavaScriptSource.snippetId);
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

            WebInspector.consoleView.runScript(scriptId);
        }
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
    _uiSourceCodes: function()
    {
        var result = this._releasedUISourceCodes();
        for (var snippetId in this._snippetJavaScriptSourceForSnippetId)
            result.push(this._snippetJavaScriptSourceForSnippetId[snippetId]);
        return result;
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
        script.setSourceMapping(this._snippetScriptMapping);
    },

    /**
     * @param {WebInspector.Script} script
     */
    _createUISourceCodeForScript: function(script)
    {
        var uiSourceCode = new WebInspector.JavaScriptSource(script.sourceURL, null, script, this._snippetScriptMapping, false);
        uiSourceCode.isSnippetEvaluation = true;
        this._uiSourceCodeForScriptId[script.scriptId] = uiSourceCode;
        this._scriptForUISourceCode.put(uiSourceCode, script);
        script.setSourceMapping(this._snippetScriptMapping);
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
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @param {Array.<Object>} breakpointLocations
     */
    _restoreBreakpoints: function(snippetJavaScriptSource, breakpointLocations)
    {
        for (var i = 0; i < breakpointLocations.length; ++i) {
            var uiLocation = breakpointLocations[i].uiLocation;
            var breakpoint = breakpointLocations[i].breakpoint;
            WebInspector.breakpointManager.setBreakpoint(uiLocation.uiSourceCode, uiLocation.lineNumber, breakpoint.condition(), breakpoint.enabled());
        }
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     */
    _releaseSnippetScript: function(snippetJavaScriptSource)
    {
        var script = this._scriptForUISourceCode.get(snippetJavaScriptSource);
        if (!script)
            return;

        delete this._uiSourceCodeForScriptId[script.scriptId];
        this._scriptForUISourceCode.remove(snippetJavaScriptSource);
        delete snippetJavaScriptSource._evaluationIndex;
        this._createUISourceCodeForScript(script);
    },

    /**
     * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
     * @return {string}
     */
    _evaluationSourceURL: function(snippetJavaScriptSource)
    {
        var snippetPrefix = WebInspector.ScriptSnippetModel.snippetSourceURLPrefix;
        var evaluationSuffix = "_" + snippetJavaScriptSource._evaluationIndex;
        return snippetPrefix + snippetJavaScriptSource.snippetId + evaluationSuffix;
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

    _reset: function()
    {
        var removedUISourceCodes = this._releasedUISourceCodes();
        this._uiSourceCodeForScriptId = {};
        this._scriptForUISourceCode = new Map();
        this._snippetJavaScriptSourceForSnippetId = {};
        setTimeout(this._loadSnippets.bind(this), 0);
    }
}

WebInspector.ScriptSnippetModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.JavaScriptSource}
 * @param {string} snippetId
 * @param {string} snippetName
 * @param {WebInspector.ContentProvider} contentProvider
 * @param {WebInspector.ScriptSnippetModel} scriptSnippetModel
 */
WebInspector.SnippetJavaScriptSource = function(snippetId, snippetName, contentProvider, scriptSnippetModel)
{
    WebInspector.JavaScriptSource.call(this, snippetName, null, contentProvider, scriptSnippetModel.scriptMapping, true);
    this._snippetId = snippetId;
    this._scriptSnippetModel = scriptSnippetModel;
    this.isSnippet = true;
}

WebInspector.SnippetJavaScriptSource.prototype = {
    /**
     * @return {boolean}
     */
    isEditable: function()
    {
        return true;
    },

    /**
     * @return {boolean}
     */
    isDivergedFromVM: function()
    {
        return this._scriptSnippetModel._isDivergedFromVM(this);
    },

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

    evaluate: function()
    {
        this._scriptSnippetModel.evaluateScriptSnippet(this);
    },

    /**
     * @return {boolean}
     */
    supportsEnabledBreakpointsWhileEditing: function()
    {
        return true;
    },

    /**
     * @return {string}
     */
    breakpointStorageId: function()
    {
        return WebInspector.ScriptSnippetModel.snippetSourceURLPrefix + this.snippetId;
    },

    /**
     * @return {string}
     */
    get snippetId()
    {
        return this._snippetId;
    }
}

WebInspector.SnippetJavaScriptSource.prototype.__proto__ = WebInspector.JavaScriptSource.prototype;

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @implements {WebInspector.SourceMapping}
 * @implements {WebInspector.UISourceCodeProvider}
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
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function()
    {
        return this._scriptSnippetModel._uiSourceCodes();
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
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _fireUISourceCodeAdded: function(uiSourceCode)
    {
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _fireUISourceCodeRemoved: function(uiSourceCode)
    {
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, uiSourceCode);
    },

    reset: function()
    {
        this._scriptSnippetModel._reset();
    }
}

WebInspector.SnippetScriptMapping.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.StaticContentProvider}
 * @param {WebInspector.Snippet} snippet
 */
WebInspector.SnippetContentProvider = function(snippet)
{
    WebInspector.StaticContentProvider.call(this, WebInspector.resourceTypes.Script, snippet.content);
}

WebInspector.SnippetContentProvider.prototype.__proto__ = WebInspector.StaticContentProvider.prototype;

/**
 * @type {?WebInspector.ScriptSnippetModel}
 */
WebInspector.scriptSnippetModel = null;

/**
 * @constructor
 * @extends {WebInspector.JavaScriptSourceFrame}
 * @param {WebInspector.ScriptsPanel} scriptsPanel
 * @param {WebInspector.SnippetJavaScriptSource} snippetJavaScriptSource
 */
WebInspector.SnippetJavaScriptSourceFrame = function(scriptsPanel, snippetJavaScriptSource)
{
    WebInspector.JavaScriptSourceFrame.call(this, scriptsPanel, snippetJavaScriptSource);
    
    this._snippetJavaScriptSource = snippetJavaScriptSource;
    this._runButton = new WebInspector.StatusBarButton(WebInspector.UIString("Run"), "evaluate-snippet-status-bar-item");
    this._runButton.addEventListener("click", this._runButtonClicked, this);
}

WebInspector.SnippetJavaScriptSourceFrame.prototype = {
    /**
     * @return {Array.<Element>}
     */
    statusBarItems: function()
    {
        return [this._runButton.element];
    },

    _runButtonClicked: function()
    {
        this._snippetJavaScriptSource.evaluate();
    }
}

WebInspector.SnippetJavaScriptSourceFrame.prototype.__proto__ = WebInspector.JavaScriptSourceFrame.prototype;
