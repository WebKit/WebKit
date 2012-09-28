/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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

#include "config.h"
#include "WebIDBMetadata.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBMetadata.h"
#include "WebIDBKeyPath.h"
#include "platform/WebString.h"
#include <public/WebVector.h>

using namespace WebCore;

namespace WebKit {

WebIDBMetadata::WebIDBMetadata(const WebCore::IDBDatabaseMetadata& metadata)
{
    name = metadata.name;
    version = metadata.version;
    intVersion = metadata.intVersion;
    objectStores = WebVector<ObjectStore>(static_cast<size_t>(metadata.objectStores.size()));

    size_t i = 0;
    for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator storeIterator = metadata.objectStores.begin(); storeIterator != metadata.objectStores.end(); ++storeIterator) {
        const IDBObjectStoreMetadata& objectStore = storeIterator->second;
        ObjectStore webObjectStore;
        webObjectStore.name = objectStore.name;
        webObjectStore.keyPath = objectStore.keyPath;
        webObjectStore.autoIncrement = objectStore.autoIncrement;
        webObjectStore.indexes = WebVector<Index>(static_cast<size_t>(objectStore.indexes.size()));

        size_t j = 0;
        for (IDBObjectStoreMetadata::IndexMap::const_iterator indexIterator = objectStore.indexes.begin(); indexIterator != objectStore.indexes.end(); ++indexIterator) {
            const IDBIndexMetadata& index = indexIterator->second;
            Index webIndex;
            webIndex.name = index.name;
            webIndex.keyPath = index.keyPath;
            webIndex.unique = index.unique;
            webIndex.multiEntry = index.multiEntry;
            webObjectStore.indexes[j++] = webIndex;
        }
        objectStores[i++] = webObjectStore;
    }
}

WebIDBMetadata::operator IDBDatabaseMetadata() const
{
    IDBDatabaseMetadata db(name, version, intVersion);
    for (size_t i = 0; i < objectStores.size(); ++i) {
        const ObjectStore webObjectStore = objectStores[i];
        IDBObjectStoreMetadata objectStore(webObjectStore.name, webObjectStore.keyPath, webObjectStore.autoIncrement);

        for (size_t j = 0; j < webObjectStore.indexes.size(); ++j) {
            const Index webIndex = webObjectStore.indexes[j];
            IDBIndexMetadata index(webIndex.name, webIndex.keyPath, webIndex.unique, webIndex.multiEntry);
            objectStore.indexes.set(index.name, index);
        }
        db.objectStores.set(objectStore.name, objectStore);
    }
    return db;
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
