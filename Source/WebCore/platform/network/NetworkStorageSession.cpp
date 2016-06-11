/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "NetworkStorageSession.h"

#include "NetworkingContext.h"
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if USE(SOUP)
#include "SoupNetworkSession.h"
#endif

namespace WebCore {

static std::unique_ptr<NetworkStorageSession>& defaultSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session;
    return session;
}

static HashMap<SessionID, NetworkStorageSession*>& sessionsMap()
{
    static NeverDestroyed<HashMap<SessionID, NetworkStorageSession*>> map;
    return map;
}

NetworkStorageSession::NetworkStorageSession(SessionID sessionID)
    : m_sessionID(sessionID)
{
    if (sessionID == SessionID::defaultSessionID())
        return;
    ASSERT(!sessionsMap().contains(sessionID));
    sessionsMap().add(sessionID, this);
}

NetworkStorageSession& NetworkStorageSession::defaultStorageSession()
{
    if (!defaultSession())
        defaultSession() = std::make_unique<NetworkStorageSession>(SessionID::defaultSessionID(), nullptr);
    return *defaultSession();
}

NetworkStorageSession::~NetworkStorageSession()
{
    if (m_sessionID == SessionID::defaultSessionID())
        return;
    ASSERT(sessionsMap().contains(m_sessionID));
    sessionsMap().remove(m_sessionID);
}

NetworkStorageSession& NetworkStorageSession::forSessionID(SessionID sessionID)
{
    if (sessionID == SessionID::defaultSessionID())
        return defaultStorageSession();

    ASSERT(sessionsMap().contains(sessionID));
    return *sessionsMap().get(sessionID);
}

void NetworkStorageSession::replaceDefaultSession(std::unique_ptr<NetworkStorageSession> newDefaultSession)
{
    defaultSession() = WTFMove(newDefaultSession);
}

};
