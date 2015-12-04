/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef LegacyIndex_h
#define LegacyIndex_h

#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBDatabaseMetadata.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"
#include "LegacyAny.h"
#include "LegacyObjectStore.h"
#include "LegacyRequest.h"
#include "LegacyTransaction.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBObjectStore;

class LegacyIndex : public ScriptWrappable, public IDBIndex {
public:
    static Ref<LegacyIndex> create(const IDBIndexMetadata& metadata, LegacyObjectStore* objectStore, LegacyTransaction* transaction)
    {
        return adoptRef(*new LegacyIndex(metadata, objectStore, transaction));
    }
    ~LegacyIndex();

    // Implement the IDL
    virtual const String& name() const override final { return m_metadata.name; }
    virtual RefPtr<IDBObjectStore> objectStore() override final { return m_objectStore; }
    LegacyObjectStore* legacyObjectStore() const { return m_objectStore.get(); }
    virtual RefPtr<IDBAny> keyPathAny() const override final { return LegacyAny::create(m_metadata.keyPath); }
    virtual const IDBKeyPath& keyPath() const override final { return m_metadata.keyPath; }
    virtual bool unique() const override final { return m_metadata.unique; }
    virtual bool multiEntry() const override final { return m_metadata.multiEntry; }
    int64_t id() const { return m_metadata.id; }

    // FIXME: Try to modify the code generator so this is unneeded.
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec) override final { return openCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec) override final { return openCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec) override final { return openCursor(context, key, IDBCursor::directionNext(), ec); }
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBRequest> count(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec) override final { return count(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    virtual RefPtr<IDBRequest> count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBRequest> count(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override final;

    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec) override final { return openKeyCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec) override final { return openKeyCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec) override final { return openKeyCursor(context, key, IDBCursor::directionNext(), ec); }
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&) override final;

    virtual RefPtr<IDBRequest> get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBRequest> get(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBRequest> getKey(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<IDBRequest> getKey(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override final;

    void markDeleted() { m_deleted = true; }

    IDBDatabaseBackend* backendDB() const;

private:
    LegacyIndex(const IDBIndexMetadata&, LegacyObjectStore*, LegacyTransaction*);

    IDBIndexMetadata m_metadata;
    RefPtr<LegacyObjectStore> m_objectStore;
    RefPtr<LegacyTransaction> m_transaction;
    bool m_deleted;
};

} // namespace WebCore

#endif

#endif // LegacyIndex_h
