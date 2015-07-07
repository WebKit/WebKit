/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef IDBPendingOpenCall_h
#define IDBPendingOpenCall_h

#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBCallbacks;
class IDBDatabaseCallbacks;

class IDBPendingOpenCall {
public:
    IDBPendingOpenCall(IDBCallbacks& callbacks, IDBDatabaseCallbacks& databaseCallbacks, int64_t transactionId, uint64_t version)
        : m_callbacks(&callbacks)
        , m_databaseCallbacks(&databaseCallbacks)
        , m_version(version)
        , m_transactionId(transactionId)
    {
    }

    IDBCallbacks* callbacks() { return m_callbacks.get(); }
    IDBDatabaseCallbacks* databaseCallbacks() { return m_databaseCallbacks.get(); }
    uint64_t version() { return m_version; }
    int64_t transactionId() const { return m_transactionId; }

private:
    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
    uint64_t m_version;
    const int64_t m_transactionId;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBPendingOpenCall_h
