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
#include "IDBOpenDBRequestImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBError.h"
#include "IDBResultData.h"
#include "Logging.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBOpenDBRequest> IDBOpenDBRequest::createDeleteRequest(IDBConnectionToServer& connection, ScriptExecutionContext* context, const IDBDatabaseIdentifier& databaseIdentifier)
{
    ASSERT(databaseIdentifier.isValid());
    return adoptRef(*new IDBOpenDBRequest(connection, context, databaseIdentifier, 0));
}

Ref<IDBOpenDBRequest> IDBOpenDBRequest::createOpenRequest(IDBConnectionToServer& connection, ScriptExecutionContext* context, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version)
{
    ASSERT(databaseIdentifier.isValid());
    return adoptRef(*new IDBOpenDBRequest(connection, context, databaseIdentifier, version));
}
    
IDBOpenDBRequest::IDBOpenDBRequest(IDBConnectionToServer& connection, ScriptExecutionContext* context, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version)
    : IDBRequest(connection, context)
    , m_databaseIdentifier(databaseIdentifier)
    , m_version(version)
{
    suspendIfNeeded();
}

IDBOpenDBRequest::~IDBOpenDBRequest()
{
}

void IDBOpenDBRequest::requestCompleted(const IDBResultData& data)
{
    LOG(IndexedDB, "IDBOpenDBRequest::requestCompleted");

    if (!data.error().isNull()) {
        LOG(IndexedDB, "  with error: (%s) '%s'", data.error().name().utf8().data(), data.error().message().utf8().data());
        m_domError = DOMError::create(data.error().name());
        enqueueEvent(Event::create(eventNames().errorEvent, true, true));
    } else
        enqueueEvent(Event::create(eventNames().successEvent, true, true));
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
