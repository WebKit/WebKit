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

#ifndef IDBRequestData_h
#define IDBRequestData_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseIdentifier.h"
#include "IDBResourceIdentifier.h"

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
class IDBOpenDBRequest;
class IDBTransaction;
class TransactionOperation;
}

class IDBRequestData {
public:
    IDBRequestData(const IDBClient::IDBConnectionToServer&, const IDBClient::IDBOpenDBRequest&);
    explicit IDBRequestData(IDBClient::TransactionOperation&);
    IDBRequestData(const IDBRequestData&);

    IDBResourceIdentifier requestIdentifier() const;
    IDBResourceIdentifier transactionIdentifier() const;

    const IDBDatabaseIdentifier& databaseIdentifier() const { return m_databaseIdentifier; }
    uint64_t requestedVersion() const;

    IDBRequestData isolatedCopy();

private:
    std::unique_ptr<IDBResourceIdentifier> m_requestIdentifier;
    std::unique_ptr<IDBResourceIdentifier> m_transactionIdentifier;

    IDBDatabaseIdentifier m_databaseIdentifier;
    uint64_t m_requestedVersion { 0 };
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBRequestData_h
