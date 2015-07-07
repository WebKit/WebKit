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

#if PLATFORM(IOS)

#import <MediaPlayer/MPRemoteCommandCenter.h>
#import <MediaPlayer/MPRemoteCommandEvent.h>
#import <MediaPlayer/MPRemoteCommand.h>
#import <WebCore/SoftLinking.h>

SOFT_LINK_FRAMEWORK(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPRemoteCommandCenter)
SOFT_LINK_CLASS(MediaPlayer, MPSeekCommandEvent)

namespace WebCore {

std::unique_ptr<RemoteCommandListener> RemoteCommandListener::create(RemoteCommandListenerClient& client)
{
    return std::make_unique<RemoteCommandListenerIOS>(client);
}

RemoteCommandListenerIOS::RemoteCommandListenerIOS(RemoteCommandListenerClient& client)
    : RemoteCommandListener(client)
    , m_weakPtrFactory(this)
{
    MPRemoteCommandCenter *center = [getMPRemoteCommandCenterClass() sharedCommandCenter];
    auto weakThis = createWeakPtr();
    
    m_pauseTarget = [[center pauseCommand] addTargetWithHandler:^(MPRemoteCommandEvent *) {
        callOnMainThread([weakThis] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(MediaSession::PauseCommand);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];

    m_playTarget = [[center playCommand] addTargetWithHandler:^(MPRemoteCommandEvent *) {
        callOnMainThread([weakThis] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(MediaSession::PlayCommand);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];

    m_togglePlayPauseTarget = [[center togglePlayPauseCommand] addTargetWithHandler:^(MPRemoteCommandEvent *) {
        callOnMainThread([weakThis] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(MediaSession::TogglePlayPauseCommand);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];

    m_seekBackwardTarget = [[center seekBackwardCommand] addTargetWithHandler:^(MPRemoteCommandEvent *event) {
        ASSERT([event isKindOfClass:getMPSeekCommandEventClass()]);

        MPSeekCommandEvent* seekEvent = static_cast<MPSeekCommandEvent *>(event);
        MediaSession::RemoteControlCommandType command = [seekEvent type] == MPSeekCommandEventTypeBeginSeeking ? MediaSession::BeginSeekingBackwardCommand : MediaSession::EndSeekingBackwardCommand;

        callOnMainThread([weakThis, command] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(command);
        });

        return MPRemoteCommandHandlerStatusSuccess;
    }];
    
    m_seekForwardTarget = [[center seekForwardCommand] addTargetWithHandler:^(MPRemoteCommandEvent *event) {
        ASSERT([event isKindOfClass:getMPSeekCommandEventClass()]);
        MPSeekCommandEvent* seekEvent = static_cast<MPSeekCommandEvent *>(event);

        MediaSession::RemoteControlCommandType command = [seekEvent type] == MPSeekCommandEventTypeBeginSeeking ? MediaSession::BeginSeekingForwardCommand : MediaSession::EndSeekingForwardCommand;

        callOnMainThread([weakThis, command] {
            if (!weakThis)
                return;
            weakThis->m_client.didReceiveRemoteControlCommand(command);
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
}

}

#endif
