/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "EditingBehaviorType.h"
#include "ForcedAccessibilityValue.h"
#include "StorageMap.h"

namespace WebCore {

#if USE(UNIFIED_TEXT_CHECKING)
static const bool defaultUnifiedTextCheckerEnabled = true;
#else
static const bool defaultUnifiedTextCheckerEnabled = false;
#endif

#if PLATFORM(COCOA)
static const bool defaultYouTubeFlashPluginReplacementEnabled = true;
#else
static const bool defaultYouTubeFlashPluginReplacementEnabled = false;
#endif

#if PLATFORM(IOS)
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
static const bool defaultQuickTimePluginReplacementEnabled = true;
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
static const bool defaultQuickTimePluginReplacementEnabled = false;
#endif

static const bool defaultMediaControlsScaleWithPageZoom = true;
static const bool defaultAllowsPictureInPictureMediaPlayback = true;
static const bool defaultRequiresUserGestureToLoadVideo = true;
static const bool defaultSmartInsertDeleteEnabled = true;
static const bool defaultSelectTrailingWhitespaceEnabled = false;
static const unsigned defaultMaximumHTMLParserDOMTreeDepth = 512;
static const double defaultIncrementalRenderingSuppressionTimeoutInSeconds = 5;
static const float defaultMinimumZoomFontSize = 15;
static const ForcedAccessibilityValue defaultForcedColorsAreInvertedAccessibilityValue = ForcedAccessibilityValue::System;
static const ForcedAccessibilityValue defaultForcedDisplayIsMonochromeAccessibilityValue = ForcedAccessibilityValue::System;
static const ForcedAccessibilityValue defaultForcedPrefersReducedMotionAccessibilityValue = ForcedAccessibilityValue::System;

inline EditingBehaviorType editingBehaviorTypeForPlatform()
{
    return
#if PLATFORM(IOS)
    EditingIOSBehavior;
#elif OS(DARWIN)
    EditingMacBehavior;
#elif OS(WINDOWS)
    EditingWindowsBehavior;
#elif OS(UNIX)
    EditingUnixBehavior;
#else
    // Fallback
    EditingMacBehavior;
#endif
}

#if PLATFORM(IOS)
WEBCORE_EXPORT bool defaultTextAutosizingEnabled();
#else
inline bool defaultTextAutosizingEnabled()
{
    return false;
}
#endif

}
