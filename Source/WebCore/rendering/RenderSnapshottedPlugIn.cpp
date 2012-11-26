/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderSnapshottedPlugIn.h"

#include "Cursor.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "Gradient.h"
#include "HTMLPlugInImageElement.h"
#include "MouseEvent.h"
#include "PaintInfo.h"
#include "Path.h"

namespace WebCore {

static const int autoStartPlugInSizeThresholdWidth = 1;
static const int autoStartPlugInSizeThresholdHeight = 1;
static const int startButtonPadding = 10;

RenderSnapshottedPlugIn::RenderSnapshottedPlugIn(HTMLPlugInImageElement* element)
    : RenderEmbeddedObject(element)
    , m_snapshotResource(RenderImageResource::create())
    , m_isMouseInButtonRect(false)
{
    m_snapshotResource->initialize(this);
}

RenderSnapshottedPlugIn::~RenderSnapshottedPlugIn()
{
    ASSERT(m_snapshotResource);
    m_snapshotResource->shutdown();
}

HTMLPlugInImageElement* RenderSnapshottedPlugIn::plugInImageElement() const
{
    return static_cast<HTMLPlugInImageElement*>(node());
}

void RenderSnapshottedPlugIn::updateSnapshot(PassRefPtr<Image> image)
{
    // Zero-size plugins will have no image.
    if (!image)
        return;

    m_snapshotResource->setCachedImage(new CachedImage(image.get()));
    repaint();
}

void RenderSnapshottedPlugIn::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (plugInImageElement()->displayState() < HTMLPlugInElement::PlayingWithPendingMouseClick) {
        RenderReplaced::paint(paintInfo, paintOffset);
        return;
    }

    RenderEmbeddedObject::paint(paintInfo, paintOffset);
}

void RenderSnapshottedPlugIn::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (plugInImageElement()->displayState() < HTMLPlugInElement::PlayingWithPendingMouseClick) {
        paintReplacedSnapshot(paintInfo, paintOffset);
        paintButton(paintInfo, paintOffset);
        return;
    }

    RenderEmbeddedObject::paintReplaced(paintInfo, paintOffset);
}

void RenderSnapshottedPlugIn::paintReplacedSnapshot(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // This code should be similar to RenderImage::paintReplaced() and RenderImage::paintIntoRect().
    LayoutUnit cWidth = contentWidth();
    LayoutUnit cHeight = contentHeight();
    if (!cWidth || !cHeight)
        return;

    RefPtr<Image> image = m_snapshotResource->image();
    if (!image || image->isNull())
        return;

    GraphicsContext* context = paintInfo.context;
#if PLATFORM(MAC)
    if (style()->highlight() != nullAtom && !context->paintingDisabled())
        paintCustomHighlight(toPoint(paintOffset - location()), style()->highlight(), true);
#endif

    LayoutSize contentSize(cWidth, cHeight);
    LayoutPoint contentLocation = paintOffset;
    contentLocation.move(borderLeft() + paddingLeft(), borderTop() + paddingTop());

    LayoutRect rect(contentLocation, contentSize);
    IntRect alignedRect = pixelSnappedIntRect(rect);
    if (alignedRect.width() <= 0 || alignedRect.height() <= 0)
        return;

    bool useLowQualityScaling = shouldPaintAtLowQuality(context, image.get(), image.get(), alignedRect.size());
    context->drawImage(image.get(), style()->colorSpace(), alignedRect, CompositeSourceOver, shouldRespectImageOrientation(), useLowQualityScaling);
}

static Image* startButtonImage()
{
    static Image* buttonImage = Image::loadPlatformResource("startButton").leakRef();
    return buttonImage;
}

static Image* startButtonPressedImage()
{
    static Image* buttonImage = Image::loadPlatformResource("startButtonPressed").leakRef();
    return buttonImage;
}

void RenderSnapshottedPlugIn::paintButton(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutRect contentRect = contentBoxRect();
    if (contentRect.isEmpty())
        return;

    Image* buttonImage = startButtonImage();
    if (plugInImageElement()->active()) {
        if (m_isMouseInButtonRect)
            buttonImage = startButtonPressedImage();
    } else if (!plugInImageElement()->hovered())
        return;

    LayoutPoint contentLocation = paintOffset + contentRect.maxXMaxYCorner() - buttonImage->size() - LayoutSize(startButtonPadding, startButtonPadding);
    paintInfo.context->drawImage(buttonImage, ColorSpaceDeviceRGB, roundedIntPoint(contentLocation), buttonImage->rect());
}

void RenderSnapshottedPlugIn::repaintButton()
{
    // FIXME: This is unfortunate. We should just repaint the button.
    repaint();
}

CursorDirective RenderSnapshottedPlugIn::getCursor(const LayoutPoint& point, Cursor& overrideCursor) const
{
    if (plugInImageElement()->displayState() < HTMLPlugInElement::PlayingWithPendingMouseClick) {
        overrideCursor = handCursor();
        return SetCursor;
    }
    return RenderEmbeddedObject::getCursor(point, overrideCursor);
}

void RenderSnapshottedPlugIn::handleEvent(Event* event)
{
    if (!event->isMouseEvent())
        return;

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);

    if (event->type() == eventNames().clickEvent && mouseEvent->button() == LeftButton) {
        if (m_isMouseInButtonRect)
            plugInImageElement()->setDisplayState(HTMLPlugInElement::Playing);
        else {
            plugInImageElement()->setDisplayState(HTMLPlugInElement::PlayingWithPendingMouseClick);
            plugInImageElement()->setPendingClickEvent(mouseEvent);
        }
        if (widget()) {
            if (Frame* frame = document()->frame())
                frame->loader()->client()->recreatePlugin(widget());
            repaint();
        }
        event->setDefaultHandled();
    } else if (event->type() == eventNames().mouseoverEvent || event->type() == eventNames().mouseoutEvent)
        repaintButton();
    else if (event->type() == eventNames().mousedownEvent) {
        bool isMouseInButtonRect = m_buttonRect.contains(IntPoint(mouseEvent->offsetX(), mouseEvent->offsetY()));
        if (isMouseInButtonRect != m_isMouseInButtonRect) {
            m_isMouseInButtonRect = isMouseInButtonRect;
            repaintButton();
        }
    }
}

void RenderSnapshottedPlugIn::layout()
{
    RenderEmbeddedObject::layout();
    if (plugInImageElement()->displayState() < HTMLPlugInElement::Playing) {
        LayoutRect rect = contentBoxRect();
        int width = rect.width();
        int height = rect.height();
        if (!width || !height || (width <= autoStartPlugInSizeThresholdWidth && height <= autoStartPlugInSizeThresholdHeight))
            plugInImageElement()->setDisplayState(HTMLPlugInElement::Playing);
    }

    LayoutSize buttonSize = startButtonImage()->size();
    m_buttonRect = LayoutRect(contentBoxRect().maxXMaxYCorner() - LayoutSize(startButtonPadding, startButtonPadding) - buttonSize, buttonSize);
}

} // namespace WebCore
