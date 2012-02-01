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
WebInspector.IndexedDBModel = function()
{
    this._indexedDBRequestManager = new WebInspector.IndexedDBRequestManager();
    
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameNavigated, this._frameNavigated, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, this._frameDetached, this);
    
    this._frames = {};
    this._frameIdsBySecurityOrigin = {};
    this._databaseNamesBySecurityOrigin = {};

    this.refreshDatabaseNames();
}

WebInspector.IndexedDBModel.prototype = {
    refreshDatabaseNames: function()
    {
        this._reset();
        this._framesNavigatedRecursively(WebInspector.resourceTreeModel.mainFrame);
    },
    
    refreshDatabase: function(frameId, databaseName)
    {
        this._loadDatabase(frameId, databaseName);
    },
    
    /**
     * @param {WebInspector.ResourceTreeFrame} resourceTreeFrame
     */
    _framesNavigatedRecursively: function(resourceTreeFrame)
    {
        this._processFrameNavigated(resourceTreeFrame);
        for (var i = 0; i < resourceTreeFrame.childFrames.length; ++i)
            this._framesNavigatedRecursively(resourceTreeFrame.childFrames[i]);            
    },
    
    /**
     * @param {WebInspector.Event} event
     */
    _frameNavigated: function(event)
    {
        var resourceTreeFrame = /** @type {WebInspector.ResourceTreeFrame} */ event.data;
        this._processFrameNavigated(resourceTreeFrame);
    },
    
    /**
     * @param {WebInspector.Event} event
     */
    _frameDetached: function(event)
    {
        var resourceTreeFrame = /** @type {WebInspector.ResourceTreeFrame} */ event.data;
        this._originRemovedFromFrame(resourceTreeFrame.id);
        this._indexedDBRequestManager._frameDetached(resourceTreeFrame.id);
    },

    _reset: function()
    {
        this._frames = {};
        this._frameIdsBySecurityOrigin = {};
        this._databaseNamesBySecurityOrigin = {};
        this._indexedDBRequestManager._reset();
        // FIXME: dispatch events?
    },

    /**
     * @param {WebInspector.ResourceTreeFrame} resourceTreeFrame
     */
    _processFrameNavigated: function(resourceTreeFrame)
    {
        if (resourceTreeFrame.securityOrigin === "null")
            return;
        if (this._frameIdsBySecurityOrigin[resourceTreeFrame.securityOrigin])
            this._originAddedToFrame(resourceTreeFrame.id, resourceTreeFrame.securityOrigin);
        else
            this._loadDatabaseNamesForFrame(resourceTreeFrame.id);
    },

    /**
     * @param {string} frameId
     * @param {string} securityOrigin
     */
    _originAddedToFrame: function(frameId, securityOrigin)
    {
        if (!this._frameIdsBySecurityOrigin[securityOrigin]) {
            this._frameIdsBySecurityOrigin[securityOrigin] = [];
            this._frameIdsBySecurityOrigin[securityOrigin].push(frameId);
            this._databaseNamesBySecurityOrigin[securityOrigin] = [];
            // FIXME: dispatch origin added event.
        }
        this._frames[frameId] = new WebInspector.IndexedDBModel.Frame(frameId, securityOrigin);
    },

    /**
     * @param {string} frameId
     */
    _originRemovedFromFrame: function(frameId)
    {
        var currentSecurityOrigin = this._frames[frameId] ? this._frames[frameId].securityOrigin : null;
        if (!currentSecurityOrigin)
            return;

        delete this._frames[frameId];

        var frameIdsForOrigin = this._frameIdsBySecurityOrigin[currentSecurityOrigin];
        for (var i = 0; i < frameIdsForOrigin; ++i) {
            if (frameIdsForOrigin[i] === frameId) {
                frameIdsForOrigin.splice(i, 1);
                break;
            }
        }
        if (!frameIdsForOrigin.length)
            this._originRemoved(currentSecurityOrigin);
    },

    /**
     * @param {string} securityOrigin
     */
    _originRemoved: function(securityOrigin)
    {
        var frameIdsForOrigin = this._frameIdsBySecurityOrigin[securityOrigin];
        for (var i = 0; i < frameIdsForOrigin; ++i)
            delete this._frames[frameIdsForOrigin[i]];
        delete this._frameIdsBySecurityOrigin[securityOrigin];
        for (var i = 0; i < this._databaseNamesBySecurityOrigin[securityOrigin]; ++i)
            this._databaseRemoved(securityOrigin, this._databaseNamesBySecurityOrigin[securityOrigin][i]);
        delete this._databaseNamesBySecurityOrigin[securityOrigin];
        // FIXME: dispatch origin removed event.
    },

    /**
     * @param {string} securityOrigin
     * @param {Array.<string>} databaseNames
     */
    _updateOriginDatabaseNames: function(securityOrigin, databaseNames)
    {
        var newDatabaseNames = {};
        for (var i = 0; i < databaseNames.length; ++i)
            newDatabaseNames[databaseNames[i]] = true;
        var oldDatabaseNames = {};
        for (var i = 0; i < this._databaseNamesBySecurityOrigin[securityOrigin].length; ++i)
            oldDatabaseNames[databaseNames[i]] = true;

        this._databaseNamesBySecurityOrigin[securityOrigin] = databaseNames;

        for (var databaseName in oldDatabaseNames) {
            if (!newDatabaseNames[databaseName])
                this._databaseRemoved(securityOrigin, databaseName);
        }
        for (var databaseName in newDatabaseNames) {
            if (!oldDatabaseNames[databaseName])
                this._databaseAdded(securityOrigin, databaseName);
        }

        if (!this._databaseNamesBySecurityOrigin[securityOrigin].length)
            this._originRemoved(securityOrigin);
    },

    /**
     * @param {string} securityOrigin
     * @param {string} databaseName
     */
    _databaseAdded: function(securityOrigin, databaseName)
    {
        // FIXME: dispatch database added event.
    },

    /**
     * @param {string} securityOrigin
     * @param {string} databaseName
     */
    _databaseRemoved: function(securityOrigin, databaseName)
    {
        this._indexedDBRequestManager._databaseRemoved(this._frameIdsBySecurityOrigin[securityOrigin], databaseName);
        // FIXME: dispatch database removed event.
    },

    /**
     * @param {string} frameId
     */
    _loadDatabaseNamesForFrame: function(frameId)
    {
        /**
         * @param {IndexedDBAgent.SecurityOriginWithDatabaseNames} securityOriginWithDatabaseNames
         */
        function callback(securityOriginWithDatabaseNames)
        {
            var databaseNames = securityOriginWithDatabaseNames.databaseNames;
            var oldSecurityOrigin = this._frames[frameId] ? this._frames[frameId].securityOrigin : null;
            if (oldSecurityOrigin && oldSecurityOrigin === securityOriginWithDatabaseNames.securityOrigin)
                this._updateOriginDatabaseNames(securityOriginWithDatabaseNames.securityOrigin, securityOriginWithDatabaseNames.databaseNames);
            else {
                this._originRemovedFromFrame(frameId);
                this._originAddedToFrame(frameId, securityOriginWithDatabaseNames.securityOrigin);
                this._updateOriginDatabaseNames(securityOriginWithDatabaseNames.securityOrigin, securityOriginWithDatabaseNames.databaseNames);
            }
        }

        this._indexedDBRequestManager.requestDatabaseNamesForFrame(frameId, callback.bind(this));
    },

    /**
     * @param {string} frameId
     * @param {string} databaseName
     */
    _loadDatabase: function(frameId, databaseName)
    {
        /**
         * @param {IndexedDBAgent.DatabaseWithObjectStores} databaseWithObjectStores
         */
        function callback(databaseWithObjectStores)
        {
            if (!this._frames[frameId])
                return;

            var databaseModel = new WebInspector.IndexedDBModel.Database(databaseName, databaseWithObjectStores.version);
            this._frames[frameId].databases[databaseName] = databaseModel;
            for (var i = 0; i < databaseWithObjectStores.objectStores.length; ++i) {
                var objectStore = databaseWithObjectStores.objectStores[i];
                var objectStoreModel = new WebInspector.IndexedDBModel.ObjectStore(objectStore.name, objectStore.keyPath);
                for (var j = 0; j < objectStore.indexes.length; ++j) {
                     var objectStoreIndex = objectStore.indexes[j];
                     var objectStoreIndexModel = new WebInspector.IndexedDBModel.ObjectStoreIndex(objectStoreIndex.name, objectStoreIndex.keyPath, objectStoreIndex.unique, objectStoreIndex.multiEntry);
                     objectStoreModel.indexes[objectStoreIndexModel.name] = objectStoreIndexModel;
                }
                databaseModel.objectStores[objectStoreModel.name] = objectStoreModel;
            }

            // FIXME: dispatch database loaded event.
        }

        this._indexedDBRequestManager.requestDatabase(frameId, databaseName, callback.bind(this));
    }
}

