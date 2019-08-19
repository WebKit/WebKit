/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "RemoteCommandListenerMac.h"

#if PLATFORM(MAC)

#import "Logging.h"
#import <wtf/MainThread.h>

#import "MediaRemoteSoftLink.h"

namespace WebCore {

std::unique_ptr<RemoteCommandListener> RemoteCommandListener::create(RemoteCommandListenerClient& client)
{
    return makeUnique<RemoteCommandListenerMac>(client);
}

void RemoteCommandListenerMac::updateSupportedCommands()
{
#if USE(MEDIAREMOTE)
    if (!isMediaRemoteFrameworkAvailable())
        return;

    static const MRMediaRemoteCommand supportedCommands[] = {
        MRMediaRemoteCommandPlay,
        MRMediaRemoteCommandPause,
        MRMediaRemoteCommandTogglePlayPause,
        MRMediaRemoteCommandBeginFastForward,
        MRMediaRemoteCommandEndFastForward,
        MRMediaRemoteCommandBeginRewind,
        MRMediaRemoteCommandEndRewind,
        MRMediaRemoteCommandSeekToPlaybackPosition,
    };

    auto commandInfoArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, sizeof(supportedCommands) / sizeof(MRMediaRemoteCommand), &kCFTypeArrayCallBacks));

    for (auto command : supportedCommands) {
        auto commandInfo = adoptCF(MRMediaRemoteCommandInfoCreate(kCFAllocatorDefault));
        MRMediaRemoteCommandInfoSetCommand(commandInfo.get(), command);
        MRMediaRemoteCommandInfoSetEnabled(commandInfo.get(), true);
        CFArrayAppendValue(commandInfoArray.get(), commandInfo.get());
    }

    auto seekCommandInfo = adoptCF(MRMediaRemoteCommandInfoCreate(kCFAllocatorDefault));
    MRMediaRemoteCommandInfoSetCommand(seekCommandInfo.get(), MRMediaRemoteCommandSeekToPlaybackPosition);
    MRMediaRemoteCommandInfoSetEnabled(seekCommandInfo.get(), client().supportsSeeking());
    CFArrayAppendValue(commandInfoArray.get(), seekCommandInfo.get());

    MRMediaRemoteSetSupportedCommands(commandInfoArray.get(), MRMediaRemoteGetLocalOrigin(), nullptr, nullptr);
#endif // USE(MEDIAREMOTE)
}

RemoteCommandListenerMac::RemoteCommandListenerMac(RemoteCommandListenerClient& client)
    : RemoteCommandListener(client)
{
#if USE(MEDIAREMOTE)
    if (!isMediaRemoteFrameworkAvailable())
        return;

    updateSupportedCommands();

    auto weakThis = makeWeakPtr(*this);
    m_commandHandler = MRMediaRemoteAddAsyncCommandHandlerBlock(^(MRMediaRemoteCommand command, CFDictionaryRef options, void(^completion)(CFArrayRef)) {

        LOG(Media, "RemoteCommandListenerMac::RemoteCommandListenerMac - received command %u", command);

        PlatformMediaSession::RemoteControlCommandType platformCommand { PlatformMediaSession::NoCommand };
        PlatformMediaSession::RemoteCommandArgument argument { 0 };
        MRMediaRemoteCommandHandlerStatus status = MRMediaRemoteCommandHandlerStatusSuccess;

        switch (command) {
        case MRMediaRemoteCommandPlay:
            platformCommand = PlatformMediaSession::PlayCommand;
            break;
        case MRMediaRemoteCommandPause:
            platformCommand = PlatformMediaSession::PauseCommand;
            break;
        case MRMediaRemoteCommandStop:
            platformCommand = PlatformMediaSession::StopCommand;
            break;
        case MRMediaRemoteCommandTogglePlayPause:
            platformCommand = PlatformMediaSession::TogglePlayPauseCommand;
            break;
        case MRMediaRemoteCommandBeginFastForward:
            platformCommand = PlatformMediaSession::BeginSeekingForwardCommand;
            break;
        case MRMediaRemoteCommandEndFastForward:
            platformCommand = PlatformMediaSession::EndSeekingForwardCommand;
            break;
        case MRMediaRemoteCommandBeginRewind:
            platformCommand = PlatformMediaSession::BeginSeekingBackwardCommand;
            break;
        case MRMediaRemoteCommandEndRewind:
            platformCommand = PlatformMediaSession::EndSeekingBackwardCommand;
            break;
        case MRMediaRemoteCommandSeekToPlaybackPosition: {
            if (!client.supportsSeeking()) {
                status = MRMediaRemoteCommandHandlerStatusCommandFailed;
                break;
            }

            CFNumberRef positionRef = static_cast<CFNumberRef>(CFDictionaryGetValue(options, kMRMediaRemoteOptionPlaybackPosition));
            if (!positionRef) {
                status = MRMediaRemoteCommandHandlerStatusCommandFailed;
                break;
            }

            CFNumberGetValue(positionRef, kCFNumberDoubleType, &argument.asDouble);
            platformCommand = PlatformMediaSession::SeekToPlaybackPositionCommand;
            break;
        }
        default:
            LOG(Media, "RemoteCommandListenerMac::RemoteCommandListenerMac - command %u not supported!", command);
            status = MRMediaRemoteCommandHandlerStatusCommandFailed;
            return;
        };

        if (!weakThis)
            return;
        weakThis->m_client.didReceiveRemoteControlCommand(platformCommand, &argument);
        completion((__bridge CFArrayRef)@[@(status)]);
    });
#endif // USE(MEDIAREMOTE)
}

RemoteCommandListenerMac::~RemoteCommandListenerMac()
{
#if USE(MEDIAREMOTE)
    if (m_commandHandler)
        MRMediaRemoteRemoveCommandHandlerBlock(m_commandHandler);
#endif
}

}

#endif
