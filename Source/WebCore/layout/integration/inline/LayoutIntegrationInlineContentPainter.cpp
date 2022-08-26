/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationInlineContentPainter.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "EllipsisBoxPainter.h"
#include "InlineBoxPainter.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderInline.h"
#include "TextBoxPainter.h"

namespace WebCore {
namespace LayoutIntegration {

InlineContentPainter::InlineContentPainter(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const RenderInline* layerRenderer, const InlineContent& inlineContent, const BoxTree& boxTree)
    : m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
    , m_layerRenderer(layerRenderer)
    , m_inlineContent(inlineContent)
    , m_boxTree(boxTree)
{
}

void InlineContentPainter::paint()
{
    auto paintPhase = m_paintInfo.phase;

    auto shouldPaintForPhase = [&] {
        switch (paintPhase) {
        case PaintPhase::Foreground:
        case PaintPhase::EventRegion:
        case PaintPhase::TextClip:
        case PaintPhase::Mask:
        case PaintPhase::Selection:
        case PaintPhase::Outline:
        case PaintPhase::ChildOutlines:
        case PaintPhase::SelfOutline:
            return true;
        default:
            return false;
        }
    };

    if (!shouldPaintForPhase())
        return;

    auto damageRect = m_paintInfo.rect;
    damageRect.moveBy(-m_paintOffset);

    auto hasDamage = [&](auto& box) {
        auto rect = enclosingLayoutRect(box.inkOverflow());
        m_boxTree.rootRenderer().flipForWritingMode(rect);
        // FIXME: This should test for intersection but horizontal ink overflow is miscomputed in a few cases (like with negative letter-spacing).
        return damageRect.maxY() > rect.y() && damageRect.y() < rect.maxY();
    };

    auto isVisuallyHidden = [&](auto& box) {
        return box.isVisuallyHidden() == InlineDisplay::Box::IsVisuallyHidden::Yes || box.style().visibility() != Visibility::Visible;
    };
    auto shouldPaintBoxForPhase = [&](auto& box) {
        switch (paintPhase) {
        case PaintPhase::ChildOutlines: return box.isNonRootInlineBox();
        case PaintPhase::SelfOutline: return box.isRootInlineBox();
        case PaintPhase::Outline: return box.isInlineBox();
        case PaintPhase::Mask: return box.isInlineBox();
        default: return true;
        }
    };

    auto layerPaintScope = LayerPaintScope { m_boxTree, m_layerRenderer };

    ListHashSet<RenderInline*> outlineObjects;

    for (auto& box : m_inlineContent.boxesForRect(damageRect)) {
        if (!shouldPaintBoxForPhase(box))
            continue;

        if (box.isLineBreak())
            continue;

        if (!layerPaintScope.includes(box))
            continue;

        if (isVisuallyHidden(box))
            continue;

        if (box.isInlineBox()) {
            if (!hasDamage(box))
                continue;

            auto inlineBoxPaintInfo = PaintInfo { m_paintInfo };
            inlineBoxPaintInfo.phase = paintPhase == PaintPhase::ChildOutlines ? PaintPhase::Outline : paintPhase;
            inlineBoxPaintInfo.outlineObjects = &outlineObjects;

            InlineBoxPainter { m_inlineContent, box, inlineBoxPaintInfo, m_paintOffset }.paint();
            continue;
        }

        if (auto& textContent = box.text()) {
            if (!textContent->length() || !hasDamage(box))
                continue;

            ModernTextBoxPainter { m_inlineContent, box, m_paintInfo, m_paintOffset }.paint();
            continue;
        }

        if (auto* renderer = dynamicDowncast<RenderBox>(m_boxTree.rendererForLayoutBox(box.layoutBox())); renderer && renderer->isReplacedOrInlineBlock()) {
            if (m_paintInfo.shouldPaintWithinRoot(*renderer)) {
                // FIXME: Painting should not require a non-const renderer.
                const_cast<RenderBox*>(renderer)->paintAsInlineBlock(m_paintInfo, flippedContentOffsetIfNeeded(*renderer));
            }
        }
    }

    for (auto* renderInline : outlineObjects)
        renderInline->paintOutline(m_paintInfo, m_paintOffset);
}

LayoutPoint InlineContentPainter::flippedContentOffsetIfNeeded(const RenderBox& childRenderer) const
{
    if (m_boxTree.rootRenderer().style().isFlippedBlocksWritingMode())
        return m_boxTree.rootRenderer().flipForWritingModeForChild(childRenderer, m_paintOffset);
    return m_paintOffset;
}

LayerPaintScope::LayerPaintScope(const BoxTree& boxTree, const RenderInline* layerRenderer)
    : m_boxTree(boxTree)
    , m_layerInlineBox(layerRenderer ? &boxTree.layoutBoxForRenderer(*layerRenderer) : nullptr)
{
}

bool LayerPaintScope::includes(const InlineDisplay::Box& box)
{
    auto isInside = [](auto& displayBox, auto& inlineBox)
    {
        ASSERT(inlineBox.isInlineBox());

        if (displayBox.isRootInlineBox())
            return false;

        for (auto* box = &displayBox.layoutBox().parent(); box->isInlineBox(); box = &box->parent()) {
            if (box == &inlineBox)
                return true;
        }
        return false;
    };

    if (m_layerInlineBox == &box.layoutBox())
        return true;
    if (m_layerInlineBox && !isInside(box, *m_layerInlineBox))
        return false;
    if (m_currentExcludedInlineBox && isInside(box, *m_currentExcludedInlineBox))
        return false;

    m_currentExcludedInlineBox = nullptr;

    if (box.isRootInlineBox() || box.isText() || box.isLineBreak())
        return true;

    auto* renderer = dynamicDowncast<RenderLayerModelObject>(m_boxTree.rendererForLayoutBox(box.layoutBox()));
    bool hasSelfPaintingLayer = renderer && renderer->hasSelfPaintingLayer();

    if (hasSelfPaintingLayer && box.isNonRootInlineBox())
        m_currentExcludedInlineBox = &downcast<Layout::ContainerBox>(box.layoutBox());

    return !hasSelfPaintingLayer;
}

}
}

#endif
