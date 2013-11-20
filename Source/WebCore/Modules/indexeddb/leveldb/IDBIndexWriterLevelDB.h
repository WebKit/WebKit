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

#ifndef IDBIndexWriterLevelDB_h
#define IDBIndexWriterLevelDB_h

#include "IDBBackingStoreLevelDB.h"
#include "IDBDatabaseBackend.h"
#include "IDBDatabaseMetadata.h"
#include <wtf/RefCounted.h>

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

namespace WebCore {

typedef Vector<RefPtr<IDBKey>> IndexKeys;

class IDBIndexWriterLevelDB : public RefCounted<IDBIndexWriterLevelDB> {
public:
    static PassRefPtr<IDBIndexWriterLevelDB> create(const IDBIndexMetadata& indexMetadata, const IndexKeys& indexKeys)
    {
        return adoptRef(new IDBIndexWriterLevelDB(indexMetadata, indexKeys));
    }

    bool verifyIndexKeys(IDBBackingStoreLevelDB&, IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, bool& canAddKeys, const IDBKey* primaryKey = 0, String* errorMessage = 0) const WARN_UNUSED_RETURN;

    void writeIndexKeys(const IDBRecordIdentifier*, IDBBackingStoreLevelDB&, IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId) const;

private:
    IDBIndexWriterLevelDB(const IDBIndexMetadata&, const IndexKeys&);

    bool addingKeyAllowed(IDBBackingStoreLevelDB&, IDBBackingStoreTransactionLevelDB&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey* indexKey, const IDBKey* primaryKey, bool& allowed) const WARN_UNUSED_RETURN;

    const IDBIndexMetadata m_indexMetadata;
    IndexKeys m_indexKeys;
};

} // namespace WebCore

#endif // #if USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBIndexWriterLevelDB_h
