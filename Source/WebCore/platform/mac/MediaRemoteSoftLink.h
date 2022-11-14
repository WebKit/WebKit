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

#pragma once

#include <pal/spi/mac/MediaRemoteSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(WebCore, MediaRemote)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteGetLocalOrigin, MROriginRef, (), ())
#define MRMediaRemoteGetLocalOrigin softLink_MediaRemote_MRMediaRemoteGetLocalOrigin
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteAddAsyncCommandHandlerBlock, void*, (MRMediaRemoteAsyncCommandHandlerBlock block), (block))
#define MRMediaRemoteAddAsyncCommandHandlerBlock softLink_MediaRemote_MRMediaRemoteAddAsyncCommandHandlerBlock
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteRemoveCommandHandlerBlock, void, (void* observer), (observer))
#define MRMediaRemoteRemoveCommandHandlerBlock softLink_MediaRemote_MRMediaRemoteRemoveCommandHandlerBlock
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteSetSupportedCommands, void, (CFArrayRef commands, MROriginRef origin, dispatch_queue_t replyQ, void(^completion)(MRMediaRemoteError err)), (commands, origin, replyQ, completion))
#define MRMediaRemoteSetSupportedCommands softLink_MediaRemote_MRMediaRemoteSetSupportedCommands
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteSetNowPlayingVisibility, void, (MROriginRef origin, MRNowPlayingClientVisibility visibility), (origin, visibility))
#define MRMediaRemoteSetNowPlayingVisibility softLink_MediaRemote_MRMediaRemoteSetNowPlayingVisibility
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteCommandInfoCreate, MRMediaRemoteCommandInfoRef, (CFAllocatorRef allocator), (allocator));
#define MRMediaRemoteCommandInfoCreate softLink_MediaRemote_MRMediaRemoteCommandInfoCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteCommandInfoSetCommand, void, (MRMediaRemoteCommandInfoRef commandInfo, MRMediaRemoteCommand command), (commandInfo, command))
#define MRMediaRemoteCommandInfoSetCommand softLink_MediaRemote_MRMediaRemoteCommandInfoSetCommand
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteCommandInfoSetEnabled, void, (MRMediaRemoteCommandInfoRef commandInfo, Boolean enabled), (commandInfo, enabled))
#define MRMediaRemoteCommandInfoSetEnabled softLink_MediaRemote_MRMediaRemoteCommandInfoSetEnabled
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteCommandInfoSetOptions, void, (MRMediaRemoteCommandInfoRef commandInfo, CFDictionaryRef options), (commandInfo, options))
#define MRMediaRemoteCommandInfoSetOptions softLink_MediaRemote_MRMediaRemoteCommandInfoSetOptions
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteSetCanBeNowPlayingApplication, Boolean, (Boolean flag), (flag))
#define MRMediaRemoteSetCanBeNowPlayingApplication softLink_MediaRemote_MRMediaRemoteSetCanBeNowPlayingApplication
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteSetNowPlayingInfo, void, (CFDictionaryRef info), (info))
#define MRMediaRemoteSetNowPlayingInfo softLink_MediaRemote_MRMediaRemoteSetNowPlayingInfo
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteSetNowPlayingInfoWithMergePolicy, void, (CFDictionaryRef info, MRMediaRemoteMergePolicy mergePolicy), (info, mergePolicy))
#define MRMediaRemoteSetNowPlayingInfoWithMergePolicy softLink_MediaRemote_MRMediaRemoteSetNowPlayingInfoWithMergePolicy
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin, void, (MROriginRef origin, MRPlaybackState playbackState, dispatch_queue_t replyQ, void(^completion)(MRMediaRemoteError)), (origin, playbackState, replyQ, completion))
#define MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin softLink_MediaRemote_MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteSetParentApplication, void, (MROriginRef origin, CFStringRef parentAppDisplayID), (origin, parentAppDisplayID))
#define MRMediaRemoteSetParentApplication softLink_MediaRemote_MRMediaRemoteSetParentApplication
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoTitle, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoTitle get_MediaRemote_kMRMediaRemoteNowPlayingInfoTitle()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoArtist, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoArtist get_MediaRemote_kMRMediaRemoteNowPlayingInfoArtist()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoAlbum, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoAlbum get_MediaRemote_kMRMediaRemoteNowPlayingInfoAlbum()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoArtworkData, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoArtworkData get_MediaRemote_kMRMediaRemoteNowPlayingInfoArtworkData()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoArtworkDataHeight, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoArtworkDataHeight get_MediaRemote_kMRMediaRemoteNowPlayingInfoArtworkDataHeight()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoArtworkDataWidth, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoArtworkDataWidth get_MediaRemote_kMRMediaRemoteNowPlayingInfoArtworkDataWidth()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoArtworkMIMEType, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoArtworkMIMEType get_MediaRemote_kMRMediaRemoteNowPlayingInfoArtworkMIMEType()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoDuration, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoArtworkIdentifier get_MediaRemote_kMRMediaRemoteNowPlayingInfoArtworkIdentifier()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoArtworkIdentifier, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoDuration get_MediaRemote_kMRMediaRemoteNowPlayingInfoDuration()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoElapsedTime, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoElapsedTime get_MediaRemote_kMRMediaRemoteNowPlayingInfoElapsedTime()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoPlaybackRate, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoPlaybackRate get_MediaRemote_kMRMediaRemoteNowPlayingInfoPlaybackRate()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteOptionPlaybackPosition, CFStringRef);
#define kMRMediaRemoteOptionPlaybackPosition get_MediaRemote_kMRMediaRemoteOptionPlaybackPosition()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoUniqueIdentifier, CFStringRef);
#define kMRMediaRemoteNowPlayingInfoUniqueIdentifier get_MediaRemote_kMRMediaRemoteNowPlayingInfoUniqueIdentifier()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteOptionSkipInterval, CFStringRef);
#define kMRMediaRemoteOptionSkipInterval get_MediaRemote_kMRMediaRemoteOptionSkipInterval()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, MediaRemote, kMRMediaRemoteCommandInfoPreferredIntervalsKey, CFStringRef);
#define kMRMediaRemoteCommandInfoPreferredIntervalsKey get_MediaRemote_kMRMediaRemoteOptionSkipInterval()

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, MediaRemote, MRMediaRemoteCopyPickableRoutes, CFArrayRef, (), ())
#define MRMediaRemoteCopyPickableRoutes softLink_MediaRemote_MRMediaRemoteCopyPickableRoutes
#endif
