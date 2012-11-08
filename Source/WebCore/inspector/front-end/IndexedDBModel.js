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
    IndexedDBAgent.enable();

    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameAdded, this._frameNavigated, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameNavigated, this._frameNavigated, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, this._frameDetached, this);

    this._frames = {};
    this._databases = new Map();
    this._frameIdsBySecurityOrigin = {};
    this._databaseNamesBySecurityOrigin = {};

    this.refreshDatabaseNames();
}

WebInspector.IndexedDBModel.KeyTypes = {
    NumberType:  "number",
    StringType:  "string",
    DateType:    "date",
    ArrayType:   "array"
};

WebInspector.IndexedDBModel.KeyPathTypes = {
    NullType:    "null",
    StringType:  "string",
    ArrayType:   "array"
};

WebInspector.IndexedDBModel.keyFromIDBKey = function(idbKey)
{
    if (typeof(idbKey) === "undefined" || idbKey === null)
        return null;

    var key = {};
    switch (typeof(idbKey)) {
    case "number":
        key.number = idbKey;
        key.type = WebInspector.IndexedDBModel.KeyTypes.NumberType;
        break;
    case "string":
        key.string = idbKey;
        key.type = WebInspector.IndexedDBModel.KeyTypes.StringType;
        break;
    case "object":
        if (idbKey instanceof Date) {
            key.date = idbKey.getTime();
            key.type = WebInspector.IndexedDBModel.KeyTypes.DateType;
        } else if (idbKey instanceof Array) {
            key.array = [];
            for (var i = 0; i < idbKey.length; ++i)
                key.array.push(WebInspector.IndexedDBModel.keyFromIDBKey(idbKey[i]));
            key.type = WebInspector.IndexedDBModel.KeyTypes.ArrayType;
        }
        break;
    default:
        return null;
    }
    return key;
}

WebInspector.IndexedDBModel.keyRangeFromIDBKeyRange = function(idbKeyRange)
{
    var IDBKeyRange = window.IDBKeyRange || window.webkitIDBKeyRange;
    if (typeof(idbKeyRange) === "undefined" || idbKeyRange === null)
        return null;

    var keyRange = {};
    keyRange.lower = WebInspector.IndexedDBModel.keyFromIDBKey(idbKeyRange.lower);
    keyRange.upper = WebInspector.IndexedDBModel.keyFromIDBKey(idbKeyRange.upper);
    keyRange.lowerOpen = idbKeyRange.lowerOpen;
    keyRange.upperOpen = idbKeyRange.upperOpen;
    return keyRange;
}

/**
 * @param {IndexedDBAgent.KeyPath} keyPath
 */
WebInspector.IndexedDBModel.idbKeyPathFromKeyPath = function(keyPath)
{
    var idbKeyPath;
    switch (keyPath.type) {
    case WebInspector.IndexedDBModel.KeyPathTypes.NullType:
        idbKeyPath = null;
        break;
    case WebInspector.IndexedDBModel.KeyPathTypes.StringType:
        idbKeyPath = keyPath.string;
        break;
    case WebInspector.IndexedDBModel.KeyPathTypes.ArrayType:
        idbKeyPath = keyPath.array;
        break;
    }
    return idbKeyPath;
}

WebInspector.IndexedDBModel.keyPathStringFromIDBKeyPath = function(idbKeyPath)
{
    if (typeof idbKeyPath === "string")
        return "\"" + idbKeyPath + "\"";
    if (idbKeyPath instanceof Array)
        return "[\"" + idbKeyPath.join("\", \"") + "\"]";
    return null;
}

WebInspector.IndexedDBModel.EventTypes = {
    DatabaseAdded: "DatabaseAdded",
    DatabaseRemoved: "DatabaseRemoved",
    DatabaseLoaded: "DatabaseLoaded"
}

