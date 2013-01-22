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

#ifndef IDBAny_h
#define IDBAny_h

#if ENABLE(INDEXED_DATABASE)

#include "ScriptValue.h"
#include "ScriptWrappable.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DOMStringList;
class IDBCursor;
class IDBCursorWithValue;
class IDBDatabase;
class IDBFactory;
class IDBIndex;
class IDBKeyPath;
class IDBObjectStore;
class IDBTransaction;

class IDBAny : public ScriptWrappable, public RefCounted<IDBAny> {
public:
    static PassRefPtr<IDBAny> createInvalid();
    static PassRefPtr<IDBAny> createNull();
    static PassRefPtr<IDBAny> createString(const String&);
    template<typename T>
    static PassRefPtr<IDBAny> create(T* idbObject)
    {
        RefPtr<IDBAny> any = IDBAny::createInvalid();
        any->set(idbObject);
        return any.release();
    }
    template<typename T>
    static PassRefPtr<IDBAny> create(const T& idbObject)
    {
        RefPtr<IDBAny> any = IDBAny::createInvalid();
        any->set(idbObject);
        return any.release();
    }
    template<typename T>
    static PassRefPtr<IDBAny> create(PassRefPtr<T> idbObject)
    {
        RefPtr<IDBAny> any = IDBAny::createInvalid();
        any->set(idbObject);
        return any.release();
    }
    static PassRefPtr<IDBAny> create(int64_t value)
    {
        RefPtr<IDBAny> any = IDBAny::createInvalid();
        any->set(value);
        return any.release();
    }
    ~IDBAny();

    enum Type {
        UndefinedType = 0,
        NullType,
        DOMStringListType,
        IDBCursorType,
        IDBCursorWithValueType,
        IDBDatabaseType,
        IDBFactoryType,
        IDBIndexType,
        IDBObjectStoreType,
        IDBTransactionType,
        ScriptValueType,
        IntegerType,
        StringType,
    };

    Type type() const { return m_type; }
    // Use type() to figure out which one of these you're allowed to call.
    PassRefPtr<DOMStringList> domStringList();
    PassRefPtr<IDBCursor> idbCursor();
    PassRefPtr<IDBCursorWithValue> idbCursorWithValue();
    PassRefPtr<IDBDatabase> idbDatabase();
    PassRefPtr<IDBFactory> idbFactory();
    PassRefPtr<IDBIndex> idbIndex();
    PassRefPtr<IDBObjectStore> idbObjectStore();
    PassRefPtr<IDBTransaction> idbTransaction();
    ScriptValue scriptValue();
    int64_t integer();
    const String& string();

    // Set can only be called once.
    void setNull();
    void set(PassRefPtr<DOMStringList>);
    void set(PassRefPtr<IDBCursor>);
    void set(PassRefPtr<IDBCursorWithValue>);
    void set(PassRefPtr<IDBDatabase>);
    void set(PassRefPtr<IDBFactory>);
    void set(PassRefPtr<IDBIndex>);
    void set(PassRefPtr<IDBObjectStore>);
    void set(PassRefPtr<IDBTransaction>);
    void set(const IDBKeyPath&);
    void set(const String&);
    void set(const ScriptValue&);
    void set(int64_t);

private:
    IDBAny();

    Type m_type;

    // Only one of the following should ever be in use at any given time.
    RefPtr<DOMStringList> m_domStringList;
    RefPtr<IDBCursor> m_idbCursor;
    RefPtr<IDBCursorWithValue> m_idbCursorWithValue;
    RefPtr<IDBDatabase> m_idbDatabase;
    RefPtr<IDBFactory> m_idbFactory;
    RefPtr<IDBIndex> m_idbIndex;
    RefPtr<IDBObjectStore> m_idbObjectStore;
    RefPtr<IDBTransaction> m_idbTransaction;
    ScriptValue m_scriptValue;
    String m_string;
    int64_t m_integer;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBAny_h
