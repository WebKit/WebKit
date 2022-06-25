/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "NetworkStorageSessionMap.h"

#include <WebCore/NetworkStorageSession.h>
#include <pal/SessionID.h>
#include <wtf/MainThread.h>
#include <wtf/ProcessID.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/UUID.h>
#include <wtf/text/StringConcatenateNumbers.h>

static std::unique_ptr<WebCore::NetworkStorageSession>& defaultNetworkStorageSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<std::unique_ptr<WebCore::NetworkStorageSession>> session;
    return session;
}

static HashMap<PAL::SessionID, std::unique_ptr<WebCore::NetworkStorageSession>>& globalSessionMap()
{
    static NeverDestroyed<HashMap<PAL::SessionID, std::unique_ptr<WebCore::NetworkStorageSession>>> map;
    return map;
}

WebCore::NetworkStorageSession* NetworkStorageSessionMap::storageSession(PAL::SessionID sessionID)
{
    if (sessionID == PAL::SessionID::defaultSessionID())
        return &defaultStorageSession();
    return globalSessionMap().get(sessionID);
}

WebCore::NetworkStorageSession& NetworkStorageSessionMap::defaultStorageSession()
{
    if (!defaultNetworkStorageSession())
        defaultNetworkStorageSession() = makeUnique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID());
    return *defaultNetworkStorageSession();
}

void NetworkStorageSessionMap::switchToNewTestingSession()
{
#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    // Session name should be short enough for shared memory region name to be under the limit, otherwise sandbox rules won't work (see <rdar://problem/13642852>).
    auto session = WebCore::createPrivateStorageSession(makeString("WebKit Test-"_s, getCurrentProcessID()).createCFString().get());

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage;
    if (WebCore::NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (session)
            cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, session.get()));
    }

    defaultNetworkStorageSession() = makeUnique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID(), WTFMove(session), WTFMove(cookieStorage));
#endif
}

void NetworkStorageSessionMap::ensureSession(PAL::SessionID sessionID, const String& identifierBase)
{
#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    auto addResult = globalSessionMap().add(sessionID, nullptr);
    if (!addResult.isNewEntry)
        return;

    auto identifier = makeString(identifierBase, ".PrivateBrowsing."_s, UUID::createVersion4()).createCFString();

    RetainPtr<CFURLStorageSessionRef> storageSession;
    if (sessionID.isEphemeral())
        storageSession = WebCore::createPrivateStorageSession(identifier.get());
    else
        storageSession = WebCore::NetworkStorageSession::createCFStorageSessionForIdentifier(identifier.get());

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage;
    if (WebCore::NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (storageSession)
            cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    }

    addResult.iterator->value = makeUnique<WebCore::NetworkStorageSession>(sessionID, WTFMove(storageSession), WTFMove(cookieStorage));

#elif USE(CURL)
    globalSessionMap().ensure(sessionID, [sessionID] {
        return makeUnique<WebCore::NetworkStorageSession>(sessionID);
    });
#endif
}

void NetworkStorageSessionMap::destroySession(PAL::SessionID sessionID)
{
    globalSessionMap().remove(sessionID);
}
