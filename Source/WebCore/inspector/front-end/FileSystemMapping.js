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

/**
 * @constructor
 */
WebInspector.FileSystemMapping.FileDescriptor = function(fileSystemPath, filePath)
{
    this.fileSystemPath = fileSystemPath;
    this.filePath = filePath;
}

WebInspector.FileSystemMapping.prototype = {
    /**
     * @return {Array.<string>}
     */
    fileSystemPaths: function() { },

    /**
     * @param {string} uri
     * @return {?WebInspector.FileSystemMapping.FileDescriptor}
     */
    fileForURI: function(uri) { },

    /**
     * @param {WebInspector.FileSystemMapping.FileDescriptor} fileDescriptor
     * @return {string}
     */
    uriForFile: function(fileDescriptor) { },

    /**
     * @param {string} path
     * @return {?string}
     */
    uriForPath: function(path) { }
}

/**
 * @constructor
 * @implements {WebInspector.FileSystemMapping}
 * @extends {WebInspector.Object}
 */
WebInspector.FileSystemMappingImpl = function()
{
    this._fileSystemMappingSetting = WebInspector.settings.createSetting("fileSystemMapping", {});
    /** @type {!Object.<string, string>} */
    this._mappedNames = this._fileSystemMappingSetting.get();
}

WebInspector.FileSystemMappingImpl.prototype = {
    /**
     * @param {string} fileSystemPath
     */
    addFileSystemMapping: function(fileSystemPath)
    {
        if (this._mappedNames[fileSystemPath])
            return;
        var pathSegments = fileSystemPath.split("/");
        var mappedName = pathSegments[pathSegments.length - 1];
        var uniqueMappedName = this._uniqueMappedName(mappedName);
        this._mappedNames[fileSystemPath] = mappedName;
        this._fileSystemMappingSetting.set(this._mappedNames);
    },

    /**
     * @param {string} mappedName
     */
    _uniqueMappedName: function(mappedName)
    {
        var uniqueMappedName = mappedName;
        var i = 1;
        while (Object.values(this._mappedNames).indexOf(uniqueMappedName) !== -1) {
            uniqueMappedName = mappedName + " (" + i + ")";
            ++i;
        }
        return uniqueMappedName;
    },

    /**
     * @param {string} fileSystemPath
     */
    removeFileSystemMapping: function(fileSystemPath)
    {
        delete this._mappedNames[fileSystemPath];
        this._fileSystemMappingSetting.set(this._mappedNames);
    },

    /**
     * @return {Array.<string>}
     */
    fileSystemPaths: function()
    {
        return Object.keys(this._mappedNames);
    },

    /**
     * @return {string}
     */
    _uriPrefixForMappedName: function(mappedName)
    {
        return "file:///" + mappedName + "/";
    },

    /**
     * @param {string} uri
     * @return {?WebInspector.FileSystemMapping.FileDescriptor}
     */
    fileForURI: function(uri)
    {
        for (var fileSystemPath in this._mappedNames) {
            var uriPrefix = this._uriPrefixForMappedName(this._mappedNames[fileSystemPath]);
            if (uri.indexOf(uriPrefix) === 0)
                return new WebInspector.FileSystemMapping.FileDescriptor(fileSystemPath, "/" + uri.substring(uriPrefix.length));
        }
        return null;
    },

    /**
     * @param {WebInspector.FileSystemMapping.FileDescriptor} fileDescriptor
     * @return {string}
     */
    uriForFile: function(fileDescriptor)
    {
        var uriPrefix = this._uriPrefixForMappedName(this._mappedNames[fileDescriptor.fileSystemPath]);
        return uriPrefix + fileDescriptor.filePath.substring("/".length);
    },
    
    /**
     * @param {string} path
     * @return {?string}
     */
    uriForPath: function(path)
    {
        for (var fileSystemPath in this._mappedNames) {
            if (path.indexOf(fileSystemPath) === 0) {
                var uriPrefix = this._uriPrefixForMappedName(this._mappedNames[fileSystemPath]);
                var subPath = path.substring(fileSystemPath.length);
                if (subPath.length === 0)
                    return uriPrefix;
                else if (subPath[0] === "/")
                    return uriPrefix + subPath.substring("/".length);
            }
        }
        return null;
    }
}
