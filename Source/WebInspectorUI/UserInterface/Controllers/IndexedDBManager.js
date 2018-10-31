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

// FIXME: IndexedDBManager lacks advanced multi-target support. (IndexedDatabase per-target)

WI.IndexedDBManager = class IndexedDBManager extends WI.Object
{
    constructor()
    {
        super();

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.SecurityOriginDidChange, this._securityOriginDidChange, this);

        this.initialize();
    }

    // Target

    initializeTarget(target)
    {
        if (target.IndexedDBAgent)
            target.IndexedDBAgent.enable();
    }

    // Public

    initialize()
    {
        this._indexedDatabases = [];
    }

    get indexedDatabases()
    {
        return this._indexedDatabases;
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
                entry.primaryKey = WI.RemoteObject.fromPayload(entryPayload.primaryKey);
                entry.key = WI.RemoteObject.fromPayload(entryPayload.key);
                entry.value = WI.RemoteObject.fromPayload(entryPayload.value);
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

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (event.target.isMainFrame()) {
            // If we are dealing with the main frame, we want to clear our list of objects, because we are navigating to a new page.
            this.initialize();
            this.dispatchEventToListeners(WI.IndexedDBManager.Event.Cleared);

            this._addIndexedDBDatabasesIfNeeded(event.target);
        }
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
            var indexedDatabase = new WI.IndexedDatabase(databasePayload.name, securityOrigin, databasePayload.version, objectStores);

            this._indexedDatabases.push(indexedDatabase);
            this.dispatchEventToListeners(WI.IndexedDBManager.Event.IndexedDatabaseWasAdded, {indexedDatabase});
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
            return new WI.IndexedDatabaseObjectStore(objectStorePayload.name, keyPath, objectStorePayload.autoIncrement, indexes);
        }

        function processObjectStoreIndex(objectStoreIndexPayload)
        {
            var keyPath = processKeyPath(objectStoreIndexPayload.keyPath);
            return new WI.IndexedDatabaseObjectStoreIndex(objectStoreIndexPayload.name, keyPath, objectStoreIndexPayload.unique, objectStoreIndexPayload.multiEntry);
        }

        IndexedDBAgent.requestDatabaseNames(securityOrigin, processDatabaseNames.bind(this));
    }

    _securityOriginDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        this._addIndexedDBDatabasesIfNeeded(event.target);
    }
};

WI.IndexedDBManager.Event = {
    IndexedDatabaseWasAdded: "indexed-db-manager-indexed-database-was-added",
    Cleared: "indexed-db-manager-cleared",
};
