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

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "IDBKeyPath.h"
#include "IDBObjectStoreInfo.h"
#include <wtf/HashSet.h>

namespace JSC {
class ExecState;
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

struct IDBKeyRangeData;

namespace IndexedDB {
enum class ObjectStoreOverwriteMode;
}

class IDBObjectStore final : public RefCounted<IDBObjectStore>, public ActiveDOMObject {
public:
    static Ref<IDBObjectStore> create(ScriptExecutionContext&, const IDBObjectStoreInfo&, IDBTransaction&);

    ~IDBObjectStore();

    const String& name() const;
    ExceptionOr<void> setName(const String&);
    const IDBKeyPath& keyPath() const;
    RefPtr<DOMStringList> indexNames() const;
    IDBTransaction& transaction();
    bool autoIncrement() const;

    struct IndexParameters {
        bool unique;
        bool multiEntry;
    };

    ExceptionOr<Ref<IDBRequest>> openCursor(JSC::ExecState&, RefPtr<IDBKeyRange>, const String& direction);
    ExceptionOr<Ref<IDBRequest>> openCursor(JSC::ExecState&, JSC::JSValue key, const String& direction);
    ExceptionOr<Ref<IDBRequest>> openKeyCursor(JSC::ExecState&, RefPtr<IDBKeyRange>, const String& direction);
    ExceptionOr<Ref<IDBRequest>> openKeyCursor(JSC::ExecState&, JSC::JSValue key, const String& direction);
    ExceptionOr<Ref<IDBRequest>> get(JSC::ExecState&, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> get(JSC::ExecState&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> add(JSC::ExecState&, JSC::JSValue, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> put(JSC::ExecState&, JSC::JSValue, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> deleteFunction(JSC::ExecState&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> deleteFunction(JSC::ExecState&, JSC::JSValue key);
    ExceptionOr<Ref<IDBRequest>> clear(JSC::ExecState&);
    ExceptionOr<Ref<IDBIndex>> createIndex(JSC::ExecState&, const String& name, IDBKeyPathVariant&&, const IndexParameters&);
    ExceptionOr<Ref<IDBIndex>> index(const String& name);
    ExceptionOr<void> deleteIndex(const String& name);
    ExceptionOr<Ref<IDBRequest>> count(JSC::ExecState&, IDBKeyRange*);
    ExceptionOr<Ref<IDBRequest>> count(JSC::ExecState&, JSC::JSValue key);

    ExceptionOr<Ref<IDBRequest>> putForCursorUpdate(JSC::ExecState&, JSC::JSValue, JSC::JSValue key);

    void markAsDeleted();
    bool isDeleted() const { return m_deleted; }

    const IDBObjectStoreInfo& info() const { return m_info; }

    void rollbackForVersionChangeAbort();

    void visitReferencedIndexes(JSC::SlotVisitor&) const;
    void renameReferencedIndex(IDBIndex&, const String& newName);

private:
    IDBObjectStore(ScriptExecutionContext&, const IDBObjectStoreInfo&, IDBTransaction&);

    enum class InlineKeyCheck { Perform, DoNotPerform };
    ExceptionOr<Ref<IDBRequest>> putOrAdd(JSC::ExecState&, JSC::JSValue, RefPtr<IDBKey>, IndexedDB::ObjectStoreOverwriteMode, InlineKeyCheck);
    ExceptionOr<Ref<IDBRequest>> doCount(JSC::ExecState&, const IDBKeyRangeData&);
    ExceptionOr<Ref<IDBRequest>> doDelete(JSC::ExecState&, IDBKeyRange*);

    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    bool hasPendingActivity() const final;

    IDBObjectStoreInfo m_info;
    IDBObjectStoreInfo m_originalInfo;
    Ref<IDBTransaction> m_transaction;

    bool m_deleted { false };

    mutable Lock m_referencedIndexLock;
    HashMap<String, std::unique_ptr<IDBIndex>> m_referencedIndexes;
    HashSet<std::unique_ptr<IDBIndex>> m_deletedIndexes;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
