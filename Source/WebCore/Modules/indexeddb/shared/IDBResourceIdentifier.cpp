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
#include "IDBResourceIdentifier.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"
#include "IDBRequestImpl.h"
#include <wtf/MainThread.h>

namespace WebCore {

static uint64_t nextResourceNumber()
{
    ASSERT(isMainThread());
    static uint64_t currentNumber = 0;
    return ++currentNumber;
}

IDBResourceIdentifier::IDBResourceIdentifier()
    : m_resourceNumber(nextResourceNumber())
{
}

IDBResourceIdentifier::IDBResourceIdentifier(const IDBClient::IDBConnectionToServer& connection)
    : m_idbConnectionIdentifier(connection.identifier())
    , m_resourceNumber(nextResourceNumber())
{
}

IDBResourceIdentifier::IDBResourceIdentifier(const IDBClient::IDBConnectionToServer& connection, const IDBClient::IDBRequest& request)
    : m_idbConnectionIdentifier(connection.identifier())
    , m_resourceNumber(request.resourceIdentifier().m_resourceNumber)
{
}

IDBResourceIdentifier IDBResourceIdentifier::emptyValue()
{
    IDBResourceIdentifier result;
    result.m_idbConnectionIdentifier = 0;
    result.m_resourceNumber = 0;
    return WTF::move(result);
}

IDBResourceIdentifier IDBResourceIdentifier::deletedValue()
{
    IDBResourceIdentifier result;
    result.m_idbConnectionIdentifier = std::numeric_limits<uint64_t>::max();
    result.m_resourceNumber = std::numeric_limits<uint64_t>::max();
    return WTF::move(result);
}

bool IDBResourceIdentifier::isHashTableDeletedValue() const
{
    return m_idbConnectionIdentifier == std::numeric_limits<uint64_t>::max()
        && m_resourceNumber == std::numeric_limits<uint64_t>::max();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
