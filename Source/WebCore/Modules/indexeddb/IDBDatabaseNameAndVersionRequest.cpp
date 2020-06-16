/*
 * Copyright (C) 2020 Darryl Pogue (darryl@dpogue.ca)
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
#include "IDBDatabaseNameAndVersionRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionProxy.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBDatabaseNameAndVersionRequest);

Ref<IDBDatabaseNameAndVersionRequest> IDBDatabaseNameAndVersionRequest::create(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, InfoCallback&& callback)
{
    return adoptRef(*new IDBDatabaseNameAndVersionRequest(context, connectionProxy, WTFMove(callback)));
}

IDBDatabaseNameAndVersionRequest::IDBDatabaseNameAndVersionRequest(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, InfoCallback&& callback)
    : IDBActiveDOMObject(&context)
    , m_connectionProxy(connectionProxy)
    , m_resourceIdentifier(connectionProxy)
    , m_callback(WTFMove(callback))
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    suspendIfNeeded();
}

IDBDatabaseNameAndVersionRequest::~IDBDatabaseNameAndVersionRequest()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
}

void IDBDatabaseNameAndVersionRequest::complete(Optional<Vector<IDBDatabaseNameAndVersion>>&& databases)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (auto callback = WTFMove(m_callback))
        callback(WTFMove(databases));
}

const char* IDBDatabaseNameAndVersionRequest::activeDOMObjectName() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    return "IDBDatabaseNameAndVersionRequest";
}

bool IDBDatabaseNameAndVersionRequest::virtualHasPendingActivity() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()) || Thread::mayBeGCThread());
    return true;
}

void IDBDatabaseNameAndVersionRequest::stop()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
