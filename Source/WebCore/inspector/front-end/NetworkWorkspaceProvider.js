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
WebInspector.NetworkWorkspaceProvider = function()
{
    /** @type {Object.<string, WebInspector.ContentProvider>} */
    this._contentProviders = {};
}

WebInspector.NetworkWorkspaceProvider.prototype = {
    /**
     * @param {string} path
     * @param {function(?string,boolean,string)} callback
     */
    requestFileContent: function(path, callback)
    {
        var contentProvider = this._contentProviders[path];
        contentProvider.requestContent(callback);
    },

    /**
     * @param {string} path
     * @param {string} newContent
     * @param {function(?string)} callback
     */
    setFileContent: function(path, newContent, callback)
    {
        callback(null);
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInFileContent: function(path, query, caseSensitive, isRegex, callback)
    {
        var contentProvider = this._contentProviders[path];
        contentProvider.searchInContent(query, caseSensitive, isRegex, callback);
    },

    /**
     * @param {string} path
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     * @param {boolean=} isSnippet
     */
    addFile: function(path, contentProvider, isEditable, isContentScript, isSnippet)
    {
        var fileDescriptor = new WebInspector.FileDescriptor(path, contentProvider.contentType(), isEditable, isContentScript, isSnippet);
        this._contentProviders[path] = contentProvider;
        this.dispatchEventToListeners(WebInspector.WorkspaceProvider.Events.FileAdded, fileDescriptor);
    },

    /**
     * @param {string} path
     */
    removeFile: function(path)
    {
        delete this._contentProviders[path];
        this.dispatchEventToListeners(WebInspector.WorkspaceProvider.Events.FileRemoved, path);
    },

    reset: function()
    {
        this._contentProviders = {};
    },
    
    __proto__: WebInspector.Object.prototype
}

/**
 * @type {?WebInspector.NetworkWorkspaceProvider}
 */
WebInspector.networkWorkspaceProvider = null;
