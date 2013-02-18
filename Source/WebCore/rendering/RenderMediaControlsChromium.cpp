/*
 * Copyright (C) 2009 Apple Inc.
 * Copyright (C) 2009 Google Inc.
 * All rights reserved.
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
#include "RenderMediaControlsChromium.h"

#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "PaintInfo.h"
#include "TimeRanges.h"

namespace WebCore {

#if ENABLE(VIDEO)

typedef WTF::HashMap<const char*, Image*> MediaControlImageMap;
static MediaControlImageMap* gMediaControlImageMap = 0;

static Image* platformResource(const char* name)
{
    if (!gMediaControlImageMap)
        gMediaControlImageMap = new MediaControlImageMap();
    if (Image* image = gMediaControlImageMap->get(name))
        return image;
    if (Image* image = Image::loadPlatformResource(name).leakRef()) {
        gMediaControlImageMap->set(name, image);
        return image;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static bool hasSource(const HTMLMediaElement* mediaElement)
{
    return mediaElement->networkState() != HTMLMediaElement::NETWORK_EMPTY
        && mediaElement->networkState() != HTMLMediaElement::NETWORK_NO_SOURCE;
}

static bool paintMediaButton(GraphicsContext* context, const IntRect& rect, Image* image)
{
    context->drawImage(image, ColorSpaceDeviceRGB, rect);
    return true;
}

static bool paintMediaMuteButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
      return false;

    static Image* soundLevel3 = platformResource("mediaplayerSoundLevel3");
    static Image* soundLevel2 = platformResource("mediaplayerSoundLevel2");
    static Image* soundLevel1 = platformResource("mediaplayerSoundLevel1");
    static Image* soundLevel0 = platformResource("mediaplayerSoundLevel0");
    static Image* soundDisabled = platformResource("mediaplayerSoundDisabled");

    if (!hasSource(mediaElement) || !mediaElement->hasAudio())
        return paintMediaButton(paintInfo.context, rect, soundDisabled);

    if (mediaElement->muted() || mediaElement->volume() <= 0)
        return paintMediaButton(paintInfo.context, rect, soundLevel0);

    if (mediaElement->volume() <= 0.33)
        return paintMediaButton(paintInfo.context, rect, soundLevel1);

    if (mediaElement->volume() <= 0.66)
        return paintMediaButton(paintInfo.context, rect, soundLevel2);

    return paintMediaButton(paintInfo.context, rect, soundLevel3);
}

static bool paintMediaPlayButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    static Image* mediaPlay = platformResource("mediaplayerPlay");
    static Image* mediaPause = platformResource("mediaplayerPause");
    static Image* mediaPlayDisabled = platformResource("mediaplayerPlayDisabled");

    if (!hasSource(mediaElement))
        return paintMediaButton(paintInfo.context, rect, mediaPlayDisabled);

    return paintMediaButton(paintInfo.context, rect, mediaElement->canPlay() ? mediaPlay : mediaPause);
}

static bool paintMediaOverlayPlayButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    if (!hasSource(mediaElement) || !mediaElement->canPlay())
        return false;

    static Image* mediaOverlayPlay = platformResource("mediaplayerOverlayPlay");
    return paintMediaButton(paintInfo.context, rect, mediaOverlayPlay);
}

static Image* getMediaSliderThumb()
{
    static Image* mediaSliderThumb = platformResource("mediaplayerSliderThumb");
    return mediaSliderThumb;
}

static void paintRoundedSliderBackground(const IntRect& rect, const RenderStyle* style, GraphicsContext* context)
{
    int borderRadius = rect.height() / 2;
    IntSize radii(borderRadius, borderRadius);
    Color sliderBackgroundColor = Color(11, 11, 11);
    context->save();
    context->fillRoundedRect(rect, radii, radii, radii, radii, sliderBackgroundColor, ColorSpaceDeviceRGB);
    context->restore();
}

static void paintSliderRangeHighlight(const IntRect& rect, const RenderStyle* style, GraphicsContext* context, int startPosition, int endPosition, Color startColor, Color endColor)
{
    // Calculate border radius; need to avoid being smaller than half the slider height
    // because of https://bugs.webkit.org/show_bug.cgi?id=30143.
    int borderRadius = rect.height() / 2;
    IntSize radii(borderRadius, borderRadius);

    // Calculate highlight rectangle and edge dimensions.
    int startOffset = startPosition;
    int endOffset = rect.width() - endPosition;
    int rangeWidth = endPosition - startPosition;

    if (rangeWidth <= 0)
        return;

    // Make sure the range width is bigger than border radius at the edges to retain rounded corners.
    if (startOffset < borderRadius && rangeWidth < borderRadius)
        rangeWidth = borderRadius;
    if (endOffset < borderRadius && rangeWidth < borderRadius) {
        startPosition -= borderRadius - rangeWidth;
        rangeWidth = borderRadius;
    }

    // Set rectangle to highlight range.
    IntRect highlightRect = rect;
    highlightRect.move(startOffset, 0);
    highlightRect.setWidth(rangeWidth);

    // Don't bother drawing an empty area.
    if (highlightRect.isEmpty())
        return;

    // Calculate white-grey gradient.
    IntPoint sliderTopLeft = highlightRect.location();
    IntPoint sliderBottomLeft = sliderTopLeft;
    sliderBottomLeft.move(0, highlightRect.height());
    RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderBottomLeft);
    gradient->addColorStop(0.0, startColor);
    gradient->addColorStop(1.0, endColor);

    // Fill highlight rectangle with gradient, potentially rounded if on left or right edge.
    context->save();
    context->setFillGradient(gradient);

    if (startOffset < borderRadius && endOffset < borderRadius)
        context->fillRoundedRect(highlightRect, radii, radii, radii, radii, startColor, ColorSpaceDeviceRGB);
    else if (startOffset < borderRadius)
        context->fillRoundedRect(highlightRect, radii, IntSize(0, 0), radii, IntSize(0, 0), startColor, ColorSpaceDeviceRGB);
    else if (endOffset < borderRadius)
        context->fillRoundedRect(highlightRect, IntSize(0, 0), radii, IntSize(0, 0), radii, startColor, ColorSpaceDeviceRGB);
    else
        context->fillRect(highlightRect);

    context->restore();
}

const int mediaSliderThumbWidth = 32;

static bool paintMediaSlider(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    RenderStyle* style = object->style();
    GraphicsContext* context = paintInfo.context;

    paintRoundedSliderBackground(rect, style, context);

    // Draw the buffered range. Since the element may have multiple buffered ranges and it'd be
    // distracting/'busy' to show all of them, show only the buffered range containing the current play head.
    RefPtr<TimeRanges> bufferedTimeRanges = mediaElement->buffered();
    float duration = mediaElement->duration();
    float currentTime = mediaElement->currentTime();
    if (std::isnan(duration) || std::isinf(duration) || !duration || std::isnan(currentTime))
        return true;

    for (unsigned i = 0; i < bufferedTimeRanges->length(); ++i) {
        float start = bufferedTimeRanges->start(i, ASSERT_NO_EXCEPTION);
        float end = bufferedTimeRanges->end(i, ASSERT_NO_EXCEPTION);
        if (std::isnan(start) || std::isnan(end) || start > currentTime || end < currentTime)
            continue;
        int startPosition = int(start * rect.width() / duration);
        int currentPosition = int(currentTime * rect.width() / duration);
        int endPosition = int(end * rect.width() / duration);

        // Add half the thumb width proportionally adjusted to the current painting position.
        int thumbCenter = mediaSliderThumbWidth / 2;
        int addWidth = thumbCenter * (1.0 - 2.0 * currentPosition / rect.width());
        currentPosition += addWidth;

        // Draw white-ish highlight before current time.
        Color startColor = Color(195, 195, 195);
        Color endColor = Color(217, 217, 217);
        if (currentPosition > startPosition)
            paintSliderRangeHighlight(rect, style, context, startPosition, currentPosition, startColor, endColor);

        // Draw grey-ish highlight after current time.
        startColor = Color(60, 60, 60);
        endColor = Color(76, 76, 76);

        if (endPosition > currentPosition)
            paintSliderRangeHighlight(rect, style, context, currentPosition, endPosition, startColor, endColor);

        return true;
    }

    return true;
}

static bool paintMediaSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    ASSERT(object->node());
    HTMLMediaElement* mediaElement = toParentMediaElement(object->node()->shadowHost());
    if (!mediaElement)
        return false;

    if (!hasSource(mediaElement))
        return true;

    Image* mediaSliderThumb = getMediaSliderThumb();
    return paintMediaButton(paintInfo.context, rect, mediaSliderThumb);
}

const int mediaVolumeSliderThumbWidth = 24;

static bool paintMediaVolumeSlider(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    GraphicsContext* context = paintInfo.context;
    RenderStyle* style = object->style();

    paintRoundedSliderBackground(rect, style, context);

    // Calculate volume position for white background rectangle.
    float volume = mediaElement->volume();
    if (std::isnan(volume) || volume < 0)
        return true;
    if (volume > 1)
        volume = 1;
    if (!hasSource(mediaElement) || !mediaElement->hasAudio() || mediaElement->muted())
        volume = 0;

    // Calculate the position relative to the center of the thumb.
    float fillWidth = 0;
    if (volume > 0) {
        float thumbCenter = mediaVolumeSliderThumbWidth / 2;
        float zoomLevel = style->effectiveZoom();
        float positionWidth = volume * (rect.width() - (zoomLevel * thumbCenter));
        fillWidth = positionWidth + (zoomLevel * thumbCenter / 2);
    }

    Color startColor = Color(195, 195, 195);
    Color endColor = Color(217, 217, 217);

    paintSliderRangeHighlight(rect, style, context, 0.0, fillWidth, startColor, endColor);

    return true;
}

static bool paintMediaVolumeSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    ASSERT(object->node());
    HTMLMediaElement* mediaElement = toParentMediaElement(object->node()->shadowHost());
    if (!mediaElement)
        return false;

    if (!hasSource(mediaElement) || !mediaElement->hasAudio())
        return true;

    static Image* mediaVolumeSliderThumb = platformResource("mediaplayerVolumeSliderThumb");
    return paintMediaButton(paintInfo.context, rect, mediaVolumeSliderThumb);
}

static bool paintMediaFullscreenButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    static Image* mediaFullscreenButton = platformResource("mediaplayerFullscreen");
    return paintMediaButton(paintInfo.context, rect, mediaFullscreenButton);
}

static bool paintMediaClosedCaptionsButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    static Image* mediaClosedCaptionButton = platformResource("mediaplayerClosedCaption");
    static Image* mediaClosedCaptionButtonDisabled = platformResource("mediaplayerClosedCaptionDisabled");

    if (mediaElement->webkitClosedCaptionsVisible())
        return paintMediaButton(paintInfo.context, rect, mediaClosedCaptionButton);

    return paintMediaButton(paintInfo.context, rect, mediaClosedCaptionButtonDisabled);
}


bool RenderMediaControlsChromium::paintMediaControlsPart(MediaControlElementType part, RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    switch (part) {
    case MediaMuteButton:
    case MediaUnMuteButton:
        return paintMediaMuteButton(object, paintInfo, rect);
    case MediaPauseButton:
    case MediaPlayButton:
        return paintMediaPlayButton(object, paintInfo, rect);
    case MediaShowClosedCaptionsButton:
        return paintMediaClosedCaptionsButton(object, paintInfo, rect);
    case MediaSlider:
        return paintMediaSlider(object, paintInfo, rect);
    case MediaSliderThumb:
        return paintMediaSliderThumb(object, paintInfo, rect);
    case MediaVolumeSlider:
        return paintMediaVolumeSlider(object, paintInfo, rect);
    case MediaVolumeSliderThumb:
        return paintMediaVolumeSliderThumb(object, paintInfo, rect);
    case MediaEnterFullscreenButton:
    case MediaExitFullscreenButton:
        return paintMediaFullscreenButton(object, paintInfo, rect);
    case MediaOverlayPlayButton:
        return paintMediaOverlayPlayButton(object, paintInfo, rect);
    case MediaVolumeSliderMuteButton:
    case MediaSeekBackButton:
    case MediaSeekForwardButton:
    case MediaVolumeSliderContainer:
    case MediaTimelineContainer:
    case MediaCurrentTimeDisplay:
    case MediaTimeRemainingDisplay:
    case MediaControlsPanel:
    case MediaRewindButton:
    case MediaReturnToRealtimeButton:
    case MediaStatusDisplay:
    case MediaHideClosedCaptionsButton:
    case MediaTextTrackDisplayContainer:
    case MediaTextTrackDisplay:
    case MediaFullScreenVolumeSlider:
    case MediaFullScreenVolumeSliderThumb:
    case MediaClosedCaptionsContainer:
    case MediaClosedCaptionsTrackList:
        ASSERT_NOT_REACHED();
        break;
    }
    return false;
}

const int mediaSliderThumbHeight = 24;
const int mediaVolumeSliderThumbHeight = 24;

void RenderMediaControlsChromium::adjustMediaSliderThumbSize(RenderStyle* style)
{
    static Image* mediaSliderThumb = platformResource("mediaplayerSliderThumb");
    static Image* mediaVolumeSliderThumb = platformResource("mediaplayerVolumeSliderThumb");
    int width = 0;
    int height = 0;

    Image* thumbImage = 0;
    if (style->appearance() == MediaSliderThumbPart) {
        thumbImage = mediaSliderThumb;
        width = mediaSliderThumbWidth;
        height = mediaSliderThumbHeight;
    } else if (style->appearance() == MediaVolumeSliderThumbPart) {
        thumbImage = mediaVolumeSliderThumb;
        width = mediaVolumeSliderThumbWidth;
        height = mediaVolumeSliderThumbHeight;
    }

    float zoomLevel = style->effectiveZoom();
    if (thumbImage) {
        style->setWidth(Length(static_cast<int>(width * zoomLevel), Fixed));
        style->setHeight(Length(static_cast<int>(height * zoomLevel), Fixed));
    }
}

static String formatChromiumMediaControlsTime(float time, float duration)
{
    if (!std::isfinite(time))
        time = 0;
    if (!std::isfinite(duration))
        duration = 0;
    int seconds = static_cast<int>(fabsf(time));
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    // duration defines the format of how the time is rendered
    int durationSecs = static_cast<int>(fabsf(duration));
    int durationHours = durationSecs / (60 * 60);
    int durationMins = (durationSecs / 60) % 60;

    if (durationHours || hours)
        return String::format("%s%01d:%02d:%02d", (time < 0 ? "-" : ""), hours, minutes, seconds);
    if (durationMins > 9)
        return String::format("%s%02d:%02d", (time < 0 ? "-" : ""), minutes, seconds);

    return String::format("%s%01d:%02d", (time < 0 ? "-" : ""), minutes, seconds);
}

String RenderMediaControlsChromium::formatMediaControlsTime(float time)
{
    return formatChromiumMediaControlsTime(time, time);
}

String RenderMediaControlsChromium::formatMediaControlsCurrentTime(float currentTime, float duration)
{
    return formatChromiumMediaControlsTime(currentTime, duration);
}

String RenderMediaControlsChromium::formatMediaControlsRemainingTime(float currentTime, float duration)
{
    return formatChromiumMediaControlsTime(currentTime - duration, duration);
}

#endif  // #if ENABLE(VIDEO)

} // namespace WebCore
