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

#include "config.h"
#include "IDBObjectStoreImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBTransactionImpl.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBObjectStore> IDBObjectStore::create(const IDBObjectStoreInfo& info, IDBTransaction& transaction)
{
    return adoptRef(*new IDBObjectStore(info, transaction));
}

IDBObjectStore::IDBObjectStore(const IDBObjectStoreInfo& info, IDBTransaction& transaction)
    : m_info(info)
    , m_transaction(transaction)
{
}

IDBObjectStore::~IDBObjectStore()
{
}

int64_t IDBObjectStore::id() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

const String IDBObjectStore::name() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBAny> IDBObjectStore::keyPathAny() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

const IDBKeyPath IDBObjectStore::keyPath() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<DOMStringList> IDBObjectStore::indexNames() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBTransaction> IDBObjectStore::transaction() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool IDBObjectStore::autoIncrement() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::add(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::put(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, IDBKeyRange*, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue&, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::get(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::add(JSC::ExecState&, Deprecated::ScriptValue&, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::put(JSC::ExecState&, Deprecated::ScriptValue&, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::clear(ScriptExecutionContext*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBIndex> IDBObjectStore::createIndex(ScriptExecutionContext*, const String&, const IDBKeyPath&, bool, bool, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBIndex> IDBObjectStore::index(const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void IDBObjectStore::deleteIndex(const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
