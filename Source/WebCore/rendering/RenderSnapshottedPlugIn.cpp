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

#include "CachedImage.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Cursor.h"
#include "FEGaussianBlur.h"
#include "Filter.h"
#include "FrameView.h"
#include "Gradient.h"
#include "HTMLPlugInImageElement.h"
#include "ImageBuffer.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PaintInfo.h"
#include "Path.h"
#include "RenderView.h"
#include "SourceGraphic.h"

namespace WebCore {

static const int autoStartPlugInSizeThresholdWidth = 1;
static const int autoStartPlugInSizeThresholdHeight = 1;
static const double showLabelAfterMouseOverDelay = 1;
static const double showLabelAutomaticallyDelay = 3;
static const int snapshotLabelBlurRadius = 5;

#if ENABLE(FILTERS)
class RenderSnapshottedPlugInBlurFilter : public Filter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<RenderSnapshottedPlugInBlurFilter> create(int radius)
    {
        return adoptRef(new RenderSnapshottedPlugInBlurFilter(radius));
    }

    void setSourceImageRect(const FloatRect& r)
    {
        m_sourceImageRect = r;
        m_filterRegion = r;
        m_sourceGraphic->setMaxEffectRect(r);
        m_blur->setMaxEffectRect(r);
    }
    virtual FloatRect sourceImageRect() const { return m_sourceImageRect; }
    virtual FloatRect filterRegion() const { return m_filterRegion; }

    void apply();
    ImageBuffer* output() const { return m_blur->asImageBuffer(); }

private:
    RenderSnapshottedPlugInBlurFilter(int radius);

    FloatRect m_sourceImageRect;
    FloatRect m_filterRegion;
    RefPtr<SourceGraphic> m_sourceGraphic;
    RefPtr<FEGaussianBlur> m_blur;
};

RenderSnapshottedPlugInBlurFilter::RenderSnapshottedPlugInBlurFilter(int radius)
{
    setFilterResolution(FloatSize(1, 1));
    m_sourceGraphic = SourceGraphic::create(this);
    m_blur = FEGaussianBlur::create(this, radius, radius);
    m_blur->inputEffects().append(m_sourceGraphic);
}

void RenderSnapshottedPlugInBlurFilter::apply()
{
    m_sourceGraphic->clearResult();
    m_blur->clearResult();
    m_blur->apply();
}
#endif

RenderSnapshottedPlugIn::RenderSnapshottedPlugIn(HTMLPlugInImageElement* element)
    : RenderBlock(element)
    , m_snapshotResource(RenderImageResource::create())
    , m_shouldShowLabel(false)
    , m_shouldShowLabelAutomatically(false)
    , m_showedLabelOnce(false)
    , m_showReason(UserMousedOver)
    , m_showLabelDelayTimer(this, &RenderSnapshottedPlugIn::showLabelDelayTimerFired)
    , m_snapshotResourceForLabel(RenderImageResource::create())
{
    m_snapshotResource->initialize(this);
    m_snapshotResourceForLabel->initialize(this);
}

RenderSnapshottedPlugIn::~RenderSnapshottedPlugIn()
{
    ASSERT(m_snapshotResource);
    m_snapshotResource->shutdown();
    ASSERT(m_snapshotResourceForLabel);
    m_snapshotResourceForLabel->shutdown();
}

HTMLPlugInImageElement* RenderSnapshottedPlugIn::plugInImageElement() const
{
    return static_cast<HTMLPlugInImageElement*>(node());
}

void RenderSnapshottedPlugIn::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    LayoutSize oldSize = contentBoxRect().size();

    RenderBlock::layout();

    LayoutSize newSize = contentBoxRect().size();
    if (newSize == oldSize)
        return;

    if (document()->view())
        document()->view()->addWidgetToUpdate(this);
}

void RenderSnapshottedPlugIn::updateSnapshot(PassRefPtr<Image> image)
{
    // Zero-size plugins will have no image.
    if (!image)
        return;

    // We may have stored a version of this snapshot to use when showing the
    // label. Invalidate it now and it will be regenerated later.
    if (m_snapshotResourceForLabel->hasImage())
        m_snapshotResourceForLabel->setCachedImage(0);

    m_snapshotResource->setCachedImage(new CachedImage(image.get()));
    repaint();
    if (m_shouldShowLabelAutomatically)
        resetDelayTimer(ShouldShowAutomatically);
}

