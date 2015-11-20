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
#include "APISession.h"

#include "NetworkProcessMessages.h"
#include "WebProcessPool.h"
#include <wtf/RunLoop.h>

namespace API {

static uint64_t generateID()
{
    ASSERT(RunLoop::isMain());

    static uint64_t uniqueSessionID = WebCore::SessionID::legacyPrivateSessionID().sessionID();

    return ++uniqueSessionID;
}

Session& Session::defaultSession()
{
    ASSERT(RunLoop::isMain());

    static Session* defaultSession = new Session(WebCore::SessionID::defaultSessionID());
    return *defaultSession;
}

Session::Session()
    : m_sessionID(generateID())
{
}

Session::Session(WebCore::SessionID sessionID)
    : m_sessionID(sessionID)
{
}

Ref<Session> Session::createEphemeral()
{
    // FIXME: support creation of non-default, non-ephemeral sessions
    return adoptRef(*new Session());
}

bool Session::isEphemeral() const
{
    return m_sessionID.isEphemeral();
}

WebCore::SessionID Session::getID() const
{
    return m_sessionID;
}

Session::~Session()
{
    if (m_sessionID.isEphemeral()) {
        for (auto& processPool : WebKit::WebProcessPool::allProcessPools())
            processPool->sendToNetworkingProcess(Messages::NetworkProcess::DestroyPrivateBrowsingSession(m_sessionID));
    }
}

} // namespace API
