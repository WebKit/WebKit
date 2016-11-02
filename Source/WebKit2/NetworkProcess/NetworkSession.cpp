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
#include "NetworkSession.h"

#if USE(NETWORK_SESSION)

#include "NetworkDataTask.h"
#include <WebCore/NetworkStorageSession.h>
#include <wtf/MainThread.h>

#if PLATFORM(COCOA)
#include "NetworkSessionCocoa.h"
#endif
#if USE(SOUP)
#include "NetworkSessionSoup.h"
#endif


using namespace WebCore;

namespace WebKit {

Ref<NetworkSession> NetworkSession::create(SessionID sessionID, CustomProtocolManager* customProtocolManager)
{
#if PLATFORM(COCOA)
    return NetworkSessionCocoa::create(sessionID, customProtocolManager);
#endif
#if USE(SOUP)
    UNUSED_PARAM(customProtocolManager);
    return NetworkSessionSoup::create(sessionID);
#endif
}

NetworkSession& NetworkSession::defaultSession()
{
#if PLATFORM(COCOA)
    return NetworkSessionCocoa::defaultSession();
#else
    ASSERT(isMainThread());
    static NetworkSession* session = &NetworkSession::create(SessionID::defaultSessionID()).leakRef();
    return *session;
#endif
}

NetworkStorageSession& NetworkSession::networkStorageSession() const
{
    auto* storageSession = NetworkStorageSession::storageSession(m_sessionID);
    RELEASE_ASSERT(storageSession);
    return *storageSession;
}

NetworkSession::NetworkSession(SessionID sessionID)
    : m_sessionID(sessionID)
{
}

NetworkSession::~NetworkSession()
{
}

void NetworkSession::invalidateAndCancel()
{
    for (auto* task : m_dataTaskSet)
        task->invalidateAndCancel();
}

} // namespace WebKit

#endif // USE(NETWORK_SESSION)
