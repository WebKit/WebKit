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
    this._uiSourceCodeForSnippet = new Map();
    this._snippetForUISourceCode = new Map();
    
    this._snippetStorage = new WebInspector.SnippetStorage("script", "Script snippet #");
    this._lastSnippetEvaluationIndexSetting = WebInspector.settings.createSetting("lastSnippetEvaluationIndex", 0);
    this._snippetScriptMapping = new WebInspector.SnippetScriptMapping(this);
    
    var snippets = this._snippetStorage.snippets;
    for (var i = 0; i < snippets.length; ++i)
        this._addScriptSnippet(snippets[i]);
}

WebInspector.ScriptSnippetModel.evaluatedSnippetExtraLinesCount = 2;
WebInspector.ScriptSnippetModel.snippetSourceURLPrefix = "snippets://";

WebInspector.ScriptSnippetModel.prototype = {
    /**
     * @return {WebInspector.SnippetScriptMapping}
     */
    get scriptMapping()
    {
        return this._snippetScriptMapping;
    },

    /**
     * @return {WebInspector.UISourceCode}
     */
    createScriptSnippet: function()
    {
        var snippet = this._snippetStorage.createSnippet();
        return this._addScriptSnippet(snippet);
    },

    /**
     * @param {WebInspector.Snippet} snippet
     * @return {WebInspector.UISourceCode}
     */
    _addScriptSnippet: function(snippet)
    {
        var uiSourceCode = new WebInspector.JavaScriptSource(snippet.name, new WebInspector.SnippetContentProvider(snippet), this._snippetScriptMapping, true);
        uiSourceCode.isSnippet = true;
        this._uiSourceCodeForSnippet.put(snippet, uiSourceCode);
        this._snippetForUISourceCode.put(uiSourceCode, snippet);
        this._snippetScriptMapping._fireUISourceCodeAdded(uiSourceCode);
        return uiSourceCode;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    deleteScriptSnippet: function(uiSourceCode)
    {
        var snippet = this._snippetForUISourceCode.get(uiSourceCode);
        this._snippetStorage.deleteSnippet(snippet);
        this._releaseSnippetScript(uiSourceCode);
        this._uiSourceCodeForSnippet.remove(snippet);
        this._snippetForUISourceCode.remove(uiSourceCode);
        this._snippetScriptMapping._fireUISourceCodeRemoved(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} newName
     */
    renameScriptSnippet: function(uiSourceCode, newName)
    {
        var snippet = this._snippetForUISourceCode.get(uiSourceCode)
        if (!snippet || !newName || snippet.name === newName)
            return;
        snippet.name = newName;
        uiSourceCode.urlChanged(snippet.name);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} newContent
     */
    setScriptSnippetContent: function(uiSourceCode, newContent)
    {
        this._snippetForUISourceCode.get(uiSourceCode).content = newContent;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    evaluateScriptSnippet: function(uiSourceCode)
    {
        this._releaseSnippetScript(uiSourceCode);
        var evaluationIndex = this._lastSnippetEvaluationIndexSetting.get() + 1;
        this._lastSnippetEvaluationIndexSetting.set(evaluationIndex);

        var snippet = this._snippetForUISourceCode.get(uiSourceCode);
        var sourceURL = this._sourceURLForSnippet(snippet, evaluationIndex);
        snippet._lastEvaluationSourceURL = sourceURL;
        var expression = "\n//@ sourceURL=" + sourceURL + "\n" + snippet.content;
        WebInspector.evaluateInConsole(expression, true);
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    _rawLocationToUILocation: function(rawLocation)
    {
        var uiSourceCode = this._uiSourceCodeForScriptId[rawLocation.scriptId];
        if (uiSourceCode.isSnippet) {
            var uiLineNumber = rawLocation.lineNumber - WebInspector.ScriptSnippetModel.evaluatedSnippetExtraLinesCount;
            return new WebInspector.UILocation(uiSourceCode, uiLineNumber, rawLocation.columnNumber || 0);
        }

        return new WebInspector.UILocation(uiSourceCode, rawLocation.lineNumber, rawLocation.columnNumber || 0);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    _uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var script = this._scriptForUISourceCode.get(uiSourceCode);
        if (!script)
            return null;

        if (uiSourceCode.isSnippet) {
            var rawLineNumber = lineNumber + WebInspector.ScriptSnippetModel.evaluatedSnippetExtraLinesCount;
            return WebInspector.debuggerModel.createRawLocation(script, rawLineNumber, columnNumber);
        }

        return WebInspector.debuggerModel.createRawLocation(script, lineNumber, columnNumber);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    _uiSourceCodes: function()
    {
        var result = this._uiSourceCodeForSnippet.values();
        result = result.concat(this._releasedUISourceCodes());
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
            if (uiSourceCode.isSnippet)
                continue;
            result.push(uiSourceCode);
        }
        return result;
    },

    /**
     * @param {WebInspector.Script} script
     */
    _addScript: function(script)
    {
        var snippet = this._snippetForSourceURL(script.sourceURL);
        if (!snippet) {
            this._createUISourceCodeForScript(script);
            return;
        }
        var uiSourceCode = this._uiSourceCodeForSnippet.get(snippet);
        console.assert(!this._scriptForUISourceCode.get(uiSourceCode));

        this._uiSourceCodeForScriptId[script.scriptId] = uiSourceCode;
        this._scriptForUISourceCode.put(uiSourceCode, script);
        script.setSourceMapping(this._snippetScriptMapping);
    },

    /**
     * @param {WebInspector.Script} script
     */
    _createUISourceCodeForScript: function(script)
    {
        var uiSourceCode = new WebInspector.JavaScriptSource(script.sourceURL, script, this._snippetScriptMapping, false);
        uiSourceCode.isSnippetEvaluation = true;
        this._uiSourceCodeForScriptId[script.scriptId] = uiSourceCode;
        this._scriptForUISourceCode.put(uiSourceCode, script);
        this._snippetScriptMapping._fireUISourceCodeAdded(uiSourceCode);
        script.setSourceMapping(this._snippetScriptMapping);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _releaseSnippetScript: function(uiSourceCode)
    {
        var script = this._scriptForUISourceCode.get(uiSourceCode);
        if (!script)
            return;

        delete this._uiSourceCodeForScriptId[script.scriptId];
        this._scriptForUISourceCode.remove(uiSourceCode);

        this._createUISourceCodeForScript(script);
    },

    /**
     * @param {WebInspector.Snippet} snippet
     * @param {string} evaluationIndex
     * @return {string}
     */
    _sourceURLForSnippet: function(snippet, evaluationIndex)
    {
        var snippetPrefix = WebInspector.ScriptSnippetModel.snippetSourceURLPrefix;
        var evaluationSuffix = evaluationIndex ? "_" + evaluationIndex : "";
        return snippetPrefix + snippet.id + evaluationSuffix;
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

    /**
     * @param {string} sourceURL
     * @return {WebInspector.Snippet|null}
     */
    _snippetForSourceURL: function(sourceURL)
    {
        var snippetId = this._snippetIdForSourceURL(sourceURL);
        if (!snippetId)
            return null;
        var snippet = this._snippetStorage.snippetForId(snippetId);
        if (!snippet || snippet._lastEvaluationSourceURL !== sourceURL)
            return null;
        return snippet;
    },

    _reset: function()
    {
        var removedUISourceCodes = this._releasedUISourceCodes();
        this._uiSourceCodeForScriptId = {};
        this._scriptForUISourceCode = new Map();
        for (var i = 0; i < removedUISourceCodes.length; ++i)
            this._snippetScriptMapping._fireUISourceCodeRemoved(removedUISourceCodes[i]);
    }
}

WebInspector.ScriptSnippetModel.prototype.__proto__ = WebInspector.Object.prototype;

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
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        return this._scriptSnippetModel._rawLocationToUILocation(rawLocation);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
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
    WebInspector.StaticContentProvider.call(this, "text/javascript", snippet.content);
}

WebInspector.SnippetContentProvider.prototype.__proto__ = WebInspector.StaticContentProvider.prototype;

/**
 * @type {?WebInspector.ScriptSnippetModel}
 */
WebInspector.scriptSnippetModel = null;
