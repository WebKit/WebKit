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

#import "config.h"
#import "RemoteCommandListenerIOS.h"

#if PLATFORM(IOS_FAMILY) && HAVE(MEDIA_PLAYER)

#import <MediaPlayer/MPRemoteCommand.h>
#import <MediaPlayer/MPRemoteCommandCenter.h>
#import <MediaPlayer/MPRemoteCommandEvent.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPRemoteCommandCenter)
SOFT_LINK_CLASS(MediaPlayer, MPSeekCommandEvent)
SOFT_LINK_CLASS(MediaPlayer, MPChangePlaybackPositionCommandEvent)

namespace WebCore {

std::unique_ptr<RemoteCommandListener> RemoteCommandListener::create(RemoteCommandListenerClient& client)
{
    if (!MediaPlayerLibrary())
        return nullptr;

    return makeUnique<RemoteCommandListenerIOS>(client);
}

RemoteCommandListenerIOS::RemoteCommandListenerIOS(RemoteCommandListenerClient& client)
    : RemoteCommandListener(client)
{
    MPRemoteCommandCenter *center = [getMPRemoteCommandCenterClass() sharedCommandCenter];
    auto weakThis = makeWeakPtr(*this);
    
    m_pauseTarget = [[center pauseCommand] addTargetWithHandler:^(MPRemoteCommandEvent *) {
        callOnMainThread([weakThis] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(PlatformMediaSession::PauseCommand, nullptr);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];

    m_playTarget = [[center playCommand] addTargetWithHandler:^(MPRemoteCommandEvent *) {
        callOnMainThread([weakThis] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(PlatformMediaSession::PlayCommand, nullptr);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];

    m_togglePlayPauseTarget = [[center togglePlayPauseCommand] addTargetWithHandler:^(MPRemoteCommandEvent *) {
        callOnMainThread([weakThis] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(PlatformMediaSession::TogglePlayPauseCommand, nullptr);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];

    m_seekBackwardTarget = [[center seekBackwardCommand] addTargetWithHandler:^(MPRemoteCommandEvent *event) {
        ASSERT([event isKindOfClass:getMPSeekCommandEventClass()]);

        MPSeekCommandEvent* seekEvent = static_cast<MPSeekCommandEvent *>(event);
        PlatformMediaSession::RemoteControlCommandType command = [seekEvent type] == MPSeekCommandEventTypeBeginSeeking ? PlatformMediaSession::BeginSeekingBackwardCommand : PlatformMediaSession::EndSeekingBackwardCommand;

        callOnMainThread([weakThis, command] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(command, nullptr);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];
    
    m_seekForwardTarget = [[center seekForwardCommand] addTargetWithHandler:^(MPRemoteCommandEvent *event) {
        ASSERT([event isKindOfClass:getMPSeekCommandEventClass()]);
        MPSeekCommandEvent* seekEvent = static_cast<MPSeekCommandEvent *>(event);

        PlatformMediaSession::RemoteControlCommandType command = [seekEvent type] == MPSeekCommandEventTypeBeginSeeking ? PlatformMediaSession::BeginSeekingForwardCommand : PlatformMediaSession::EndSeekingForwardCommand;

        callOnMainThread([weakThis, command] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(command, nullptr);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];

    m_seekToTimeTarget = [[center changePlaybackPositionCommand] addTargetWithHandler:^(MPRemoteCommandEvent *event) {
        ASSERT([event isKindOfClass:getMPChangePlaybackPositionCommandEventClass()]);

        if (!client.supportsSeeking())
            return MPRemoteCommandHandlerStatusCommandFailed;

        MPChangePlaybackPositionCommandEvent* seekEvent = static_cast<MPChangePlaybackPositionCommandEvent *>(event);
        PlatformMediaSession::RemoteCommandArgument argument { [seekEvent positionTime] };

        callOnMainThread([weakThis, argument] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(PlatformMediaSession::SeekToPlaybackPositionCommand, &argument);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];
}

RemoteCommandListenerIOS::~RemoteCommandListenerIOS()
{
    MPRemoteCommandCenter *center = [getMPRemoteCommandCenterClass() sharedCommandCenter];
    [[center pauseCommand] removeTarget:m_pauseTarget.get()];
    [[center playCommand] removeTarget:m_playTarget.get()];
    [[center togglePlayPauseCommand] removeTarget:m_togglePlayPauseTarget.get()];
    [[center seekForwardCommand] removeTarget:m_seekForwardTarget.get()];
    [[center seekBackwardCommand] removeTarget:m_seekBackwardTarget.get()];
    [[center changePlaybackPositionCommand] removeTarget:m_seekToTimeTarget.get()];
}

void RemoteCommandListenerIOS::updateSupportedCommands()
{
    [[[getMPRemoteCommandCenterClass() sharedCommandCenter] changePlaybackPositionCommand] setEnabled:!!client().supportsSeeking()];
}

}

#endif // PLATFORM(IOS_FAMILY) && HAVE(MEDIA_PLAYER)
