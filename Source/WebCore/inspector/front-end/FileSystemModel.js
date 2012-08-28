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
 * @extends {WebInspector.Object}
 */
WebInspector.FileSystemModel = function()
{
    WebInspector.Object.call(this);

    this._originForFrameId = {};
    this._frameIdsForOrigin = {};
    this._fileSystemsForOrigin = {};

    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameAdded, this._frameAdded, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameNavigated, this._frameNavigated, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, this._frameDetached, this);

    this._agentWrapper = new WebInspector.FileSystemRequestManager();

    if (WebInspector.resourceTreeModel.mainFrame)
        this._attachFrameRecursively(WebInspector.resourceTreeModel.mainFrame);
}

WebInspector.FileSystemModel.prototype = {
    /**
     * @param {WebInspector.Event} event
     */
    _frameAdded: function(event)
    {
        var frame = /** @type {WebInspector.ResourceTreeFrame} */ event.data;
        this._attachFrameRecursively(frame);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _frameNavigated: function(event)
    {
        var frame = /** @type {WebInspector.ResourceTreeFrame} */ event.data;
        this._attachFrameRecursively(frame);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _frameDetached: function(event)
    {
        var frame = /** @type {WebInspector.ResourceTreeFrame} */ event.data;
        this._detachFrameRecursively(frame);
    },

    /**
     * @param {WebInspector.ResourceTreeFrame} frame
     */
    _attachFrame: function(frame)
    {
        if (this._originForFrameId[frame.id])
            this._detachFrameRecursively(frame);

        if (frame.securityOrigin === "null")
            return;

        this._originForFrameId[frame.id] = frame.securityOrigin;

        var newOrigin = false;
        if (!this._frameIdsForOrigin[frame.securityOrigin]) {
            this._frameIdsForOrigin[frame.securityOrigin] = {};
            newOrigin = true;
        }
        this._frameIdsForOrigin[frame.securityOrigin][frame.id] = frame.id;
        if (newOrigin)
            this._originAdded(frame.securityOrigin);
    },

    /**
     * @param {WebInspector.ResourceTreeFrame} frame
     */
    _attachFrameRecursively: function(frame)
    {
        this._attachFrame(frame);
        for (var i = 0; i < frame.childFrames.length; ++i)
            this._attachFrameRecursively(frame.childFrames[i]);
    },

    /**
     * @param {WebInspector.ResourceTreeFrame} frame
     */
    _detachFrame: function(frame)
    {
        if (!this._originForFrameId[frame.id])
            return;
        var origin = this._originForFrameId[frame.id];
        delete this._originForFrameId[frame.id];
        delete this._frameIdsForOrigin[origin][frame.id];

        var lastOrigin = Object.isEmpty(this._frameIdsForOrigin[origin]);
        if (lastOrigin) {
            delete this._frameIdsForOrigin[origin];
            this._originRemoved(origin);
        }
    },

    /**
     * @param {WebInspector.ResourceTreeFrame} frame
     */
    _detachFrameRecursively: function(frame)
    {
        for (var i = 0; i < frame.childFrames.length; ++i)
            this._detachFrameRecursively(frame.childFrames[i]);
        this._detachFrame(frame);
    },

    /**
     * @param {string} origin
     */
    _originAdded: function(origin)
    {
        this._fileSystemsForOrigin[origin] = {};

        var types = ["persistent", "temporary"];
        for (var i = 0; i < types.length; ++i)
            this._agentWrapper.requestFileSystemRoot(origin, types[i], this._fileSystemRootReceived.bind(this, origin, types[i], this._fileSystemsForOrigin[origin]));
    },

    /**
     * @param {string} origin
     */
    _originRemoved: function(origin)
    {
        for (var type in this._fileSystemsForOrigin[origin]) {
            var fileSystem = this._fileSystemsForOrigin[origin][type];
            delete this._fileSystemsForOrigin[origin][type];
            this._fileSystemRemoved(fileSystem);
        }
        delete this._fileSystemsForOrigin[origin];
    },

    /**
     * @param {WebInspector.FileSystemModel.FileSystem} fileSystem
     */
    _fileSystemAdded: function(fileSystem)
    {
        this.dispatchEventToListeners(WebInspector.FileSystemModel.EventTypes.FileSystemAdded, fileSystem);
    },

    /**
     * @param {WebInspector.FileSystemModel.FileSystem} fileSystem
     */
    _fileSystemRemoved: function(fileSystem)
    {
        this.dispatchEventToListeners(WebInspector.FileSystemModel.EventTypes.FileSystemRemoved, fileSystem);
    },

    refreshFileSystemList: function()
    {
        if (WebInspector.resourceTreeModel.mainFrame) {
            this._detachFrameRecursively(WebInspector.resourceTreeModel.mainFrame);
            this._attachFrameRecursively(WebInspector.resourceTreeModel.mainFrame);
        }
    },

    /**
     * @param {string} origin
     * @param {string} type
     * @param {Object.<WebInspector.FileSystemModel.FileSystem>} store
     * @param {number} errorCode
     * @param {FileSystemAgent.Entry=} backendRootEntry
     */
    _fileSystemRootReceived: function(origin, type, store, errorCode, backendRootEntry)
    {
        if (!errorCode && backendRootEntry && this._fileSystemsForOrigin[origin] === store) {
            var fileSystem = new WebInspector.FileSystemModel.FileSystem(this, origin, type, backendRootEntry);
            store[type] = fileSystem;
            this._fileSystemAdded(fileSystem);
        }
    },

    /**
     * @param {WebInspector.FileSystemModel.Directory} directory
     * @param {function(number, Array.<WebInspector.FileSystemModel.Entry>=)} callback
     */
    requestDirectoryContent: function(directory, callback)
    {
        this._agentWrapper.requestDirectoryContent(directory.url, this._directoryContentReceived.bind(this, directory, callback));
    },

    /**
     * @param {WebInspector.FileSystemModel.Directory} parentDirectory
     * @param {function(number, Array.<WebInspector.FileSystemModel.Entry>=)} callback
     * @param {number} errorCode
     * @param {Array.<FileSystemAgent.Entry>=} backendEntries
     */
    _directoryContentReceived: function(parentDirectory, callback, errorCode, backendEntries)
    {
        if (errorCode !== 0) {
            callback(errorCode, null);
            return;
        }

        var entries = [];
        for (var i = 0; i < backendEntries.length; ++i) {
            if (backendEntries[i].isDirectory)
                entries.push(new WebInspector.FileSystemModel.Directory(this, parentDirectory.fileSystem, backendEntries[i]));
            else
                entries.push(new WebInspector.FileSystemModel.File(this, parentDirectory.fileSystem, backendEntries[i]));
        }

        callback(errorCode, entries);
    },

    /**
     * @param {WebInspector.FileSystemModel.Entry} entry
     * @param {function(number, FileSystemAgent.Metadata=)} callback
     */
    requestMetadata: function(entry, callback)
    {
        this._agentWrapper.requestMetadata(entry.url, callback);
    },

    /**
     * @param {WebInspector.FileSystemModel.File} file
     * @param {boolean} readAsText
     * @param {number=} start
     * @param {number=} end
     * @param {string=} charset
     * @param {function(number, string=, string=)=} callback
     */
    requestFileContent: function(file, readAsText, start, end, charset, callback)
    {
        this._agentWrapper.requestFileContent(file.url, readAsText, start, end, charset, callback);
    },

    /**
     * @param {WebInspector.FileSystemModel.Entry} entry
     * @param {function(number)=} callback
     */
    deleteEntry: function(entry, callback)
    {
        var fileSystemModel = this;
        if (entry === entry.fileSystem.root)
            this._agentWrapper.deleteEntry(entry.url, hookFileSystemDeletion);
        else
            this._agentWrapper.deleteEntry(entry.url, callback);

        function hookFileSystemDeletion(errorCode)
        {
            callback(errorCode);
            if (!errorCode)
                fileSystemModel._removeFileSystem(entry.fileSystem);
        }
    },

    /**
     * @param {WebInspector.FileSystemModel.FileSystem} fileSystem
     */
    _removeFileSystem: function(fileSystem)
    {
        var origin = fileSystem.origin;
        var type = fileSystem.type;
        if (this._fileSystemsForOrigin[origin] && this._fileSystemsForOrigin[origin][type]) {
            delete this._fileSystemsForOrigin[origin][type];
            this._fileSystemRemoved(fileSystem);

            if (Object.isEmpty(this._fileSystemsForOrigin[origin]))
                delete this._fileSystemsForOrigin[origin];
        }
    }
}

WebInspector.FileSystemModel.EventTypes = {
    FileSystemAdded: "FileSystemAdded",
    FileSystemRemoved: "FileSystemRemoved"
}

WebInspector.FileSystemModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {WebInspector.FileSystemModel} fileSystemModel
 * @param {string} origin
 * @param {string} type
 * @param {FileSystemAgent.Entry} backendRootEntry
 */
WebInspector.FileSystemModel.FileSystem = function(fileSystemModel, origin, type, backendRootEntry)
{
    this.origin = origin;
    this.type = type;

    this.root = new WebInspector.FileSystemModel.Directory(fileSystemModel, this, backendRootEntry);
}

WebInspector.FileSystemModel.FileSystem.prototype = {
    /**
     * @type {string}
     */
    get name()
    {
        return "filesystem:" + this.origin + "/" + this.type;
    }
}

/**
 * @constructor
 * @param {WebInspector.FileSystemModel} fileSystemModel
 * @param {WebInspector.FileSystemModel.FileSystem} fileSystem
 * @param {FileSystemAgent.Entry} backendEntry
 */
WebInspector.FileSystemModel.Entry = function(fileSystemModel, fileSystem, backendEntry)
{
    this._fileSystemModel = fileSystemModel;
    this._fileSystem = fileSystem;

    this._url = backendEntry.url;
    this._name = backendEntry.name;
    this._isDirectory = backendEntry.isDirectory;
}

/**
 * @param {WebInspector.FileSystemModel.Entry} x
 * @param {WebInspector.FileSystemModel.Entry} y
 * @return {number}
 */
WebInspector.FileSystemModel.Entry.compare = function(x, y)
{
    if (x.isDirectory != y.isDirectory)
        return y.isDirectory ? 1 : -1;
    return x.name.localeCompare(y.name);
}

WebInspector.FileSystemModel.Entry.prototype = {
    /**
     * @type {WebInspector.FileSystemModel}
     */
    get fileSystemModel()
    {
        return this._fileSystemModel;
    },

    /**
     * @type {WebInspector.FileSystemModel.FileSystem}
     */
    get fileSystem()
    {
        return this._fileSystem;
    },

    /**
     * @type {string}
     */
    get url()
    {
        return this._url;
    },

    /**
     * @type {string}
     */
    get name()
    {
        return this._name;
    },

    /**
     * @type {boolean}
     */
    get isDirectory()
    {
        return this._isDirectory;
    },

    /**
     * @param {function(number, FileSystemAgent.Metadata)} callback
     */
    requestMetadata: function(callback)
    {
        this.fileSystemModel.requestMetadata(this, callback);
    },

    /**
     * @param {function(number)} callback
     */
    deleteEntry: function(callback)
    {
        this.fileSystemModel.deleteEntry(this, callback);
    }
}

/**
 * @constructor
 * @extends {WebInspector.FileSystemModel.Entry}
 * @param {WebInspector.FileSystemModel} fileSystemModel
 * @param {WebInspector.FileSystemModel.FileSystem} fileSystem
 * @param {FileSystemAgent.Entry} backendEntry
 */
WebInspector.FileSystemModel.Directory = function(fileSystemModel, fileSystem, backendEntry)
{
    WebInspector.FileSystemModel.Entry.call(this, fileSystemModel, fileSystem, backendEntry);
}

WebInspector.FileSystemModel.Directory.prototype = {
    /**
     * @param {function(number, Array.<WebInspector.FileSystemModel.Directory>)} callback
     */
    requestDirectoryContent: function(callback)
    {
        this.fileSystemModel.requestDirectoryContent(this, callback);
    }
}

WebInspector.FileSystemModel.Directory.prototype.__proto__ = WebInspector.FileSystemModel.Entry.prototype;

/**
 * @constructor
 * @extends {WebInspector.FileSystemModel.Entry}
 * @param {WebInspector.FileSystemModel} fileSystemModel
 * @param {WebInspector.FileSystemModel.FileSystem} fileSystem
 * @param {FileSystemAgent.Entry} backendEntry
 */
WebInspector.FileSystemModel.File = function(fileSystemModel, fileSystem, backendEntry)
{
    WebInspector.FileSystemModel.Entry.call(this, fileSystemModel, fileSystem, backendEntry);

    this._mimeType = backendEntry.mimeType;
    this._resourceType = WebInspector.resourceTypes[backendEntry.resourceType];
    this._isTextFile = backendEntry.isTextFile;
}

WebInspector.FileSystemModel.File.prototype = {
    /**
     * @type {string}
     */
    get mimeType()
    {
        return this._mimeType;
    },

    /**
     * @type {WebInspector.ResourceType}
     */
    get resourceType()
    {
        return this._resourceType;
    },

    /**
     * @type {boolean}
     */
    get isTextFile()
    {
        return this._isTextFile;
    },

    /**
     * @param {boolean} readAsText
     * @param {number=} start
     * @param {number=} end
     * @param {string=} charset
     * @param {function(number, string=)=} callback
     */
    requestFileContent: function(readAsText, start, end, charset, callback)
    {
        this.fileSystemModel.requestFileContent(this, readAsText, start, end, charset, callback);
    }
}

WebInspector.FileSystemModel.File.prototype.__proto__ = WebInspector.FileSystemModel.Entry.prototype;

/**
 * @constructor
 */
WebInspector.FileSystemRequestManager = function()
{
    this._pendingFileSystemRootRequests = {};
    this._pendingDirectoryContentRequests = {};
    this._pendingMetadataRequests = {};
    this._pendingFileContentRequests = {};
    this._pendingDeleteEntryRequests = {};

    InspectorBackend.registerFileSystemDispatcher(new WebInspector.FileSystemDispatcher(this));
    FileSystemAgent.enable();
}

WebInspector.FileSystemRequestManager.prototype = {
    /**
     * @param {string} origin
     * @param {string} type
     * @param {function(number, FileSystemAgent.Entry=)=} callback
     */
    requestFileSystemRoot: function(origin, type, callback)
    {
        var store = this._pendingFileSystemRootRequests;
        FileSystemAgent.requestFileSystemRoot(origin, type, requestAccepted);

        function requestAccepted(error, requestId)
        {
            if (error)
                callback(FileError.SECURITY_ERR);
            else
                store[requestId] = callback || function() {};
        }
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     * @param {FileSystemAgent.Entry=} backendRootEntry
     */
    _fileSystemRootReceived: function(requestId, errorCode, backendRootEntry)
    {
        var callback = this._pendingFileSystemRootRequests[requestId];
        if (!callback)
            return;
        delete this._pendingFileSystemRootRequests[requestId];
        callback(errorCode, backendRootEntry);
    },

    /**
     * @param {string} url
     * @param {function(number, Array.<FileSystemAgent.Entry>=)=} callback
     */
    requestDirectoryContent: function(url, callback)
    {
        var store = this._pendingDirectoryContentRequests;
        FileSystemAgent.requestDirectoryContent(url, requestAccepted);

        function requestAccepted(error, requestId)
        {
            if (error)
                callback(FileError.SECURITY_ERR);
            else
                store[requestId] = callback || function() {};
        }
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     * @param {Array.<FileSystemAgent.Entry>=} backendEntries
     */
    _directoryContentReceived: function(requestId, errorCode, backendEntries)
    {
        var callback = /** @type {function(number, Array.<FileSystemAgent.Entry>=)} */ this._pendingDirectoryContentRequests[requestId];
        if (!callback)
            return;
        delete this._pendingDirectoryContentRequests[requestId];
        callback(errorCode, backendEntries);
    },

    /**
     * @param {string} url
     * @param {function(number, FileSystemAgent.Metadata=)=} callback
     */
    requestMetadata: function(url, callback)
    {
        var store = this._pendingMetadataRequests;
        FileSystemAgent.requestMetadata(url, requestAccepted);

        function requestAccepted(error, requestId)
        {
            if (error)
                callback(FileError.SECURITY_ERR);
            else
                store[requestId] = callback || function() {};
        }
    },

    _metadataReceived: function(requestId, errorCode, metadata)
    {
        var callback = this._pendingMetadataRequests[requestId];
        if (!callback)
            return;
        delete this._pendingMetadataRequests[requestId];
        callback(errorCode, metadata);
    },

    /**
     * @param {string} url
     * @param {boolean} readAsText
     * @param {number=} start
     * @param {number=} end
     * @param {string=} charset
     * @param {function(number, string=, string=)=} callback
     */
    requestFileContent: function(url, readAsText, start, end, charset, callback)
    {
        var store = this._pendingFileContentRequests;
        FileSystemAgent.requestFileContent(url, readAsText, start, end, charset, requestAccepted);

        function requestAccepted(error, requestId)
        {
            if (error)
                callback(FileError.SECURITY_ERR);
            else
                store[requestId] = callback || function() {};
        }
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     * @param {string=} content
     * @param {string=} charset
     */
    _fileContentReceived: function(requestId, errorCode, content, charset)
    {
        var callback = /** @type {function(number, string=, string=)} */ this._pendingFileContentRequests[requestId];
        if (!callback)
            return;
        delete this._pendingFileContentRequests[requestId];
        callback(errorCode, content, charset);
    },

    /**
     * @param {string} url
     * @param {function(number)=} callback
     */
    deleteEntry: function(url, callback)
    {
        var store = this._pendingDeleteEntryRequests;
        FileSystemAgent.deleteEntry(url, requestAccepted);

        function requestAccepted(error, requestId)
        {
            if (error)
                callback(FileError.SECURITY_ERR);
            else
                store[requestId] = callback || function() {};
        }
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     */
    _deletionCompleted: function(requestId, errorCode)
    {
        var callback = /** @type {function(number)} */ this._pendingDeleteEntryRequests[requestId];
        if (!callback)
            return;
        delete this._pendingDeleteEntryRequests[requestId];
        callback(errorCode);
    }
}

/**
 * @constructor
 * @implements {FileSystemAgent.Dispatcher}
 * @param {WebInspector.FileSystemRequestManager} agentWrapper
 */
WebInspector.FileSystemDispatcher = function(agentWrapper)
{
    this._agentWrapper = agentWrapper;
}

WebInspector.FileSystemDispatcher.prototype = {
    /**
     * @param {number} requestId
     * @param {number} errorCode
     * @param {FileSystemAgent.Entry=} backendRootEntry
     */
    fileSystemRootReceived: function(requestId, errorCode, backendRootEntry)
    {
        this._agentWrapper._fileSystemRootReceived(requestId, errorCode, backendRootEntry);
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     * @param {Array.<FileSystemAgent.Entry>=} backendEntries
     */
    directoryContentReceived: function(requestId, errorCode, backendEntries)
    {
        this._agentWrapper._directoryContentReceived(requestId, errorCode, backendEntries);
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     * @param {FileSystemAgent.Metadata=} metadata
     */
    metadataReceived: function(requestId, errorCode, metadata)
    {
        this._agentWrapper._metadataReceived(requestId, errorCode, metadata);
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     * @param {string=} content
     * @param {string=} charset
     */
    fileContentReceived: function(requestId, errorCode, content, charset)
    {
        this._agentWrapper._fileContentReceived(requestId, errorCode, content, charset);
    },

    /**
     * @param {number} requestId
     * @param {number} errorCode
     */
    deletionCompleted: function(requestId, errorCode)
    {
        this._agentWrapper._deletionCompleted(requestId, errorCode);
    }
}
