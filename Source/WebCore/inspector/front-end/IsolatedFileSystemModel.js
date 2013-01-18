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
 * @param {WebInspector.Workspace} workspace
 */
WebInspector.IsolatedFileSystemModel = function(workspace)
{
    /** @type {!Object.<string, WebInspector.IsolatedFileSystemModel.FileSystem>} */
    this._fileSystems = {};
    /** @type {Object.<string, Array.<function(DOMFileSystem)>>} */
    this._pendingFileSystemRequests = {};
    this._fileSystemMapping = new WebInspector.FileSystemMappingImpl();

    if (this.supportsFileSystems())
        this._requestFileSystems();

    this._fileSystemWorkspaceProvider = new WebInspector.FileSystemWorkspaceProvider(this);
    workspace.addProject(WebInspector.projectNames.FileSystem, this._fileSystemWorkspaceProvider);
}

/** @typedef {{fileSystemName: string, rootURL: string, fileSystemPath: string}} */
WebInspector.IsolatedFileSystemModel.FileSystem;

WebInspector.IsolatedFileSystemModel.prototype = {
    /**
     * @return {WebInspector.FileSystemMapping}
     */
    mapping: function()
    {
        return this._fileSystemMapping;
    },

    /**
     * @return {boolean}
     */
    supportsFileSystems: function()
    {
        return InspectorFrontendHost.supportsFileSystems();
    },

    _requestFileSystems: function()
    {
        console.assert(!this._loaded);
        InspectorFrontendHost.requestFileSystems();
    },

    /**
     * @param {function(?string)} callback
     */
    addFileSystem: function(callback)
    {
        this._selectFileSystemPathCallback = callback;
        InspectorFrontendHost.addFileSystem();
    },

    /**
     * @param {function()} callback
     */
    removeFileSystem: function(fileSystemPath, callback)
    {
        this._removeFileSystemCallback = callback;
        InspectorFrontendHost.removeFileSystem(fileSystemPath);
    },

    /**
     * @param {Array.<WebInspector.IsolatedFileSystemModel.FileSystem>} fileSystems
     */
    _fileSystemsLoaded: function(fileSystems)
    {
        for (var i = 0; i < fileSystems.length; ++i)
            this._innerAddFileSystem(fileSystems[i]);
        this._loaded = true;
        this._processPendingFileSystemRequests();
    },

    /**
     * @param {WebInspector.IsolatedFileSystemModel.FileSystem} fileSystem
     */
    _innerAddFileSystem: function(fileSystem)
    {
        this._fileSystems[fileSystem.fileSystemPath] = fileSystem;
        this._fileSystemMapping.addFileSystemMapping(fileSystem.fileSystemPath);
    },

    /**
     * @return {Array.<string>}
     */
    _fileSystemPaths: function()
    {
        return Object.keys(this._fileSystems);
    },

    _processPendingFileSystemRequests: function()
    {
        for (var fileSystemPath in this._pendingFileSystemRequests) {
            var callbacks = this._pendingFileSystemRequests[fileSystemPath];
            for (var i = 0; i < callbacks.length; ++i)
                callbacks[i](this._isolatedFileSystem(fileSystemPath));
        }
    },

    /**
     * @param {string} errorMessage
     * @param {WebInspector.IsolatedFileSystemModel.FileSystem} fileSystem
     */
    _fileSystemAdded: function(errorMessage, fileSystem)
    {
        var fileSystemPath;
        if (errorMessage)
            WebInspector.showErrorMessage(errorMessage)
        else if (fileSystem) {
            this._innerAddFileSystem(fileSystem);
            fileSystemPath = fileSystem.fileSystemPath;
        }

        if (this._selectFileSystemPathCallback) {
            this._selectFileSystemPathCallback(fileSystemPath);
            delete this._selectFileSystemPathCallback;
        }
    },

    /**
     * @param {string} fileSystemPath
     */
    _fileSystemRemoved: function(fileSystemPath)
    {
        delete this._fileSystems[fileSystemPath];
        this._fileSystemMapping.removeFileSystemMapping(fileSystemPath);

        if (this._removeFileSystemCallback) {
            this._removeFileSystemCallback(fileSystemPath);
            delete this._removeFileSystemCallback;
        }
    },

    /**
     * @param {string} fileSystemPath
     * @return {DOMFileSystem}
     */
    _isolatedFileSystem: function(fileSystemPath)
    {
        var fileSystem = this._fileSystems[fileSystemPath];
        if (!fileSystem)
            return null;
        if (!InspectorFrontendHost.isolatedFileSystem)
            return null;
        return InspectorFrontendHost.isolatedFileSystem(fileSystem.fileSystemName, fileSystem.rootURL);
    },

    /**
     * @param {string} fileSystemPath
     * @param {function(DOMFileSystem)} callback
     */
    requestDOMFileSystem: function(fileSystemPath, callback)
    {
        if (!this._loaded) {
            if (!this._pendingFileSystemRequests[fileSystemPath])
                this._pendingFileSystemRequests[fileSystemPath] = this._pendingFileSystemRequests[fileSystemPath] || [];
            this._pendingFileSystemRequests[fileSystemPath].push(callback);
            return;
        }
        callback(this._isolatedFileSystem(fileSystemPath));
    }
}

/**
 * @type {?WebInspector.IsolatedFileSystemModel}
 */
WebInspector.isolatedFileSystemModel = null;

/**
 * @constructor
 * @param {WebInspector.IsolatedFileSystemModel} isolatedFileSystemModel
 */
WebInspector.IsolatedFileSystemDispatcher = function(isolatedFileSystemModel)
{
    this._isolatedFileSystemModel = isolatedFileSystemModel;
}

WebInspector.IsolatedFileSystemDispatcher.prototype = {
    /**
     * @param {Array.<WebInspector.IsolatedFileSystemModel.FileSystem>} fileSystems
     */
    fileSystemsLoaded: function(fileSystems)
    {
        this._isolatedFileSystemModel._fileSystemsLoaded(fileSystems);
    },

    /**
     * @param {string} fileSystemPath
     */
    fileSystemRemoved: function(fileSystemPath)
    {
        this._isolatedFileSystemModel._fileSystemRemoved(fileSystemPath);
    },

    /**
     * @param {string} errorMessage
     * @param {WebInspector.IsolatedFileSystemModel.FileSystem} fileSystem
     */
    fileSystemAdded: function(errorMessage, fileSystem)
    {
        this._isolatedFileSystemModel._fileSystemAdded(errorMessage, fileSystem);
    }
}

/**
 * @type {?WebInspector.IsolatedFileSystemDispatcher}
 */
WebInspector.isolatedFileSystemDispatcher = null;
