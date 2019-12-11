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
#include "IDBTransactionInfo.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBTransaction.h"

namespace WebCore {

IDBTransactionInfo::IDBTransactionInfo()
{
}

IDBTransactionInfo::IDBTransactionInfo(const IDBResourceIdentifier& identifier)
    : m_identifier(identifier)
{
}

IDBTransactionInfo IDBTransactionInfo::clientTransaction(const IDBClient::IDBConnectionProxy& connectionProxy, const Vector<String>& objectStores, IDBTransactionMode mode)
{
    IDBTransactionInfo result((IDBResourceIdentifier(connectionProxy)));
    result.m_mode = mode;
    result.m_objectStores = objectStores;

    return result;
}

IDBTransactionInfo IDBTransactionInfo::versionChange(const IDBServer::IDBConnectionToClient& connection, const IDBDatabaseInfo& originalDatabaseInfo, uint64_t newVersion)
{
    IDBTransactionInfo result((IDBResourceIdentifier(connection)));
    result.m_mode = IDBTransactionMode::Versionchange;
    result.m_newVersion = newVersion;
    result.m_originalDatabaseInfo = makeUnique<IDBDatabaseInfo>(originalDatabaseInfo);

    return result;
}

IDBTransactionInfo::IDBTransactionInfo(const IDBTransactionInfo& info)
    : m_identifier(info.identifier())
    , m_mode(info.m_mode)
    , m_newVersion(info.m_newVersion)
    , m_objectStores(info.m_objectStores)
{
    if (info.m_originalDatabaseInfo)
        m_originalDatabaseInfo = makeUnique<IDBDatabaseInfo>(*info.m_originalDatabaseInfo);
}

IDBTransactionInfo::IDBTransactionInfo(const IDBTransactionInfo& that, IsolatedCopyTag)
{
    isolatedCopy(that, *this);
}

IDBTransactionInfo IDBTransactionInfo::isolatedCopy() const
{
    return { *this, IsolatedCopy };
}

void IDBTransactionInfo::isolatedCopy(const IDBTransactionInfo& source, IDBTransactionInfo& destination)
{
    destination.m_identifier = source.m_identifier.isolatedCopy();
    destination.m_mode = source.m_mode;
    destination.m_newVersion = source.m_newVersion;

    destination.m_objectStores.reserveCapacity(source.m_objectStores.size());
    for (auto& objectStore : source.m_objectStores)
        destination.m_objectStores.uncheckedAppend(objectStore.isolatedCopy());

    if (source.m_originalDatabaseInfo)
        destination.m_originalDatabaseInfo = makeUnique<IDBDatabaseInfo>(*source.m_originalDatabaseInfo, IDBDatabaseInfo::IsolatedCopy);
}

#if !LOG_DISABLED

String IDBTransactionInfo::loggingString() const
{
    String modeString;
    switch (m_mode) {
    case IDBTransactionMode::Readonly:
        modeString = "readonly"_s;
        break;
    case IDBTransactionMode::Readwrite:
        modeString = "readwrite"_s;
        break;
    case IDBTransactionMode::Versionchange:
        modeString = "versionchange"_s;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    return makeString("Transaction: ", m_identifier.loggingString(), " mode ", modeString, " newVersion ", m_newVersion);
}

#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
