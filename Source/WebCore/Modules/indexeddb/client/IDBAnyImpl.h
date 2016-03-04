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

#ifndef IDBAnyImpl_h
#define IDBAnyImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBDatabaseImpl.h"
#include "IDBIndexImpl.h"
#include "IDBObjectStoreImpl.h"

namespace WebCore {
namespace IDBClient {

class IDBAny : public WebCore::IDBAny {
public:
    static RefPtr<IDBAny> create(Ref<IDBDatabase>&& database)
    {
        return adoptRef(new IDBAny(WTFMove(database)));
    }

    static Ref<IDBAny> create(Ref<IDBObjectStore>&& objectStore)
    {
        return adoptRef(*new IDBAny(WTFMove(objectStore)));
    }

    static Ref<IDBAny> create(Ref<IDBIndex>&& index)
    {
        return adoptRef(*new IDBAny(WTFMove(index)));
    }

    static RefPtr<IDBAny> create(Ref<IDBCursor>&& cursor)
    {
        return adoptRef(new IDBAny(WTFMove(cursor)));
    }

    static RefPtr<IDBAny> create(const IDBKeyPath& keyPath)
    {
        return adoptRef(new IDBAny(keyPath));
    }

    static RefPtr<IDBAny> create(const Deprecated::ScriptValue& value)
    {
        return adoptRef(new IDBAny(value));
    }

    static RefPtr<IDBAny> createUndefined()
    {
        return adoptRef(new IDBAny(IDBAny::Type::Undefined));
    }

    virtual ~IDBAny();

    Type type() const final { return m_type; }
    RefPtr<WebCore::DOMStringList> domStringList() final;
    RefPtr<WebCore::IDBCursor> idbCursor() final;
    RefPtr<WebCore::IDBCursorWithValue> idbCursorWithValue() final;
    RefPtr<WebCore::IDBDatabase> idbDatabase() final;
    RefPtr<WebCore::IDBFactory> idbFactory() final;
    RefPtr<WebCore::IDBIndex> idbIndex() final;
    RefPtr<WebCore::IDBObjectStore> idbObjectStore() final;
    RefPtr<WebCore::IDBTransaction> idbTransaction() final;
    const Deprecated::ScriptValue& scriptValue() final;
    int64_t integer() final;
    const String& string() final;
    const IDBKeyPath& keyPath() final;

    IDBObjectStore* modernIDBObjectStore();
    IDBIndex* modernIDBIndex();
    IDBCursor* modernIDBCursor();

private:
    explicit IDBAny(IDBAny::Type);
    explicit IDBAny(Ref<IDBDatabase>&&);
    explicit IDBAny(Ref<IDBObjectStore>&&);
    explicit IDBAny(Ref<IDBIndex>&&);
    explicit IDBAny(Ref<IDBCursor>&&);
    explicit IDBAny(const IDBKeyPath&);
    explicit IDBAny(const Deprecated::ScriptValue&);

    IDBAny::Type m_type { IDBAny::Type::Undefined };
    RefPtr<IDBDatabase> m_database;
    RefPtr<IDBObjectStore> m_objectStore;
    RefPtr<IDBIndex> m_index;
    RefPtr<IDBCursor> m_cursor;
    RefPtr<IDBCursor> m_cursorWithValue;

    const IDBKeyPath m_idbKeyPath;
    const Deprecated::ScriptValue m_scriptValue;
    const String m_string;
    const int64_t m_integer { 0 };
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBAnyImpl_h
