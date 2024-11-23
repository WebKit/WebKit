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

#import <wtf/SortedArrayMap.h>

#import "MediaRemoteSoftLink.h"

namespace WebCore {

static std::optional<MRMediaRemoteCommand> mediaRemoteCommandForPlatformCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    static constexpr std::pair<PlatformMediaSession::RemoteControlCommandType, MRMediaRemoteCommand> mappings[] = {
        { PlatformMediaSession::RemoteControlCommandType::PlayCommand, MRMediaRemoteCommandPlay },
        { PlatformMediaSession::RemoteControlCommandType::PauseCommand, MRMediaRemoteCommandPause },
        { PlatformMediaSession::RemoteControlCommandType::StopCommand, MRMediaRemoteCommandStop },
        { PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand, MRMediaRemoteCommandTogglePlayPause },
        { PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand, MRMediaRemoteCommandBeginRewind },
        { PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand, MRMediaRemoteCommandEndRewind },
        { PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand, MRMediaRemoteCommandBeginFastForward },
        { PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand, MRMediaRemoteCommandEndFastForward },
        { PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand, MRMediaRemoteCommandSeekToPlaybackPosition },
        { PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand, MRMediaRemoteCommandSkipForward },
        { PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand, MRMediaRemoteCommandSkipBackward },
        { PlatformMediaSession::RemoteControlCommandType::NextTrackCommand, MRMediaRemoteCommandNextTrack },
        { PlatformMediaSession::RemoteControlCommandType::PreviousTrackCommand, MRMediaRemoteCommandPreviousTrack },
    };
    static constexpr SortedArrayMap map { mappings };
    return makeOptionalFromPointer(map.tryGet(command));
}

Ref<RemoteCommandListenerCocoa> RemoteCommandListenerCocoa::create(RemoteCommandListenerClient& client)
{
    return adoptRef(*new RemoteCommandListenerCocoa(client));
}

static RemoteCommandListener::RemoteCommandsSet& defaultCommands()
{
    static NeverDestroyed<RemoteCommandListener::RemoteCommandsSet> commands(std::initializer_list<PlatformMediaSession::RemoteControlCommandType> {
        PlatformMediaSession::RemoteControlCommandType::PlayCommand,
        PlatformMediaSession::RemoteControlCommandType::PauseCommand,
        PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand,
        PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand,
        PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand,
        PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand,
        PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand,
        PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand,
        PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand,
        PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand,
    });

    return commands;
}

static RemoteCommandListener::RemoteCommandsSet& minimalCommands()
{
    static NeverDestroyed<RemoteCommandListener::RemoteCommandsSet> commands(std::initializer_list<PlatformMediaSession::RemoteControlCommandType> {
        PlatformMediaSession::RemoteControlCommandType::PlayCommand,
        PlatformMediaSession::RemoteControlCommandType::PauseCommand,
    });

    return commands;
}

static bool isSeekCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    return command == PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand
        || command == PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand
        || command == PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand
        || command == PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand
        || command == PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand;
}

void RemoteCommandListenerCocoa::updateSupportedCommands()
{
    if (!isMediaRemoteFrameworkAvailable())
        return;

    auto currentCommands = !supportedCommands().isEmpty() ? supportedCommands().unionWith(minimalCommands()) : defaultCommands();
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
        if (platformCommand == PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand || platformCommand == PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand)
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

    ThreadSafeWeakPtr weakThis { *this };
    m_commandHandler = MRMediaRemoteAddAsyncCommandHandlerBlock(^(MRMediaRemoteCommand command, CFDictionaryRef options, void(^completion)(CFArrayRef)) {

        LOG(Media, "RemoteCommandListenerCocoa::RemoteCommandListenerCocoa - received command %u", command);

        auto platformCommand { PlatformMediaSession::RemoteControlCommandType::NoCommand };
        PlatformMediaSession::RemoteCommandArgument argument;
        MRMediaRemoteCommandHandlerStatus status = MRMediaRemoteCommandHandlerStatusSuccess;

        switch (command) {
        case MRMediaRemoteCommandPlay:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::PlayCommand;
            break;
        case MRMediaRemoteCommandPause:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::PauseCommand;
            break;
        case MRMediaRemoteCommandStop:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::StopCommand;
            break;
        case MRMediaRemoteCommandTogglePlayPause:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand;
            break;
        case MRMediaRemoteCommandBeginFastForward:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand;
            break;
        case MRMediaRemoteCommandEndFastForward:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand;
            break;
        case MRMediaRemoteCommandBeginRewind:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand;
            break;
        case MRMediaRemoteCommandEndRewind:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand;
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
            platformCommand = PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand;
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

            platformCommand = (command == MRMediaRemoteCommandSkipForward) ? PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand : PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand;
            break;
        case MRMediaRemoteCommandNextTrack:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::NextTrackCommand;
            break;
        case MRMediaRemoteCommandPreviousTrack:
            platformCommand = PlatformMediaSession::RemoteControlCommandType::PreviousTrackCommand;
            break;
        default:
            LOG(Media, "RemoteCommandListenerCocoa::RemoteCommandListenerCocoa - command %u not supported!", command);
            status = MRMediaRemoteCommandHandlerStatusCommandFailed;
        };

        ensureOnMainThread([weakThis = WTFMove(weakThis), platformCommand, argument] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->client().didReceiveRemoteControlCommand(platformCommand, argument);
        });

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
