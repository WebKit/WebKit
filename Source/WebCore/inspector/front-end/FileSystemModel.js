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
 * @implements {FileSystemAgent.Dispatcher}
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

    InspectorBackend.registerFileSystemDispatcher(this);
    FileSystemAgent.enable();

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
        for (var i = 0; i < types.length; ++i) {
            // FIXME(88602): Check if the filesystem is initialized, and show it only if it was.
            var fileSystem = new WebInspector.FileSystemModel.FileSystem(this, origin, types[i]);
            this._fileSystemsForOrigin[origin][types[i]] = fileSystem;
            this._fileSystemAdded(fileSystem);
        }
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
        for (var origin in this._fileSystemsForOrigin)
            for (var type in this._fileSystemsForOrigin[origin])
                this._fileSystemRemoved(this._fileSystemsForOrigin[origin][type]);

        for (var origin in this._fileSystemsForOrigin)
            for (var type in this._fileSystemsForOrigin[origin])
                this._fileSystemAdded(this._fileSystemsForOrigin[origin][type]);
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
 */
WebInspector.FileSystemModel.FileSystem = function(fileSystemModel, origin, type)
{
    this.origin = origin;
    this.type = type;
}

WebInspector.FileSystemModel.FileSystem.prototype = {
    get name()
    {
        return "filesystem:" + this.origin + "/" + this.type;
    }
}
