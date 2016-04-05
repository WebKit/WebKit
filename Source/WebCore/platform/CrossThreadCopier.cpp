/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CrossThreadCopier.h"

#include "URL.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SerializedScriptValue.h"
#include "SessionID.h"
#include "ThreadSafeDataBuffer.h"
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)
#include "IDBCursorInfo.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBDatabaseInfo.h"
#include "IDBError.h"
#include "IDBGetResult.h"
#include "IDBIndexInfo.h"
#include "IDBKeyData.h"
#include "IDBKeyRangeData.h"
#include "IDBObjectStoreInfo.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransactionInfo.h"
#include "IDBValue.h"
#endif

namespace WebCore {

CrossThreadCopierBase<false, false, URL>::Type CrossThreadCopierBase<false, false, URL>::copy(const URL& url)
{
    return url.isolatedCopy();
}

CrossThreadCopierBase<false, false, String>::Type CrossThreadCopierBase<false, false, String>::copy(const String& str)
{
    return str.isolatedCopy();
}

CrossThreadCopierBase<false, false, ResourceError>::Type CrossThreadCopierBase<false, false, ResourceError>::copy(const ResourceError& error)
{
    return error.copy();
}

CrossThreadCopierBase<false, false, ResourceRequest>::Type CrossThreadCopierBase<false, false, ResourceRequest>::copy(const ResourceRequest& request)
{
    return request.copyData();
}

CrossThreadCopierBase<false, false, ResourceResponse>::Type CrossThreadCopierBase<false, false, ResourceResponse>::copy(const ResourceResponse& response)
{
    return response.copyData();
}

CrossThreadCopierBase<false, false, SessionID>::Type CrossThreadCopierBase<false, false, SessionID>::copy(const SessionID& sessionID)
{
    return sessionID;
}

CrossThreadCopierBase<false, false, ThreadSafeDataBuffer>::Type CrossThreadCopierBase<false, false, ThreadSafeDataBuffer>::copy(const ThreadSafeDataBuffer& buffer)
{
    return ThreadSafeDataBuffer(buffer);
}

#if ENABLE(INDEXED_DATABASE)

IndexedDB::TransactionMode CrossThreadCopierBase<false, false, IndexedDB::TransactionMode>::copy(const IndexedDB::TransactionMode& mode)
{
    return mode;
}

IndexedDB::CursorDirection CrossThreadCopierBase<false, false, IndexedDB::CursorDirection>::copy(const IndexedDB::CursorDirection& direction)
{
    return direction;
}

IndexedDB::CursorType CrossThreadCopierBase<false, false, IndexedDB::CursorType>::copy(const IndexedDB::CursorType& type)
{
    return type;
}

CrossThreadCopierBase<false, false, IDBGetResult>::Type CrossThreadCopierBase<false, false, IDBGetResult>::copy(const IDBGetResult& result)
{
    return result.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBKeyData>::Type CrossThreadCopierBase<false, false, IDBKeyData>::copy(const IDBKeyData& keyData)
{
    return keyData.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBKeyRangeData>::Type CrossThreadCopierBase<false, false, IDBKeyRangeData>::copy(const IDBKeyRangeData& keyRangeData)
{
    return keyRangeData.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBDatabaseInfo>::Type CrossThreadCopierBase<false, false, IDBDatabaseInfo>::copy(const IDBDatabaseInfo& info)
{
    return info.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBDatabaseIdentifier>::Type CrossThreadCopierBase<false, false, IDBDatabaseIdentifier>::copy(const IDBDatabaseIdentifier& identifier)
{
    return identifier.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBTransactionInfo>::Type CrossThreadCopierBase<false, false, IDBTransactionInfo>::copy(const IDBTransactionInfo& info)
{
    return info.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBResourceIdentifier>::Type CrossThreadCopierBase<false, false, IDBResourceIdentifier>::copy(const IDBResourceIdentifier& identifier)
{
    return identifier.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBError>::Type CrossThreadCopierBase<false, false, IDBError>::copy(const IDBError& error)
{
    return error.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBObjectStoreInfo>::Type CrossThreadCopierBase<false, false, IDBObjectStoreInfo>::copy(const IDBObjectStoreInfo& info)
{
    return info.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBIndexInfo>::Type CrossThreadCopierBase<false, false, IDBIndexInfo>::copy(const IDBIndexInfo& info)
{
    return info.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBCursorInfo>::Type CrossThreadCopierBase<false, false, IDBCursorInfo>::copy(const IDBCursorInfo& info)
{
    return info.isolatedCopy();
}

CrossThreadCopierBase<false, false, IDBValue>::Type CrossThreadCopierBase<false, false, IDBValue>::copy(const IDBValue& value)
{
    return value.isolatedCopy();
}

#endif // ENABLE(INDEXED_DATABASE)

// Test CrossThreadCopier using COMPILE_ASSERT.

// Verify that ThreadSafeRefCounted objects get handled correctly.
class CopierThreadSafeRefCountedTest : public ThreadSafeRefCounted<CopierThreadSafeRefCountedTest> {
};

COMPILE_ASSERT((std::is_same<
                  PassRefPtr<CopierThreadSafeRefCountedTest>,
                  CrossThreadCopier<PassRefPtr<CopierThreadSafeRefCountedTest>>::Type
                  >::value),
               PassRefPtrTest);
COMPILE_ASSERT((std::is_same<
                  PassRefPtr<CopierThreadSafeRefCountedTest>,
                  CrossThreadCopier<RefPtr<CopierThreadSafeRefCountedTest>>::Type
                  >::value),
               RefPtrTest);
COMPILE_ASSERT((std::is_same<
                  PassRefPtr<CopierThreadSafeRefCountedTest>,
                  CrossThreadCopier<CopierThreadSafeRefCountedTest*>::Type
                  >::value),
               RawPointerTest);


// Add a generic specialization which will let's us verify that no other template matches.
template<typename T> struct CrossThreadCopierBase<false, false, T> {
    typedef int Type;
};

// Verify that RefCounted objects only match our generic template which exposes Type as int.
class CopierRefCountedTest : public RefCounted<CopierRefCountedTest> {
};

COMPILE_ASSERT((std::is_same<
                  int,
                  CrossThreadCopier<PassRefPtr<CopierRefCountedTest>>::Type
                  >::value),
               PassRefPtrRefCountedTest);

COMPILE_ASSERT((std::is_same<
                  int,
                  CrossThreadCopier<RefPtr<CopierRefCountedTest>>::Type
                  >::value),
               RefPtrRefCountedTest);

COMPILE_ASSERT((std::is_same<
                  int,
                  CrossThreadCopier<CopierRefCountedTest*>::Type
                  >::value),
               RawPointerRefCountedTest);

} // namespace WebCore
