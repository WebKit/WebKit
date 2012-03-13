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
    SnippetDeleted: "SnippetDeleted",
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
                this.dispatchEventToListeners(WebInspector.SnippetsModel.EventTypes.SnippetDeleted, snippet);
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
