/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "NowPlayingManager.h"

#if PLATFORM(COCOA)
#include "MediaSessionManagerCocoa.h"
#endif

namespace WebCore {

NowPlayingManager::NowPlayingManager() = default;
NowPlayingManager::~NowPlayingManager() = default;

void NowPlayingManager::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument* argument)
{
    ASSERT(m_nowPlayingInfo);

    if (!m_client)
        return;

    Optional<double> value;
    if (argument)
        value = argument->asDouble;
    m_client->didReceiveRemoteControlCommand(type, value);
}

bool NowPlayingManager::supportsSeeking() const
{
    return m_nowPlayingInfo && m_nowPlayingInfo->supportsSeeking;
}

void NowPlayingManager::clearNowPlayingInfoClient(Client& client)
{
    if (m_client.get() != &client)
        return;

    m_remoteCommandListener = nullptr;
    m_client.clear();
    m_nowPlayingInfo = { };

#if PLATFORM(COCOA)
    MediaSessionManagerCocoa::clearNowPlayingInfo();
#endif
}

void NowPlayingManager::setNowPlayingInfo(Client& client, NowPlayingInfo&& nowPlayingInfo)
{
    if (!m_remoteCommandListener)
        m_remoteCommandListener = RemoteCommandListener::create(*this);

    m_client = makeWeakPtr(client);
    m_nowPlayingInfo = WTFMove(nowPlayingInfo);

#if PLATFORM(COCOA)
    MediaSessionManagerCocoa::setNowPlayingInfo(!m_nowPlayingInfo, *m_nowPlayingInfo);
#endif
}

}
