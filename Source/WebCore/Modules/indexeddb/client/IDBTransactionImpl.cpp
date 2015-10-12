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
#include "IDBTransactionImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMError.h"
#include "IDBDatabaseImpl.h"
#include "IDBObjectStore.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBTransaction> IDBTransaction::create(IDBDatabase& database, const IDBTransactionInfo& info)
{
    return adoptRef(*new IDBTransaction(database, info));
}

IDBTransaction::IDBTransaction(IDBDatabase& database, const IDBTransactionInfo& info)
    : WebCore::IDBTransaction(database.scriptExecutionContext())
    , m_info(info)
{
    suspendIfNeeded();
}

IDBTransaction::~IDBTransaction()
{
}

const String& IDBTransaction::mode() const
{
    switch (m_info.mode()) {
    case IndexedDB::TransactionMode::ReadOnly:
        return IDBTransaction::modeReadOnly();
    case IndexedDB::TransactionMode::ReadWrite:
        return IDBTransaction::modeReadWrite();
    case IndexedDB::TransactionMode::VersionChange:
        return IDBTransaction::modeVersionChange();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

WebCore::IDBDatabase* IDBTransaction::db() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<DOMError> IDBTransaction::error() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<IDBObjectStore> IDBTransaction::objectStore(const String&, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void IDBTransaction::abort(ExceptionCode&)
{
    ASSERT_NOT_REACHED();
}

const char* IDBTransaction::activeDOMObjectName() const
{
    return "IDBTransaction";
}

bool IDBTransaction::canSuspendForPageCache() const
{
    return false;
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
