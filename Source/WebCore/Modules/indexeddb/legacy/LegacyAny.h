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

#ifndef LegacyAny_h
#define LegacyAny_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBKeyPath.h"
#include "ScriptWrappable.h"
#include <bindings/ScriptValue.h>
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

class LegacyCursor;
class LegacyCursorWithValue;
class LegacyDatabase;
class LegacyFactory;
class LegacyIndex;
class LegacyObjectStore;
class LegacyTransaction;

class LegacyAny : public IDBAny {
public:
    static PassRefPtr<LegacyAny> createInvalid();
    static PassRefPtr<LegacyAny> createNull();
    static PassRefPtr<LegacyAny> createString(const String&);
    template<typename T>
    static PassRefPtr<LegacyAny> create(T* idbObject)
    {
        return adoptRef(new LegacyAny(idbObject));
    }
    template<typename T>
    static PassRefPtr<LegacyAny> create(const T& idbObject)
    {
        return adoptRef(new LegacyAny(idbObject));
    }
    template<typename T>
    static PassRefPtr<LegacyAny> create(PassRefPtr<T> idbObject)
    {
        return adoptRef(new LegacyAny(idbObject));
    }
    static PassRefPtr<LegacyAny> create(int64_t value)
    {
        return adoptRef(new LegacyAny(value));
    }
    ~LegacyAny();

    virtual Type type() const override final { return m_type; }
    // Use type() to figure out which one of these you're allowed to call.
    virtual PassRefPtr<DOMStringList> domStringList() override final;
    virtual PassRefPtr<IDBCursor> idbCursor() override final;
    virtual PassRefPtr<IDBCursorWithValue> idbCursorWithValue() override final;
    virtual PassRefPtr<IDBDatabase> idbDatabase() override final;
    virtual PassRefPtr<IDBFactory> idbFactory() override final;
    virtual PassRefPtr<IDBIndex> idbIndex() override final;
    virtual PassRefPtr<IDBObjectStore> idbObjectStore() override final;
    virtual PassRefPtr<IDBTransaction> idbTransaction() override final;
    virtual const Deprecated::ScriptValue& scriptValue() override final;
    virtual int64_t integer() override final;
    virtual const String& string() override final;
    virtual const IDBKeyPath& keyPath() override final { return m_idbKeyPath; };

    LegacyCursor* legacyCursor();
    LegacyCursorWithValue* legacyCursorWithValue();
    LegacyDatabase* legacyDatabase();
    LegacyFactory* legacyFactory();
    LegacyIndex* legacyIndex();
    LegacyObjectStore* legacyObjectStore();
    LegacyTransaction* legacyTransaction();

private:
    explicit LegacyAny(Type);
    explicit LegacyAny(PassRefPtr<DOMStringList>);
    explicit LegacyAny(PassRefPtr<LegacyCursor>);
    explicit LegacyAny(PassRefPtr<LegacyCursorWithValue>);
    explicit LegacyAny(PassRefPtr<LegacyDatabase>);
    explicit LegacyAny(PassRefPtr<LegacyFactory>);
    explicit LegacyAny(PassRefPtr<LegacyIndex>);
    explicit LegacyAny(PassRefPtr<LegacyObjectStore>);
    explicit LegacyAny(PassRefPtr<LegacyTransaction>);
    explicit LegacyAny(const IDBKeyPath&);
    explicit LegacyAny(const String&);
    explicit LegacyAny(const Deprecated::ScriptValue&);
    explicit LegacyAny(int64_t);

    const Type m_type;

    // Only one of the following should ever be in use at any given time.
    const RefPtr<DOMStringList> m_domStringList;
    const RefPtr<LegacyCursor> m_idbCursor;
    const RefPtr<LegacyCursorWithValue> m_idbCursorWithValue;
    const RefPtr<LegacyDatabase> m_idbDatabase;
    const RefPtr<LegacyFactory> m_idbFactory;
    const RefPtr<LegacyIndex> m_idbIndex;
    const RefPtr<LegacyObjectStore> m_idbObjectStore;
    const RefPtr<LegacyTransaction> m_idbTransaction;
    const IDBKeyPath m_idbKeyPath;
    const Deprecated::ScriptValue m_scriptValue;
    const String m_string;
    const int64_t m_integer;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // LegacyAny_h
