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

#pragma once

#if USE(MEDIAREMOTE)

#if __has_include(<MediaRemote/MRNowPlayingTypes.h>)

#include <MediaRemote/MRNowPlayingTypes.h>

#else

enum {
    MRNowPlayingClientVisibilityUndefined = 0,
    MRNowPlayingClientVisibilityAlwaysVisible,
    MRNowPlayingClientVisibilityVisibleWhenBackgrounded,
    MRNowPlayingClientVisibilityNeverVisible
};
typedef uint32_t MRNowPlayingClientVisibility;

#endif

#if USE(APPLE_INTERNAL_SDK)

#include <MediaRemote/MediaRemote.h>

#else

enum {
    MRMediaRemoteCommandPlay,
    MRMediaRemoteCommandPause,
    MRMediaRemoteCommandTogglePlayPause,
    MRMediaRemoteCommandStop,
    MRMediaRemoteCommandNextTrack,
    MRMediaRemoteCommandPreviousTrack,
    MRMediaRemoteCommandAdvanceShuffleMode,
    MRMediaRemoteCommandAdvanceRepeatMode,
    MRMediaRemoteCommandBeginFastForward,
    MRMediaRemoteCommandEndFastForward,
    MRMediaRemoteCommandBeginRewind,
    MRMediaRemoteCommandEndRewind,
    MRMediaRemoteCommandRewind15Seconds,
    MRMediaRemoteCommandFastForward15Seconds,
    MRMediaRemoteCommandRewind30Seconds,
    MRMediaRemoteCommandFastForward30Seconds,
    MRMediaRemoteCommandToggleRecord,
    MRMediaRemoteCommandSkipForward,
    MRMediaRemoteCommandSkipBackward,
    MRMediaRemoteCommandChangePlaybackRate,
    MRMediaRemoteCommandRateTrack,
    MRMediaRemoteCommandLikeTrack,
    MRMediaRemoteCommandDislikeTrack,
    MRMediaRemoteCommandBookmarkTrack,
    MRMediaRemoteCommandSeekToPlaybackPosition,
    MRMediaRemoteCommandChangeRepeatMode,
    MRMediaRemoteCommandChangeShuffleMode,
    MRMediaRemoteCommandEnableLanguageOption,
    MRMediaRemoteCommandDisableLanguageOption
};
typedef uint32_t MRMediaRemoteCommand;

enum {
    kMRPlaybackStateUnknown = 0,
    kMRPlaybackStatePlaying,
    kMRPlaybackStatePaused,
    kMRPlaybackStateStopped,
    kMRPlaybackStateInterrupted
};
typedef uint32_t MRPlaybackState;

enum {
    MRMediaRemoteCommandHandlerStatusSuccess = 0,
    MRMediaRemoteCommandHandlerStatusNoSuchContent = 1,
    MRMediaRemoteCommandHandlerStatusCommandFailed = 2,
    MRMediaRemoteCommandHandlerStatusNoActionableNowPlayingItem = 10,
    MRMediaRemoteCommandHandlerStatusUIKitLegacy = 3
};
typedef uint32_t MRMediaRemoteCommandHandlerStatus;

typedef uint32_t MRMediaRemoteError;
typedef struct _MROrigin *MROriginRef;
typedef struct _MRMediaRemoteCommandInfo *MRMediaRemoteCommandInfoRef;
typedef void *MRNowPlayingClientRef;
typedef void(^MRMediaRemoteAsyncCommandHandlerBlock)(MRMediaRemoteCommand command, CFDictionaryRef options, void(^completion)(CFArrayRef));

WTF_EXTERN_C_BEGIN

#pragma mark - MRRemoteControl

void* MRMediaRemoteAddAsyncCommandHandlerBlock(MRMediaRemoteAsyncCommandHandlerBlock);
void MRMediaRemoteRemoveCommandHandlerBlock(void *observer);
void MRMediaRemoteSetSupportedCommands(CFArrayRef commands, MROriginRef, dispatch_queue_t replyQ, void(^completion)(MRMediaRemoteError err));
void MRMediaRemoteSetNowPlayingVisibility(MROriginRef, MRNowPlayingClientVisibility);

#pragma mark - MROrigin

MROriginRef MRMediaRemoteGetLocalOrigin();

#pragma mark - MRCommandInfo

MRMediaRemoteCommandInfoRef MRMediaRemoteCommandInfoCreate(CFAllocatorRef);
void MRMediaRemoteCommandInfoSetCommand(MRMediaRemoteCommandInfoRef, MRMediaRemoteCommand);
void MRMediaRemoteCommandInfoSetEnabled(MRMediaRemoteCommandInfoRef, Boolean);
void MRMediaRemoteCommandInfoSetOptions(MRMediaRemoteCommandInfoRef, CFDictionaryRef);

#pragma mark - MRNowPlaying

Boolean MRMediaRemoteSetCanBeNowPlayingApplication(Boolean);
void MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MROriginRef, MRPlaybackState, dispatch_queue_t replyQ, void(^completion)(MRMediaRemoteError));
void MRMediaRemoteSetNowPlayingInfo(CFDictionaryRef);

#pragma mark - MRAVRouting

CFArrayRef MRMediaRemoteCopyPickableRoutes();

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // USE(MEDIAREMOTE)