WebInspector.IndexedDBModel.prototype = {
    refreshDatabaseNames: function()
    {
        this._reset();
        if (WebInspector.resourceTreeModel.mainFrame)
            this._framesNavigatedRecursively(WebInspector.resourceTreeModel.mainFrame);
    },

    /**
     * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
     */
    refreshDatabase: function(databaseId)
    {
        this._loadDatabase(databaseId);
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
        var resourceTreeFrame = /** @type {WebInspector.ResourceTreeFrame} */ (event.data);
        this._processFrameNavigated(resourceTreeFrame);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _frameDetached: function(event)
    {
        var resourceTreeFrame = /** @type {WebInspector.ResourceTreeFrame} */ (event.data);
        this._originRemovedFromFrame(resourceTreeFrame.id);
    },

    _reset: function()
    {
        for (var frameId in this._frames)
            this._originRemovedFromFrame(frameId);
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
        for (var i = 0; i < this._databaseNamesBySecurityOrigin[securityOrigin].length; ++i)
            this._databaseRemoved(securityOrigin, this._databaseNamesBySecurityOrigin[securityOrigin][i]);
        delete this._databaseNamesBySecurityOrigin[securityOrigin];
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
        var databaseId = new WebInspector.IndexedDBModel.DatabaseId(securityOrigin, databaseName);
        this.dispatchEventToListeners(WebInspector.IndexedDBModel.EventTypes.DatabaseAdded, databaseId);
    },

    /**
     * @param {string} securityOrigin
     * @param {string} databaseName
     */
    _databaseRemoved: function(securityOrigin, databaseName)
    {
        var databaseId = new WebInspector.IndexedDBModel.DatabaseId(securityOrigin, databaseName);
        this.dispatchEventToListeners(WebInspector.IndexedDBModel.EventTypes.DatabaseRemoved, databaseId);
    },

    /**
     * @param {string} frameId
     */
    _loadDatabaseNamesForFrame: function(frameId)
    {
        /**
         * @param {?Protocol.Error} error
         * @param {IndexedDBAgent.SecurityOriginWithDatabaseNames} securityOriginWithDatabaseNames
         */
        function callback(error, securityOriginWithDatabaseNames)
        {
            if (error) {
                console.error("IndexedDBAgent error: " + error);
                return;
            }

            var databaseNames = securityOriginWithDatabaseNames.databaseNames;
            var oldSecurityOrigin = this._frames[frameId] ? this._frames[frameId].securityOrigin : null;
            if (!oldSecurityOrigin || oldSecurityOrigin !== securityOriginWithDatabaseNames.securityOrigin) {
                this._originRemovedFromFrame(frameId);
                this._originAddedToFrame(frameId, securityOriginWithDatabaseNames.securityOrigin);
            }
            this._updateOriginDatabaseNames(securityOriginWithDatabaseNames.securityOrigin, securityOriginWithDatabaseNames.databaseNames);
        }

        IndexedDBAgent.requestDatabaseNamesForFrame(frameId, callback.bind(this));
    },

    /**
     * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
     * @return {string|null}
     */
    _assertFrameId: function(databaseId)
    {
        var frameIds = this._frameIdsBySecurityOrigin[databaseId.securityOrigin];
        if (!frameIds || !frameIds.length)
            return null;

        return frameIds[0];
    },

    /**
     * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
     */
    _loadDatabase: function(databaseId)
    {
        var frameId = this._assertFrameId(databaseId);
        if (!frameId)
            return;

        /**
         * @param {?Protocol.Error} error
         * @param {IndexedDBAgent.DatabaseWithObjectStores} databaseWithObjectStores
         */
        function callback(error, databaseWithObjectStores)
        {
            if (error) {
                console.error("IndexedDBAgent error: " + error);
                return;
            }

            if (!this._frames[frameId])
                return;

            var databaseModel = new WebInspector.IndexedDBModel.Database(databaseId, databaseWithObjectStores.version, databaseWithObjectStores.intVersion);
            this._databases.put(databaseId, databaseModel); 
            for (var i = 0; i < databaseWithObjectStores.objectStores.length; ++i) {
                var objectStore = databaseWithObjectStores.objectStores[i];
                var objectStoreIDBKeyPath = WebInspector.IndexedDBModel.idbKeyPathFromKeyPath(objectStore.keyPath);
                var objectStoreModel = new WebInspector.IndexedDBModel.ObjectStore(objectStore.name, objectStoreIDBKeyPath, objectStore.autoIncrement);
                for (var j = 0; j < objectStore.indexes.length; ++j) {
                     var index = objectStore.indexes[j];
                     var indexIDBKeyPath = WebInspector.IndexedDBModel.idbKeyPathFromKeyPath(index.keyPath);
                     var indexModel = new WebInspector.IndexedDBModel.Index(index.name, indexIDBKeyPath, index.unique, index.multiEntry);
                     objectStoreModel.indexes[indexModel.name] = indexModel;
                }
                databaseModel.objectStores[objectStoreModel.name] = objectStoreModel;
            }

            this.dispatchEventToListeners(WebInspector.IndexedDBModel.EventTypes.DatabaseLoaded, databaseModel);
        }

        IndexedDBAgent.requestDatabase(frameId, databaseId.name, callback.bind(this));
    },

    /**
     * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
     * @param {string} objectStoreName
     * @param {webkitIDBKeyRange} idbKeyRange
     * @param {number} skipCount
     * @param {number} pageSize
     * @param {function(Array.<WebInspector.IndexedDBModel.Entry>, boolean)} callback
     */
    loadObjectStoreData: function(databaseId, objectStoreName, idbKeyRange, skipCount, pageSize, callback)
    {
        this._requestData(databaseId, databaseId.name, objectStoreName, "", idbKeyRange, skipCount, pageSize, callback);
    },

    /**
     * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
     * @param {string} objectStoreName
     * @param {string} indexName
     * @param {webkitIDBKeyRange} idbKeyRange
     * @param {number} skipCount
     * @param {number} pageSize
     * @param {function(Array.<WebInspector.IndexedDBModel.Entry>, boolean)} callback
     */
    loadIndexData: function(databaseId, objectStoreName, indexName, idbKeyRange, skipCount, pageSize, callback)
    {
        this._requestData(databaseId, databaseId.name, objectStoreName, indexName, idbKeyRange, skipCount, pageSize, callback);
    },

    /**
     * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
     * @param {string} databaseName
     * @param {string} objectStoreName
     * @param {string} indexName
     * @param {webkitIDBKeyRange} idbKeyRange
     * @param {number} skipCount
     * @param {number} pageSize
     * @param {function(Array.<IndexedDBAgent.DataEntry>, boolean)} callback
     */
    _requestData: function(databaseId, databaseName, objectStoreName, indexName, idbKeyRange, skipCount, pageSize, callback)
    {
        var frameId = this._assertFrameId(databaseId);
        if (!frameId)
            return;

        /**
         * @param {?Protocol.Error} error
         * @param {Array.<IndexedDBAgent.DataEntry>} dataEntries
         * @param {boolean} hasMore
         */
        function innerCallback(error, dataEntries, hasMore)
        {
            if (error) {
                console.error("IndexedDBAgent error: " + error);
                return;
            }
            
            if (!this._frames[frameId])
                return;

            var entries = [];
            for (var i = 0; i < dataEntries.length; ++i) {
                var key = WebInspector.RemoteObject.fromPayload(dataEntries[i].key);
                var primaryKey = WebInspector.RemoteObject.fromPayload(dataEntries[i].primaryKey);
                var value = WebInspector.RemoteObject.fromPayload(dataEntries[i].value);
                entries.push(new WebInspector.IndexedDBModel.Entry(key, primaryKey, value));
            }
            callback(entries, hasMore);
        }

        var keyRange = WebInspector.IndexedDBModel.keyRangeFromIDBKeyRange(idbKeyRange);
        IndexedDBAgent.requestData(frameId, databaseName, objectStoreName, indexName, skipCount, pageSize, keyRange ? keyRange : undefined, innerCallback.bind(this));
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @param {WebInspector.RemoteObject} key
 * @param {WebInspector.RemoteObject} primaryKey
 * @param {WebInspector.RemoteObject} value
 */
WebInspector.IndexedDBModel.Entry = function(key, primaryKey, value)
{
    this.key = key;
    this.primaryKey = primaryKey;
    this.value = value;
}

/**
 * @constructor
 * @param {string} frameId
 * @param {string} securityOrigin
 */
WebInspector.IndexedDBModel.Frame = function(frameId, securityOrigin)
{
    this.frameId = frameId;
    this.securityOrigin = securityOrigin;
    this.databaseNames = {};
}

/**
 * @constructor
 * @param {string} securityOrigin
 * @param {string} name
 */
WebInspector.IndexedDBModel.DatabaseId = function(securityOrigin, name)
{
    this.securityOrigin = securityOrigin;
    this.name = name;
}

WebInspector.IndexedDBModel.DatabaseId.prototype = {
    /**
     * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
     */
    equals: function(databaseId)
    {
        return this.name === databaseId.name && this.securityOrigin === databaseId.securityOrigin;
    },
}
/**
 * @constructor
 * @param {WebInspector.IndexedDBModel.DatabaseId} databaseId
 * @param {string} version
 */
WebInspector.IndexedDBModel.Database = function(databaseId, version, intVersion)
{
    this.databaseId = databaseId;
    this.version = version;
    this.intVersion = intVersion;
    this.objectStores = {};
}

/**
 * @constructor
 * @param {string} name
 * @param {*} keyPath
 */
WebInspector.IndexedDBModel.ObjectStore = function(name, keyPath, autoIncrement)
{
    this.name = name;
    this.keyPath = keyPath;
    this.autoIncrement = autoIncrement;
    this.indexes = {};
}

WebInspector.IndexedDBModel.ObjectStore.prototype = {
    /**
     * @type {string}
     */
    get keyPathString()
    {
        return WebInspector.IndexedDBModel.keyPathStringFromIDBKeyPath(this.keyPath);
    }
}

/**
 * @constructor
 * @param {string} name
 * @param {*} keyPath
 */
WebInspector.IndexedDBModel.Index = function(name, keyPath, unique, multiEntry)
{
    this.name = name;
    this.keyPath = keyPath;
    this.unique = unique;
    this.multiEntry = multiEntry;
}

WebInspector.IndexedDBModel.Index.prototype = {
    /**
     * @type {string}
     */
    get keyPathString()
    {
        return WebInspector.IndexedDBModel.keyPathStringFromIDBKeyPath(this.keyPath);
    }
}
