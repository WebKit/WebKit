/*
 * Copyright (C) 2016 Igalia S.L.
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
#include "NetworkSession.h"

#include <WebCore/NetworkStorageSession.h>
#include <WebCore/SoupNetworkSession.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

Ref<NetworkSession> NetworkSession::create(Type type, SessionID sessionID, CustomProtocolManager*)
{
    return adoptRef(*new NetworkSession(type, sessionID, nullptr));
}

NetworkSession& NetworkSession::defaultSession()
{
    ASSERT(isMainThread());
    static NetworkSession* session = &NetworkSession::create(NetworkSession::Type::Normal, SessionID::defaultSessionID(), nullptr).leakRef();
    return *session;
}

NetworkStorageSession& NetworkSession::networkStorageSession() const
{
    auto* storageSession = NetworkStorageSession::storageSession(m_sessionID);
    RELEASE_ASSERT(storageSession);
    return *storageSession;
}

NetworkSession::NetworkSession(Type type, SessionID sessionID, CustomProtocolManager*)
    : m_sessionID(sessionID)
{
}

NetworkSession::~NetworkSession()
{
}

SoupSession* NetworkSession::soupSession() const
{
    return networkStorageSession().soupNetworkSession().soupSession();
}

void NetworkSession::invalidateAndCancel()
{
    for (auto* task : m_dataTaskSet)
        task->invalidateAndCancel();
}

void NetworkSession::clearCredentials()
{
}

} // namespace WebKit
