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

#ifndef IDBIndexWriter_h
#define IDBIndexWriter_h

#include "IDBBackingStoreInterface.h"
#include "IDBDatabaseBackendInterface.h"
#include "IDBMetadata.h"
#include <wtf/RefCounted.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

typedef Vector<RefPtr<IDBKey>> IndexKeys;

class IDBIndexWriter : public RefCounted<IDBIndexWriter> {
public:
    static PassRefPtr<IDBIndexWriter> create(const IDBIndexMetadata& indexMetadata, const IndexKeys& indexKeys)
    {
        return adoptRef(new IDBIndexWriter(indexMetadata, indexKeys));
    }

    bool verifyIndexKeys(IDBBackingStoreInterface&, IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, bool& canAddKeys, const IDBKey* primaryKey = 0, String* errorMessage = 0) const WARN_UNUSED_RETURN;

    void writeIndexKeys(const IDBRecordIdentifier*, IDBBackingStoreInterface&, IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId) const;

private:
    IDBIndexWriter(const IDBIndexMetadata&, const IndexKeys&);

    bool addingKeyAllowed(IDBBackingStoreInterface&, IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey* indexKey, const IDBKey* primaryKey, bool& allowed) const WARN_UNUSED_RETURN;

    const IDBIndexMetadata m_indexMetadata;
    IndexKeys m_indexKeys;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBIndexWriter_h
