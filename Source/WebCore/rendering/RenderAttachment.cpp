/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "RenderAttachment.h"

#if ENABLE(ATTACHMENT_ELEMENT)

#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "FrameSelection.h"
#include "HTMLAttachmentElement.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderAttachment);

RenderAttachment::RenderAttachment(HTMLAttachmentElement& element, RenderStyle&& style)
    : RenderReplaced(Type::Attachment, element, WTFMove(style), LayoutSize())
    , m_isWideLayout(element.isWideLayout())
{
    ASSERT(isRenderAttachment());
#if ENABLE(SERVICE_CONTROLS)
    m_hasShadowControls = element.isImageMenuEnabled();
#endif
}

RenderAttachment::~RenderAttachment() = default;

HTMLAttachmentElement& RenderAttachment::attachmentElement() const
{
    return downcast<HTMLAttachmentElement>(nodeForNonAnonymous());
}

LayoutSize RenderAttachment::layoutWideLayoutAttachmentOnly()
{
    if (auto* wideLayoutShadowElement = attachmentElement().wideLayoutShadowContainer()) {
        if (auto* wideLayoutShadowRenderer = downcast<RenderBox>(wideLayoutShadowElement->renderer())) {
            if (wideLayoutShadowRenderer->needsLayout())
                wideLayoutShadowRenderer->layout();
            ASSERT(!wideLayoutShadowRenderer->needsLayout());
            return wideLayoutShadowRenderer->size();
        }
    }
    return { };
}

void RenderAttachment::layout()
{
    if (auto size = layoutWideLayoutAttachmentOnly(); !size.isEmpty()) {
        setIntrinsicSize(size);
        RenderReplaced::layout();
        if (hasShadowContent())
            layoutShadowContent(intrinsicSize());
        return;
    }

    LayoutSize newIntrinsicSize = theme().attachmentIntrinsicSize(*this);

    if (!theme().attachmentShouldAllowWidthToShrink(*this)) {
        m_minimumIntrinsicWidth = std::max(m_minimumIntrinsicWidth, newIntrinsicSize.width());
        newIntrinsicSize.setWidth(m_minimumIntrinsicWidth);
    }

    setIntrinsicSize(newIntrinsicSize);

    RenderReplaced::layout();
    
    if (hasShadowContent())
        layoutShadowContent(newIntrinsicSize);
}

LayoutUnit RenderAttachment::baselinePosition(FontBaseline, bool, LineDirectionMode, LinePositionMode) const
{
    if (auto* baselineElement = attachmentElement().wideLayoutImageElement()) {
        if (auto* baselineElementRenderBox = baselineElement->renderBox()) {
            // This is the bottom of the image assuming it is vertically centered.
            return (height() + baselineElementRenderBox->height()) / 2;
        }
        // Fallback to the bottom of the attachment if there is no image.
        return height();
    }

    return theme().attachmentBaseline(*this);
}

bool RenderAttachment::shouldDrawBorder() const
{
    if (style().usedAppearance() == StyleAppearance::BorderlessAttachment)
        return false;
    return m_shouldDrawBorder;
}

void RenderAttachment::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& offset)
{
    if (paintInfo.phase != PaintPhase::Selection || !hasVisibleBoxDecorations() || !style().hasUsedAppearance())
        return;

    auto paintRect = borderBoxRect();
    paintRect.moveBy(offset);

    theme().paint(*this, paintInfo, paintRect);
}

void RenderAttachment::layoutShadowContent(const LayoutSize& size)
{
    for (auto& renderBox : childrenOfType<RenderBox>(*this)) {
        renderBox.mutableStyle().setHeight(Length(size.height(), LengthType::Fixed));
        renderBox.mutableStyle().setWidth(Length(size.width(), LengthType::Fixed));
        renderBox.setNeedsLayout(MarkOnlyThis);
        renderBox.layout();
    }
}

bool RenderAttachment::paintWideLayoutAttachmentOnly(const PaintInfo& paintInfo, const LayoutPoint& offset) const
{
    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection)
        return false;

    if (auto* wideLayoutShadowElement = attachmentElement().wideLayoutShadowContainer()) {
        if (auto* wideLayoutShadowRenderer = wideLayoutShadowElement->renderer()) {
            auto shadowPaintInfo = paintInfo;
            for (PaintPhase phase : { PaintPhase::BlockBackground, PaintPhase::ChildBlockBackgrounds, PaintPhase::Float, PaintPhase::Foreground, PaintPhase::Outline }) {
                shadowPaintInfo.phase = phase;
                wideLayoutShadowRenderer->paint(shadowPaintInfo, offset);
            }
        }

        attachmentElement().requestWideLayoutIconIfNeeded();
        return true;
    }
    return false;
}

} // namespace WebCore

#endif
