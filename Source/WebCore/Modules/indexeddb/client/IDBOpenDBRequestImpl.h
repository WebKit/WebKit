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

#ifndef IDBOpenDBRequestImpl_h
#define IDBOpenDBRequestImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "DOMError.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBRequestImpl.h"

namespace WebCore {

class IDBDatabaseIdentifier;

namespace IDBClient {

class IDBConnectionToServer;

class IDBOpenDBRequest : public IDBRequest {
public:
    static Ref<IDBOpenDBRequest> createDeleteRequest(IDBConnectionToServer&, ScriptExecutionContext&, const IDBDatabaseIdentifier&);
    static Ref<IDBOpenDBRequest> createOpenRequest(IDBConnectionToServer&, ScriptExecutionContext&, const IDBDatabaseIdentifier&, uint64_t version);

    ~IDBOpenDBRequest() final;
    
    const IDBDatabaseIdentifier& databaseIdentifier() const { return m_databaseIdentifier; }
    uint64_t version() const { return m_version; }

    void requestCompleted(const IDBResultData&);
    void requestBlocked(uint64_t oldVersion, uint64_t newVersion);

    void versionChangeTransactionDidFinish();
    void fireSuccessAfterVersionChangeCommit();
    void fireErrorAfterVersionChangeCompletion();

    bool dispatchEvent(Event&) final;

private:
    IDBOpenDBRequest(IDBConnectionToServer&, ScriptExecutionContext&, const IDBDatabaseIdentifier&, uint64_t version, IndexedDB::RequestType);

    void onError(const IDBResultData&);
    void onSuccess(const IDBResultData&);
    void onUpgradeNeeded(const IDBResultData&);
    void onDeleteDatabaseSuccess(const IDBResultData&);

    bool isOpenDBRequest() const override { return true; }

    IDBDatabaseIdentifier m_databaseIdentifier;
    uint64_t m_version { 0 };
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBOpenDBRequestImpl_h
