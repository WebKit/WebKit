/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
#include "RenderMediaControls.h"

#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "RenderThemeSafari.h"
#include "SoftLinking.h"
#include <CoreGraphics/CoreGraphics.h>
 
using namespace std;
 
namespace WebCore {

#ifdef DEBUG_ALL
SOFT_LINK_DEBUG_LIBRARY(SafariTheme)
#else
SOFT_LINK_LIBRARY(SafariTheme)
#endif

SOFT_LINK(SafariTheme, paintThemePart, void, __stdcall, (ThemePart part, CGContextRef context, const CGRect& rect, NSControlSize size, ThemeControlState state), (part, context, rect, size, state))
SOFT_LINK(SafariTheme, STPaintProgressIndicator, void, APIENTRY, (ProgressIndicatorType type, CGContextRef context, const CGRect& rect, NSControlSize size, ThemeControlState state, float value), (type, context, rect, size, state, value))

#if ENABLE(VIDEO)

static ThemeControlState determineState(RenderObject* o)
{
    ThemeControlState result = 0;
    RenderTheme* theme = o->theme();
    if (theme->isActive(o))
        result |= SafariTheme::ActiveState;
    if (theme->isEnabled(o) && !theme->isReadOnlyControl(o))
        result |= SafariTheme::EnabledState;
    if (theme->isPressed(o))
        result |= SafariTheme::PressedState;
    if (theme->isChecked(o))
        result |= SafariTheme::CheckedState;
    if (theme->isIndeterminate(o))
        result |= SafariTheme::IndeterminateCheckedState;
    if (theme->isFocused(o))
        result |= SafariTheme::FocusedState;
    if (theme->isDefault(o))
        result |= SafariTheme::DefaultState;
    return result;
}

static const int mediaSliderThumbWidth = 13;
static const int mediaSliderThumbHeight = 14;

void RenderMediaControls::adjustMediaSliderThumbSize(RenderObject* o)
{
    if (o->style()->appearance() != MediaSliderThumbPart)
        return;

    float zoomLevel = o->style()->effectiveZoom();
    o->style()->setWidth(Length(static_cast<int>(mediaSliderThumbWidth * zoomLevel), Fixed));
    o->style()->setHeight(Length(static_cast<int>(mediaSliderThumbHeight * zoomLevel), Fixed));
}

bool RenderMediaControls::paintMediaControlsPart(MediaControlElementType part, RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    ASSERT(SafariThemeLibrary());

    switch (part) {
        case MediaFullscreenButton:
            paintThemePart(SafariTheme::MediaFullscreenButtonPart, paintInfo.context->platformContext(), r, NSRegularControlSize, determineState(o));
            break;
        case MediaShowClosedCaptionsButton:
        case MediaHideClosedCaptionsButton:
#if SAFARI_THEME_VERSION >= 4
            if (MediaControlToggleClosedCaptionsButtonElement* btn = static_cast<MediaControlToggleClosedCaptionsButtonElement*>(o->node())) {
                bool captionsVisible = btn->displayType() == MediaHideClosedCaptionsButton;
                paintThemePart(captionsVisible ? SafariTheme::MediaHideClosedCaptionsButtonPart : SafariTheme::MediaShowClosedCaptionsButtonPart, paintInfo.context->platformContext(), r, NSRegularControlSize, determineState(o));
            }
#endif
            break;
        case MediaMuteButton:
        case MediaUnMuteButton:
            if (MediaControlMuteButtonElement* btn = static_cast<MediaControlMuteButtonElement*>(o->node())) {
                bool audioEnabled = btn->displayType() == MediaMuteButton;
                paintThemePart(audioEnabled ? SafariTheme::MediaMuteButtonPart : SafariTheme::MediaUnMuteButtonPart, paintInfo.context->platformContext(), r, NSRegularControlSize, determineState(o));
            }
            break;
        case MediaPauseButton:
        case MediaPlayButton:
            if (MediaControlPlayButtonElement* btn = static_cast<MediaControlPlayButtonElement*>(o->node())) {
                bool canPlay = btn->displayType() == MediaPlayButton;
                paintThemePart(canPlay ? SafariTheme::MediaPlayButtonPart : SafariTheme::MediaPauseButtonPart, paintInfo.context->platformContext(), r, NSRegularControlSize, determineState(o));
            }
            break;
        case MediaSeekBackButton:
            paintThemePart(SafariTheme::MediaSeekBackButtonPart, paintInfo.context->platformContext(), r, NSRegularControlSize, determineState(o));
            break;
        case MediaSeekForwardButton:
            paintThemePart(SafariTheme::MediaSeekForwardButtonPart, paintInfo.context->platformContext(), r, NSRegularControlSize, determineState(o));
            break;
        case MediaSlider: {
            if (HTMLMediaElement* mediaElement = toParentMediaElement(o))
                STPaintProgressIndicator(SafariTheme::MediaType, paintInfo.context->platformContext(), r, NSRegularControlSize, 0, mediaElement->percentLoaded());
            break;
        }
        case MediaSliderThumb:
            paintThemePart(SafariTheme::MediaSliderThumbPart, paintInfo.context->platformContext(), r, NSRegularControlSize, determineState(o));
            break;
        case MediaVolumeSliderContainer:
            // FIXME: Implement volume slider.
            ASSERT_NOT_REACHED();
            break;
        case MediaVolumeSlider:
            // FIXME: Implement volume slider.
            ASSERT_NOT_REACHED();
            break;
        case MediaVolumeSliderThumb:
            // FIXME: Implement volume slider.
            ASSERT_NOT_REACHED();
            break;
        case MediaTimelineContainer:
            ASSERT_NOT_REACHED();
            break;
        case MediaCurrentTimeDisplay:
            ASSERT_NOT_REACHED();
            break;
        case MediaTimeRemainingDisplay:
            ASSERT_NOT_REACHED();
            break;
        case MediaControlsPanel:
            ASSERT_NOT_REACHED();
            break;
    }
    return false;
}

#endif  // #if ENABLE(VIDEO)

} // namespace WebCore
