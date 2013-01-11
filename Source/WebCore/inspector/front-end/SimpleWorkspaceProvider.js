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
 */
WebInspector.SimpleWorkspaceProvider = function()
{
    /** @type {Object.<string, WebInspector.ContentProvider>} */
    this._contentProviders = {};
}

/**
 * @param {string} url
 * @return {string}
 */
WebInspector.SimpleWorkspaceProvider.uriForURL = function(url)
{
    var uri = url;
    return uri;
},

WebInspector.SimpleWorkspaceProvider.prototype = {
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
     * @param {string} uri
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     * @param {boolean=} isSnippet
     */
    addFile: function(uri, url, contentProvider, isEditable, isContentScript, isSnippet)
    {
        console.assert(!this._contentProviders[uri]);
        var fileDescriptor = new WebInspector.FileDescriptor(uri, url, contentProvider.contentType(), isEditable, isContentScript, isSnippet);
        this._contentProviders[uri] = contentProvider;
        this.dispatchEventToListeners(WebInspector.WorkspaceProvider.Events.FileAdded, fileDescriptor);
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
    uniqueURI: function(uri)
    {
        var uniqueURI = uri;
        for (var i = 1; this._contentProviders[uniqueURI]; ++i)
            uniqueURI = uri + " (" + i + ")";
        return uniqueURI;
    },

    reset: function()
    {
        this._contentProviders = {};
    },
    
    __proto__: WebInspector.Object.prototype
}
