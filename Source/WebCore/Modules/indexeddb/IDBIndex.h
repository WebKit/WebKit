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

#include "IDBCursor.h"
#include "IDBIndexInfo.h"
#include "IDBRequest.h"

namespace JSC {
class ExecState;
}

namespace WebCore {

class IDBKeyRange;

struct IDBKeyRangeData;

class IDBIndex final : private ActiveDOMObject {
public:
    IDBIndex(ScriptExecutionContext&, const IDBIndexInfo&, IDBObjectStore&);

    virtual ~IDBIndex();

    const String& name() const;
    IDBObjectStore& objectStore();
    const IDBKeyPath& keyPath() const;
    bool unique() const;
    bool multiEntry() const;

    RefPtr<IDBRequest> openCursor(JSC::ExecState&, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openCursor(JSC::ExecState&, JSC::JSValue key, const String& direction, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> count(JSC::ExecState&, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> count(JSC::ExecState&, JSC::JSValue key, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> openKeyCursor(JSC::ExecState&, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> openKeyCursor(JSC::ExecState&, JSC::JSValue key, const String& direction, ExceptionCodeWithMessage&);

    RefPtr<IDBRequest> get(JSC::ExecState&, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> get(JSC::ExecState&, JSC::JSValue key, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> getKey(JSC::ExecState&, IDBKeyRange*, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> getKey(JSC::ExecState&, JSC::JSValue key, ExceptionCodeWithMessage&);

    const IDBIndexInfo& info() const { return m_info; }

    void markAsDeleted();
    bool isDeleted() const { return m_deleted; }

    void ref();
    void deref();

    void* objectStoreAsOpaqueRoot() { return &m_objectStore; }

private:
    RefPtr<IDBRequest> doCount(JSC::ExecState&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> doGet(JSC::ExecState&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);
    RefPtr<IDBRequest> doGetKey(JSC::ExecState&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);

    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    bool hasPendingActivity() const final;

    IDBIndexInfo m_info;

    bool m_deleted { false };

    // IDBIndex objects are always owned by their referencing IDBObjectStore.
    // Indexes will never outlive ObjectStores so its okay to keep a raw C++ reference here.
    IDBObjectStore& m_objectStore;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
