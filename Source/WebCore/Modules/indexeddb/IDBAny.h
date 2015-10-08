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

    // FIXME: This is a temporary hack to allow casts in WebInspector code while Modern IDB and Legacy IDB live side-by-side.
    // It should be removed when the legacy implementation is removed as part of https://bugs.webkit.org/show_bug.cgi?id=149117
    virtual bool isLegacy() const { return false; }

    virtual Type type() const = 0;
    virtual RefPtr<DOMStringList> domStringList() = 0;
    virtual RefPtr<IDBCursor> idbCursor() = 0;
    virtual RefPtr<IDBCursorWithValue> idbCursorWithValue() = 0;
    virtual RefPtr<IDBDatabase> idbDatabase() = 0;
    virtual RefPtr<IDBFactory> idbFactory() = 0;
    virtual RefPtr<IDBIndex> idbIndex() = 0;
    virtual RefPtr<IDBObjectStore> idbObjectStore() = 0;
    virtual RefPtr<IDBTransaction> idbTransaction() = 0;
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
