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
static const int startLabelPadding = 10;
static const double hoverDelay = 1;

RenderSnapshottedPlugIn::RenderSnapshottedPlugIn(HTMLPlugInImageElement* element)
    : RenderEmbeddedObject(element)
    , m_snapshotResource(RenderImageResource::create())
    , m_shouldShowLabel(false)
    , m_hoverDelayTimer(this, &RenderSnapshottedPlugIn::hoverDelayTimerFired, hoverDelay)
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
        if (m_shouldShowLabel)
            paintLabel(paintInfo, paintOffset);
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

static Image* startLabelImage()
{
    static Image* labelImage = Image::loadPlatformResource("startButton").leakRef();
    return labelImage;
}

void RenderSnapshottedPlugIn::paintLabel(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutRect contentRect = contentBoxRect();
    if (contentRect.isEmpty())
        return;

    if (!plugInImageElement()->hovered())
        return;

    Image* labelImage = startLabelImage();
    LayoutPoint contentLocation = paintOffset + contentRect.maxXMaxYCorner() - labelImage->size() - LayoutSize(startLabelPadding, startLabelPadding);
    paintInfo.context->drawImage(labelImage, ColorSpaceDeviceRGB, roundedIntPoint(contentLocation), labelImage->rect());
}

void RenderSnapshottedPlugIn::repaintLabel()
{
    // FIXME: This is unfortunate. We should just repaint the label.
    repaint();
}

void RenderSnapshottedPlugIn::hoverDelayTimerFired(DeferrableOneShotTimer<RenderSnapshottedPlugIn>*)
{
    m_shouldShowLabel = true;
    repaintLabel();
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

    if (event->type() == eventNames().clickEvent) {
        if (mouseEvent->button() != LeftButton)
            return;

        plugInImageElement()->setDisplayState(HTMLPlugInElement::PlayingWithPendingMouseClick);
        plugInImageElement()->setPendingClickEvent(mouseEvent);

        if (widget()) {
            if (Frame* frame = document()->frame())
                frame->loader()->client()->recreatePlugin(widget());
            repaint();
        }
        event->setDefaultHandled();
    } else if (event->type() == eventNames().mousedownEvent) {
        if (mouseEvent->button() != LeftButton)
            return;

        if (m_hoverDelayTimer.isActive())
            m_hoverDelayTimer.stop();

        event->setDefaultHandled();
    } else if (event->type() == eventNames().mouseoverEvent) {
        m_hoverDelayTimer.restart();
        event->setDefaultHandled();
    } else if (event->type() == eventNames().mouseoutEvent) {
        if (m_hoverDelayTimer.isActive())
            m_hoverDelayTimer.stop();
        m_shouldShowLabel = false;
        repaintLabel();
        event->setDefaultHandled();
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

    LayoutSize labelSize = startLabelImage()->size();
    m_labelRect = LayoutRect(contentBoxRect().maxXMaxYCorner() - LayoutSize(startLabelPadding, startLabelPadding) - labelSize, labelSize);
}

} // namespace WebCore
