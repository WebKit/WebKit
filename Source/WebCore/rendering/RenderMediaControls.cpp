/*
 * Copyright (C) 2009, 2013 Apple Inc. All Rights Reserved.
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

#if ENABLE(VIDEO)

#include "RenderMediaControls.h"

#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "PaintInfo.h"
#include "RenderTheme.h"

// FIXME: Unify more of the code for Mac and Win.
#if PLATFORM(WIN) && USE(CG)

#include <CoreGraphics/CoreGraphics.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

// The Windows version of WKSI defines these functions as capitalized, while the Mac version defines them as lower case.
// FIXME: Is this necessary anymore?
inline bool wkHitTestMediaUIPart(int part, const CGRect& bounds, const CGPoint& point)
{
    WKHitTestMediaUIPart(part, bounds, point);
}

inline void wkMeasureMediaUIPart(int part, CGRect* bounds, CGSize* naturalSize)
{
    WKMeasureMediaUIPart(part, bounds, naturalSize);
}

inline void wkDrawMediaUIPart(int part, CGContextRef context, const CGRect& rect, unsigned state)
{
    WKDrawMediaUIPart(part, context, rect, state);
}

inline void wkDrawMediaSliderTrack(CGContextRef context, const CGRect& rect, float timeLoaded, float currentTime, float duration, unsigned state)
{
    WKDrawMediaSliderTrack(context, rect, timeLoaded, currentTime, duration, state);
}

#endif
 

namespace WebCore {

#if PLATFORM(WIN) && USE(CG)

static WKMediaControllerThemeState determineState(const RenderObject& o)
{
    int result = 0;
    const RenderTheme& theme = o.theme();
    if (!theme.isEnabled(o) || theme.isReadOnlyControl(o))
        result |= WKMediaControllerFlagDisabled;
    if (theme.isPressed(o))
        result |= WKMediaControllerFlagPressed;
    if (theme.isFocused(o))
        result |= WKMediaControllerFlagFocused;
    return static_cast<WKMediaControllerThemeState>(result);
}

// Utility to scale when the UI part are not scaled by wkDrawMediaUIPart
static FloatRect getUnzoomedRectAndAdjustCurrentContext(const RenderObject& o, const PaintInfo& paintInfo, const IntRect &originalRect)
{
    float zoomLevel = o.style().effectiveZoom();
    FloatRect unzoomedRect(originalRect);
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }
    return unzoomedRect;
}

static const int mediaSliderThumbWidth = 13;
static const int mediaSliderThumbHeight = 14;

void RenderMediaControls::adjustMediaSliderThumbSize(RenderStyle& style)
{
    int part;
    switch (style.appearance()) {
    case MediaSliderThumbPart:
        part = MediaSliderThumb;
        break;
    case MediaVolumeSliderThumbPart:
        part = MediaVolumeSliderThumb;
        break;
    case MediaFullScreenVolumeSliderThumbPart:
        part = MediaFullScreenVolumeSliderThumb;
        break;
    default:
        return;
    }

    CGSize size;
    wkMeasureMediaUIPart(part, 0, &size);

    float zoomLevel = style.effectiveZoom();
    style.setWidth(Length(static_cast<int>(size.width * zoomLevel), Fixed));
    style.setHeight(Length(static_cast<int>(size.height * zoomLevel), Fixed));
}

bool RenderMediaControls::paintMediaControlsPart(MediaControlElementType part, const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    switch (part) {
    case MediaEnterFullscreenButton:
    case MediaExitFullscreenButton:
        if (MediaControlFullscreenButtonElement* btn = static_cast<MediaControlFullscreenButtonElement*>(o.node())) {
            bool enterButton = btn->displayType() == MediaEnterFullscreenButton;
            wkDrawMediaUIPart(enterButton ? WKMediaUIPartFullscreenButton : WKMediaUIPartExitFullscreenButton, paintInfo.context->platformContext(), r, determineState(o));
        }
        break;
    case MediaShowClosedCaptionsButton:
    case MediaHideClosedCaptionsButton:
        if (MediaControlToggleClosedCaptionsButtonElement* btn = static_cast<MediaControlToggleClosedCaptionsButtonElement*>(o.node())) {
            bool captionsVisible = btn->displayType() == MediaHideClosedCaptionsButton;
            wkDrawMediaUIPart(captionsVisible ? WKMediaUIPartHideClosedCaptionsButton : WKMediaUIPartShowClosedCaptionsButton, paintInfo.context->platformContext(), r, determineState(o));
        }
        break;
    case MediaMuteButton:
    case MediaUnMuteButton:
        if (MediaControlMuteButtonElement* btn = static_cast<MediaControlMuteButtonElement*>(o.node())) {
            bool audioEnabled = btn->displayType() == MediaMuteButton;
            wkDrawMediaUIPart(audioEnabled ? WKMediaUIPartMuteButton : WKMediaUIPartUnMuteButton, paintInfo.context->platformContext(), r, determineState(o));
        }
        break;
    case MediaPauseButton:
    case MediaPlayButton:
        if (MediaControlPlayButtonElement* btn = static_cast<MediaControlPlayButtonElement*>(o.node())) {
            bool canPlay = btn->displayType() == MediaPlayButton;
            wkDrawMediaUIPart(canPlay ? WKMediaUIPartPlayButton : WKMediaUIPartPauseButton, paintInfo.context->platformContext(), r, determineState(o));
        }
        break;
    case MediaRewindButton:
        wkDrawMediaUIPart(WKMediaUIPartRewindButton, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaReturnToRealtimeButton:
        wkDrawMediaUIPart(WKMediaUIPartSeekToRealtimeButton, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaSeekBackButton:
        wkDrawMediaUIPart(WKMediaUIPartSeekBackButton, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaSeekForwardButton:
        wkDrawMediaUIPart(WKMediaUIPartSeekForwardButton, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaSlider: {
        if (HTMLMediaElement* mediaElement = parentMediaElement(o)) {
            FloatRect unzoomedRect = getUnzoomedRectAndAdjustCurrentContext(o, paintInfo, r);
            wkDrawMediaSliderTrack(paintInfo.context->platformContext(), unzoomedRect, mediaElement->percentLoaded() * mediaElement->duration(), mediaElement->currentTime(), mediaElement->duration(), determineState(o));
        }
        break;
    }
    case MediaSliderThumb:
        wkDrawMediaUIPart(WKMediaUIPartTimelineSliderThumb, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaVolumeSliderContainer:
        wkDrawMediaUIPart(WKMediaUIPartVolumeSliderContainer, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaVolumeSlider:
        wkDrawMediaUIPart(WKMediaUIPartVolumeSlider, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaVolumeSliderThumb:
        wkDrawMediaUIPart(WKMediaUIPartVolumeSliderThumb, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaFullScreenVolumeSlider:
        wkDrawMediaUIPart(WKMediaUIPartFullScreenVolumeSlider, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaFullScreenVolumeSliderThumb:
        wkDrawMediaUIPart(WKMediaUIPartFullScreenVolumeSliderThumb, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaTimelineContainer:
        wkDrawMediaUIPart(WKMediaUIPartBackground, paintInfo.context->platformContext(), r, determineState(o));
        break;
    case MediaCurrentTimeDisplay:
        ASSERT_NOT_REACHED();
        break;
    case MediaTimeRemainingDisplay:
        ASSERT_NOT_REACHED();
        break;
    case MediaControlsPanel:
        ASSERT_NOT_REACHED();
    case MediaTextTrackDisplayContainer:
    case MediaTextTrackDisplay:
    case MediaClosedCaptionsContainer:
    case MediaClosedCaptionsTrackList:
        ASSERT_NOT_REACHED();
        break;
    }

    return false;
}

#endif

IntPoint RenderMediaControls::volumeSliderOffsetFromMuteButton(RenderBox* muteButtonBox, const IntSize& size)
{
    static const int xOffset = -4;
    static const int yOffset = 5;

    float zoomLevel = muteButtonBox->style().effectiveZoom();
    int y = yOffset * zoomLevel + muteButtonBox->pixelSnappedOffsetHeight() - size.height();
    FloatPoint absPoint = muteButtonBox->localToAbsolute(FloatPoint(muteButtonBox->pixelSnappedOffsetLeft(), y), IsFixed | UseTransforms);
    if (absPoint.y() < 0)
        y = muteButtonBox->pixelSnappedHeight();
    return IntPoint(xOffset * zoomLevel, y);
}

}

#endif
