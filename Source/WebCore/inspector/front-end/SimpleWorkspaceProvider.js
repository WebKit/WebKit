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
 * @implements {WebInspector.ProjectDelegate}
 * @extends {WebInspector.Object}
 */
WebInspector.SimpleProjectDelegate = function(type)
{
    this._type = type;
    /** @type {Object.<string, WebInspector.ContentProvider>} */
    this._contentProviders = {};
    this._lastUniqueSuffix = 0;
}

WebInspector.SimpleProjectDelegate.prototype = {
    /**
     * @return {string}
     */
    id: function()
    {
        return this._type;
    },

    /**
     * @return {string}
     */
    type: function()
    {
        return this._type;
    },

    /**
     * @return {string}
     */
    displayName: function()
    {
        return "";
    },

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
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     */
    addFile: function(path, forceUniquePath, url, contentProvider, isEditable, isContentScript)
    {
        if (forceUniquePath)
            path = this._uniquePath(path); 
        console.assert(!this._contentProviders[path]);
        var fileDescriptor = new WebInspector.FileDescriptor(path, url, url, contentProvider.contentType(), isEditable, isContentScript);
        this._contentProviders[path] = contentProvider;
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.FileAdded, fileDescriptor);
        return path;
    },

    /**
     * @param {string} path
     * @return {string}
     */
    _uniquePath: function(path)
    {
        var uniquePath = path;
        while (this._contentProviders[uniquePath])
            uniquePath = path + " (" + (++this._lastUniqueSuffix) + ")";
        return uniquePath;
    },

    /**
     * @param {string} path
     */
    removeFile: function(path)
    {
        delete this._contentProviders[path];
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.FileRemoved, path);
    },

    reset: function()
    {
        this._contentProviders = {};
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.Reset, null);
    },
    
    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {WebInspector.Workspace} workspace
 * @param {string} type
 */
WebInspector.SimpleWorkspaceProvider = function(workspace, type)
{
    this._workspace = workspace;
    this._type = type;
    this._simpleProjectDelegate = new WebInspector.SimpleProjectDelegate(this._type);
    this._workspace.addProject(this._simpleProjectDelegate);
}

/**
 * @param {string} url
 * @return {string}
 */
WebInspector.SimpleWorkspaceProvider.uriForURL = function(url, type)
{   
    var uriTypePrefix = type !== WebInspector.projectTypes.Network ? (type + ":") : "";
    var uri = uriTypePrefix + url;
    return uri;
}

WebInspector.SimpleWorkspaceProvider.prototype = {
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
        var path = WebInspector.SimpleWorkspaceProvider.uriForURL(url, this._type);
        path = this._simpleProjectDelegate.addFile(path, forceUnique, url, contentProvider, isEditable, isContentScript);
        return this._workspace.uiSourceCode(this._simpleProjectDelegate.id(), path);
    },

    /**
     * @param {string} url
     */
    removeFile: function(url)
    {
        var path = WebInspector.SimpleWorkspaceProvider.uriForURL(url, this._type);
        this._simpleProjectDelegate.removeFile(path);
    },

    reset: function()
    {
        this._simpleProjectDelegate.reset();
    },
    
    __proto__: WebInspector.Object.prototype
}
