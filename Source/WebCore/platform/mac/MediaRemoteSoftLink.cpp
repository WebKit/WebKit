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

#include <pal/spi/mac/MediaRemoteSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE(WebCore, MediaRemote)

#if USE(MEDIAREMOTE)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteGetLocalOrigin, MROriginRef, (), ())
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteAddAsyncCommandHandlerBlock, void*, (MRMediaRemoteAsyncCommandHandlerBlock block), (block))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteRemoveCommandHandlerBlock, void, (void* observer), (observer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteSetSupportedCommands, void, (CFArrayRef commands, MROriginRef origin, dispatch_queue_t replyQ, void(^completion)(MRMediaRemoteError err)), (commands, origin, replyQ, completion))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteSetNowPlayingVisibility, void, (MROriginRef origin, MRNowPlayingClientVisibility visibility), (origin, visibility))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteCommandInfoCreate, MRMediaRemoteCommandInfoRef, (CFAllocatorRef allocator), (allocator));
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteCommandInfoSetCommand, void, (MRMediaRemoteCommandInfoRef commandInfo, MRMediaRemoteCommand command), (commandInfo, command))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteCommandInfoSetEnabled, void, (MRMediaRemoteCommandInfoRef commandInfo, Boolean enabled), (commandInfo, enabled))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteCommandInfoSetOptions, void, (MRMediaRemoteCommandInfoRef commandInfo, CFDictionaryRef options), (commandInfo, options))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteSetCanBeNowPlayingApplication, Boolean, (Boolean flag), (flag))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteSetNowPlayingInfo, void, (CFDictionaryRef info), (info))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin, void, (MROriginRef origin, MRPlaybackState playbackState, dispatch_queue_t replyQ, void(^completion)(MRMediaRemoteError)), (origin, playbackState, replyQ, completion))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteSetParentApplication, void, (MROriginRef origin, CFStringRef parentAppDisplayID), (origin, parentAppDisplayID))
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoTitle, CFStringRef);
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoDuration, CFStringRef);
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoElapsedTime, CFStringRef);
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoPlaybackRate, CFStringRef);
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, MediaRemote, kMRMediaRemoteOptionPlaybackPosition, CFStringRef);
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, MediaRemote, kMRMediaRemoteNowPlayingInfoUniqueIdentifier, CFStringRef);

#endif // USE(MEDIAREMOTE)

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaRemote, MRMediaRemoteCopyPickableRoutes, CFArrayRef, (), ());
#endif