WebInspector.IndexedDBModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {string} frameId
 * @param {string} securityOrigin
 */
WebInspector.IndexedDBModel.Frame = function(frameId, securityOrigin)
{
    this.frameId = frameId;
    this.securityOrigin = securityOrigin;
    this.databases = {};
}

/**
 * @constructor
 * @param {string} name
 */
WebInspector.IndexedDBModel.Database = function(name, version)
{
    this.name = name;
    this.version = version;
    this.objectStores = {};
}

/**
 * @constructor
 * @param {string} name
 * @param {string} keyPath
 */
WebInspector.IndexedDBModel.ObjectStore = function(name, keyPath)
{
    this.name = name;
    this.keyPath = keyPath;
    this.indexes = {};
}

/**
 * @constructor
 * @param {string} name
 * @param {string} keyPath
 */
WebInspector.IndexedDBModel.ObjectStoreIndex = function(name, keyPath, unique, multiEntry)
{
    this.name = name;
    this.keyPath = keyPath;
    this.unique = unique;
    this.multiEntry = multiEntry;
}

/**
 * @constructor
 */
WebInspector.IndexedDBRequestManager = function()
{
    this._lastRequestId = 0;
    this._requestDatabaseNamesForFrameCallbacks = {};

    IndexedDBAgent.enable();
    InspectorBackend.registerIndexedDBDispatcher(new WebInspector.IndexedDBDispatcher(this));
}

