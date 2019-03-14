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
#include "WebDatabaseProvider.h"

#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static HashMap<uint64_t, WebDatabaseProvider*>& databaseProviders()
{
    static NeverDestroyed<HashMap<uint64_t, WebDatabaseProvider*>> databaseProviders;

    return databaseProviders;
}

Ref<WebDatabaseProvider> WebDatabaseProvider::getOrCreate(uint64_t identifier)
{
    auto& slot = databaseProviders().add(identifier, nullptr).iterator->value;
    if (slot)
        return *slot;

    Ref<WebDatabaseProvider> databaseProvider = adoptRef(*new WebDatabaseProvider(identifier));
    slot = databaseProvider.ptr();

    return databaseProvider;
}

WebDatabaseProvider::WebDatabaseProvider(uint64_t identifier)
    : m_identifier(identifier)
{
}

WebDatabaseProvider::~WebDatabaseProvider()
{
    ASSERT(databaseProviders().contains(m_identifier));

    databaseProviders().remove(m_identifier);
}

#if ENABLE(INDEXED_DATABASE)
WebCore::IDBClient::IDBConnectionToServer& WebDatabaseProvider::idbConnectionToServerForSession(const PAL::SessionID& sessionID)
{
    return WebProcess::singleton().ensureNetworkProcessConnection().idbConnectionToServerForSession(sessionID).coreConnectionToServer();
}
#endif

}
