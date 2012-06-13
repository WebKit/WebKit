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
    static Image* soundDisabled = platformResource("mediaplayerSoundDisabled");

    if (!hasSource(mediaElement) || !mediaElement->hasAudio() || mediaElement->muted() || mediaElement->volume() <= 0)
        return paintMediaButton(paintInfo.context, rect, soundDisabled);

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

static Image* getMediaSliderThumb()
{
    static Image* mediaSliderThumb = platformResource("mediaplayerSliderThumb");
    return mediaSliderThumb;
}

static bool paintMediaSlider(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    RenderStyle* style = object->style();
    GraphicsContext* context = paintInfo.context;

    // Draw the border of the time bar.
    // FIXME: this should be a rounded rect but need to fix GraphicsContextSkia first.
    // https://bugs.webkit.org/show_bug.cgi?id=30143
    context->save();
    context->setShouldAntialias(true);
    context->setStrokeStyle(SolidStroke);
    context->setStrokeColor(style->visitedDependentColor(CSSPropertyBorderLeftColor), ColorSpaceDeviceRGB);
    context->setStrokeThickness(style->borderLeftWidth());
    context->setFillColor(style->visitedDependentColor(CSSPropertyBackgroundColor), ColorSpaceDeviceRGB);
    context->drawRect(rect);
    context->restore();

    // Draw the buffered range. Since the element may have multiple buffered ranges and it'd be
    // distracting/'busy' to show all of them, show only the buffered range containing the current play head.
    IntRect bufferedRect = rect;
    bufferedRect.inflate(-style->borderLeftWidth());

    RefPtr<TimeRanges> bufferedTimeRanges = mediaElement->buffered();
    float duration = mediaElement->duration();
    float currentTime = mediaElement->currentTime();
    if (isnan(duration) || isinf(duration) || !duration || isnan(currentTime))
        return true;

    for (unsigned i = 0; i < bufferedTimeRanges->length(); ++i) {
        float start = bufferedTimeRanges->start(i, ASSERT_NO_EXCEPTION);
        float end = bufferedTimeRanges->end(i, ASSERT_NO_EXCEPTION);
        if (isnan(start) || isnan(end) || start > currentTime || end < currentTime)
            continue;
        float startFraction = start / duration;
        float endFraction = end / duration;
        float widthFraction = endFraction - startFraction;
        bufferedRect.move(startFraction * bufferedRect.width(), 0);
        bufferedRect.setWidth(widthFraction * bufferedRect.width());

        // Don't bother drawing an empty area.
        if (bufferedRect.isEmpty())
            return true;

        IntPoint sliderTopLeft = bufferedRect.location();
        IntPoint sliderBottomLeft = sliderTopLeft;
        sliderBottomLeft.move(0, bufferedRect.height());

        RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderBottomLeft);
        Color startColor = object->style()->visitedDependentColor(CSSPropertyColor);
        gradient->addColorStop(0.0, startColor);
        gradient->addColorStop(1.0, Color(startColor.red() / 2, startColor.green() / 2, startColor.blue() / 2, startColor.alpha()));

        context->save();
        context->setStrokeStyle(NoStroke);
        context->setFillGradient(gradient);
        context->fillRect(bufferedRect);
        context->restore();
        return true;
    }

    return true;
}

static bool paintMediaSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    ASSERT(object->node());
    Node* hostNode = object->node()->shadowAncestorNode();
    ASSERT(hostNode);
    HTMLMediaElement* mediaElement = toParentMediaElement(hostNode);
    if (!mediaElement)
        return false;

    if (!hasSource(mediaElement))
        return true;

    Image* mediaSliderThumb = getMediaSliderThumb();
    return paintMediaButton(paintInfo.context, rect, mediaSliderThumb);
}

static bool paintMediaVolumeSlider(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    GraphicsContext* context = paintInfo.context;
    Color originalColor = context->strokeColor();
    if (originalColor != Color::white)
        context->setStrokeColor(Color::white, ColorSpaceDeviceRGB);

    int y = rect.y() + rect.height() / 2;
    context->drawLine(IntPoint(rect.x(), y),  IntPoint(rect.x() + rect.width(), y));

    if (originalColor != Color::white)
        context->setStrokeColor(originalColor, ColorSpaceDeviceRGB);
    return true;
}

static bool paintMediaVolumeSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    static Image* mediaVolumeSliderThumb = platformResource("mediaplayerVolumeSliderThumb");
    return paintMediaButton(paintInfo.context, rect, mediaVolumeSliderThumb);
}

static bool paintMediaFullscreenButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    DEFINE_STATIC_LOCAL(Image*, mediaFullscreen, (platformResource("mediaFullscreen")));
    return paintMediaButton(paintInfo.context, rect, mediaFullscreen);
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
    case MediaShowClosedCaptionsButton:
    case MediaHideClosedCaptionsButton:
    case MediaTextTrackDisplayContainer:
    case MediaTextTrackDisplay:
    case MediaFullScreenVolumeSlider:
    case MediaFullScreenVolumeSliderThumb:
        ASSERT_NOT_REACHED();
        break;
    }
    return false;
}

const int mediaSliderThumbWidth = 32;
const int mediaSliderThumbHeight = 24;
const int mediaVolumeSliderThumbWidth = 24;
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

#endif  // #if ENABLE(VIDEO)

} // namespace WebCore
