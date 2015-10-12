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

namespace WebCore {

IDBTransactionInfo::IDBTransactionInfo(const IDBResourceIdentifier& identifier)
    : m_identifier(identifier)
{
}

IDBTransactionInfo IDBTransactionInfo::versionChange(const IDBServer::IDBConnectionToClient& connection, uint64_t newVersion)
{
    IDBTransactionInfo result((IDBResourceIdentifier(connection)));
    result.m_mode = IndexedDB::TransactionMode::VersionChange;
    result.m_newVersion = newVersion;

    return WTF::move(result);
}

IDBTransactionInfo IDBTransactionInfo::isolatedCopy() const
{
    IDBTransactionInfo result(m_identifier);
    result.m_mode = m_mode;
    result.m_newVersion = m_newVersion;

    return WTF::move(result);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
