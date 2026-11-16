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

WI.IndexedDBManager = class IndexedDBManager extends WI.Object
{
    constructor()
    {
        super();

        this._enabled = false;
        this._requestedSecurityOrigins = new Set;

        this._reset();
    }

    // Agent

    get domains() { return ["IndexedDB"]; }

    activateExtraDomain(domain)
    {
        // COMPATIBILITY (iOS 14.0): Inspector.activateExtraDomains was removed in favor of a declared debuggable type

        console.assert(domain === "IndexedDB");

        for (let target of WI.targets)
            this.initializeTarget(target);
    }

    // Target

    initializeTarget(target)
    {
        if (!this._enabled)
            return;

        if (target.hasDomain("IndexedDB"))
            target.IndexedDBAgent.enable();
    }

    // Public

    get indexedDatabases() { return this._indexedDatabases; }

    enable()
    {
        console.assert(!this._enabled);

        this._enabled = true;

        this._reset();

        for (let target of WI.targets)
            this.initializeTarget(target);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.SecurityOriginDidChange, this._securityOriginDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.PageExecutionContextChanged, this._framePageExecutionContextChanged, this);
    }

    disable()
    {
        console.assert(this._enabled);

        this._enabled = false;

        for (let target of WI.targets) {
            if (target.hasDomain("IndexedDB"))
                target.IndexedDBAgent.disable();
        }

        WI.Frame.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.removeEventListener(WI.Frame.Event.SecurityOriginDidChange, this._securityOriginDidChange, this);
        WI.Frame.removeEventListener(WI.Frame.Event.PageExecutionContextChanged, this._framePageExecutionContextChanged, this);

        this._reset();
    }

    requestIndexedDatabaseData(objectStore, objectStoreIndex, startEntryIndex, maximumEntryCount, callback)
    {
        console.assert(this._enabled);
        console.assert(InspectorBackend.hasDomain("IndexedDB"));
        console.assert(objectStore);
        console.assert(callback);

        let target = this._targetForSecurityOrigin(objectStore.parentDatabase.securityOrigin);
        if (!target) {
            callback([], false);
            return;
        }

        function processData(error, entryPayloads, moreAvailable)
        {
            if (error) {
                callback([], false);
                return;
            }

            var entries = [];

            for (var entryPayload of entryPayloads) {
                var entry = {};
                entry.primaryKey = WI.RemoteObject.fromPayload(entryPayload.primaryKey, target);
                entry.key = WI.RemoteObject.fromPayload(entryPayload.key, target);
                entry.value = WI.RemoteObject.fromPayload(entryPayload.value, target);
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

        target.IndexedDBAgent.requestData.invoke(requestArguments, processData);
    }

    clearObjectStore(objectStore)
    {
        console.assert(this._enabled);

        let securityOrigin = objectStore.parentDatabase.securityOrigin;
        let databaseName = objectStore.parentDatabase.name;
        let objectStoreName = objectStore.name;

        let target = this._targetForSecurityOrigin(securityOrigin);
        if (!target)
            return;

        return target.IndexedDBAgent.clearObjectStore(securityOrigin, databaseName, objectStoreName);
    }

    // Private

    _reset()
    {
        this._indexedDatabases = [];
        this._requestedSecurityOrigins.clear();
        this.dispatchEventToListeners(WI.IndexedDBManager.Event.Cleared);

        let mainFrame = WI.networkManager.mainFrame;
        if (mainFrame)
            this._addIndexedDBDatabasesIfNeeded(mainFrame);
    }

    _addIndexedDBDatabasesIfNeeded(frame)
    {
        if (!this._enabled)
            return;

        var securityOrigin = frame.securityOrigin;

        // Don't show storage if we don't have a security origin (about:blank).
        if (!securityOrigin || securityOrigin === "://")
            return;

        if (this._requestedSecurityOrigins.has(securityOrigin))
            return;

        let target = this._targetForSecurityOrigin(securityOrigin);
        if (!target || !target.hasDomain("IndexedDB"))
            return;

        this._requestedSecurityOrigins.add(securityOrigin);

        function processDatabaseNames(error, names)
        {
            if (error || !names)
                return;

            for (var name of names)
                target.IndexedDBAgent.requestDatabase(securityOrigin, name, processDatabase.bind(this));
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
            case InspectorBackend.Enum.IndexedDB.KeyPathType.Null:
                return null;
            case InspectorBackend.Enum.IndexedDB.KeyPathType.String:
                return keyPathPayload.string;
            case InspectorBackend.Enum.IndexedDB.KeyPathType.Array:
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

        target.IndexedDBAgent.requestDatabaseNames(securityOrigin, processDatabaseNames.bind(this));
    }

    _targetForSecurityOrigin(securityOrigin)
    {
        if (!WI.targets.some((target) => target instanceof WI.FrameTarget && target.hasDomain("IndexedDB")))
            return WI.assumingMainTarget();

        // WI.Frame prefers the main-world context from its own frame target over the page target's, and WebKitLegacy frame targets never attach theirs because they name frames differently from the Page domain.
        // With no such context yet, wait rather than ask the main target, which cannot reach a frame in another process.
        for (let frame of WI.networkManager.frames) {
            if (frame.securityOrigin !== securityOrigin)
                continue;

            let target = frame.pageExecutionContext?.target;
            if (target && !target.isProvisional && target.hasDomain("IndexedDB"))
                return target;
        }

        return null;
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (event.target.isMainFrame())
            this._reset();
    }

    _securityOriginDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        this._addIndexedDBDatabasesIfNeeded(event.target);
    }

    _framePageExecutionContextChanged(event)
    {
        this._addIndexedDBDatabasesIfNeeded(event.target);
    }
};

WI.IndexedDBManager.Event = {
    IndexedDatabaseWasAdded: "indexed-db-manager-indexed-database-was-added",
    Cleared: "indexed-db-manager-cleared",
};
