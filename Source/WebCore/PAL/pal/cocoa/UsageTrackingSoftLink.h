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

#pragma once

#if HAVE(MEDIA_USAGE_FRAMEWORK)

#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, UsageTracking);

SOFT_LINK_CLASS_FOR_HEADER(PAL, USVideoUsage);
#define _AXSIsolatedTreeModeFunctionIsAvailable PAL::canLoad_libAccessibility__AXSIsolatedTreeMode

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyCanShowControlsManager, NSString *)
#define USVideoMetadataKeyCanShowControlsManager PAL::get_UsageTracking_USVideoMetadataKeyCanShowControlsManagerSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyCanShowNowPlayingControls, NSString *)
#define USVideoMetadataKeyCanShowNowPlayingControls PAL::get_UsageTracking_USVideoMetadataKeyCanShowNowPlayingControlsSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsSuspended, NSString *)
#define USVideoMetadataKeyIsSuspended PAL::get_UsageTracking_USVideoMetadataKeyIsSuspendedSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsInActiveDocument, NSString *)
#define USVideoMetadataKeyIsInActiveDocument PAL::get_UsageTracking_USVideoMetadataKeyIsInActiveDocumentSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsFullscreen, NSString *)
#define USVideoMetadataKeyIsFullscreen PAL::get_UsageTracking_USVideoMetadataKeyIsFullscreenSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsMuted, NSString *)
#define USVideoMetadataKeyIsMuted PAL::get_UsageTracking_USVideoMetadataKeyIsMutedSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsMediaDocumentInMainFrame, NSString *)
#define USVideoMetadataKeyIsMediaDocumentInMainFrame PAL::get_UsageTracking_USVideoMetadataKeyIsMediaDocumentInMainFrameSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsAudio, NSString *)
#define USVideoMetadataKeyIsAudio PAL::get_UsageTracking_USVideoMetadataKeyIsAudioSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyAudioElementWithUserGesture, NSString *)
#define USVideoMetadataKeyAudioElementWithUserGesture PAL::get_UsageTracking_USVideoMetadataKeyAudioElementWithUserGestureSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyUserHasPlayedAudioBefore, NSString *)
#define USVideoMetadataKeyUserHasPlayedAudioBefore PAL::get_UsageTracking_USVideoMetadataKeyUserHasPlayedAudioBeforeSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsElementRectMostlyInMainFrame, NSString *)
#define USVideoMetadataKeyIsElementRectMostlyInMainFrame PAL::get_UsageTracking_USVideoMetadataKeyIsElementRectMostlyInMainFrameSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyNoAudio, NSString *)
#define USVideoMetadataKeyNoAudio PAL::get_UsageTracking_USVideoMetadataKeyNoAudioSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyPlaybackPermitted, NSString *)
#define USVideoMetadataKeyPlaybackPermitted PAL::get_UsageTracking_USVideoMetadataKeyPlaybackPermittedSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyPageMediaPlaybackSuspended, NSString *)
#define USVideoMetadataKeyPageMediaPlaybackSuspended PAL::get_UsageTracking_USVideoMetadataKeyPageMediaPlaybackSuspendedSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsMediaDocumentAndNotOwnerElement, NSString *)
#define USVideoMetadataKeyIsMediaDocumentAndNotOwnerElement PAL::get_UsageTracking_USVideoMetadataKeyIsMediaDocumentAndNotOwnerElementSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyPageExplicitlyAllowsElementToAutoplayInline, NSString *)
#define USVideoMetadataKeyPageExplicitlyAllowsElementToAutoplayInline PAL::get_UsageTracking_USVideoMetadataKeyPageExplicitlyAllowsElementToAutoplayInlineSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyRequiresFullscreenForVideoPlaybackAndFullscreenNotPermitted, NSString *)
#define USVideoMetadataKeyRequiresFullscreenForVideoPlaybackAndFullscreenNotPermitted PAL::get_UsageTracking_USVideoMetadataKeyRequiresFullscreenForVideoPlaybackAndFullscreenNotPermittedSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoRateChange, NSString *)
#define USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoRateChange PAL::get_UsageTracking_USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoRateChangeSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsAudioAndRequiresUserGestureForAudioRateChange, NSString *)
#define USVideoMetadataKeyIsAudioAndRequiresUserGestureForAudioRateChange PAL::get_UsageTracking_USVideoMetadataKeyIsAudioAndRequiresUserGestureForAudioRateChangeSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoDueToLowPowerMode, NSString *)
#define USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoDueToLowPowerMode PAL::get_UsageTracking_USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoDueToLowPowerModeSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyNoUserGestureRequired, NSString *)
#define USVideoMetadataKeyNoUserGestureRequired PAL::get_UsageTracking_USVideoMetadataKeyNoUserGestureRequiredSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyRequiresPlaybackAndIsNotPlaying, NSString *)
#define USVideoMetadataKeyRequiresPlaybackAndIsNotPlaying PAL::get_UsageTracking_USVideoMetadataKeyRequiresPlaybackAndIsNotPlayingSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyHasEverNotifiedAboutPlaying, NSString *)
#define USVideoMetadataKeyHasEverNotifiedAboutPlaying PAL::get_UsageTracking_USVideoMetadataKeyHasEverNotifiedAboutPlayingSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyOutsideOfFullscreen, NSString *)
#define USVideoMetadataKeyOutsideOfFullscreen PAL::get_UsageTracking_USVideoMetadataKeyOutsideOfFullscreenSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsVideo, NSString *)
#define USVideoMetadataKeyIsVideo PAL::get_UsageTracking_USVideoMetadataKeyIsVideoSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyRenderer, NSString *)
#define USVideoMetadataKeyRenderer PAL::get_UsageTracking_USVideoMetadataKeyRendererSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyNoVideo, NSString *)
#define USVideoMetadataKeyNoVideo PAL::get_UsageTracking_USVideoMetadataKeyNoVideoSingleton()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, UsageTracking, USVideoMetadataKeyIsLargeEnoughForMainContent, NSString *)
#define USVideoMetadataKeyIsLargeEnoughForMainContent PAL::get_UsageTracking_USVideoMetadataKeyIsLargeEnoughForMainContentSingleton()

#endif // HAVE(MEDIA_USAGE_FRAMEWORK)
