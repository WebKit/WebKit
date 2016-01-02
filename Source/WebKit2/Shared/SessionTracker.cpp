/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "SessionTracker.h"

#include "NetworkSession.h"
#include <WebCore/NetworkStorageSession.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace WebKit {

static HashMap<SessionID, std::unique_ptr<NetworkStorageSession>>& staticStorageSessionMap()
{
    ASSERT(RunLoop::isMain());

    static NeverDestroyed<HashMap<SessionID, std::unique_ptr<NetworkStorageSession>>> map;
    return map;
}

static HashMap<const NetworkStorageSession*, SessionID>& storageSessionToID()
{
    ASSERT(RunLoop::isMain());

    static NeverDestroyed<HashMap<const NetworkStorageSession*, SessionID>> map;
    return map;
}

static String& identifierBase()
{
    ASSERT(RunLoop::isMain());

    static NeverDestroyed<String> base;
    return base;
}

const HashMap<SessionID, std::unique_ptr<NetworkStorageSession>>& SessionTracker::storageSessionMap()
{
    return staticStorageSessionMap();
}

const String& SessionTracker::getIdentifierBase()
{
    return identifierBase();
}

NetworkStorageSession* SessionTracker::storageSession(SessionID sessionID)
{
    if (sessionID == SessionID::defaultSessionID())
        return &NetworkStorageSession::defaultStorageSession();
    return staticStorageSessionMap().get(sessionID);
}

#if USE(NETWORK_SESSION)
static HashMap<SessionID, std::unique_ptr<NetworkSession>>& staticSessionMap()
{
    ASSERT(RunLoop::isMain());
    
    static NeverDestroyed<HashMap<SessionID, std::unique_ptr<NetworkSession>>> map;
    return map;
}

NetworkSession* SessionTracker::networkSession(SessionID sessionID)
{
    if (sessionID == SessionID::defaultSessionID())
        return &NetworkSession::defaultSession();
    return staticSessionMap().get(sessionID);
}
#endif

SessionID SessionTracker::sessionID(const NetworkStorageSession& session)
{
    if (&session == &NetworkStorageSession::defaultStorageSession())
        return SessionID::defaultSessionID();
    return storageSessionToID().get(&session);
}

void SessionTracker::setSession(SessionID sessionID, std::unique_ptr<NetworkStorageSession> storageSession
#if USE(NETWORK_SESSION)
    , std::unique_ptr<NetworkSession> session
#endif
)
{
    ASSERT(storageSession);
    ASSERT(sessionID != SessionID::defaultSessionID());
    storageSessionToID().set(storageSession.get(), sessionID);
    staticStorageSessionMap().set(sessionID, WTFMove(storageSession));
#if USE(NETWORK_SESSION)
    ASSERT(session);
    staticSessionMap().set(sessionID, WTFMove(session));
#endif
}

void SessionTracker::destroySession(SessionID sessionID)
{
    ASSERT(RunLoop::isMain());
    if (staticStorageSessionMap().contains(sessionID)) {
#if USE(NETWORK_SESSION)
        ASSERT_WITH_MESSAGE(staticSessionMap().contains(sessionID), "NetworkSessions and NetworkStorageSessions should always be created, deleted, and managed as pairs");
        staticSessionMap().remove(sessionID);
#endif
        storageSessionToID().remove(storageSession(sessionID));
        staticStorageSessionMap().remove(sessionID);
    }
}

void SessionTracker::setIdentifierBase(const String& identifier)
{
    ASSERT(RunLoop::isMain());

    identifierBase() = identifier;
}

} // namespace WebKit
