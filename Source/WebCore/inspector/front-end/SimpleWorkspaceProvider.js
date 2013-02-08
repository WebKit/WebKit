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
 * @implements {WebInspector.WorkspaceProvider}
 * @extends {WebInspector.Object}
 * @param {WebInspector.Workspace} workspace
 * @param {string} type
 */
WebInspector.SimpleWorkspaceProvider = function(workspace, type)
{
    this._workspace = workspace;
    this._type = type;
    /** @type {Object.<string, WebInspector.ContentProvider>} */
    this._contentProviders = {};
    this._lastUniqueSuffix = 0;
    this._workspace.addProject(this._type, this);
}

/**
 * @param {string} url
 * @param {string} type
 * @return {string}
 */
WebInspector.SimpleWorkspaceProvider.uriForURL = function(url, type)
{
    var uriTypePrefix = type !== WebInspector.projectTypes.Network ? (type + ":") : "";
    var uri = uriTypePrefix + url;
    return uri;
},

WebInspector.SimpleWorkspaceProvider.prototype = {
    /**
     * @return {string}
     */
    type: function()
    {
        return this._type;
    },

    /**
     * @param {string} uri
     * @param {function(?string,boolean,string)} callback
     */
    requestFileContent: function(uri, callback)
    {
        var contentProvider = this._contentProviders[uri];
        contentProvider.requestContent(callback);
    },

    /**
     * @param {string} uri
     * @param {string} newContent
     * @param {function(?string)} callback
     */
    setFileContent: function(uri, newContent, callback)
    {
        callback(null);
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInFileContent: function(uri, query, caseSensitive, isRegex, callback)
    {
        var contentProvider = this._contentProviders[uri];
        contentProvider.searchInContent(query, caseSensitive, isRegex, callback);
    },

   /**
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     */
    addFileForURL: function(url, contentProvider, isEditable, isContentScript)
    {
        return this._innerAddFileForURL(url, contentProvider, isEditable, false, isContentScript);
    },

    /**
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     */
    addUniqueFileForURL: function(url, contentProvider, isEditable, isContentScript)
    {
        return this._innerAddFileForURL(url, contentProvider, isEditable, true, isContentScript);
    },

    /**
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     */
    _innerAddFileForURL: function(url, contentProvider, isEditable, forceUnique, isContentScript)
    {
        var uri = WebInspector.SimpleWorkspaceProvider.uriForURL(url, this._type);
        if (forceUnique)
            uri = this._uniqueURI(uri);
        console.assert(!this._contentProviders[uri]);
        var fileDescriptor = new WebInspector.FileDescriptor(uri, url, url, contentProvider.contentType(), isEditable, isContentScript);
        this._contentProviders[uri] = contentProvider;
        this.dispatchEventToListeners(WebInspector.WorkspaceProvider.Events.FileAdded, fileDescriptor);
        return this._workspace.uiSourceCodeForURI(uri);
    },

    /**
     * @param {string} uri
     */
    removeFile: function(uri)
    {
        delete this._contentProviders[uri];
        this.dispatchEventToListeners(WebInspector.WorkspaceProvider.Events.FileRemoved, uri);
    },

    /**
     * @param {string} uri
     * @return {string}
     */
    _uniqueURI: function(uri)
    {
        var uniqueURI = uri;
        while (this._contentProviders[uniqueURI])
            uniqueURI = uri + " (" + (++this._lastUniqueSuffix) + ")";
        return uniqueURI;
    },

    reset: function()
    {
        this._contentProviders = {};
        this.dispatchEventToListeners(WebInspector.WorkspaceProvider.Events.Reset, null);
    },
    
    __proto__: WebInspector.Object.prototype
}
