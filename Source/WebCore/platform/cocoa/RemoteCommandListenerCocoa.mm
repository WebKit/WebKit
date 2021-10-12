/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteCommandListenerCocoa.h"

#if PLATFORM(COCOA)

#import "Logging.h"
#import <wtf/MainThread.h>

#import "MediaRemoteSoftLink.h"

namespace WebCore {

static std::optional<MRMediaRemoteCommand> mediaRemoteCommandForPlatformCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    static const auto commandMap = makeNeverDestroyed([] {
        using CommandToActionMap = HashMap<PlatformMediaSession::RemoteControlCommandType, MRMediaRemoteCommand, WTF::IntHash<PlatformMediaSession::RemoteControlCommandType>, WTF::StrongEnumHashTraits<PlatformMediaSession::RemoteControlCommandType>>;

        return CommandToActionMap {
            { PlatformMediaSession::PlayCommand, MRMediaRemoteCommandPlay },
            { PlatformMediaSession::PauseCommand, MRMediaRemoteCommandPause },
            { PlatformMediaSession::StopCommand, MRMediaRemoteCommandStop },
            { PlatformMediaSession::TogglePlayPauseCommand, MRMediaRemoteCommandTogglePlayPause },
            { PlatformMediaSession::BeginSeekingBackwardCommand, MRMediaRemoteCommandBeginRewind },
            { PlatformMediaSession::EndSeekingBackwardCommand, MRMediaRemoteCommandEndRewind },
            { PlatformMediaSession::BeginSeekingForwardCommand, MRMediaRemoteCommandBeginFastForward },
            { PlatformMediaSession::EndSeekingForwardCommand, MRMediaRemoteCommandEndFastForward },
            { PlatformMediaSession::SeekToPlaybackPositionCommand, MRMediaRemoteCommandSeekToPlaybackPosition },
            { PlatformMediaSession::SkipForwardCommand, MRMediaRemoteCommandSkipForward },
            { PlatformMediaSession::SkipBackwardCommand, MRMediaRemoteCommandSkipBackward },
            { PlatformMediaSession::NextTrackCommand, MRMediaRemoteCommandNextTrack },
            { PlatformMediaSession::PreviousTrackCommand, MRMediaRemoteCommandPreviousTrack },
        };
    }());

    auto it = commandMap.get().find(command);
    if (it != commandMap.get().end())
        return { it->value };

    return { };
}

std::unique_ptr<RemoteCommandListenerCocoa> RemoteCommandListenerCocoa::create(RemoteCommandListenerClient& client)
{
    return makeUnique<RemoteCommandListenerCocoa>(client);
}

const RemoteCommandListener::RemoteCommandsSet& RemoteCommandListenerCocoa::defaultCommands()
{
    static NeverDestroyed<RemoteCommandsSet> commands(std::initializer_list<PlatformMediaSession::RemoteControlCommandType> {
        PlatformMediaSession::PlayCommand,
        PlatformMediaSession::PauseCommand,
        PlatformMediaSession::TogglePlayPauseCommand,
        PlatformMediaSession::BeginSeekingForwardCommand,
        PlatformMediaSession::EndSeekingForwardCommand,
        PlatformMediaSession::BeginSeekingBackwardCommand,
        PlatformMediaSession::EndSeekingBackwardCommand,
        PlatformMediaSession::SeekToPlaybackPositionCommand,
        PlatformMediaSession::SkipForwardCommand,
        PlatformMediaSession::SkipBackwardCommand,
    });

    return commands;
}

static bool isSeekCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    return command == PlatformMediaSession::SeekToPlaybackPositionCommand
        || command == PlatformMediaSession::SkipForwardCommand
        || command == PlatformMediaSession::SkipBackwardCommand
        || command == PlatformMediaSession::BeginSeekingForwardCommand
        || command == PlatformMediaSession::BeginSeekingBackwardCommand;
}

