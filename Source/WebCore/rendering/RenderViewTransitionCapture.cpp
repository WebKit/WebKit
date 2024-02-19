/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "RenderViewTransitionCapture.h"

#include "RenderBoxModelObjectInlines.h"

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderViewTransitionCapture);

RenderViewTransitionCapture::RenderViewTransitionCapture(Type type, Document& document, RenderStyle&& style)
    : RenderReplaced(type, document, WTFMove(style), { }, ReplacedFlag::IsViewTransitionCapture)
{ }

void RenderViewTransitionCapture::setImage(RefPtr<ImageBuffer> oldImage, const LayoutSize& size)
{
    setIntrinsicSize(size);
    m_oldImage = oldImage;
}

void RenderViewTransitionCapture::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto& context = paintInfo.context();

    LayoutRect contentBoxRect = this->contentBoxRect();

    if (context.detectingContentfulPaint())
        return;

    contentBoxRect.moveBy(paintOffset);
    LayoutRect replacedContentRect = this->replacedContentRect();
    replacedContentRect.moveBy(paintOffset);

    // Not allowed to overflow the content box.
    bool clip = !contentBoxRect.contains(replacedContentRect);
    GraphicsContextStateSaver stateSaver(paintInfo.context(), clip);
    if (clip)
        paintInfo.context().clip(snappedIntRect(contentBoxRect));

    if (paintInfo.phase == PaintPhase::Foreground)
        page().addRelevantRepaintedObject(*this, intersection(replacedContentRect, contentBoxRect));

    InterpolationQualityMaintainer interpolationMaintainer(context, ImageQualityController::interpolationQualityFromStyle(style()));
    if (m_oldImage)
        context.drawImageBuffer(*m_oldImage, snappedIntRect(replacedContentRect), { context.compositeOperation() });

}

} // namespace WebCore
