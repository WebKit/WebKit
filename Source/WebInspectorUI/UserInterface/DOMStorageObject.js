/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DOMStorageObject = function(id, host, isLocalStorage)
{
    this._id = id;
    this._host = host;
    this._isLocalStorage = isLocalStorage;
    this._entries = new Map;
};

WebInspector.DOMStorageObject.TypeIdentifier = "dom-storage";
WebInspector.DOMStorageObject.HostCookieKey = "dom-storage-object-host";
WebInspector.DOMStorageObject.LocalStorageCookieKey = "dom-storage-object-local-storage";

WebInspector.DOMStorageObject.Event = {
    ItemsCleared: "dom-storage-object-items-cleared",
    ItemAdded: "dom-storage-object-item-added",
    ItemRemoved: "dom-storage-object-item-removed",
    ItemUpdated: "dom-storage-object-updated",
};

WebInspector.DOMStorageObject.prototype = {
    constructor: WebInspector.DOMStorageObject,
    __proto__: WebInspector.Object.prototype,

    get id()
    {
        return this._id;
    },

    get host()
    {
        return this._host;
    },

    get entries()
    {
        return this._entries;
    },

    saveIdentityToCookie: function(cookie)
    {
        cookie[WebInspector.DOMStorageObject.HostCookieKey] = this.host;
        cookie[WebInspector.DOMStorageObject.LocalStorageCookieKey] = this.isLocalStorage();
    },

    isLocalStorage: function()
    {
        return this._isLocalStorage;
    },

    getEntries: function(callback)
    {
        // COMPATIBILITY (iOS 6): The getDOMStorageItems function was later renamed to getDOMStorageItems.
        if (DOMStorageAgent.getDOMStorageEntries)
            DOMStorageAgent.getDOMStorageEntries(this._id, callback);
        else
            DOMStorageAgent.getDOMStorageItems(this._id, callback);
    },

    removeItem: function(key)
    {
        DOMStorageAgent.removeDOMStorageItem(this._id, key);
    },

    setItem: function(key, value)
    {
        DOMStorageAgent.setDOMStorageItem(this._id, key, value);
    },

    itemsCleared: function()
    {
        this._entries.clear();
        this.dispatchEventToListeners(WebInspector.DOMStorageObject.Event.ItemsCleared);
    },

    itemRemoved: function(key)
    {
        this._entries.delete(key);
        this.dispatchEventToListeners(WebInspector.DOMStorageObject.Event.ItemRemoved, {key: key});
    },

    itemAdded: function(key, value)
    {
        this._entries.set(key, value);
        this.dispatchEventToListeners(WebInspector.DOMStorageObject.Event.ItemAdded, {key: key, value: value});
    },

    itemUpdated: function(key, oldValue, value)
    {
        this._entries.set(key, value);
        var data = {key: key, oldValue: oldValue, value: value};
        this.dispatchEventToListeners(WebInspector.DOMStorageObject.Event.ItemUpdated, data);
    }
};
