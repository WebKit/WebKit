/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "IDBCursorDirection.h"
#include "IDBKeyPath.h"
#include "IDBObjectStoreInfo.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Lock.h>

namespace JSC {
class CallFrame;
class JSGlobalObject;
class JSValue;
class SlotVisitor;
}

namespace WebCore {

class DOMStringList;
class IDBIndex;
class IDBKey;
class IDBKeyRange;
class IDBRequest;
class IDBTransaction;
class SerializedScriptValue;

struct IDBKeyRangeData;

namespace IndexedDB {
enum class ObjectStoreOverwriteMode : uint8_t;
}

class IDBObjectStore final : public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(IDBObjectStore);
public:
    IDBObjectStore(ScriptExecutionContext&, const IDBObjectStoreInfo&, IDBTransaction&);
    ~IDBObjectStore();

    const String& name() const;
    ExceptionOr<void> setName(const String&);
    const std::optional<IDBKeyPath>& keyPath() const;
    Ref<DOMStringList> indexNames() const;
    IDBTransaction& transaction();
    bool autoIncrement() const;

    struct IndexParameters {
        bool unique;
        bool multiEntry;
    };

    ExceptionOr<Ref<IDBRequest>> openCursor(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, IDBCursorDirection);
    ExceptionOr<Ref<IDBRequest>> openCursor(JSC::JSGlobalObject&, JSC::JSValue key, IDBCursorDirection);
    ExceptionOr<Ref<IDBRequest>> openKeyCursor(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, IDBCursorDirection);
    ExceptionOr<Ref<IDBRequest>> openKeyCursor(JSC::JSGlobalObject&, JSC::JSValue key, IDBCursorDirection);
    ExceptionOr<Ref<IDBRequest>> get(JSC::JSGlobalObject&, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> get(JSC::JSGlobalObject&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> getKey(JSC::JSGlobalObject&, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> getKey(JSC::JSGlobalObject&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> add(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> put(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> deleteFunction(JSC::JSGlobalObject&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> deleteFunction(JSC::JSGlobalObject&, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> clear(JSC::JSGlobalObject&);
    ExceptionOr<Ref<IDBIndex>> createIndex(JSC::JSGlobalObject&, const String& name, IDBKeyPath&&, const IndexParameters&);
    ExceptionOr<Ref<IDBIndex>> index(const String& name);
    ExceptionOr<void> deleteIndex(const String& name);
    ExceptionOr<Ref<IDBRequest>> count(JSC::JSGlobalObject&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> count(JSC::JSGlobalObject&, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> getAll(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, std::optional<uint32_t> count);
    ExceptionOr<Ref<IDBRequest>> getAll(JSC::JSGlobalObject&, JSC::JSValue key, std::optional<uint32_t> count);
    ExceptionOr<Ref<IDBRequest>> getAllKeys(JSC::JSGlobalObject&, RefPtr<IDBKeyRange>&&, std::optional<uint32_t> count);
    ExceptionOr<Ref<IDBRequest>> getAllKeys(JSC::JSGlobalObject&, JSC::JSValue key, std::optional<uint32_t> count);

    ExceptionOr<Ref<IDBRequest>> putForCursorUpdate(JSC::JSGlobalObject&, JSC::JSValue, RefPtr<IDBKey>&&, RefPtr<SerializedScriptValue>&&);

    void markAsDeleted();
    bool isDeleted() const { return m_deleted; }

    const IDBObjectStoreInfo& info() const { return m_info; }

    void rollbackForVersionChangeAbort();

    void ref();
    void deref();

    template<typename Visitor> void visitReferencedIndexes(Visitor&) const;
    void renameReferencedIndex(IDBIndex&, const String& newName);

private:
    enum class InlineKeyCheck { Perform, DoNotPerform };
    ExceptionOr<Ref<IDBRequest>> putOrAdd(JSC::JSGlobalObject&, JSC::JSValue, RefPtr<IDBKey>, IndexedDB::ObjectStoreOverwriteMode, InlineKeyCheck, RefPtr<SerializedScriptValue>&& = nullptr);
    ExceptionOr<Ref<IDBRequest>> doCount(JSC::JSGlobalObject&, const IDBKeyRangeData&);
    ExceptionOr<Ref<IDBRequest>> doDelete(JSC::JSGlobalObject&, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()> &&);
    ExceptionOr<Ref<IDBRequest>> doOpenCursor(JSC::JSGlobalObject&, IDBCursorDirection, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()>&&);
    ExceptionOr<Ref<IDBRequest>> doOpenKeyCursor(JSC::JSGlobalObject&, IDBCursorDirection, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()>&&);
    ExceptionOr<Ref<IDBRequest>> doGetAll(JSC::JSGlobalObject&, std::optional<uint32_t> count, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()> &&);
    ExceptionOr<Ref<IDBRequest>> doGetAllKeys(JSC::JSGlobalObject&, std::optional<uint32_t> count, WTF::Function<ExceptionOr<RefPtr<IDBKeyRange>>()> &&);

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    IDBObjectStoreInfo m_info;
    IDBObjectStoreInfo m_originalInfo;

    // IDBObjectStore objects are always owned by their referencing IDBTransaction.
    // ObjectStores will never outlive transactions so its okay to keep a raw C++ reference here.
    IDBTransaction& m_transaction;

    bool m_deleted { false };

    mutable Lock m_referencedIndexLock;
    HashMap<String, std::unique_ptr<IDBIndex>> m_referencedIndexes WTF_GUARDED_BY_LOCK(m_referencedIndexLock);
    HashMap<uint64_t, std::unique_ptr<IDBIndex>> m_deletedIndexes WTF_GUARDED_BY_LOCK(m_referencedIndexLock);
};

} // namespace WebCore
