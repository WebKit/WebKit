/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
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
WebInspector.SnippetsModel = function()
{
    /** @type {Array.<WebInspector.Snippet>} */ this._snippets;

    this._lastSnippetIdentifierSetting = WebInspector.settings.createSetting("lastSnippetIdentifier", 0);
    this._snippetsSetting = WebInspector.settings.createSetting("snippets", []);

    this._loadSettings();
}

WebInspector.SnippetsModel.EventTypes = {
    SnippetAdded: "SnippetAdded",
    SnippetRenamed: "SnippetRenamed",
    SnippetRemoved: "SnippetRemoved",
}

WebInspector.SnippetsModel.prototype = {
    /**
     * @return {Array.<WebInspector.Snippet>}
     */
    get snippets()
    {
        return this._snippets.slice();
    },

    _saveSettings: function()
    {
        var savedSnippets = [];
        for (var i = 0; i < this._snippets.length; ++i)
            savedSnippets.push(this._snippets[i].serializeToObject());

        this._snippetsSetting.set(savedSnippets);
    },

    _loadSettings: function()
    {
        this._snippets = [];
        var savedSnippets = this._snippetsSetting.get();
        for (var i = 0; i < savedSnippets.length; ++i)
            this._snippetAdded(WebInspector.Snippet.fromObject(savedSnippets[i]));
    },

    /**
     * @param {WebInspector.Snippet} snippet
     */
    deleteSnippet: function(snippet)
    {
        for (var i = 0; i < this._snippets.length; ++i) {
            if (snippet.id === this._snippets[i].id) {
                this._snippets.splice(i, 1);
                this._saveSettings();
                this.dispatchEventToListeners(WebInspector.SnippetsModel.EventTypes.SnippetRemoved, snippet);
                break;
            }
        }
    },

    /**
     * @return {WebInspector.Snippet}
     */
    createSnippet: function()
    {
        var snippetId = this._lastSnippetIdentifierSetting.get() + 1;
        this._lastSnippetIdentifierSetting.set(snippetId);
        var snippet = new WebInspector.Snippet(this, snippetId);
        this._snippetAdded(snippet);
        this._saveSettings();

        return snippet;
    },

    /**
     * @param {WebInspector.Snippet} snippet
     */
    _snippetAdded: function(snippet)
    {
        this._snippets.push(snippet);
        this.dispatchEventToListeners(WebInspector.SnippetsModel.EventTypes.SnippetAdded, snippet);
    },

    /**
     * @param {number} snippetId
     */
    _snippetContentUpdated: function(snippetId)
    {
        this._saveSettings();
    },

    /**
     * @param {WebInspector.Snippet} snippet
     */
    _snippetRenamed: function(snippet)
    {
        this._saveSettings();
        this.dispatchEventToListeners(WebInspector.SnippetsModel.EventTypes.SnippetRenamed, snippet);
    },

    /**
     * @param {number} snippetId
     * @return {WebInspector.Snippet|undefined}
     */
    snippetForId: function(snippetId)
    {
        return this._snippets[snippetId];
    },

    /**
     * @param {string} sourceURL
     * @return {string|null}
     */
    snippetIdForSourceURL: function(sourceURL)
    {
        // FIXME: to be implemented.
        return null;
    },

    /**
     * @param {string} sourceURL
     * @return {WebInspector.Snippet|null}
     */
    snippetForSourceURL: function(sourceURL)
    {
        // FIXME: to be implemented.
        return null;
    }
}

WebInspector.SnippetsModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {WebInspector.SnippetsModel} model
 * @param {number} id
 * @param {string=} name
 * @param {string=} content
 */
WebInspector.Snippet = function(model, id, name, content)
{
    this._model = model;
    this._id = id;
    this._name = name || "Snippet #" + id;
    this._content = content || "";
}

WebInspector.Snippet.evaluatedSnippetExtraLinesCount = 2;

/**
 * @param {Object} serializedSnippet
 * @return {WebInspector.Snippet}
 */
WebInspector.Snippet.fromObject = function(serializedSnippet)
{
    return new WebInspector.Snippet(this, serializedSnippet.id, serializedSnippet.name, serializedSnippet.content);
}

WebInspector.Snippet.prototype = {
    /**
     * @type {number}
     */
    get id()
    {
        return this._id;
    },

    /**
     * @type {string}
     */
    get name()
    {
        return this._name;
    },

    set name(name)
    {
        if (this._name === name)
            return;

        this._name = name;
        this._model._snippetRenamed(this);
    },

    /**
     * @type {string}
     */
    get content()
    {
        return this._content;
    },

    set content(content)
    {
        if (this._content === content)
            return;

        this._content = content;
        this._model._snippetContentUpdated(this._id);
    },

    /**
     * @return {Object}
     */
    serializeToObject: function()
    {
        var serializedSnippet = {};
        serializedSnippet.id = this.id;
        serializedSnippet.name = this.name;
        serializedSnippet.content = this.content;
        return serializedSnippet;
    }
}

WebInspector.Snippet.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.ScriptMapping}
 */
