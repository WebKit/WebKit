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

#ifndef IDBConnectionToClient_h
#define IDBConnectionToClient_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClientDelegate.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBError;
class IDBResourceIdentifier;
class IDBResultData;

namespace IDBServer {

class UniqueIDBDatabaseConnection;

class IDBConnectionToClient : public RefCounted<IDBConnectionToClient> {
public:
    static Ref<IDBConnectionToClient> create(IDBConnectionToClientDelegate&);
    
    uint64_t identifier() const;

    void didDeleteDatabase(const IDBResultData&);
    void didOpenDatabase(const IDBResultData&);
    void didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);
    void didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);
    void didCreateObjectStore(const IDBResultData&);
    void didDeleteObjectStore(const IDBResultData&);
    void didClearObjectStore(const IDBResultData&);
    void didCreateIndex(const IDBResultData&);
    void didDeleteIndex(const IDBResultData&);
    void didPutOrAdd(const IDBResultData&);
    void didGetRecord(const IDBResultData&);
    void didGetCount(const IDBResultData&);
    void didDeleteRecord(const IDBResultData&);
    void didOpenCursor(const IDBResultData&);
    void didIterateCursor(const IDBResultData&);

    void fireVersionChangeEvent(UniqueIDBDatabaseConnection&, uint64_t requestedVersion);
    void didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);

    void notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion);

private:
    IDBConnectionToClient(IDBConnectionToClientDelegate&);
    
    Ref<IDBConnectionToClientDelegate> m_delegate;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBConnectionToClient_h
