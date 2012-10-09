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
#include "FrameView.h"
#include "Gradient.h"
#include "HTMLPlugInImageElement.h"
#include "PaintInfo.h"
#include "Path.h"

namespace WebCore {

RenderSnapshottedPlugIn::RenderSnapshottedPlugIn(HTMLPlugInImageElement* element)
    : RenderEmbeddedObject(element)
    , m_snapshotResource(RenderImageResource::create())
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
    if (plugInImageElement()->displayState() < HTMLPlugInElement::Playing) {
        RenderReplaced::paint(paintInfo, paintOffset);
        return;
    }

    RenderEmbeddedObject::paint(paintInfo, paintOffset);
}

void RenderSnapshottedPlugIn::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (plugInImageElement()->displayState() < HTMLPlugInElement::Playing) {
        paintReplacedSnapshot(paintInfo, paintOffset);
        theme()->paintPlugInSnapshotOverlay(this, paintInfo, paintOffset);
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

} // namespace WebCore
