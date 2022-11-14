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

#pragma once

#include "NowPlayingInfo.h"
#include "PlatformMediaSession.h"
#include "RemoteCommandListener.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class Image;
struct NowPlayingInfo;

class WEBCORE_EXPORT NowPlayingManager : public RemoteCommandListenerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    NowPlayingManager();
    ~NowPlayingManager();

    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument&) final;

    class Client : public CanMakeWeakPtr<Client> {
    public:
        virtual ~Client() = default;
        virtual void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument&) = 0;
    };

    void addSupportedCommand(PlatformMediaSession::RemoteControlCommandType);
    void removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType);
    RemoteCommandListener::RemoteCommandsSet supportedCommands() const;

    void addClient(Client&);
    void removeClient(Client&);

    void clearNowPlayingInfo();
    bool setNowPlayingInfo(const NowPlayingInfo&);
    void setSupportsSeeking(bool);
    void setSupportedRemoteCommands(const RemoteCommandListener::RemoteCommandsSet&);
    void updateSupportedCommands();

private:
    virtual void clearNowPlayingInfoPrivate();
    virtual void setNowPlayingInfoPrivate(const NowPlayingInfo&);
    void ensureRemoteCommandListenerCreated();
    std::unique_ptr<RemoteCommandListener> m_remoteCommandListener;
    WeakPtr<Client> m_client;
    std::optional<NowPlayingInfo> m_nowPlayingInfo;
    struct ArtworkCache {
        String src;
        RefPtr<Image> image;
    };
    std::optional<ArtworkCache> m_nowPlayingInfoArtwork;
    bool m_setAsNowPlayingApplication { false };
};

}
