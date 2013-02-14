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
 * @param {WebInspector.IsolatedFileSystemModel} isolatedFileSystemModel
 * @param {string} fileSystemId
 * @param {string} fileSystemPath
 */
WebInspector.FileSystemProjectDelegate = function(isolatedFileSystemModel, fileSystemId, fileSystemPath)
{
    this._isolatedFileSystemModel = isolatedFileSystemModel;
    this._fileSystemId = fileSystemId;
    this._fileSystemPath = fileSystemPath;
    this._files = {};
    this._populate();
}

WebInspector.FileSystemProjectDelegate._scriptExtensions = ["js", "java", "cc", "cpp", "h", "cs", "py", "php"].keySet();

WebInspector.FileSystemProjectDelegate.prototype = {
    /**
     * @return {string}
     */
    id: function()
    {
        return this._fileSystemId;
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
        return this._fileSystemPath.substr(this._fileSystemPath.lastIndexOf("/") + 1);
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
        WebInspector.FileSystemUtils.requestFileContent(this._isolatedFileSystemModel, this._fileSystemPath, filePath, innerCallback.bind(this));
        
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
        WebInspector.FileSystemUtils.setFileContent(this._isolatedFileSystemModel, this._fileSystemPath, filePath, newContent, callback.bind(this, ""));
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
        WebInspector.FileSystemUtils.requestFilesRecursive(this._isolatedFileSystemModel, this._fileSystemPath, "", filesLoaded.bind(this));

        function filesLoaded(files)
        {
            for (var i = 0; i < files.length; ++i) {
                var uri = this.id() + files[i];
                var contentType = this._contentTypeForPath(files[i]);
                var url = WebInspector.fileMapping.urlForURI(uri);
                var fileDescriptor = new WebInspector.FileDescriptor(uri, "file://" + this._fileSystemPath + files[i], url, contentType, true);
                this._addFile(fileDescriptor);
            } 
        }
    },

    /**
     * @param {WebInspector.FileDescriptor} fileDescriptor
     */
    _addFile: function(fileDescriptor)
    {
        if (!this._files[this._fileSystemPath])
            this._files[this._fileSystemPath] = {};
        this._files[this._fileSystemPath][fileDescriptor.path] = true;
        this.dispatchEventToListeners(WebInspector.ProjectDelegate.Events.FileAdded, fileDescriptor);
    },

    /**
     * @param {string} uri
     */
    _removeFile: function(uri)
    {
        delete this._files[this._fileSystemPath][uri];
        if (Object.keys(this._files[this._fileSystemPath]).length === 0)
            delete this._files[this._fileSystemPath];
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

/**
 * @constructor
 */
WebInspector.FileSystemUtils = function()
{
}

WebInspector.FileSystemUtils.errorMessage = function(error)
{
    var msg;
    switch (error.code) {
    case FileError.QUOTA_EXCEEDED_ERR:
        msg = "QUOTA_EXCEEDED_ERR";
        break;
    case FileError.NOT_FOUND_ERR:
        msg = "NOT_FOUND_ERR";
        break;
    case FileError.SECURITY_ERR:
        msg = "SECURITY_ERR";
        break;
    case FileError.INVALID_MODIFICATION_ERR:
        msg = "INVALID_MODIFICATION_ERR";
        break;
    case FileError.INVALID_STATE_ERR:
        msg = "INVALID_STATE_ERR";
        break;
    default:
        msg = "Unknown Error";
        break;
    };

    return "File system error: " + msg;
}

/**
 * @param {WebInspector.IsolatedFileSystemModel} isolatedFileSystemModel
 * @param {string} fileSystemPath
 * @param {function(DOMFileSystem)} callback
 */
WebInspector.FileSystemUtils.requestFileSystem = function(isolatedFileSystemModel, fileSystemPath, callback)
{
    isolatedFileSystemModel.requestDOMFileSystem(fileSystemPath, callback);
}

/**
 * @param {WebInspector.IsolatedFileSystemModel} isolatedFileSystemModel
 * @param {string} fileSystemPath
 * @param {string} path
 * @param {function(Array.<string>)} callback
 */
WebInspector.FileSystemUtils.requestFilesRecursive = function(isolatedFileSystemModel, fileSystemPath, path, callback)
{
    WebInspector.FileSystemUtils.requestFileSystem(isolatedFileSystemModel, fileSystemPath, fileSystemLoaded);

    var fileSystem;
    /**
     * @param {DOMFileSystem} fs
     */
    function fileSystemLoaded(fs)
    {
        fileSystem = fs;
        WebInspector.FileSystemUtils._requestEntries(fileSystem, path, innerCallback);
    }

    var result = [];
    var callbacksLeft = 1;
    /**
     * @param {Array.<FileEntry>} entries
     */
    function innerCallback(entries)
    {
        for (var i = 0; i < entries.length; ++i) {
            var entry = entries[i];
            if (!entry.isDirectory)
                result.push(entry.fullPath);
            else {
                callbacksLeft++;
                WebInspector.FileSystemUtils._requestEntries(fileSystem, entry.fullPath, innerCallback);
            }
        }
        if (!--callbacksLeft)
            callback(result);
    }
}

/**
 * @param {WebInspector.IsolatedFileSystemModel} isolatedFileSystemModel
 * @param {string} fileSystemPath
 * @param {string} path
 * @param {function(?string)} callback
 */
WebInspector.FileSystemUtils.requestFileContent = function(isolatedFileSystemModel, fileSystemPath, path, callback)
{
    WebInspector.FileSystemUtils.requestFileSystem(isolatedFileSystemModel, fileSystemPath, fileSystemLoaded);

    var fileSystem;
    
    /**
     * @param {DOMFileSystem} fs
     */
    function fileSystemLoaded(fs)
    {
        fs.root.getFile(path, null, fileEntryLoaded, errorHandler);
    }

    /**
     * @param {FileEntry} entry
     */
    function fileEntryLoaded(entry)
    {
        entry.file(fileLoaded, errorHandler);
    }

    /**
     * @param {!Blob} file
     */
    function fileLoaded(file)
    {
        var reader = new FileReader();
        reader.onloadend = readerLoadEnd;
        reader.readAsText(file);
    }

    /**
     * @this {FileReader}
     */
    function readerLoadEnd()
    {
        callback(/** @type {string} */ (this.result));
    }

    function errorHandler(error)
    {
        var errorMessage = WebInspector.FileSystemUtils.errorMessage(error);
        console.error(errorMessage + " when getting content for file '" + (fileSystemPath + "/" + path) + "'");
        callback(null);
    }
}

/**
 * @param {WebInspector.IsolatedFileSystemModel} isolatedFileSystemModel
 * @param {string} fileSystemPath
 * @param {string} path
 * @param {string} content
 * @param {function()} callback
 */
WebInspector.FileSystemUtils.setFileContent = function(isolatedFileSystemModel, fileSystemPath, path, content, callback)
{
    WebInspector.FileSystemUtils.requestFileSystem(isolatedFileSystemModel, fileSystemPath, fileSystemLoaded);

    var fileSystem;

    /**
     * @param {DOMFileSystem} fs
     */
    function fileSystemLoaded(fs)
    {
        fs.root.getFile(path, null, fileEntryLoaded, errorHandler);
    }

    /**
     * @param {FileEntry} entry
     */
    function fileEntryLoaded(entry)
    {
        entry.createWriter(fileWriterCreated, errorHandler);
    }

    /**
     * @param {FileWriter} fileWriter
     */
    function fileWriterCreated(fileWriter)
    {
        fileWriter.onerror = errorHandler;
        fileWriter.onwriteend = fileTruncated;
        fileWriter.truncate(0);

        function fileTruncated()
        {
            fileWriter.onwriteend = writerEnd;
            var blob = new Blob([content], { type: "text/plain" });
            fileWriter.write(blob);
        }
    }

    function writerEnd()
    {
        callback();
    }

    function errorHandler(error)
    {
        var errorMessage = WebInspector.FileSystemUtils.errorMessage(error);
        console.error(errorMessage + " when setting content for file '" + (fileSystemPath + "/" + path) + "'");
        callback();
    }
}

/**
 * @param {DirectoryEntry} dirEntry
 * @param {function(Array.<FileEntry>)} callback
 */
WebInspector.FileSystemUtils._readDirectory = function(dirEntry, callback)
{
    var dirReader = dirEntry.createReader();
    var entries = [];

    function innerCallback(results)
    {
        if (!results.length)
            callback(entries.sort());
        else {
            entries = entries.concat(toArray(results));
            dirReader.readEntries(innerCallback, errorHandler);
        }
    }

    function toArray(list)
    {
        return Array.prototype.slice.call(list || [], 0);
    }    

    dirReader.readEntries(innerCallback, errorHandler);

    function errorHandler(error)
    {
        var errorMessage = WebInspector.FileSystemUtils.errorMessage(error);
        console.error(errorMessage + " when reading directory '" + dirEntry.fullPath + "'");
        callback([]);
    }
}

/**
 * @param {DOMFileSystem} fileSystem
 * @param {string} path
 * @param {function(Array.<FileEntry>)} callback
 */
WebInspector.FileSystemUtils._requestEntries = function(fileSystem, path, callback)
{
    fileSystem.root.getDirectory(path, null, innerCallback, errorHandler);

    function innerCallback(dirEntry)
    {
        WebInspector.FileSystemUtils._readDirectory(dirEntry, callback)
    }

    function errorHandler(error)
    {
        var errorMessage = WebInspector.FileSystemUtils.errorMessage(error);
        console.error(errorMessage + " when requesting entry '" + path + "'");
        callback([]);
    }
}
