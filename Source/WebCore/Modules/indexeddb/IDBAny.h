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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyPath.h"
#include "ScriptWrappable.h"
#include <heap/Strong.h>
#include <runtime/JSCJSValue.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DOMStringList;
class IDBCursor;
class IDBCursorWithValue;
class IDBDatabase;
class IDBFactory;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;

class IDBAny : public ScriptWrappable, public RefCounted<IDBAny> {
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

    static RefPtr<IDBAny> create(const JSC::Strong<JSC::Unknown>& value)
    {
        return adoptRef(new IDBAny(value));
    }

    static RefPtr<IDBAny> createUndefined()
    {
        return adoptRef(new IDBAny(IDBAny::Type::Undefined));
    }

    virtual ~IDBAny();

    enum class Type {
        Undefined = 0,
        Null,
        DOMStringList,
        IDBCursor,
        IDBCursorWithValue,
        IDBDatabase,
        IDBFactory,
        IDBIndex,
        IDBObjectStore,
        IDBTransaction,
        ScriptValue,
        Integer,
        String,
        KeyPath,
    };

    Type type() const { return m_type; }
    RefPtr<DOMStringList> domStringList();
    RefPtr<IDBCursor> idbCursor();
    RefPtr<IDBCursorWithValue> idbCursorWithValue();
    RefPtr<IDBDatabase> idbDatabase();
    RefPtr<IDBFactory> idbFactory();
    RefPtr<IDBIndex> idbIndex();
    RefPtr<IDBObjectStore> idbObjectStore();
    RefPtr<IDBTransaction> idbTransaction();
    JSC::JSValue scriptValue();
    int64_t integer();
    const String& string();
    const IDBKeyPath& keyPath();

private:
    explicit IDBAny(IDBAny::Type);
    explicit IDBAny(Ref<IDBDatabase>&&);
    explicit IDBAny(Ref<IDBObjectStore>&&);
    explicit IDBAny(Ref<IDBIndex>&&);
    explicit IDBAny(Ref<IDBCursor>&&);
    explicit IDBAny(const IDBKeyPath&);
    explicit IDBAny(const JSC::Strong<JSC::Unknown>&);

    IDBAny::Type m_type { IDBAny::Type::Undefined };
    RefPtr<IDBDatabase> m_database;
    RefPtr<IDBObjectStore> m_objectStore;
    RefPtr<IDBIndex> m_index;
    RefPtr<IDBCursor> m_cursor;
    RefPtr<IDBCursorWithValue> m_cursorWithValue;

    const IDBKeyPath m_idbKeyPath;
    JSC::Strong<JSC::Unknown> m_scriptValue;
    const String m_string;
    const int64_t m_integer { 0 };
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
