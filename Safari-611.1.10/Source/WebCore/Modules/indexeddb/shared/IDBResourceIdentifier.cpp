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

#include "IDBConnectionToClient.h"
#include "IDBConnectionToServer.h"
#include "IDBRequest.h"
#include <wtf/MainThread.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static uint64_t nextClientResourceNumber()
{
    static std::atomic<uint64_t> currentNumber(1);
    return currentNumber += 2;
}

static uint64_t nextServerResourceNumber()
{
    static uint64_t currentNumber = 0;
    return currentNumber += 2;
}

IDBResourceIdentifier::IDBResourceIdentifier()
{
}

IDBResourceIdentifier::IDBResourceIdentifier(IDBConnectionIdentifier connectionIdentifier, uint64_t resourceIdentifier)
    : m_idbConnectionIdentifier(connectionIdentifier)
    , m_resourceNumber(resourceIdentifier)
{
}

IDBResourceIdentifier::IDBResourceIdentifier(const IDBClient::IDBConnectionProxy& connectionProxy)
    : m_idbConnectionIdentifier(connectionProxy.serverConnectionIdentifier())
    , m_resourceNumber(nextClientResourceNumber())
{
}

IDBResourceIdentifier::IDBResourceIdentifier(const IDBClient::IDBConnectionProxy& connectionProxy, const IDBRequest& request)
    : m_idbConnectionIdentifier(connectionProxy.serverConnectionIdentifier())
    , m_resourceNumber(request.resourceIdentifier().m_resourceNumber)
{
}

IDBResourceIdentifier::IDBResourceIdentifier(const IDBServer::IDBConnectionToClient& connection)
    : m_idbConnectionIdentifier(connection.identifier())
    , m_resourceNumber(nextServerResourceNumber())
{
}

IDBResourceIdentifier IDBResourceIdentifier::isolatedCopy() const
{
    return IDBResourceIdentifier(m_idbConnectionIdentifier, m_resourceNumber);
}

IDBResourceIdentifier IDBResourceIdentifier::emptyValue()
{
    return IDBResourceIdentifier({ }, 0);
}

IDBResourceIdentifier IDBResourceIdentifier::deletedValue()
{
    return IDBResourceIdentifier(IDBConnectionIdentifier { WTF::HashTableDeletedValue }, std::numeric_limits<uint64_t>::max());
}

bool IDBResourceIdentifier::isHashTableDeletedValue() const
{
    return m_idbConnectionIdentifier.isHashTableDeletedValue() && m_resourceNumber == std::numeric_limits<uint64_t>::max();
}

#if !LOG_DISABLED

String IDBResourceIdentifier::loggingString() const
{
    return makeString('<', m_idbConnectionIdentifier.toUInt64(), ", ", m_resourceNumber, '>');
}

#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
