/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBOpenRequestData.h"

#include "IDBConnectionProxy.h"
#include "IDBDatabaseIdentifier.h"

namespace WebCore {

IDBOpenRequestData::IDBOpenRequestData(const IDBClient::IDBConnectionProxy& connectionProxy, const IDBOpenDBRequest& request)
    : m_serverConnectionIdentifier(connectionProxy.serverConnectionIdentifier())
    , m_requestIdentifier(IDBResourceIdentifier { connectionProxy, request })
    , m_databaseIdentifier(request.databaseIdentifier())
    , m_requestedVersion(request.version())
    , m_requestType(request.requestType())
{
}

IDBOpenRequestData::IDBOpenRequestData(IDBConnectionIdentifier serverConnectionIdentifier, IDBResourceIdentifier requestIdentifier, IDBDatabaseIdentifier&& databaseIdentifier, uint64_t requestedVersion, IndexedDB::RequestType type)
    : m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_requestIdentifier(requestIdentifier)
    , m_databaseIdentifier(databaseIdentifier)
    , m_requestedVersion(requestedVersion)
    , m_requestType(type)
{
}

IDBOpenRequestData IDBOpenRequestData::isolatedCopy() const
{
    return IDBOpenRequestData {
        m_serverConnectionIdentifier,
        m_requestIdentifier,
        m_databaseIdentifier.isolatedCopy(),
        m_requestedVersion,
        m_requestType
    };
}

} // namespace WebCore
