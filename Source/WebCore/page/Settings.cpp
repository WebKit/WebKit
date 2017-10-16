/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Settings.h"

#include "AudioSession.h"
#include "BackForwardController.h"
#include "CachedResourceLoader.h"
#include "CookieStorage.h"
#include "DOMTimer.h"
#include "Database.h"
#include "Document.h"
#include "FontCascade.h"
#include "FontGenericFamilies.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLMediaElement.h"
#include "HistoryItem.h"
#include "MainFrame.h"
#include "Page.h"
#include "StorageMap.h"
#include <limits>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

// NOTEs
//  1) EditingMacBehavior comprises Tiger, Leopard, SnowLeopard and iOS builds, as well as QtWebKit when built on Mac;
//  2) EditingWindowsBehavior comprises Win32 build;
//  3) EditingUnixBehavior comprises all unix-based systems, but Darwin/MacOS (and then abusing the terminology);
// 99) MacEditingBehavior is used as a fallback.
static EditingBehaviorType editingBehaviorTypeForPlatform()
{
    return
#if PLATFORM(IOS)
    EditingIOSBehavior
#elif OS(DARWIN)
    EditingMacBehavior
#elif OS(WINDOWS)
    EditingWindowsBehavior
#elif OS(UNIX)
    EditingUnixBehavior
#else
    // Fallback
    EditingMacBehavior
#endif
    ;
}

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
static const bool defaultMediaControlsScaleWithPageZoom = true;
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
static const bool defaultMediaControlsScaleWithPageZoom = true;
static const bool defaultQuickTimePluginReplacementEnabled = false;
#endif

static const bool defaultRequiresUserGestureToLoadVideo = true;
static const bool defaultAllowsPictureInPictureMediaPlayback = true;

static const double defaultIncrementalRenderingSuppressionTimeoutInSeconds = 5;
#if USE(UNIFIED_TEXT_CHECKING)
static const bool defaultUnifiedTextCheckerEnabled = true;
#else
static const bool defaultUnifiedTextCheckerEnabled = false;
#endif
static const bool defaultSmartInsertDeleteEnabled = true;
static const bool defaultSelectTrailingWhitespaceEnabled = false;

Ref<Settings> Settings::create(Page* page)
{
    return adoptRef(*new Settings(page));
}

Settings::Settings(Page* page)
    : SettingsBase(page)
    SETTINGS_INITIALIZER_LIST
{
}

Settings::~Settings()
{
}

SETTINGS_SETTER_BODIES

FrameFlattening Settings::effectiveFrameFlattening()
{
#if PLATFORM(IOS)
    // On iOS when async frame scrolling is enabled, it does not make sense to use full frame flattening.
    // In that case, we just consider that frame flattening is disabled. This allows people to test
    // frame scrolling on iOS by enabling "Async Frame Scrolling" via the Safari menu.
    if (asyncFrameScrollingEnabled() && frameFlattening() == FrameFlatteningFullyEnabled)
        return FrameFlatteningDisabled;
#endif
    return frameFlattening();
}

} // namespace WebCore
