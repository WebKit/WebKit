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

#include "IDBIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "ActiveDOMObject.h"
#include "ExceptionCode.h"
#include "IDBCursor.h"
#include "IDBIndexInfo.h"
#include "IDBRequest.h"
#include <bindings/ScriptValue.h>

namespace WebCore {

class IDBAny;
class IDBKeyRange;
class IDBObjectStore;
struct IDBKeyRangeData;

class IDBIndex : public ActiveDOMObject {
public:
    IDBIndex(ScriptExecutionContext&, const IDBIndexInfo&, IDBObjectStore&);

    virtual ~IDBIndex();

    // Implement the IDL
    const String& name() const;
    RefPtr<IDBObjectStore> objectStore();
    RefPtr<IDBAny> keyPathAny() const;
    const IDBKeyPath& keyPath() const;
    bool unique() const;
    bool multiEntry() const;

    RefPtr<IDBRequest> openCursor(ScriptExecutionContext& context, ExceptionCodeWithMessage& ec) { return openCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext& context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec) { return openCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext& context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec) { return openCursor(context, key, IDBCursor::directionNext(), ec); }
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext&, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(ScriptExecutionContext&, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> count(ScriptExecutionContext&, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> count(ScriptExecutionContext&, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> count(ScriptExecutionContext&, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext& context, ExceptionCodeWithMessage& ec) { return openKeyCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext& context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec) { return openKeyCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext& context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec) { return openKeyCursor(context, key, IDBCursor::directionNext(), ec); }
    RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext&, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext&, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> get(ScriptExecutionContext&, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> get(ScriptExecutionContext&, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> getKey(ScriptExecutionContext&, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> getKey(ScriptExecutionContext&, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);

    const IDBIndexInfo& info() const { return m_info; }

    IDBObjectStore& modernObjectStore() { return m_objectStore; }

    void markAsDeleted();
    bool isDeleted() const { return m_deleted; }

    void ref();
    void deref();

private:
    RefPtr<IDBRequest> doCount(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> doGet(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> doGetKey(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);

    // ActiveDOMObject
    const char* activeDOMObjectName() const;
    bool canSuspendForDocumentSuspension() const;
    bool hasPendingActivity() const;

    IDBIndexInfo m_info;

    bool m_deleted { false };

    // IDBIndex objects are always owned by their referencing IDBObjectStore.
    // Indexes will never outlive ObjectStores so its okay to keep a raw C++ reference here.
    IDBObjectStore& m_objectStore;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
