/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * @param {WebInspector.IsolatedFileSystem} isolatedFileSystem
 */
WebInspector.FileSystemProjectDelegate = function(isolatedFileSystem)
{
    this._fileSystem = isolatedFileSystem;
    this._populate();
}

WebInspector.FileSystemProjectDelegate._scriptExtensions = ["js", "java", "cc", "cpp", "h", "cs", "py", "php"].keySet();

WebInspector.FileSystemProjectDelegate.prototype = {
    /**
     * @return {string}
     */
    id: function()
    {
        return this._fileSystem.id();
    },

    /**
     * @return {string}
     */
    type: function()
    {
        return WebInspector.projectTypes.FileSystem;
    },

    /**
     * @return {string}
     */
    displayName: function()
    {
        return this._fileSystem.path().substr(this._fileSystem.path().lastIndexOf("/") + 1);
    },

    /**
     * @param {string} uri
     * @return {string}
     */
    _filePathForURI: function(uri)
    {
        return uri.substr(uri.indexOf("/") + 1);
    },

    /**
     * @param {string} uri
     * @param {function(?string,boolean,string)} callback
     */
    requestFileContent: function(uri, callback)
    {
        var filePath = this._filePathForURI(uri);
        this._fileSystem.requestFileContent(filePath, innerCallback.bind(this));
        
        /**
         * @param {?string} content
         */
        function innerCallback(content)
        {
            var contentType = this._contentTypeForPath(filePath);
            callback(content, false, contentType.canonicalMimeType());
        }
    },

    /**
     * @param {string} uri
     * @param {string} newContent
     * @param {function(?string)} callback
     */
    setFileContent: function(uri, newContent, callback)
    {
        var filePath = this._filePathForURI(uri);
        this._fileSystem.setFileContent(filePath, newContent, callback.bind(this, ""));
    },

    /**
     * @param {string} uri
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInFileContent: function(uri, query, caseSensitive, isRegex, callback)
    {
        this.requestFileContent(uri, contentCallback.bind(this));

        /**
         * @param {?string} content
         * @param {boolean} base64Encoded
         * @param {string} mimeType
         */
        function contentCallback(content, base64Encoded, mimeType)
        {
            var result = [];
            if (content !== null)
                result = WebInspector.ContentProvider.performSearchInContent(content, query, caseSensitive, isRegex);
            callback(result);
        }
    },

    /**
     * @param {string} path
     * @return {WebInspector.ResourceType}
     */
    _contentTypeForPath: function(path)
    {
        var splittedPath = path.split("/");
        var fileName = splittedPath[splittedPath.length - 1];
        var extensionIndex = fileName.lastIndexOf(".");
        var extension = "";
        if (extensionIndex !== -1)
            extension = fileName.substring(extensionIndex + 1);
        var contentType = WebInspector.resourceTypes.Other;
        if (WebInspector.FileSystemProjectDelegate._scriptExtensions[extension])
            return WebInspector.resourceTypes.Script;
        if (extension === "css")
            return WebInspector.resourceTypes.Stylesheet;
        if (extension === "html")
            return WebInspector.resourceTypes.Document;
        return WebInspector.resourceTypes.Other;
    },

    _populate: function()
    {
        this._fileSystem.requestFilesRecursive("", filesLoaded.bind(this));

        function filesLoaded(files)
        {
            for (var i = 0; i < files.length; ++i) {
                var uri = this.id() + files[i];
                var contentType = this._contentTypeForPath(files[i]);
                var url = WebInspector.fileMapping.urlForURI(uri);
                var fileDescriptor = new WebInspector.FileDescriptor(uri, "file://" + this._fileSystem.path() + files[i], url, contentType, true);
                this._addFile(fileDescriptor);
            } 
        }
    },

    /**
     * @param {WebInspector.FileDescriptor} fileDescriptor
     */
    _addFile: function(fileDescriptor)
    {
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.FileAdded, fileDescriptor);
    },

    /**
     * @param {string} uri
     */
    _removeFile: function(uri)
    {
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.FileRemoved, uri);
    },

    reset: function()
    {
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.Reset, null);
    },
    
    __proto__: WebInspector.Object.prototype
}

/**
 * @type {?WebInspector.FileSystemProjectDelegate}
 */
WebInspector.fileSystemProjectDelegate = null;
