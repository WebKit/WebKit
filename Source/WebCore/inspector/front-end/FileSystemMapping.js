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
 * @interface
 */
WebInspector.FileSystemMapping = function() { }

WebInspector.FileSystemMapping.prototype = {
    /**
     * @return {Array.<string>}
     */
    fileSystemPaths: function() { },

    /**
     * @param {string} fileSystemPath
     * @param {string} uri
     * @return {?string}
     */
    fileForURI: function(fileSystemPath, uri) { },

    /**
     * @param {string} fileSystemPath
     * @param {string} filePath
     * @return {?string}
     */
    uriForFile: function(fileSystemPath, filePath) { },

    /**
     * @param {string} pathPrefix
     * @return {?string}
     */
    uriPrefixForPathPrefix: function(pathPrefix) { },
}

/**
 * @constructor
 * @implements {WebInspector.FileSystemMapping}
 * @extends {WebInspector.Object}
 */
WebInspector.FileSystemMappingImpl = function()
{
    WebInspector.Object.call(this);
    this._fileSystemMappingSetting = WebInspector.settings.createSetting("fileSystemMapping", {});
    /** @type {!Object.<string, string>} */
    this._fileSystemPaths = {};
    /** @type {!Object.<string, string>} */
    this._fileSystemIds = {};
    /** @type {!Object.<string, string>} */
    this._fileSystemNames = {};
    this._loadFromSettings();
}

WebInspector.FileSystemMappingImpl.prototype = {
    _loadFromSettings: function()
    {
        var savedMapping = this._fileSystemMappingSetting.get();
        this._nextUniqueId = savedMapping ? savedMapping._nextUniqueId || 0 : 0;
        this._fileSystemPaths = savedMapping ? /** @type {!Object.<string, string>} */ (savedMapping.fileSystemPaths) || {} : {};
        for (var id in this._fileSystemPaths) {
            var fileSystemPath = this._fileSystemPaths[id];
            var name = this._fileSystemName(fileSystemPath);
            this._fileSystemIds[fileSystemPath] = id;
            this._fileSystemNames[id] = name;
        }
    },

    _saveToSettings: function()
    {
        var savedMapping = {};
        savedMapping.nextUniqueId = this._nextUniqueId;
        savedMapping.fileSystemPaths = this._fileSystemPaths;
        this._fileSystemMappingSetting.set(savedMapping);
    },

    /**
     * @param {string} fileSystemPath
     */
    _fileSystemName: function(fileSystemPath)
    {
        var lastIndexOfSlash = fileSystemPath.lastIndexOf("/");
        return fileSystemPath.substr(lastIndexOfSlash + 1);
    },

    /**
     * @param {string} fileSystemPath
     * @return {?string}
     */
    fileSystemId: function(fileSystemPath)
    {
        return this._fileSystemIds[fileSystemPath];
    },

    /**
     * @param {string} fileSystemPath
     * @return {string}
     */
    addFileSystemMapping: function(fileSystemPath)
    {
        if (this._fileSystemIds[fileSystemPath])
            return this._fileSystemIds[fileSystemPath];

        var name = this._fileSystemName(fileSystemPath);
        var id = String(this._nextUniqueId++) + "@" + name;
        this._fileSystemIds[fileSystemPath] = id;
        this._fileSystemNames[id] = name;
        this._fileSystemPaths[id] = fileSystemPath;
        this._saveToSettings();
        delete this._cachedURIPrefixes;
        return id;
    },

    /**
     * @param {string} fileSystemPath
     */
    removeFileSystemMapping: function(fileSystemPath)
    {
        var id = this._fileSystemIds[fileSystemPath];
        if (!id)
            return;
        delete this._fileSystemIds[fileSystemPath];
        delete this._fileSystemNames[id];
        delete this._fileSystemPaths[id];
        this._saveToSettings();
        delete this._cachedURIPrefixes;
    },

    /**
     * @return {Array.<string>}
     */
    fileSystemPaths: function()
    {
        return Object.values(this._fileSystemPaths);
    },

    /**
     * @param {string} fileSystemPath
     * @param {string} uri
     * @return {?string}
     */
    fileForURI: function(fileSystemPath, uri)
    {
        var indexOfSlash = uri.indexOf("/");
        var uriId = uri.substr(0, indexOfSlash);
        var id = this._fileSystemIds[fileSystemPath]
        if (!uriId || uriId !== id)
            return null;
        var filePath = uri.substr(indexOfSlash);
        return filePath;
    },

    /**
     * @param {string} fileSystemPath
     * @param {string} filePath
     * @return {?string}
     */
    uriForFile: function(fileSystemPath, filePath)
    {
        var id = this._fileSystemIds[fileSystemPath];
        if (!id)
            return null;
        return id + filePath;
    },
    
    /**
     * @param {string} pathPrefix
     * @return {?string}
     */
    uriPrefixForPathPrefix: function(pathPrefix)
    {
        this._cachedURIPrefixes = this._cachedURIPrefixes || {};
        if (this._cachedURIPrefixes.hasOwnProperty(pathPrefix))
            return this._cachedURIPrefixes[pathPrefix];
        var uriPrefix = null;
        for (var id in this._fileSystemPaths) {
            var fileSystemPath = this._fileSystemPaths[id];
            if (pathPrefix.startsWith(fileSystemPath + "/")) {
                uriPrefix = id + pathPrefix.substr(fileSystemPath.length);
                break;
            }
        }
        this._cachedURIPrefixes[pathPrefix] = uriPrefix;
        return uriPrefix;
    },

    __proto__: WebInspector.Object.prototype
}