WebInspector.SnippetsScriptMapping = function()
{
    this._snippetForScriptId = {};
    this._uiSourceCodeForScriptId = {};
    this._scriptForUISourceCode = new Map();
    this._snippetForUISourceCode = new Map();
    this._uiSourceCodeForSnippet = new Map();
    
    WebInspector.snippetsModel.addEventListener(WebInspector.SnippetsModel.EventTypes.SnippetAdded, this._snippetAdded.bind(this));
    WebInspector.snippetsModel.addEventListener(WebInspector.SnippetsModel.EventTypes.SnippetRemoved, this._snippetRemoved.bind(this));
}

WebInspector.SnippetsScriptMapping.prototype = {
    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var uiSourceCode = this._uiSourceCodeForScriptId[rawLocation.scriptId];

        var snippet = this._snippetForScriptId[rawLocation.scriptId];
        if (snippet) {
            var uiLineNumber = rawLocation.lineNumber - WebInspector.Snippet.evaluatedSnippetExtraLinesCount;
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
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var script = this._scriptForUISourceCode.get(uiSourceCode);
        if (!script)
            return null;

        var snippet = this._snippetForUISourceCode.get(uiSourceCode);
        if (snippet) {
            var rawLineNumber = lineNumber + WebInspector.Snippet.evaluatedSnippetExtraLinesCount;
            return WebInspector.debuggerModel.createRawLocation(script, rawLineNumber, columnNumber);
        }

        return WebInspector.debuggerModel.createRawLocation(script, lineNumber, columnNumber);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodeList: function()
    {
        var result = [];
        for (var uiSourceCode in this._uiSourceCodeForSnippet.values())
            result.push(uiSourceCode);
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
            if (this._snippetForUISourceCode.get(uiSourceCode))
                continue;
            result.push(uiSourceCode);
        }
        return result;
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        var snippet = WebInspector.snippetsModel.snippetForSourceURL(script.sourceURL);
        if (!snippet) {
            this._createUISourceCodeForScript(script);
            return;
        }
        this._releaseSnippetScript(snippet);        
        var uiSourceCode = this._uiSourceCodeForSnippet.get(snippet);
        this._uiSourceCodeForScriptId[script.scriptId] = uiSourceCode;
        this._snippetForScriptId[script.scriptId] = snippet;
        this._scriptForUISourceCode.put(uiSourceCode, script);
        var data = { removedItems: [], addedItems: [], scriptIds: [script.scriptId] };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);        
    },

    /**
     * @param {WebInspector.Event} event
     */
    _snippetAdded: function(event)
    {
        var snippet = /** @type {WebInspector.Snippet} */ event.data;
        var uiSourceCodeId = ""; // FIXME: to be implemented.
        var uiSourceCodeURL = ""; // FIXME: to be implemented.
        var uiSourceCode = new WebInspector.UISourceCode(uiSourceCodeId, uiSourceCodeURL, new WebInspector.SnippetContentProvider(snippet));
        uiSourceCode.isSnippet = true;
        uiSourceCode.isEditable = true;
        this._uiSourceCodeForSnippet.put(snippet, uiSourceCode);
        this._snippetForUISourceCode.put(uiSourceCode, snippet);
        var data = { removedItems: [], addedItems: [uiSourceCode], scriptIds: [] };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);
    },

    /**
     * @param {WebInspector.Script} script
     */
    _createUISourceCodeForScript: function(script)
    {
        var uiSourceCode = new WebInspector.UISourceCode(script.sourceURL, script.sourceURL, new WebInspector.ScriptContentProvider(script));
        uiSourceCode.isSnippetEvaluation = true;
        this._uiSourceCodeForScriptId[script.scriptId] = uiSourceCode;
        this._scriptForUISourceCode.put(uiSourceCode, script);
        var data = { removedItems: [], addedItems: [uiSourceCode], scriptIds: [script.scriptId] };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);
    },

    /**
     * @param {WebInspector.Snippet} snippet
     */
    _releaseSnippetScript: function(snippet)
    {
        var uiSourceCode = this._uiSourceCodeForSnippet.get(snippet);
        var script = this._scriptForUISourceCode.get(uiSourceCode);
        if (!script)
            return;

        delete this._uiSourceCodeForScriptId[script.scriptId];
        delete this._snippetForScriptId[script.scriptId];
        this._scriptForUISourceCode.remove(uiSourceCode);

        this._createUISourceCodeForScript(script);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _snippetRemoved: function(event)
    {
        var snippet = /** @type {WebInspector.Snippet} */ event.data;
        var uiSourceCode = this._uiSourceCodeForSnippet.get(snippet);
        this._releaseSnippetScript(snippet);
        this._uiSourceCodeForSnippet.remove(snippet);
        this._snippetForUISourceCode.remove(uiSourceCode);
        var data = { removedItems: [uiSourceCode], addedItems: [], scriptIds: [] };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);
    },

    reset: function()
    {
        var removedUISourceCodes = this._releasedUISourceCodes();
        this._snippetForScriptId = {};
        this._uiSourceCodeForScriptId = {};
        this._scriptForUISourceCode = new Map();
        var data = { removedItems: removedUISourceCodes, addedItems: [], scriptIds: [] };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);
    }
}

WebInspector.SnippetsScriptMapping.prototype.__proto__ = WebInspector.ScriptMapping.prototype;

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
 * @type {?WebInspector.SnippetsModel}
 */
WebInspector.snippetsModel = null;
