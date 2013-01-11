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

#include "Chrome.h"
#include "ChromeClient.h"
#include "Cursor.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "Gradient.h"
#include "HTMLPlugInImageElement.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PaintInfo.h"
#include "Path.h"

namespace WebCore {

static const int autoStartPlugInSizeThresholdWidth = 1;
static const int autoStartPlugInSizeThresholdHeight = 1;
static const int startLabelPadding = 10; // Label should be 10px from edge of box.
static const int startLabelInset = 20; // But the label is inset from its box also. FIXME: This will be removed when we go to a ShadowDOM approach.
static const double showLabelAfterMouseOverDelay = 1;
static const double showLabelAutomaticallyDelay = 3;

RenderSnapshottedPlugIn::RenderSnapshottedPlugIn(HTMLPlugInImageElement* element)
    : RenderEmbeddedObject(element)
    , m_snapshotResource(RenderImageResource::create())
    , m_shouldShowLabel(false)
    , m_shouldShowLabelAutomatically(false)
    , m_showedLabelOnce(false)
    , m_showReason(UserMousedOver)
    , m_showLabelDelayTimer(this, &RenderSnapshottedPlugIn::showLabelDelayTimerFired)
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
    if (m_shouldShowLabelAutomatically)
        resetDelayTimer(ShouldShowAutomatically);
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

Image* RenderSnapshottedPlugIn::startLabelImage(LabelSize size) const
{
    static Image* labelImages[2] = { 0, 0 };
    static bool initializedImages[2] = { false, false };

    int arrayIndex = static_cast<int>(size);
    if (labelImages[arrayIndex])
        return labelImages[arrayIndex];
    if (initializedImages[arrayIndex])
        return 0;

    if (document()->page()) {
        labelImages[arrayIndex] = document()->page()->chrome()->client()->plugInStartLabelImage(size).leakRef();
        initializedImages[arrayIndex] = true;
    }
    return labelImages[arrayIndex];
}

void RenderSnapshottedPlugIn::paintLabel(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (contentBoxRect().isEmpty())
        return;

    if (!plugInImageElement()->hovered() && m_showReason == UserMousedOver)
        return;

    m_showedLabelOnce = true;
    LayoutRect rect = contentBoxRect();
    LayoutRect labelRect = tryToFitStartLabel(LabelSizeLarge, rect);
    LabelSize size = NoLabel;
    if (!labelRect.isEmpty())
        size = LabelSizeLarge;
    else {
        labelRect = tryToFitStartLabel(LabelSizeSmall, rect);
        if (!labelRect.isEmpty())
            size = LabelSizeSmall;
        else
            return;
    }

    Image* labelImage = startLabelImage(size);
    if (!labelImage)
        return;

    // Remember that the labelRect includes the label inset, so we need to adjust for it.
    paintInfo.context->drawImage(labelImage, ColorSpaceDeviceRGB,
                                 IntRect(roundedIntPoint(paintOffset + labelRect.location() - IntSize(startLabelInset, startLabelInset)),
                                         roundedIntSize(labelRect.size() + IntSize(2 * startLabelInset, 2 * startLabelInset))),
                                 labelImage->rect());
}

void RenderSnapshottedPlugIn::repaintLabel()
{
    // FIXME: This is unfortunate. We should just repaint the label.
    repaint();
}

void RenderSnapshottedPlugIn::showLabelDelayTimerFired(Timer<RenderSnapshottedPlugIn>*)
{
    m_shouldShowLabel = true;
    repaintLabel();
}

void RenderSnapshottedPlugIn::setShouldShowLabelAutomatically(bool show)
{
    m_shouldShowLabelAutomatically = show;
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
        plugInImageElement()->userDidClickSnapshot(mouseEvent);

        if (widget()) {
            if (Frame* frame = document()->frame())
                frame->loader()->client()->recreatePlugin(widget());
            repaint();
        }
        event->setDefaultHandled();
    } else if (event->type() == eventNames().mousedownEvent) {
        if (mouseEvent->button() != LeftButton)
            return;

        if (m_showLabelDelayTimer.isActive())
            m_showLabelDelayTimer.stop();

        event->setDefaultHandled();
    } else if (event->type() == eventNames().mouseoverEvent) {
        if (!m_showedLabelOnce || m_showReason != ShouldShowAutomatically)
            resetDelayTimer(UserMousedOver);
        event->setDefaultHandled();
    } else if (event->type() == eventNames().mouseoutEvent) {
        if (m_showLabelDelayTimer.isActive())
            m_showLabelDelayTimer.stop();
        if (m_shouldShowLabel) {
            if (m_showReason == UserMousedOver) {
                m_shouldShowLabel = false;
                repaintLabel();
            }
        } else if (m_shouldShowLabelAutomatically)
            resetDelayTimer(ShouldShowAutomatically);
        event->setDefaultHandled();
    }
}

LayoutRect RenderSnapshottedPlugIn::tryToFitStartLabel(LabelSize size, const LayoutRect& contentBox) const
{
    Image* labelImage = startLabelImage(size);
    if (!labelImage)
        return LayoutRect();

    // Assume that the labelImage has been provided to match our device scale.
    float scaleFactor = 1;
    if (document()->page())
        scaleFactor = document()->page()->deviceScaleFactor();
    IntSize labelImageSize = labelImage->size();
    labelImageSize.scale(1 / (scaleFactor ? scaleFactor : 1));

    LayoutSize labelSize = labelImageSize - LayoutSize(2 * startLabelInset, 2 * startLabelInset);
    LayoutRect candidateRect(contentBox.maxXMinYCorner() + LayoutSize(-startLabelPadding, startLabelPadding) + LayoutSize(-labelSize.width(), 0), labelSize);
    // The minimum allowed content box size is the label image placed in the center of the box, surrounded by startLabelPadding.
    if (candidateRect.x() < startLabelPadding || candidateRect.maxY() > contentBox.height() - startLabelPadding)
        return LayoutRect();
    return candidateRect;
}

void RenderSnapshottedPlugIn::resetDelayTimer(ShowReason reason)
{
    m_showReason = reason;
    m_showLabelDelayTimer.startOneShot(reason == UserMousedOver ? showLabelAfterMouseOverDelay : showLabelAutomaticallyDelay);
}

} // namespace WebCore
