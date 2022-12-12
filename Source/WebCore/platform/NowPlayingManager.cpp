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

void NowPlayingManager::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    if (m_client)
        m_client->didReceiveRemoteControlCommand(type, argument);
}

void NowPlayingManager::addClient(Client& client)
{
    m_client = client;
    ensureRemoteCommandListenerCreated();
}

void NowPlayingManager::removeClient(Client& client)
{
    if (m_client.get() != &client)
        return;

    m_remoteCommandListener = nullptr;
    m_client.clear();
    m_nowPlayingInfo = { };

    clearNowPlayingInfo();
}

void NowPlayingManager::clearNowPlayingInfo()
{
    clearNowPlayingInfoPrivate();
    m_setAsNowPlayingApplication = false;
}

void NowPlayingManager::clearNowPlayingInfoPrivate()
{
#if PLATFORM(COCOA)
    MediaSessionManagerCocoa::clearNowPlayingInfo();
#endif
}

bool NowPlayingManager::setNowPlayingInfo(const NowPlayingInfo& nowPlayingInfo)
{
    if (m_nowPlayingInfo && *m_nowPlayingInfo == nowPlayingInfo)
        return false;
    m_nowPlayingInfo = nowPlayingInfo;
    // We do not want to send the artwork's image over each time nowPlayingInfo gets updated.
    // So if present we store it once locally. On the receiving end, a null imageData indicates to use the cached image.
    if (!nowPlayingInfo.artwork)
        m_nowPlayingInfoArtwork = { };
    else if (!m_nowPlayingInfoArtwork || nowPlayingInfo.artwork->src != m_nowPlayingInfoArtwork->src)
        m_nowPlayingInfoArtwork = ArtworkCache { nowPlayingInfo.artwork->src, nowPlayingInfo.artwork->image };
    else
        m_nowPlayingInfo->artwork->image = nullptr;

    setNowPlayingInfoPrivate(*m_nowPlayingInfo);
    m_setAsNowPlayingApplication = true;
    return true;
}

void NowPlayingManager::setNowPlayingInfoPrivate(const NowPlayingInfo& nowPlayingInfo)
{
    setSupportsSeeking(nowPlayingInfo.supportsSeeking);
#if PLATFORM(COCOA)
    if (nowPlayingInfo.artwork && !nowPlayingInfo.artwork->image) {
        ASSERT(m_nowPlayingInfoArtwork, "cached value must have been initialized");
        NowPlayingInfo nowPlayingInfoRebuilt = nowPlayingInfo;
        nowPlayingInfoRebuilt.artwork->image = m_nowPlayingInfoArtwork->image;
        MediaSessionManagerCocoa::setNowPlayingInfo(!m_setAsNowPlayingApplication, nowPlayingInfoRebuilt);
        return;
    }
    MediaSessionManagerCocoa::setNowPlayingInfo(!m_setAsNowPlayingApplication, nowPlayingInfo);
#else
    (void)nowPlayingInfo;
#endif
}

void NowPlayingManager::setSupportsSeeking(bool supports)
{
    if (m_remoteCommandListener)
        m_remoteCommandListener->setSupportsSeeking(supports);
}

void NowPlayingManager::addSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    if (m_remoteCommandListener)
        m_remoteCommandListener->addSupportedCommand(command);
}

void NowPlayingManager::removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    if (m_remoteCommandListener)
        m_remoteCommandListener->removeSupportedCommand(command);
}

RemoteCommandListener::RemoteCommandsSet NowPlayingManager::supportedCommands() const
{
    if (!m_remoteCommandListener)
        return { };
    return m_remoteCommandListener->supportedCommands();
}

void NowPlayingManager::setSupportedRemoteCommands(const RemoteCommandListener::RemoteCommandsSet& commands)
{
    if (m_remoteCommandListener)
        m_remoteCommandListener->setSupportedCommands(commands);
}

void NowPlayingManager::updateSupportedCommands()
{
    if (m_remoteCommandListener)
        m_remoteCommandListener->updateSupportedCommands();
}

void NowPlayingManager::ensureRemoteCommandListenerCreated()
{
    if (!m_remoteCommandListener)
        m_remoteCommandListener = RemoteCommandListener::create(*this);
}

}