void RenderSnapshottedPlugIn::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase == PaintPhaseBlockBackground && plugInImageElement()->displayState() < HTMLPlugInElement::PlayingWithPendingMouseClick) {
        if (m_shouldShowLabel)
            paintSnapshotWithLabel(paintInfo, paintOffset);
        else
            paintSnapshot(paintInfo, paintOffset);
    }

    RenderBlock::paint(paintInfo, paintOffset);
}

void RenderSnapshottedPlugIn::paintSnapshotImage(Image* image, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutUnit cWidth = contentWidth();
    LayoutUnit cHeight = contentHeight();
    if (!cWidth || !cHeight)
        return;

    GraphicsContext* context = paintInfo.context;
#if PLATFORM(MAC)
    if (style()->highlight() != nullAtom && !context->paintingDisabled())
        paintCustomHighlight(toPoint(paintOffset - location()), style()->highlight(), true);
#endif

    LayoutSize contentSize(cWidth, cHeight);
    LayoutPoint contentLocation = location() + paintOffset;
    contentLocation.move(borderLeft() + paddingLeft(), borderTop() + paddingTop());

    LayoutRect rect(contentLocation, contentSize);
    IntRect alignedRect = pixelSnappedIntRect(rect);
    if (alignedRect.width() <= 0 || alignedRect.height() <= 0)
        return;

    bool useLowQualityScaling = shouldPaintAtLowQuality(context, image, image, alignedRect.size());
    context->drawImage(image, style()->colorSpace(), alignedRect, CompositeSourceOver, shouldRespectImageOrientation(), useLowQualityScaling);
}

void RenderSnapshottedPlugIn::paintSnapshot(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    RefPtr<Image> image = m_snapshotResource->image();
    if (!image || image->isNull())
        return;

    paintSnapshotImage(image.get(), paintInfo, paintOffset);
}

#if ENABLE(FILTERS)
static PassRefPtr<Image> snapshottedPluginImageForLabelDisplay(PassRefPtr<Image> snapshot, const LayoutRect& blurRegion)
{
    OwnPtr<ImageBuffer> snapshotBuffer = ImageBuffer::create(snapshot->size());
    snapshotBuffer->context()->drawImage(snapshot.get(), ColorSpaceDeviceRGB, IntPoint(0, 0));

    OwnPtr<ImageBuffer> blurBuffer = ImageBuffer::create(roundedIntSize(blurRegion.size()));
    blurBuffer->context()->drawImage(snapshot.get(), ColorSpaceDeviceRGB, IntPoint(-blurRegion.x(), -blurRegion.y()));

    RefPtr<RenderSnapshottedPlugInBlurFilter> blurFilter = RenderSnapshottedPlugInBlurFilter::create(snapshotLabelBlurRadius);
    blurFilter->setSourceImage(blurBuffer.release());
    blurFilter->setSourceImageRect(FloatRect(FloatPoint(), blurRegion.size()));
    blurFilter->apply();

    snapshotBuffer->context()->drawImageBuffer(blurFilter->output(), ColorSpaceDeviceRGB, roundedIntPoint(blurRegion.location()));
    return snapshotBuffer->copyImage();
}
#endif

void RenderSnapshottedPlugIn::paintSnapshotWithLabel(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (contentBoxRect().isEmpty())
        return;

    if (!plugInImageElement()->hovered() && m_showReason == UserMousedOver)
        return;

    m_showedLabelOnce = true;
    LayoutRect labelRect;

    RefPtr<Image> snapshotImage = m_snapshotResource->image();
    if (!snapshotImage || snapshotImage->isNull())
        return;

#if ENABLE(FILTERS)
    // FIXME: Temporarily disabling the blur behind the label.
    // https://bugs.webkit.org/show_bug.cgi?id=108368
    if (!labelRect.isEmpty()) {
        RefPtr<Image> blurredSnapshotImage = m_snapshotResourceForLabel->image();
        if (!blurredSnapshotImage || blurredSnapshotImage->isNull()) {
            blurredSnapshotImage = snapshottedPluginImageForLabelDisplay(snapshotImage, labelRect);
            m_snapshotResourceForLabel->setCachedImage(new CachedImage(blurredSnapshotImage.get()));
        }
        snapshotImage = blurredSnapshotImage;
    }
#endif

    paintSnapshotImage(snapshotImage.get(), paintInfo, paintOffset);
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
    return RenderBlock::getCursor(point, overrideCursor);
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

void RenderSnapshottedPlugIn::resetDelayTimer(ShowReason reason)
{
    m_showReason = reason;
    m_showLabelDelayTimer.startOneShot(reason == UserMousedOver ? showLabelAfterMouseOverDelay : showLabelAutomaticallyDelay);
}

} // namespace WebCore
