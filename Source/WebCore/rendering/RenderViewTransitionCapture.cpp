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

#include "ImageOverlayController.h"
#include "ImageQualityController.h"
#include "PaintInfo.h"
#include "RenderBoxModelObjectInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderViewTransitionCapture);

RenderViewTransitionCapture::RenderViewTransitionCapture(Type type, Document& document, RenderStyle&& style)
    : RenderReplaced(type, document, WTFMove(style), { }, ReplacedFlag::IsViewTransitionCapture)
{
}

RenderViewTransitionCapture::~RenderViewTransitionCapture() = default;

void RenderViewTransitionCapture::setImage(RefPtr<ImageBuffer> oldImage)
{
    m_oldImage = oldImage;
}

void RenderViewTransitionCapture::setSize(const LayoutSize& size, const LayoutRect& overflowRect)
{
    setIntrinsicSize(size);
    m_overflowRect = overflowRect;
}

void RenderViewTransitionCapture::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto& context = paintInfo.context();
    if (context.detectingContentfulPaint())
        return;

    LayoutRect replacedContentRect = this->replacedContentRect();
    replacedContentRect.moveBy(paintOffset);

    FloatRect paintRect = m_localOverflowRect;

    InterpolationQualityMaintainer interpolationMaintainer(context, ImageQualityController::interpolationQualityFromStyle(style()));
    if (m_oldImage)
        context.drawImageBuffer(*m_oldImage, paintRect, { context.compositeOperation() });
}

void RenderViewTransitionCapture::layout()
{
    RenderReplaced::layout();
    m_localOverflowRect = m_overflowRect;
    m_scale = { replacedContentRect().width().toFloat() / intrinsicSize().width().toFloat() , replacedContentRect().height().toFloat() / intrinsicSize().height().toFloat()  };
    m_localOverflowRect.scale(m_scale.width(), m_scale.height());
    m_localOverflowRect.moveBy(replacedContentRect().location());
    addVisualOverflow(m_localOverflowRect);
}

LayoutPoint RenderViewTransitionCapture::captureContentInset() const
{
    LayoutPoint location = m_localOverflowRect.location();
    location.moveBy(-visualOverflowRect().location());
    return location;
}

String RenderViewTransitionCapture::debugDescription() const
{
    StringBuilder builder;

    builder.append(renderName(), " 0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase));

    builder.append(" ::view-transition-"_s, style().pseudoElementType() == PseudoId::ViewTransitionNew ? "new("_s : "old("_s);
    builder.append(style().pseudoElementNameArgument(), ')');
    return builder.toString();
}

} // namespace WebCore
