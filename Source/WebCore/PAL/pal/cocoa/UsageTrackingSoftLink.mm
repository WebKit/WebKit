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

#import "config.h"

#import <wtf/SoftLinking.h>

#if HAVE(MEDIA_USAGE_FRAMEWORK)

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, PAL_EXPORT);

SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(PAL, UsageTracking, USVideoUsage, PAL_EXPORT);

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyCanShowControlsManager, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyCanShowNowPlayingControls, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsSuspended, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsInActiveDocument, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsFullscreen, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsMuted, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsMediaDocumentInMainFrame, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsAudio, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyAudioElementWithUserGesture, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyUserHasPlayedAudioBefore, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsElementRectMostlyInMainFrame, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyNoAudio, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyPlaybackPermitted, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyPageMediaPlaybackSuspended, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsMediaDocumentAndNotOwnerElement, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyPageExplicitlyAllowsElementToAutoplayInline, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyRequiresFullscreenForVideoPlaybackAndFullscreenNotPermitted, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoRateChange, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsAudioAndRequiresUserGestureForAudioRateChange, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoDueToLowPowerMode, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyNoUserGestureRequired, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyRequiresPlaybackAndIsNotPlaying, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyHasEverNotifiedAboutPlaying, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyOutsideOfFullscreen, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsVideo, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyRenderer, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyNoVideo, NSString*, PAL_EXPORT);
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, UsageTracking, USVideoMetadataKeyIsLargeEnoughForMainContent, NSString*, PAL_EXPORT);

#endif // HAVE(MEDIA_USAGE_FRAMEWORK)
