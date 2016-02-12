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

#ifndef IDBIndexImpl_h
#define IDBIndexImpl_h

#include "IDBIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBIndexInfo.h"

namespace WebCore {

struct IDBKeyRangeData;

namespace IDBClient {

class IDBObjectStore;

class IDBIndex : public WebCore::IDBIndex {
public:
    IDBIndex(const IDBIndexInfo&, IDBObjectStore&);

    virtual ~IDBIndex();

    // Implement the IDL
    virtual const String& name() const override final;
    virtual RefPtr<WebCore::IDBObjectStore> objectStore() override final;
    virtual RefPtr<WebCore::IDBAny> keyPathAny() const override final;
    virtual const IDBKeyPath& keyPath() const override final;
    virtual bool unique() const override final;
    virtual bool multiEntry() const override final;

    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec) override final { return openCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec) override final { return openCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec) override final { return openCursor(context, key, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&) override final;

    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> count(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override final;

    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec) override final { return openKeyCursor(context, static_cast<IDBKeyRange*>(nullptr), ec); }
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec) override final { return openKeyCursor(context, keyRange, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec) override final { return openKeyCursor(context, key, IDBCursor::directionNext(), ec); }
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> openKeyCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&) override final;

    virtual RefPtr<WebCore::IDBRequest> get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> get(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> getKey(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> getKey(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override final;

    const IDBIndexInfo& info() const { return m_info; }

    IDBObjectStore& modernObjectStore() { return m_objectStore; }

    void markAsDeleted(std::unique_ptr<IDBIndex>&&);
    bool isDeleted() const { return m_deleted; }

    virtual bool isModern() const override { return true; }

    void ref() override;
    void deref() override;

private:
    RefPtr<WebCore::IDBRequest> doCount(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);
    RefPtr<WebCore::IDBRequest> doGet(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);
    RefPtr<WebCore::IDBRequest> doGetKey(ScriptExecutionContext&, const IDBKeyRangeData&, ExceptionCodeWithMessage&);

    IDBIndexInfo m_info;

    bool m_deleted { false };

    // Most of the time, an IDBObjectStore owns an IDBIndex through a std::unique_ptr.
    // In that scenario, attempts to ref() the IDBIndex directly ref the IDBObjectStore, so it is okay to
    // keep a raw reference to the IDBObjectStore because it will always outlive the IDBIndex.
    IDBObjectStore& m_objectStore;

    // But when an IDBIndex is deleted from its IDBObjectStore that lifetime is no longer guaranteed.
    // The IDBObjectStore no longer owns the IDBIndex, so the following needs to change:
    // 1 - The IDBIndex must directly ref its IDBObjectStore to keep it alive.
    // 2 - The IDBIndex becomes traditionally RefCounted.
    // 2 - The IDBIndex holds its own std::unique_ptr, which it will clear out when its RefCount reaches 0.
    RefPtr<IDBObjectStore> m_objectStoreRef;
    unsigned m_refCount { 0 };
    std::unique_ptr<IDBIndex> m_selfOwner;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBIndexImpl_h
