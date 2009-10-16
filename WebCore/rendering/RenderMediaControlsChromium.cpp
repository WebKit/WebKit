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
    if (Image* image = Image::loadPlatformResource(name).releaseRef()) {
        gMediaControlImageMap->set(name, image);
        return image;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static bool paintMediaButton(GraphicsContext* context, const IntRect& rect, Image* image)
{
    // Create a destination rectangle for the image that is centered in the drawing rectangle, rounded left, and down.
    IntRect imageRect = image->rect();
    imageRect.setY(rect.y() + (rect.height() - image->height() + 1) / 2);
    imageRect.setX(rect.x() + (rect.width() - image->width() + 1) / 2);

    context->drawImage(image, imageRect);
    return true;
}

static bool paintMediaMuteButton(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
      return false;

    static Image* soundFull = platformResource("mediaSoundFull");
    static Image* soundNone = platformResource("mediaSoundNone");
    static Image* soundDisabled = platformResource("mediaSoundDisabled");

    if (mediaElement->networkState() == HTMLMediaElement::NETWORK_NO_SOURCE || !mediaElement->hasAudio())
        return paintMediaButton(paintInfo.context, rect, soundDisabled);

    return paintMediaButton(paintInfo.context, rect, mediaElement->muted() ? soundNone: soundFull);
}

static bool paintMediaPlayButton(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    static Image* mediaPlay = platformResource("mediaPlay");
    static Image* mediaPause = platformResource("mediaPause");
    static Image* mediaPlayDisabled = platformResource("mediaPlayDisabled");

    if (mediaElement->networkState() == HTMLMediaElement::NETWORK_EMPTY ||
        mediaElement->networkState() == HTMLMediaElement::NETWORK_NO_SOURCE)
        return paintMediaButton(paintInfo.context, rect, mediaPlayDisabled);

    return paintMediaButton(paintInfo.context, rect, mediaElement->paused() ? mediaPlay : mediaPause);
}

static bool paintMediaSlider(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
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
    context->setStrokeStyle(SolidStroke);
    context->setStrokeColor(style->borderLeftColor());
    context->setStrokeThickness(style->borderLeftWidth());
    context->setFillColor(style->backgroundColor());
    context->drawRect(rect);
    context->restore();

    // Draw the buffered ranges.
    // FIXME: Draw multiple ranges if there are multiple buffered ranges.
    // FIXME: percentLoaded() doesn't always hit 1.0 so we're using round().
    IntRect bufferedRect = rect;
    bufferedRect.inflate(-style->borderLeftWidth());
    bufferedRect.setWidth(round((bufferedRect.width() * mediaElement->percentLoaded())));

    // Don't bother drawing an empty area.
    if (!bufferedRect.isEmpty()) {
        IntPoint sliderTopLeft = bufferedRect.location();
        IntPoint sliderTopRight = sliderTopLeft;
        sliderTopRight.move(0, bufferedRect.height());

        RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderTopRight);
        Color startColor = object->style()->color();
        gradient->addColorStop(0.0, startColor);
        gradient->addColorStop(1.0, Color(startColor.red() / 2, startColor.green() / 2, startColor.blue() / 2, startColor.alpha()));

        context->save();
        context->setStrokeStyle(NoStroke);
        context->setFillGradient(gradient);
        context->drawRect(bufferedRect);
        context->restore();
    }

    return true;
}

static bool paintMediaSliderThumb(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
    if (!object->parent()->isSlider())
        return false;

    static Image* mediaSliderThumb = platformResource("mediaSliderThumb");
    return paintMediaButton(paintInfo.context, rect, mediaSliderThumb);
}

static bool paintMediaVolumeSlider(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    GraphicsContext* context = paintInfo.context;
    Color originalColor = context->strokeColor();
    if (originalColor != Color::white)
        context->setStrokeColor(Color::white);

    int x = rect.x() + rect.width() / 2;
    context->drawLine(IntPoint(x, rect.y()),  IntPoint(x, rect.y() + rect.height()));

    if (originalColor != Color::white)
        context->setStrokeColor(originalColor);
    return true;
}

static bool paintMediaVolumeSliderThumb(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
    if (!object->parent()->isSlider())
        return false;

    static Image* mediaVolumeSliderThumb = platformResource("mediaVolumeSliderThumb");
    return paintMediaButton(paintInfo.context, rect, mediaVolumeSliderThumb);
}

static bool paintMediaTimelineContainer(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(object);
    if (!mediaElement)
        return false;

    if (!rect.isEmpty()) {
        GraphicsContext* context = paintInfo.context;
        Color originalColor = context->strokeColor();
        float originalThickness = context->strokeThickness();
        StrokeStyle originalStyle = context->strokeStyle();

        context->setStrokeStyle(SolidStroke);

        // Draw the left border using CSS defined width and color.
        context->setStrokeThickness(object->style()->borderLeftWidth());
        context->setStrokeColor(object->style()->borderLeftColor().rgb());
        context->drawLine(IntPoint(rect.x() + 1, rect.y()),
                          IntPoint(rect.x() + 1, rect.y() + rect.height()));

        // Draw the right border using CSS defined width and color.
        context->setStrokeThickness(object->style()->borderRightWidth());
        context->setStrokeColor(object->style()->borderRightColor().rgb());
        context->drawLine(IntPoint(rect.x() + rect.width() - 1, rect.y()),
                          IntPoint(rect.x() + rect.width() - 1, rect.y() + rect.height()));

        context->setStrokeColor(originalColor);
        context->setStrokeThickness(originalThickness);
        context->setStrokeStyle(originalStyle);
    }
    return true;
}

bool RenderMediaControlsChromium::shouldRenderMediaControlPart(ControlPart part, Element* e)
{
    UNUSED_PARAM(e);

    switch (part) {
    case MediaMuteButtonPart:
    case MediaPlayButtonPart:
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaVolumeSliderContainerPart:
    case MediaVolumeSliderPart:
    case MediaVolumeSliderThumbPart:
    case MediaControlsBackgroundPart:
    case MediaCurrentTimePart:
    case MediaTimeRemainingPart:
        return true;
    }
    return false;
}

bool RenderMediaControlsChromium::paintMediaControlsPart(MediaControlElementType part, RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
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
    case MediaTimelineContainer:
        return paintMediaTimelineContainer(object, paintInfo, rect);
    case MediaFullscreenButton:
    case MediaSeekBackButton:
    case MediaSeekForwardButton:
    case MediaVolumeSliderContainer:
    case MediaCurrentTimeDisplay:
    case MediaTimeRemainingDisplay:
    case MediaControlsPanel:
        ASSERT_NOT_REACHED();
        break;
    }
    return false;
}

void RenderMediaControlsChromium::adjustMediaSliderThumbSize(RenderObject* object)
{
    static Image* mediaSliderThumb = platformResource("mediaSliderThumb");
    static Image* mediaVolumeSliderThumb = platformResource("mediaVolumeSliderThumb");

    Image* thumbImage = 0;
    if (object->style()->appearance() == MediaSliderThumbPart)
        thumbImage = mediaSliderThumb;
    else if (object->style()->appearance() == MediaVolumeSliderThumbPart)
        thumbImage = mediaVolumeSliderThumb;

    if (thumbImage) {
        object->style()->setWidth(Length(thumbImage->width(), Fixed));
        object->style()->setHeight(Length(thumbImage->height(), Fixed));
    }
}

#endif  // #if ENABLE(VIDEO)

} // namespace WebCore
