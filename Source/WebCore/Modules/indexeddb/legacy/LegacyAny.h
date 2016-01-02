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
    static RefPtr<LegacyAny> createInvalid();
    static RefPtr<LegacyAny> createNull();
    static RefPtr<LegacyAny> createString(const String&);
    template<typename T>
    static RefPtr<LegacyAny> create(T* idbObject)
    {
        return adoptRef(new LegacyAny(idbObject));
    }
    template<typename T>
    static RefPtr<LegacyAny> create(const T& idbObject)
    {
        return adoptRef(new LegacyAny(idbObject));
    }
    template<typename T>
    static RefPtr<LegacyAny> create(PassRefPtr<T> idbObject)
    {
        RefPtr<T> refObject = idbObject;
        return adoptRef(new LegacyAny(WTFMove(refObject)));
    }
    static RefPtr<LegacyAny> create(int64_t value)
    {
        return adoptRef(new LegacyAny(value));
    }
    ~LegacyAny();

    // FIXME: This is a temporary hack to allow casts in WebInspector code while Modern IDB and Legacy IDB live side-by-side.
    // It should be removed when the legacy implementation is removed as part of https://bugs.webkit.org/show_bug.cgi?id=149117
    virtual bool isLegacy() const override final { return true; }

    virtual Type type() const override final { return m_type; }
    // Use type() to figure out which one of these you're allowed to call.
    virtual RefPtr<DOMStringList> domStringList() override final;
    virtual RefPtr<IDBCursor> idbCursor() override final;
    virtual RefPtr<IDBCursorWithValue> idbCursorWithValue() override final;
    virtual RefPtr<IDBDatabase> idbDatabase() override final;
    virtual RefPtr<IDBFactory> idbFactory() override final;
    virtual RefPtr<IDBIndex> idbIndex() override final;
    virtual RefPtr<IDBObjectStore> idbObjectStore() override final;
    virtual RefPtr<IDBTransaction> idbTransaction() override final;
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
    explicit LegacyAny(RefPtr<DOMStringList>);
    explicit LegacyAny(RefPtr<LegacyCursor>);
    explicit LegacyAny(RefPtr<LegacyCursorWithValue>);
    explicit LegacyAny(RefPtr<LegacyDatabase>);
    explicit LegacyAny(RefPtr<LegacyFactory>);
    explicit LegacyAny(RefPtr<LegacyIndex>);
    explicit LegacyAny(RefPtr<LegacyObjectStore>);
    explicit LegacyAny(RefPtr<LegacyTransaction>);
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
