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
class CallFrame;
}

namespace WebCore {

class IDBKeyRange;

struct IDBKeyRangeData;

class IDBIndex final : public ActiveDOMObject {
    WTF_MAKE_NONCOPYABLE(IDBIndex);
    WTF_MAKE_FAST_ALLOCATED;
public:
    IDBIndex(ScriptExecutionContext&, const IDBIndexInfo&, IDBObjectStore&);

    virtual ~IDBIndex();

    const String& name() const;
    ExceptionOr<void> setName(const String&);
    IDBObjectStore& objectStore();
    const IDBKeyPath& keyPath() const;
    bool unique() const;
    bool multiEntry() const;

    void rollbackInfoForVersionChangeAbort();

    ExceptionOr<Ref<IDBRequest>> openCursor(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, IDBCursorDirection);
    ExceptionOr<Ref<IDBRequest>> openCursor(JSC::JSGlobalObject&, JSC::JSValue key, IDBCursorDirection);
    ExceptionOr<Ref<IDBRequest>> openKeyCursor(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, IDBCursorDirection);
    ExceptionOr<Ref<IDBRequest>> openKeyCursor(JSC::JSGlobalObject&, JSC::JSValue key, IDBCursorDirection);

    ExceptionOr<Ref<IDBRequest>> count(JSC::JSGlobalObject&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> count(JSC::JSGlobalObject&, JSC::JSValue key);

    ExceptionOr<Ref<IDBRequest>> get(JSC::JSGlobalObject&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> get(JSC::JSGlobalObject&, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> getKey(JSC::JSGlobalObject&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> getKey(JSC::JSGlobalObject&, JSC::JSValue key);

    ExceptionOr<Ref<IDBRequest>> getAll(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, Optional<uint32_t> count);
    ExceptionOr<Ref<IDBRequest>> getAll(JSC::JSGlobalObject&, JSC::JSValue key, Optional<uint32_t> count);
    ExceptionOr<Ref<IDBRequest>> getAllKeys(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, Optional<uint32_t> count);
    ExceptionOr<Ref<IDBRequest>> getAllKeys(JSC::JSGlobalObject&, JSC::JSValue key, Optional<uint32_t> count);

    const IDBIndexInfo& info() const { return m_info; }

    void markAsDeleted();
    bool isDeleted() const { return m_deleted; }

    void ref();
    void deref();

    void* objectStoreAsOpaqueRoot() { return &m_objectStore; }

    bool hasPendingActivity() const final;

private:
    ExceptionOr<Ref<IDBRequest>> doCount(JSC::JSGlobalObject&, const IDBKeyRangeData&);
    ExceptionOr<Ref<IDBRequest>> doGet(JSC::JSGlobalObject&, ExceptionOr<IDBKeyRangeData>);
    ExceptionOr<Ref<IDBRequest>> doGetKey(JSC::JSGlobalObject&, ExceptionOr<IDBKeyRangeData>);
    ExceptionOr<Ref<IDBRequest>> doOpenCursor(JSC::JSGlobalObject&, IDBCursorDirection, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()> &&);
    ExceptionOr<Ref<IDBRequest>> doOpenKeyCursor(JSC::JSGlobalObject&, IDBCursorDirection, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()> &&);
    ExceptionOr<Ref<IDBRequest>> doGetAll(JSC::JSGlobalObject&, Optional<uint32_t> count, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()> &&);
    ExceptionOr<Ref<IDBRequest>> doGetAllKeys(JSC::JSGlobalObject&, Optional<uint32_t> count, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()> &&);

    const char* activeDOMObjectName() const final;

    IDBIndexInfo m_info;
    IDBIndexInfo m_originalInfo;

    bool m_deleted { false };

    // IDBIndex objects are always owned by their referencing IDBObjectStore.
    // Indexes will never outlive ObjectStores so its okay to keep a raw C++ reference here.
    IDBObjectStore& m_objectStore;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
