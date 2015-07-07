/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.IndexedDatabaseObjectStore = function(name, keyPath, autoIncrement, indexes)
{
    WebInspector.Object.call(this);

    this._name = name;
    this._keyPath = keyPath;
    this._autoIncrement = autoIncrement || false;
    this._indexes = indexes || [];
    this._parentDatabase = null;

    for (var index of this._indexes)
        index.establishRelationship(this);
};

WebInspector.IndexedDatabaseObjectStore.TypeIdentifier = "indexed-database-object-store";
WebInspector.IndexedDatabaseObjectStore.NameCookieKey = "indexed-database-object-store-name";
WebInspector.IndexedDatabaseObjectStore.KeyPathCookieKey = "indexed-database-object-store-key-path";

WebInspector.IndexedDatabaseObjectStore.prototype = {
    constructor: WebInspector.IndexedDatabaseObjectStore,
    __proto__: WebInspector.Object.prototype,

    // Public

    get name()
    {
        return this._name;
    },

    get keyPath()
    {
        return this._keyPath;
    },

    get autoIncrement()
    {
        return this._autoIncrement;
    },

    get parentDatabase()
    {
        return this._parentDatabase;
    },

    get indexes()
    {
        return this._indexes;
    },

    saveIdentityToCookie: function(cookie)
    {
        cookie[WebInspector.IndexedDatabaseObjectStore.NameCookieKey] = this._name;
        cookie[WebInspector.IndexedDatabaseObjectStore.KeyPathCookieKey] = this._keyPath;
    },

    // Protected

    establishRelationship: function(parentDatabase)
    {
        this._parentDatabase = parentDatabase || null;
    }
};
