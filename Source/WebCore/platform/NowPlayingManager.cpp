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
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include "MediaSessionManagerCocoa.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NowPlayingManager);

NowPlayingManager::NowPlayingManager() = default;
NowPlayingManager::~NowPlayingManager() = default;

void NowPlayingManager::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    if (RefPtr client = m_client.get())
        client->didReceiveRemoteControlCommand(type, argument);
}

void NowPlayingManager::addClient(NowPlayingManagerClient& client)
{
    m_client = client;
    ensureRemoteCommandListenerCreated();
}

void NowPlayingManager::removeClient(NowPlayingManagerClient& client)
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

    bool shouldUpdateNowPlayingSuppression = [&] {
#if USE(NOW_PLAYING_ACTIVITY_SUPPRESSION)
        if (!m_nowPlayingInfo)
            return true;

        if (m_nowPlayingInfo->isVideo != nowPlayingInfo.isVideo)
            return true;

        if (m_nowPlayingInfo->metadata.sourceApplicationIdentifier != nowPlayingInfo.metadata.sourceApplicationIdentifier)
            return true;
#endif

        return false;
    }();

    m_nowPlayingInfo = nowPlayingInfo;

    // We do not want to send the artwork's image over each time nowPlayingInfo gets updated.
    // So if present we store it once locally. On the receiving end, a null imageData indicates to use the cached image.
    if (!nowPlayingInfo.metadata.artwork)
        m_nowPlayingInfoArtwork = { };
    else if (!m_nowPlayingInfoArtwork || nowPlayingInfo.metadata.artwork->src != m_nowPlayingInfoArtwork->src)
        m_nowPlayingInfoArtwork = ArtworkCache { nowPlayingInfo.metadata.artwork->src, nowPlayingInfo.metadata.artwork->image };
    else
        m_nowPlayingInfo->metadata.artwork->image = nullptr;

    setNowPlayingInfoPrivate(*m_nowPlayingInfo, shouldUpdateNowPlayingSuppression);
    m_setAsNowPlayingApplication = true;
    return true;
}

void NowPlayingManager::setNowPlayingInfoPrivate(const NowPlayingInfo& nowPlayingInfo, bool shouldUpdateNowPlayingSuppression)
{
    setSupportsSeeking(nowPlayingInfo.supportsSeeking);
#if PLATFORM(COCOA)
    if (nowPlayingInfo.metadata.artwork && !nowPlayingInfo.metadata.artwork->image) {
        ASSERT(m_nowPlayingInfoArtwork, "cached value must have been initialized");
        NowPlayingInfo nowPlayingInfoRebuilt = nowPlayingInfo;
        nowPlayingInfoRebuilt.metadata.artwork->image = m_nowPlayingInfoArtwork->image;
        MediaSessionManagerCocoa::setNowPlayingInfo(!m_setAsNowPlayingApplication, shouldUpdateNowPlayingSuppression, nowPlayingInfoRebuilt);
        return;
    }
    MediaSessionManagerCocoa::setNowPlayingInfo(!m_setAsNowPlayingApplication, shouldUpdateNowPlayingSuppression, nowPlayingInfo);
#else
    UNUSED_PARAM(shouldUpdateNowPlayingSuppression);
#endif
}

void NowPlayingManager::setSupportsSeeking(bool supports)
{
    if (RefPtr commandListener = m_remoteCommandListener)
        commandListener->setSupportsSeeking(supports);
}

void NowPlayingManager::addSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    if (RefPtr commandListener = m_remoteCommandListener)
        commandListener->addSupportedCommand(command);
}

void NowPlayingManager::removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    if (RefPtr commandListener = m_remoteCommandListener)
        commandListener->removeSupportedCommand(command);
}

RemoteCommandListener::RemoteCommandsSet NowPlayingManager::supportedCommands() const
{
    if (RefPtr commandListener = m_remoteCommandListener)
        return commandListener->supportedCommands();
    return { };
}

void NowPlayingManager::setSupportedRemoteCommands(const RemoteCommandListener::RemoteCommandsSet& commands)
{
    if (RefPtr commandListener = m_remoteCommandListener)
        commandListener->setSupportedCommands(commands);
}

void NowPlayingManager::updateSupportedCommands()
{
    if (RefPtr commandListener = m_remoteCommandListener)
        commandListener->updateSupportedCommands();
}

void NowPlayingManager::ensureRemoteCommandListenerCreated()
{
    if (!m_remoteCommandListener)
        m_remoteCommandListener = RemoteCommandListener::create(*this);
}

}