WebInspector.IndexedDBRequestManager.prototype = {
    /**
     * @param {string} frameId
     * @param {function(IndexedDBAgent.SecurityOriginWithDatabaseNames)} callback
     */
    requestDatabaseNamesForFrame: function(frameId, callback)
    {
        var requestId = this._requestId();
        var request = new WebInspector.IndexedDBRequestManager.DatabasesForFrameRequest(frameId, callback);
        this._requestDatabaseNamesForFrameCallbacks[requestId] = request;

        function innerCallback(error)
        {
            if (error) {
                console.error("IndexedDBAgent error: " + error);
                return;
            }
        }

        IndexedDBAgent.requestDatabaseNamesForFrame(requestId, frameId, innerCallback);
    },

    /**
     * @param {number} requestId
     * @param {IndexedDBAgent.SecurityOriginWithDatabaseNames} securityOriginWithDatabaseNames
     */
    _databaseNamesLoaded: function(requestId, securityOriginWithDatabaseNames)
    {
        var request = this._requestDatabaseNamesForFrameCallbacks[requestId];
        if (!request)
            return;

        request.callback(securityOriginWithDatabaseNames);
    },

    /**
     * @param {string} frameId
     * @param {string} databaseName
     * @param {function(IndexedDBAgent.DatabaseWithObjectStores)} callback
     */
    requestDatabase: function(frameId, databaseName, callback)
    {
        var requestId = this._requestId();
        var request = new WebInspector.IndexedDBRequestManager.DatabaseRequest(frameId, databaseName, callback);
        this._requestDatabaseCallbacks[requestId] = request;

        function innerCallback(error)
        {
            if (error) {
                console.error("IndexedDBAgent error: " + error);
                return;
            }
        }

        IndexedDBAgent.requestDatabase(requestId, frameId, databaseName, innerCallback);
    },
    
    /**
     * @param {number} requestId
     * @param {IndexedDBAgent.DatabaseWithObjectStores} databaseWithObjectStores
     */
    _databaseLoaded: function(requestId, databaseWithObjectStores)
    {
        var request = this._requestDatabaseCallbacks[requestId];
        if (!request)
            return;

        request.callback(databaseWithObjectStores);
    },

    /**
     * @return {number}
     */
    _requestId: function()
    {
        return ++this._lastRequestId;
    },

    /**
     * @param {string} frameId
     */
    _frameDetached: function(frameId)
    {
        for (var requestId in this._requestDatabaseNamesForFrameCallbacks) {
            if (this._requestDatabaseNamesForFrameCallbacks[requestId].frameId === frameId)
                delete this._requestDatabaseNamesForFrameCallbacks[requestId];
        }
        
        for (var requestId in this._requestDatabaseCallbacks) {
            if (this._requestDatabaseCallbacks[requestId].frameId === frameId)
                delete this._requestDatabaseCallbacks[requestId];
        }
    },

    /**
     * @param {string} frameId
     */
    _databaseRemoved: function(frameId, databaseName)
    {
        for (var requestId in this._requestDatabaseCallbacks) {
            if (this._requestDatabaseCallbacks[requestId].frameId === frameId && this._requestDatabaseCallbacks[requestId].databaseName === databaseName)
                delete this._requestDatabaseCallbacks[requestId];
        }
    },

    _reset: function()
    {
        this._requestDatabaseNamesForFrameCallbacks = {};
        this._requestDatabaseCallbacks = {};

    }
}

/**
 * @constructor
 */
WebInspector.IndexedDBRequestManager.DatabasesForFrameRequest = function(frameId, callback)
{
    this.frameId = frameId;
    this.callback = callback;
}

/**
 * @constructor
 */
WebInspector.IndexedDBRequestManager.DatabaseRequest = function(frameId, databaseName, callback)
{
    this.frameId = frameId;
    this.databaseName = databaseName;
    this.callback = callback;
}

/**
 * @constructor
 * @implements {IndexedDBAgent.Dispatcher}
 * @param {WebInspector.IndexedDBRequestManager} indexedDBRequestManager
 */
WebInspector.IndexedDBDispatcher = function(indexedDBRequestManager)
{
    this._agentWrapper = indexedDBRequestManager;
}

WebInspector.IndexedDBDispatcher.prototype = {
    /**
     * @param {number} requestId
     * @param {IndexedDBAgent.SecurityOriginWithDatabaseNames} securityOriginWithDatabaseNames
     */
    databaseNamesLoaded: function(requestId, securityOriginWithDatabaseNames)
    {
        this._agentWrapper._databaseNamesLoaded(requestId, securityOriginWithDatabaseNames);
    },

    /**
     * @param {number} requestId
     * @param {IndexedDBAgent.DatabaseWithObjectStores} databaseWithObjectStores
     */
    databaseLoaded: function(requestId, databaseWithObjectStores)
    {
        this._agentWrapper._databaseLoaded(requestId, databaseWithObjectStores);
    }
}
