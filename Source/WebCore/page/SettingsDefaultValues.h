/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "EditingBehaviorTypes.h"

namespace WebCore {

// NOTEs
//  1) EditingMacBehavior comprises Tiger, Leopard, SnowLeopard and iOS builds, as well as QtWebKit when built on Mac;
//  2) EditingWindowsBehavior comprises Win32 build;
//  3) EditingUnixBehavior comprises all unix-based systems, but Darwin/MacOS (and then abusing the terminology);
// 99) MacEditingBehavior is used as a fallback.
inline EditingBehaviorType editingBehaviorTypeForPlatform()
{
#if PLATFORM(IOS_FAMILY)
    return EditingIOSBehavior;
#elif OS(DARWIN)
    return EditingMacBehavior;
#elif OS(WINDOWS)
    return EditingWindowsBehavior;
#elif OS(UNIX)
    return EditingUnixBehavior;
#else
    // Fallback
    return EditingMacBehavior;
#endif
    
}

#if PLATFORM(COCOA)
static const bool defaultYouTubeFlashPluginReplacementEnabled = true;
#else
static const bool defaultYouTubeFlashPluginReplacementEnabled = false;
#endif

#if PLATFORM(IOS_FAMILY)
static const bool defaultFixedBackgroundsPaintRelativeToDocument = true;
static const bool defaultAcceleratedCompositingForFixedPositionEnabled = true;
static const bool defaultAllowsInlineMediaPlayback = false;
static const bool defaultInlineMediaPlaybackRequiresPlaysInlineAttribute = true;
static const bool defaultVideoPlaybackRequiresUserGesture = true;
static const bool defaultAudioPlaybackRequiresUserGesture = true;
static const bool defaultMediaDataLoadsAutomatically = false;
static const bool defaultShouldRespectImageOrientation = true;
static const bool defaultImageSubsamplingEnabled = true;
static const bool defaultScrollingTreeIncludesFrames = true;
static const bool defaultMediaControlsScaleWithPageZoom = true;
static const bool defaultQuickTimePluginReplacementEnabled = true;
static const bool defaultRequiresUserGestureToLoadVideo = true;
#else
static const bool defaultFixedBackgroundsPaintRelativeToDocument = false;
static const bool defaultAcceleratedCompositingForFixedPositionEnabled = false;
static const bool defaultAllowsInlineMediaPlayback = true;
static const bool defaultInlineMediaPlaybackRequiresPlaysInlineAttribute = false;
static const bool defaultVideoPlaybackRequiresUserGesture = false;
static const bool defaultAudioPlaybackRequiresUserGesture = false;
static const bool defaultMediaDataLoadsAutomatically = true;
static const bool defaultShouldRespectImageOrientation = false;
static const bool defaultImageSubsamplingEnabled = false;
static const bool defaultScrollingTreeIncludesFrames = false;
static const bool defaultMediaControlsScaleWithPageZoom = true;
static const bool defaultQuickTimePluginReplacementEnabled = false;
static const bool defaultRequiresUserGestureToLoadVideo = false;
#endif

static const bool defaultAllowsPictureInPictureMediaPlayback = true;

static const double defaultIncrementalRenderingSuppressionTimeoutInSeconds = 5;
#if USE(UNIFIED_TEXT_CHECKING)
static const bool defaultUnifiedTextCheckerEnabled = true;
#else
static const bool defaultUnifiedTextCheckerEnabled = false;
#endif
static const bool defaultSmartInsertDeleteEnabled = true;
static const bool defaultSelectTrailingWhitespaceEnabled = false;

#if ENABLE(VIDEO) && (USE(AVFOUNDATION) || USE(GSTREAMER) || USE(MEDIA_FOUNDATION))
static const bool defaultMediaEnabled = true;
#else
static const bool defaultMediaEnabled = false;
#endif
    
#if (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(WATCHOS)
static const bool defaultConicGradient = true;
#else
static const bool defaultConicGradient = false;
#endif
    
#if ENABLE(APPLE_PAY_REMOTE_UI)
static const bool defaultApplePayEnabled = true;
#else
static const bool defaultApplePayEnabled = false;
#endif

}
