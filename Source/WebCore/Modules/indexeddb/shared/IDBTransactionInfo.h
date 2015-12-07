/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef IDBTransactionInfo_h
#define IDBTransactionInfo_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseInfo.h"
#include "IDBResourceIdentifier.h"
#include "IndexedDB.h"
#include <wtf/Vector.h>

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

namespace IDBServer {
class IDBConnectionToClient;
}

class IDBTransactionInfo {
public:
    static IDBTransactionInfo clientTransaction(const IDBClient::IDBConnectionToServer&, const Vector<String>& objectStores, IndexedDB::TransactionMode);
    static IDBTransactionInfo versionChange(const IDBServer::IDBConnectionToClient&, const IDBDatabaseInfo& originalDatabaseInfo, uint64_t newVersion);

    IDBTransactionInfo(const IDBTransactionInfo&);

    IDBTransactionInfo isolatedCopy() const;

    IDBResourceIdentifier identifier() const { return m_identifier; }

    IndexedDB::TransactionMode mode() const { return m_mode; }
    uint64_t newVersion() const { return m_newVersion; }

    const Vector<String>& objectStores() const { return m_objectStores; }

    IDBDatabaseInfo* originalDatabaseInfo() const { return m_originalDatabaseInfo.get(); }

private:
    IDBTransactionInfo(const IDBResourceIdentifier&);

    IDBResourceIdentifier m_identifier;

    IndexedDB::TransactionMode m_mode { IndexedDB::TransactionMode::ReadOnly };
    uint64_t m_newVersion { 0 };
    Vector<String> m_objectStores;
    std::unique_ptr<IDBDatabaseInfo> m_originalDatabaseInfo;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBTransactionInfo_h