void RemoteCommandListenerCocoa::updateSupportedCommands()
{
    if (!isMediaRemoteFrameworkAvailable())
        return;

    auto& currentCommands = !supportedCommands().isEmpty() ? supportedCommands() : defaultCommands();
    if (m_currentCommands == currentCommands)
        return;

    auto commandInfoArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, currentCommands.size(), &kCFTypeArrayCallBacks));
    for (auto platformCommand : currentCommands) {
        if (isSeekCommand(platformCommand) && !supportsSeeking())
            continue;

        auto command = mediaRemoteCommandForPlatformCommand(platformCommand);
        ASSERT(command);
        if (!command)
            continue;

        auto commandInfo = adoptCF(MRMediaRemoteCommandInfoCreate(kCFAllocatorDefault));
        MRMediaRemoteCommandInfoSetCommand(commandInfo.get(), command.value());
        MRMediaRemoteCommandInfoSetEnabled(commandInfo.get(), true);
        if (platformCommand == PlatformMediaSession::SkipForwardCommand || platformCommand == PlatformMediaSession::SkipBackwardCommand)
            MRMediaRemoteCommandInfoSetOptions(commandInfo.get(), (__bridge CFDictionaryRef)(@{(__bridge NSString *)kMRMediaRemoteCommandInfoPreferredIntervalsKey : @[@(15.0)]}));
        CFArrayAppendValue(commandInfoArray.get(), commandInfo.get());
    }

    MRMediaRemoteSetSupportedCommands(commandInfoArray.get(), MRMediaRemoteGetLocalOrigin(), nullptr, nullptr);
    m_currentCommands = currentCommands;
}

RemoteCommandListenerCocoa::RemoteCommandListenerCocoa(RemoteCommandListenerClient& client)
    : RemoteCommandListener(client)
{
    if (!isMediaRemoteFrameworkAvailable())
        return;

    scheduleSupportedCommandsUpdate();

    WeakPtr weakThis { *this };
    m_commandHandler = MRMediaRemoteAddAsyncCommandHandlerBlock(^(MRMediaRemoteCommand command, CFDictionaryRef options, void(^completion)(CFArrayRef)) {

        LOG(Media, "RemoteCommandListenerCocoa::RemoteCommandListenerCocoa - received command %u", command);

        PlatformMediaSession::RemoteControlCommandType platformCommand { PlatformMediaSession::NoCommand };
        PlatformMediaSession::RemoteCommandArgument argument;
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
            if (!supportsSeeking()) {
                status = MRMediaRemoteCommandHandlerStatusCommandFailed;
                break;
            }

            CFNumberRef positionRef = static_cast<CFNumberRef>(CFDictionaryGetValue(options, kMRMediaRemoteOptionPlaybackPosition));
            if (!positionRef) {
                status = MRMediaRemoteCommandHandlerStatusCommandFailed;
                break;
            }

            double position = 0;
            CFNumberGetValue(positionRef, kCFNumberDoubleType, &position);
            argument.time = position;
            platformCommand = PlatformMediaSession::SeekToPlaybackPositionCommand;
            break;
        }
        case MRMediaRemoteCommandSkipForward:
        case MRMediaRemoteCommandSkipBackward:
            if (!supportsSeeking()) {
                status = MRMediaRemoteCommandHandlerStatusCommandFailed;
                break;
            }

            if (auto positionRef = static_cast<CFNumberRef>(CFDictionaryGetValue(options, kMRMediaRemoteOptionSkipInterval))) {
                double position = 0;
                CFNumberGetValue(positionRef, kCFNumberDoubleType, &position);
                argument.time = position;
            }

            platformCommand = (command == MRMediaRemoteCommandSkipForward) ? PlatformMediaSession::SkipForwardCommand : PlatformMediaSession::SkipBackwardCommand;
            break;
        case MRMediaRemoteCommandNextTrack:
            platformCommand = PlatformMediaSession::NextTrackCommand;
            break;
        case MRMediaRemoteCommandPreviousTrack:
            platformCommand = PlatformMediaSession::PreviousTrackCommand;
            break;
        default:
            LOG(Media, "RemoteCommandListenerCocoa::RemoteCommandListenerCocoa - command %u not supported!", command);
            status = MRMediaRemoteCommandHandlerStatusCommandFailed;
        };

        if (weakThis && status != MRMediaRemoteCommandHandlerStatusCommandFailed)
            weakThis->client().didReceiveRemoteControlCommand(platformCommand, argument);

        completion((__bridge CFArrayRef)@[@(status)]);
    });
}

RemoteCommandListenerCocoa::~RemoteCommandListenerCocoa()
{
    if (m_commandHandler)
        MRMediaRemoteRemoveCommandHandlerBlock(m_commandHandler);
}

}

#endif
