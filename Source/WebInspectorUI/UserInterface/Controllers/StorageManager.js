/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

WebInspector.StorageManager = class StorageManager extends WebInspector.Object
{
    constructor()
    {
        super();

        if (window.DOMStorageAgent)
            DOMStorageAgent.enable();
        if (window.DatabaseAgent)
            DatabaseAgent.enable();
        if (window.IndexedDBAgent)
            IndexedDBAgent.enable();

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.SecurityOriginDidChange, this._securityOriginDidChange, this);

        this.initialize();
    }

    // Public

    initialize()
    {
        this._domStorageObjects = [];
        this._databaseObjects = [];
        this._indexedDatabases = [];
        this._cookieStorageObjects = {};
    }

    get domStorageObjects()
    {
        return this._domStorageObjects;
    }

    get databases()
    {
        return this._databaseObjects;
    }

    get indexedDatabases()
    {
        return this._indexedDatabases;
    }

    get cookieStorageObjects()
    {
        var cookieStorageObjects = [];
        for (var host in this._cookieStorageObjects)
            cookieStorageObjects.push(this._cookieStorageObjects[host]);
        return cookieStorageObjects;
    }

    domStorageWasAdded(id, host, isLocalStorage)
    {
        var domStorage = new WebInspector.DOMStorageObject(id, host, isLocalStorage);

        this._domStorageObjects.push(domStorage);
        this.dispatchEventToListeners(WebInspector.StorageManager.Event.DOMStorageObjectWasAdded, {domStorage});
    }

    databaseWasAdded(id, host, name, version)
    {
        var database = new WebInspector.DatabaseObject(id, host, name, version);

        this._databaseObjects.push(database);
        this.dispatchEventToListeners(WebInspector.StorageManager.Event.DatabaseWasAdded, {database});
    }

    itemsCleared(storageId)
    {
        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemsCleared(storageId);
    }

    itemRemoved(storageId, key)
    {
        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemRemoved(key);
    }

    itemAdded(storageId, key, value)
    {
        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemAdded(key, value);
    }

    itemUpdated(storageId, key, oldValue, value)
    {
        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemUpdated(key, oldValue, value);
    }

    inspectDatabase(id)
    {
        var database = this._databaseForIdentifier(id);
        console.assert(database);
        if (!database)
            return;
        this.dispatchEventToListeners(WebInspector.StorageManager.Event.DatabaseWasInspected, {database});
    }

    inspectDOMStorage(id)
    {
        var domStorage = this._domStorageForIdentifier(id);
        console.assert(domStorage);
        if (!domStorage)
            return;
        this.dispatchEventToListeners(WebInspector.StorageManager.Event.DOMStorageObjectWasInspected, {domStorage});
    }

    requestIndexedDatabaseData(objectStore, objectStoreIndex, startEntryIndex, maximumEntryCount, callback)
    {
        console.assert(window.IndexedDBAgent);
        console.assert(objectStore);
        console.assert(callback);

        function processData(error, entryPayloads, moreAvailable)
        {
            if (error) {
                callback(null, false);
                return;
            }

            var entries = [];

            for (var entryPayload of entryPayloads) {
                var entry = {};
                entry.primaryKey = WebInspector.RemoteObject.fromPayload(entryPayload.primaryKey);
                entry.key = WebInspector.RemoteObject.fromPayload(entryPayload.key);
                entry.value = WebInspector.RemoteObject.fromPayload(entryPayload.value);
                entries.push(entry);
            }

            callback(entries, moreAvailable);
        }

        var requestArguments = {
            securityOrigin: objectStore.parentDatabase.securityOrigin,
            databaseName: objectStore.parentDatabase.name,
            objectStoreName: objectStore.name,
            indexName: objectStoreIndex && objectStoreIndex.name || "",
            skipCount: startEntryIndex || 0,
            pageSize: maximumEntryCount || 100
        };

        IndexedDBAgent.requestData.invoke(requestArguments, processData);
    }

    clearObjectStore(objectStore)
    {
        let securityOrigin = objectStore.parentDatabase.securityOrigin;
        let databaseName = objectStore.parentDatabase.name;
        let objectStoreName = objectStore.name;

        IndexedDBAgent.clearObjectStore(securityOrigin, databaseName, objectStoreName);
    }

    // Private

    _domStorageForIdentifier(id)
    {
        for (var storageObject of this._domStorageObjects) {
            // The id is an object, so we need to compare the properties using Object.shallowEqual.
            if (Object.shallowEqual(storageObject.id, id))
                return storageObject;
        }

        return null;
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WebInspector.Frame);

        if (event.target.isMainFrame()) {
            // If we are dealing with the main frame, we want to clear our list of objects, because we are navigating to a new page.
            this.initialize();
            this.dispatchEventToListeners(WebInspector.StorageManager.Event.Cleared);

            this._addDOMStorageIfNeeded(event.target);
            this._addIndexedDBDatabasesIfNeeded(event.target);
        }

        // Add the host of the frame that changed the main resource to the list of hosts there could be cookies for.
        var host = parseURL(event.target.url).host;
        if (!host)
            return;

        if (this._cookieStorageObjects[host])
            return;

        this._cookieStorageObjects[host] = new WebInspector.CookieStorageObject(host);
        this.dispatchEventToListeners(WebInspector.StorageManager.Event.CookieStorageObjectWasAdded, {cookieStorage: this._cookieStorageObjects[host]});
    }

    _addDOMStorageIfNeeded(frame)
    {
        if (!window.DOMStorageAgent)
            return;

        // Don't show storage if we don't have a security origin (about:blank).
        if (!frame.securityOrigin || frame.securityOrigin === "://")
            return;

        // FIXME: Consider passing the other parts of the origin along to domStorageWasAdded.

        var localStorageIdentifier = {securityOrigin: frame.securityOrigin, isLocalStorage: true};
        if (!this._domStorageForIdentifier(localStorageIdentifier))
            this.domStorageWasAdded(localStorageIdentifier, frame.mainResource.urlComponents.host, true);

        var sessionStorageIdentifier = {securityOrigin: frame.securityOrigin, isLocalStorage: false};
        if (!this._domStorageForIdentifier(sessionStorageIdentifier))
            this.domStorageWasAdded(sessionStorageIdentifier, frame.mainResource.urlComponents.host, false);
    }

    _addIndexedDBDatabasesIfNeeded(frame)
    {
        if (!window.IndexedDBAgent)
            return;

        var securityOrigin = frame.securityOrigin;

        // Don't show storage if we don't have a security origin (about:blank).
        if (!securityOrigin || securityOrigin === "://")
            return;

        function processDatabaseNames(error, names)
        {
            if (error || !names)
                return;

            for (var name of names)
                IndexedDBAgent.requestDatabase(securityOrigin, name, processDatabase.bind(this));
        }

        function processDatabase(error, databasePayload)
        {
            if (error || !databasePayload)
                return;

            var objectStores = databasePayload.objectStores.map(processObjectStore);
            var indexedDatabase = new WebInspector.IndexedDatabase(databasePayload.name, securityOrigin, databasePayload.version, objectStores);

            this._indexedDatabases.push(indexedDatabase);
            this.dispatchEventToListeners(WebInspector.StorageManager.Event.IndexedDatabaseWasAdded, {indexedDatabase});
        }

        function processKeyPath(keyPathPayload)
        {
            switch (keyPathPayload.type) {
            case IndexedDBAgent.KeyPathType.Null:
                return null;
            case IndexedDBAgent.KeyPathType.String:
                return keyPathPayload.string;
            case IndexedDBAgent.KeyPathType.Array:
                return keyPathPayload.array;
            default:
                console.error("Unknown KeyPath type:", keyPathPayload.type);
                return null;
            }
        }

        function processObjectStore(objectStorePayload)
        {
            var keyPath = processKeyPath(objectStorePayload.keyPath);
            var indexes = objectStorePayload.indexes.map(processObjectStoreIndex);
            return new WebInspector.IndexedDatabaseObjectStore(objectStorePayload.name, keyPath, objectStorePayload.autoIncrement, indexes);
        }

        function processObjectStoreIndex(objectStoreIndexPayload)
        {
            var keyPath = processKeyPath(objectStoreIndexPayload.keyPath);
            return new WebInspector.IndexedDatabaseObjectStoreIndex(objectStoreIndexPayload.name, keyPath, objectStoreIndexPayload.unique, objectStoreIndexPayload.multiEntry);
        }

        IndexedDBAgent.requestDatabaseNames(securityOrigin, processDatabaseNames.bind(this));
    }

    _securityOriginDidChange(event)
    {
        console.assert(event.target instanceof WebInspector.Frame);

        this._addDOMStorageIfNeeded(event.target);
        this._addIndexedDBDatabasesIfNeeded(event.target);
    }

    _databaseForIdentifier(id)
    {
        for (var i = 0; i < this._databaseObjects.length; ++i) {
            if (this._databaseObjects[i].id === id)
                return this._databaseObjects[i];
        }

        return null;
    }
};

WebInspector.StorageManager.Event = {
    CookieStorageObjectWasAdded: "storage-manager-cookie-storage-object-was-added",
    DOMStorageObjectWasAdded: "storage-manager-dom-storage-object-was-added",
    DOMStorageObjectWasInspected: "storage-dom-object-was-inspected",
    DatabaseWasAdded: "storage-manager-database-was-added",
    DatabaseWasInspected: "storage-object-was-inspected",
    IndexedDatabaseWasAdded: "storage-manager-indexed-database-was-added",
    Cleared: "storage-manager-cleared"
};
