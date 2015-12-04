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

#ifndef LegacyObjectStore_h
#define LegacyObjectStore_h

#include "Dictionary.h"
#include "IDBCursor.h"
#include "IDBDatabaseBackend.h"
#include "IDBDatabaseMetadata.h"
#include "IDBIndex.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"
#include "LegacyIndex.h"
#include "LegacyTransaction.h"
#include "ScriptWrappable.h"
#include "SerializedScriptValue.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class DOMStringList;
class IDBAny;
class LegacyIndex;

class LegacyObjectStore : public ScriptWrappable, public IDBObjectStore {
public:
    static Ref<LegacyObjectStore> create(const IDBObjectStoreMetadata& metadata, LegacyTransaction* transaction)
    {
        return adoptRef(*new LegacyObjectStore(metadata, transaction));
    }
    ~LegacyObjectStore() { }

    // Implement the IDBObjectStore IDL
    int64_t id() const { return m_metadata.id; }
    const String name() const { return m_metadata.name; }
    RefPtr<IDBAny> keyPathAny() const { return LegacyAny::create(m_metadata.keyPath); }
    const IDBKeyPath keyPath() const { return m_metadata.keyPath; }
    RefPtr<DOMStringList> indexNames() const;
    RefPtr<IDBTransaction> transaction() { return m_transaction; }
    bool autoIncrement() const { return m_metadata.autoIncrement; }

    RefPtr<IDBRequest> add(JSC::ExecState&, JSC::JSValue, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> put(JSC::ExecState&, JSC::JSValue, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, IDBDatabaseBackend::TaskType, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> get(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> add(JSC::ExecState&, JSC::JSValue, JSC::JSValue key, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> put(JSC::ExecState&, JSC::JSValue, JSC::JSValue key, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> put(IDBDatabaseBackend::PutMode, RefPtr<LegacyAny>, JSC::ExecState&, Deprecated::ScriptValue&, RefPtr<IDBKey>, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> put(IDBDatabaseBackend::PutMode, RefPtr<LegacyAny> source, JSC::ExecState&, Deprecated::ScriptValue&, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> deleteFunction(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> deleteFunction(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> clear(ScriptExecutionContext*, ExceptionCodeWithMessage&);

    RefPtr<IDBIndex> createIndex(ScriptExecutionContext* context, const String& name, const String& keyPath, const Dictionary& options, ExceptionCodeWithMessage& ec) { return createIndex(context, name, IDBKeyPath(keyPath), options, ec); }
    RefPtr<IDBIndex> createIndex(ScriptExecutionContext* context, const String& name, const Vector<String>& keyPath, const Dictionary& options, ExceptionCodeWithMessage& ec) { return createIndex(context, name, IDBKeyPath(keyPath), options, ec); }
    RefPtr<IDBIndex> createIndex(ScriptExecutionContext*, const String& name, const IDBKeyPath&, const Dictionary&, ExceptionCodeWithMessage&);
    RefPtr<IDBIndex> createIndex(ScriptExecutionContext*, const String& name, const IDBKeyPath&, bool unique, bool multiEntry, ExceptionCodeWithMessage&);

    RefPtr<IDBIndex> index(const String& name, ExceptionCodeWithMessage&);
    void deleteIndex(const String& name, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> count(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec) { return count(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    RefPtr<IDBRequest> count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> count(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);

    void markDeleted() { m_deleted = true; }
    void transactionFinished();

    IDBObjectStoreMetadata metadata() const { return m_metadata; }
    void setMetadata(const IDBObjectStoreMetadata& metadata) { m_metadata = metadata; }

    typedef Vector<RefPtr<IDBKey>> IndexKeys;
    typedef HashMap<String, IndexKeys> IndexKeyMap;

    IDBDatabaseBackend* backendDB() const;

private:
    LegacyObjectStore(const IDBObjectStoreMetadata&, LegacyTransaction*);

    int64_t findIndexId(const String& name) const;
    bool containsIndex(const String& name) const
    {
        return findIndexId(name) != IDBIndexMetadata::InvalidId;
    }

    IDBObjectStoreMetadata m_metadata;
    RefPtr<LegacyTransaction> m_transaction;
    bool m_deleted;

    typedef HashMap<String, RefPtr<LegacyIndex>> IDBIndexMap;
    IDBIndexMap m_indexMap;
};

} // namespace WebCore

#endif

#endif // LegacyObjectStore_h
