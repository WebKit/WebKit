/*
 * Copyright (C) 2008 Nokia Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 */
WebInspector.DOMStorage = function(id, domain, isLocalStorage)
{
    this._id = id;
    this._domain = domain;
    this._isLocalStorage = isLocalStorage;
}

WebInspector.DOMStorage.prototype = {
    /** @return {string} */
    get id()
    {
        return this._id;
    },

    /** @return {string} */
    get domain()
    {
        return this._domain;
    },

    /** @return {boolean} */
    get isLocalStorage()
    {
        return this._isLocalStorage;
    },

    /**
     * @param {function(?Protocol.Error, Array.<DOMStorageAgent.Entry>):void=} callback
     */
    getEntries: function(callback)
    {
        DOMStorageAgent.getDOMStorageEntries(this._id, callback);
    },

    /**
     * @param {string} key
     * @param {string} value
     * @param {function(?Protocol.Error, boolean):void=} callback
     */
    setItem: function(key, value, callback)
    {
        DOMStorageAgent.setDOMStorageItem(this._id, key, value, callback);
    },

    /**
     * @param {string} key
     * @param {function(?Protocol.Error, boolean):void=} callback
     */
    removeItem: function(key, callback)
    {
        DOMStorageAgent.removeDOMStorageItem(this._id, key, callback);
    }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.DOMStorageModel = function()
{
    this._storages = {};
    InspectorBackend.registerDOMStorageDispatcher(new WebInspector.DOMStorageDispatcher(this));
    DOMStorageAgent.enable();
}

WebInspector.DOMStorageModel.Events = {
    DOMStorageAdded: "DOMStorageAdded",
    DOMStorageUpdated: "DOMStorageUpdated"
}

WebInspector.DOMStorageModel.prototype = {
    /**
     * @param {WebInspector.DOMStorage} domStorage
     */
    _addDOMStorage: function(domStorage)
    {
        this._storages[domStorage.id] = domStorage;
        this.dispatchEventToListeners(WebInspector.DOMStorageModel.Events.DOMStorageAdded, domStorage);
    },

    /**
     * @param {DOMStorageAgent.StorageId} storageId
     */
    _domStorageUpdated: function(storageId)
    {
        this.dispatchEventToListeners(WebInspector.DOMStorageModel.Events.DOMStorageUpdated, this._storages[storageId]);
    },

    /**
     * @param {DOMStorageAgent.StorageId} storageId
     * @return {WebInspector.DOMStorage}
     */
    storageForId: function(storageId)
    {
        return this._storages[storageId];
    },

    /**
     * @return {Array.<WebInspector.DOMStorage>}
     */
    storages: function()
    {
        var result = [];
        for (var storageId in this._storages)
            result.push(this._storages[storageId]);
        return result;
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @implements {DOMStorageAgent.Dispatcher}
 * @param {WebInspector.DOMStorageModel} model
 */
WebInspector.DOMStorageDispatcher = function(model)
{
    this._model = model;
}

WebInspector.DOMStorageDispatcher.prototype = {

    /**
     * @param {DOMStorageAgent.Entry} payload
     */
    addDOMStorage: function(payload)
    {
        this._model._addDOMStorage(new WebInspector.DOMStorage(
            payload.id,
            payload.origin,
            payload.isLocalStorage));
    },

    /**
     * @param {string} storageId
     */
    domStorageUpdated: function(storageId)
    {
        this._model._domStorageUpdated(storageId);
    }
}

/**
 * @type {WebInspector.DOMStorageModel}
 */
WebInspector.domStorageModel = null;
