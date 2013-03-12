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
WebInspector.SimpleProjectDelegate = function(name, type)
{
    this._name = name;
    this._type = type;
    /** @type {Object.<string, WebInspector.ContentProvider>} */
    this._contentProviders = {};
    this._lastUniqueSuffix = 0;
}

WebInspector.SimpleProjectDelegate.projectId = function(name, type)
{
    var typePrefix = type !== WebInspector.projectTypes.Network ? (type + ":") : "";
    return typePrefix + name;
}

WebInspector.SimpleProjectDelegate.prototype = {
    /**
     * @return {string}
     */
    id: function()
    {
        return WebInspector.SimpleProjectDelegate.projectId(this._name, this._type);
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
        if (typeof this._displayName !== "undefined")
            return this._displayName;
        if (!this._name) {
            this._displayName = this._type !== WebInspector.projectTypes.Snippets ? WebInspector.UIString("(no domain)") : "";
            return this._displayName;
        }
        var parsedURL = new WebInspector.ParsedURL(this._name);
        if (parsedURL.isValid) {
            this._displayName = parsedURL.host + (parsedURL.port ? (":" + parsedURL.port) : "");
            if (!this._displayName)
                this._displayName = this._name;
        }
        else
            this._displayName = this._name;
        return this._displayName;
    },

    /**
     * @param {Array.<string>} path
     * @param {function(?string,boolean,string)} callback
     */
    requestFileContent: function(path, callback)
    {
        var contentProvider = this._contentProviders[path.join("/")];
        contentProvider.requestContent(callback);
    },

    /**
     * @return {boolean}
     */
    canSetFileContent: function()
    {
        return false;
    },

    /**
     * @param {Array.<string>} path
     * @param {string} newContent
     * @param {function(?string)} callback
     */
    setFileContent: function(path, newContent, callback)
    {
        callback(null);
    },

    /**
     * @param {Array.<string>} path
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInFileContent: function(path, query, caseSensitive, isRegex, callback)
    {
        var contentProvider = this._contentProviders[path.join("/")];
        contentProvider.searchInContent(query, caseSensitive, isRegex, callback);
    },

    /**
     * @param {Array.<string>} path
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     * @return {Array.<string>}
     */
    addFile: function(path, forceUniquePath, url, contentProvider, isEditable, isContentScript)
    {
        if (forceUniquePath)
            this._ensureUniquePath(path);
        var fileDescriptor = new WebInspector.FileDescriptor(path, url, url, contentProvider.contentType(), isEditable, isContentScript);
        this._contentProviders[path.join("/")] = contentProvider;
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.FileAdded, fileDescriptor);
        return path;
    },

    /**
     * @param {Array.<string>} path
     */
    _ensureUniquePath: function(path)
     {
        var uniquePath = path.join("/");
        var suffix = "";
        while (this._contentProviders[uniquePath]) {
            suffix = " (" + (++this._lastUniqueSuffix) + ")";
            uniquePath = path + suffix;
        }
        path[path.length - 1] += suffix;
    },

    /**
     * @param {Array.<string>} path
     */
    removeFile: function(path)
    {
        delete this._contentProviders[path.join("/")];
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
    this._simpleProjectDelegates = {};
}

/**
 * @param {string} url
 * @return {Array.<string>}
 */
WebInspector.SimpleWorkspaceProvider.splitURL = function(url)
{
    var parsedURL = new WebInspector.ParsedURL(url);
    var origin;
    var folderPath;
    var name;
    if (parsedURL.isValid) {
        origin = parsedURL.scheme + "://" + parsedURL.host;
        if (parsedURL.port)
            origin += ":" + parsedURL.port;
        folderPath = parsedURL.folderPathComponents;
        name = parsedURL.lastPathComponent;
        if (parsedURL.queryParams)
            name += "?" + parsedURL.queryParams;
    } else {
        origin = "";
        folderPath = "";
        name = url;
    }
    var result = [origin];
    var splittedPath = folderPath.split("/");
    for (var i = 1; i < splittedPath.length; ++i)
        result.push(splittedPath[i]);
    result.push(name);
    return result;
}

/**
 * @param {Array.<string>} splittedURL
 * @return {Array.<string>}
 */
WebInspector.SimpleWorkspaceProvider.pathForSplittedURL = function(splittedURL)
{
    var result = splittedURL.slice();
    result.shift();
    return result;
}

WebInspector.SimpleWorkspaceProvider.prototype = {
    /**
     * @param {string} projectName
     * @return {WebInspector.ProjectDelegate}
     */
    _projectDelegate: function(projectName)
    {
        if (this._simpleProjectDelegates[projectName])
            return this._simpleProjectDelegates[projectName];
        var simpleProjectDelegate = new WebInspector.SimpleProjectDelegate(projectName, this._type);
        this._simpleProjectDelegates[projectName] = simpleProjectDelegate;
        this._workspace.addProject(simpleProjectDelegate);
        return simpleProjectDelegate;
    },
 
    /**
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     * @return {WebInspector.UISourceCode}
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
     * @return {WebInspector.UISourceCode}
     */
    addUniqueFileForURL: function(url, contentProvider, isEditable, isContentScript)
    {
        return this._innerAddFileForURL(url, contentProvider, isEditable, true, isContentScript);
    },

    /**
     * @param {string} url
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean} forceUnique
     * @param {boolean=} isContentScript
     * @return {WebInspector.UISourceCode}
     */
    _innerAddFileForURL: function(url, contentProvider, isEditable, forceUnique, isContentScript)
    {
        var splittedURL = WebInspector.SimpleWorkspaceProvider.splitURL(url);
        var projectName = splittedURL[0];
        var path = WebInspector.SimpleWorkspaceProvider.pathForSplittedURL(splittedURL);
        return this._innerAddFile(projectName, path, url, contentProvider, isEditable, forceUnique, isContentScript);
    },

    /**
     * @param {string} projectName
     * @param {string} name
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean=} isContentScript
     * @return {WebInspector.UISourceCode}
     */
    addFileByName: function(projectName, name, contentProvider, isEditable, isContentScript)
    {
        return this._innerAddFile("", [name], name, contentProvider, isEditable, false, isContentScript);
    },

    /**
     * @param {string} projectName
     * @param {Array.<string>} path
     * @param {WebInspector.ContentProvider} contentProvider
     * @param {boolean} isEditable
     * @param {boolean} forceUnique
     * @param {boolean=} isContentScript
     * @return {WebInspector.UISourceCode}
     */
    _innerAddFile: function(projectName, path, url, contentProvider, isEditable, forceUnique, isContentScript)
    {
        var projectDelegate = this._projectDelegate(projectName);
        path = projectDelegate.addFile(path, forceUnique, url, contentProvider, isEditable, isContentScript);
        return this._workspace.uiSourceCode(projectDelegate.id(), path);
    },

    /**
     * @param {string} projectName
     * @param {string} name
     */
    removeFileByName: function(projectName, name)
    {
        var projectDelegate = this._projectDelegate(projectName);
        projectDelegate.removeFile([name]);
    },

    reset: function()
    {
        for (var projectName in this._simpleProjectDelegates)
            this._simpleProjectDelegates[projectName].reset();
        this._simpleProjectDelegates = {};
    },
    
    __proto__: WebInspector.Object.prototype
}
