/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "RenderHTMLCanvas.h"

#include "CanvasRenderingContext.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLNames.h"
#include "ImageQualityController.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderHTMLCanvas);

RenderHTMLCanvas::RenderHTMLCanvas(HTMLCanvasElement& element, RenderStyle&& style)
    : RenderReplaced(element, WTFMove(style), element.size())
{
}

HTMLCanvasElement& RenderHTMLCanvas::canvasElement() const
{
    return downcast<HTMLCanvasElement>(nodeForNonAnonymous());
}

bool RenderHTMLCanvas::requiresLayer() const
{
    if (RenderReplaced::requiresLayer())
        return true;

    if (CanvasRenderingContext* context = canvasElement().renderingContext())
        return context->isAccelerated();

    return false;
}

void RenderHTMLCanvas::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GraphicsContext& context = paintInfo.context();

    LayoutRect contentBoxRect = this->contentBoxRect();

    if (context.detectingContentfulPaint()) {
        if (!context.contenfulPaintDetected() && canvasElement().renderingContext())
            context.setContentfulPaintDetected();
        return;
    }

    contentBoxRect.moveBy(paintOffset);
    LayoutRect replacedContentRect = this->replacedContentRect();
    replacedContentRect.moveBy(paintOffset);

    // Not allowed to overflow the content box.
    bool clip = !contentBoxRect.contains(replacedContentRect);
    GraphicsContextStateSaver stateSaver(paintInfo.context(), clip);
    if (clip)
        paintInfo.context().clip(snappedIntRect(contentBoxRect));

    if (paintInfo.phase == PaintPhase::Foreground)
        page().addRelevantRepaintedObject(this, intersection(replacedContentRect, contentBoxRect));

    InterpolationQualityMaintainer interpolationMaintainer(context, ImageQualityController::interpolationQualityFromStyle(style()));

    canvasElement().setIsSnapshotting(paintInfo.paintBehavior.contains(PaintBehavior::Snapshotting));
    canvasElement().paint(context, replacedContentRect);
    canvasElement().setIsSnapshotting(false);
}

void RenderHTMLCanvas::canvasSizeChanged()
{
    IntSize canvasSize = canvasElement().size();
    LayoutSize zoomedSize(canvasSize.width() * style().effectiveZoom(), canvasSize.height() * style().effectiveZoom());

    if (zoomedSize == intrinsicSize())
        return;

    setIntrinsicSize(zoomedSize);

    if (!parent())
        return;
    setNeedsLayoutIfNeededAfterIntrinsicSizeChange();
}

} // namespace WebCore
