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

#ifndef IDBAny_h
#define IDBAny_h

#if ENABLE(INDEXED_DATABASE)

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

class IDBAny : public ScriptWrappable, public RefCounted<IDBAny> {
public:
    virtual ~IDBAny() { }

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
        KeyPathType,
    };

    virtual Type type() const = 0;
    virtual PassRefPtr<DOMStringList> domStringList() = 0;
    virtual PassRefPtr<IDBCursor> idbCursor() = 0;
    virtual PassRefPtr<IDBCursorWithValue> idbCursorWithValue() = 0;
    virtual PassRefPtr<IDBDatabase> idbDatabase() = 0;
    virtual PassRefPtr<IDBFactory> idbFactory() = 0;
    virtual PassRefPtr<IDBIndex> idbIndex() = 0;
    virtual PassRefPtr<IDBObjectStore> idbObjectStore() = 0;
    virtual PassRefPtr<IDBTransaction> idbTransaction() = 0;
    virtual const Deprecated::ScriptValue& scriptValue() = 0;
    virtual int64_t integer() = 0;
    virtual const String& string() = 0;
    virtual const IDBKeyPath& keyPath() = 0;

protected:
    IDBAny();
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBAny_h
