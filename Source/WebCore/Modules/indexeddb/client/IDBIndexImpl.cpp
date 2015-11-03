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
#include "IDBIndexImpl.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {
namespace IDBClient {

IDBIndex::~IDBIndex()
{
}

const String& IDBIndex::name() const
{
    return m_info.name();
}

RefPtr<IDBObjectStore> IDBIndex::objectStore() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBAny> IDBIndex::keyPathAny() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

const IDBKeyPath& IDBIndex::keyPath() const
{
    return m_info.keyPath();
}

bool IDBIndex::unique() const
{
    return m_info.unique();
}

bool IDBIndex::multiEntry() const
{
    return m_info.multiEntry();
}

RefPtr<IDBRequest> IDBIndex::openCursor(ScriptExecutionContext*, IDBKeyRange*, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue&, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::count(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::openKeyCursor(ScriptExecutionContext*, IDBKeyRange*, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::openKeyCursor(ScriptExecutionContext*, const Deprecated::ScriptValue&, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::get(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::getKey(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBRequest> IDBIndex::getKey(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
