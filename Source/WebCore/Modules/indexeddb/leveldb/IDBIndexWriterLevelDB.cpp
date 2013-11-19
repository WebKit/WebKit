/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *               2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IDBIndexWriterLevelDB.h"

#include "IDBKey.h"
#include <wtf/text/CString.h>

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

namespace WebCore {

IDBIndexWriterLevelDB::IDBIndexWriterLevelDB(const IDBIndexMetadata& metadata, const IndexKeys& keys)
    : m_indexMetadata(metadata)
    , m_indexKeys(keys)
{
}

void IDBIndexWriterLevelDB::writeIndexKeys(const IDBRecordIdentifier* recordIdentifier, IDBBackingStoreLevelDB& backingStore, IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId) const
{
    ASSERT(recordIdentifier);
    int64_t indexId = m_indexMetadata.id;
    for (size_t i = 0; i < m_indexKeys.size(); ++i) {
        bool ok = backingStore.putIndexDataForRecord(transaction, databaseId, objectStoreId, indexId, *(m_indexKeys)[i].get(), recordIdentifier);
        // This should have already been verified as a valid write during verifyIndexKeys.
        ASSERT_UNUSED(ok, ok);
    }
}

bool IDBIndexWriterLevelDB::verifyIndexKeys(IDBBackingStoreLevelDB& backingStore, IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, bool& canAddKeys, const IDBKey* primaryKey, String* errorMessage) const
{
    canAddKeys = false;
    for (size_t i = 0; i < m_indexKeys.size(); ++i) {
        bool ok = addingKeyAllowed(backingStore, transaction, databaseId, objectStoreId, indexId, (m_indexKeys)[i].get(), primaryKey, canAddKeys);
        if (!ok)
            return false;
        if (!canAddKeys) {
            if (errorMessage)
                *errorMessage = String::format("Unable to add key to index '%s': at least one key does not satisfy the uniqueness requirements.", m_indexMetadata.name.utf8().data());
            return true;
        }
    }
    canAddKeys = true;
    return true;
}

bool IDBIndexWriterLevelDB::addingKeyAllowed(IDBBackingStoreLevelDB& backingStore, IDBBackingStoreTransactionLevelDB& transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey* indexKey, const IDBKey* primaryKey, bool& allowed) const
{
    allowed = false;
    if (!m_indexMetadata.unique) {
        allowed = true;
        return true;
    }

    RefPtr<IDBKey> foundPrimaryKey;
    bool found = false;
    bool ok = backingStore.keyExistsInIndex(transaction, databaseId, objectStoreId, indexId, *indexKey, foundPrimaryKey, found);
    if (!ok)
        return false;
    if (!found || (primaryKey && foundPrimaryKey->isEqual(primaryKey)))
        allowed = true;
    return true;
}


} // namespace WebCore

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
