/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "MediaRemoteSoftLink.h"

SOFT_LINK_FRAMEWORK_FOR_SOURCE(TestWebKitAPI, MediaRemote)

SOFT_LINK_CONSTANT_FOR_SOURCE(TestWebKitAPI, MediaRemote, kMRMediaRemoteNowPlayingApplicationDidChangeNotification, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(TestWebKitAPI, MediaRemote, kMRMediaRemoteNowPlayingApplicationPIDUserInfoKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(TestWebKitAPI, MediaRemote, kMRMediaRemoteOptionPlaybackPosition, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(TestWebKitAPI, MediaRemote, kMRMediaRemoteOptionSkipInterval, CFStringRef)
SOFT_LINK_FUNCTION_FOR_SOURCE(TestWebKitAPI, MediaRemote, MRMediaRemoteGetLocalOrigin, MROriginRef, (), ());
SOFT_LINK_FUNCTION_FOR_SOURCE(TestWebKitAPI, MediaRemote, MRMediaRemoteGetNowPlayingClient, void, (dispatch_queue_t queue, void(^completion)(MRNowPlayingClientRef, CFErrorRef)), (queue, completion))
SOFT_LINK_FUNCTION_FOR_SOURCE(TestWebKitAPI, MediaRemote, MRMediaRemoteGetSupportedCommandsForOrigin, void, (MROriginRef origin, dispatch_queue_t queue, void(^completion)(CFArrayRef commands)), (origin, queue, completion));
SOFT_LINK_FUNCTION_FOR_SOURCE(TestWebKitAPI, MediaRemote, MRMediaRemoteSendCommandToApp, Boolean, (MRMediaRemoteCommand command, CFDictionaryRef options, MROriginRef origin, CFStringRef appDisplayID, MRSendCommandAppOptions appOptions, dispatch_queue_t replyQ, void(^completion)(MRSendCommandError err, CFArrayRef handlerReturnStatuses)), (command, options, origin, appDisplayID, appOptions, replyQ, completion));
SOFT_LINK_FUNCTION_FOR_SOURCE(TestWebKitAPI, MediaRemote, MRMediaRemoteSetWantsNowPlayingNotifications, void, (bool wantsNotifications), (wantsNotifications))
SOFT_LINK_FUNCTION_FOR_SOURCE(TestWebKitAPI, MediaRemote, MRNowPlayingClientGetProcessIdentifier, pid_t, (MRNowPlayingClientRef client), (client))
