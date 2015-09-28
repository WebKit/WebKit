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
#include "IDBRequestIdentifier.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"
#include "IDBRequestImpl.h"
#include <wtf/MainThread.h>

namespace WebCore {

static uint64_t nextRequestNumber()
{
    ASSERT(isMainThread());
    static uint64_t currentNumber = 0;
    return ++currentNumber;
}

IDBRequestIdentifier::IDBRequestIdentifier()
    : m_requestNumber(nextRequestNumber())
{
}

IDBRequestIdentifier::IDBRequestIdentifier(const IDBClient::IDBConnectionToServer& connection)
    : m_idbClientServerConnectionNumber(connection.identifier())
    , m_requestNumber(nextRequestNumber())
{
}

IDBRequestIdentifier::IDBRequestIdentifier(const IDBClient::IDBConnectionToServer& connection, const IDBClient::IDBRequest& request)
    : m_idbClientServerConnectionNumber(connection.identifier())
    , m_requestNumber(request.requestIdentifier().m_requestNumber)
{
}

IDBRequestIdentifier IDBRequestIdentifier::emptyValue()
{
    IDBRequestIdentifier result;
    result.m_idbClientServerConnectionNumber = 0;
    result.m_requestNumber = 0;
    return WTF::move(result);
}

IDBRequestIdentifier IDBRequestIdentifier::deletedValue()
{
    IDBRequestIdentifier result;
    result.m_idbClientServerConnectionNumber = std::numeric_limits<int64_t>::max();
    result.m_requestNumber = std::numeric_limits<int64_t>::max();
    return WTF::move(result);
}

bool IDBRequestIdentifier::isHashTableDeletedValue() const
{
    return m_idbClientServerConnectionNumber == std::numeric_limits<int64_t>::max()
        && m_requestNumber == std::numeric_limits<int64_t>::max();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
