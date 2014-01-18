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

#include <wtf/MainThread.h>

namespace API {

static uint64_t generateID(bool isEphemeral)
{
    ASSERT(isMainThread());

    static uint64_t uniqueSessionID = WebKit::SessionTracker::legacyPrivateSessionID;
    ASSERT(isEphemeral);
    return ++uniqueSessionID;
}

Session& Session::defaultSession()
{
    ASSERT(isMainThread());

    static Session* defaultSession = new Session(false, WebKit::SessionTracker::defaultSessionID);
    return *defaultSession;
}

Session& Session::legacyPrivateSession()
{
    ASSERT(isMainThread());

    static Session* legacyPrivateSession = new Session(true, WebKit::SessionTracker::legacyPrivateSessionID);
    return *legacyPrivateSession;
}

Session::Session(bool isEphemeral)
    : m_isEphemeral(isEphemeral)
    , m_sessionID(generateID(isEphemeral))
{
}

Session::Session(bool isEphemeral, uint64_t sessionID)
    : m_isEphemeral(isEphemeral)
    , m_sessionID(sessionID)
{
}

PassRefPtr<Session> Session::create(bool isEphemeral)
{
    // FIXME: support creation of non-default, non-ephemeral sessions
    return adoptRef(new Session(isEphemeral));
}

bool Session::isEphemeral() const
{
    return m_isEphemeral;
}

uint64_t Session::getID() const
{
    return m_sessionID;
}

Session::~Session()
{
}

} // namespace API
