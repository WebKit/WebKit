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

#ifndef IDBOpenDBRequest_h
#define IDBOpenDBRequest_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBRequest.h"
#include "IndexedDB.h"

namespace WebCore {

class IDBDatabaseCallbacks;

class IDBOpenDBRequest : public IDBRequest {
public:
    static PassRefPtr<IDBOpenDBRequest> create(ScriptExecutionContext*, PassRefPtr<IDBDatabaseCallbacks>, int64_t transactionId, uint64_t version, IndexedDB::VersionNullness);
    virtual ~IDBOpenDBRequest();

    using IDBRequest::onSuccess;

    virtual void onBlocked(uint64_t existingVersion) override;
    virtual void onUpgradeNeeded(uint64_t oldVersion, PassRefPtr<IDBDatabaseBackend>, const IDBDatabaseMetadata&) override;
    virtual void onSuccess(PassRefPtr<IDBDatabaseBackend>, const IDBDatabaseMetadata&) override;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const;
    virtual bool dispatchEvent(PassRefPtr<Event>) override;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(blocked);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(upgradeneeded);

protected:
    virtual bool shouldEnqueueEvent() const override;

private:
    IDBOpenDBRequest(ScriptExecutionContext*, PassRefPtr<IDBDatabaseCallbacks>, int64_t transactionId, uint64_t version, IndexedDB::VersionNullness);

    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
    const int64_t m_transactionId;
    uint64_t m_version;
    IndexedDB::VersionNullness m_versionNullness;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBOpenDBRequest_h
