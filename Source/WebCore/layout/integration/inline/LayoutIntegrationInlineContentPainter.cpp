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

#include "EllipsisBoxPainter.h"
#include "InlineBoxPainter.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderInline.h"
#include "RenderStyleInlines.h"
#include "TextBoxPainter.h"

namespace WebCore {
namespace LayoutIntegration {

InlineContentPainter::InlineContentPainter(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const RenderInline* inlineBoxWithLayer, const InlineContent& inlineContent, const BoxTree& boxTree)
    : m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
    , m_inlineBoxWithLayer(inlineBoxWithLayer)
    , m_inlineContent(inlineContent)
    , m_boxTree(boxTree)
{
    m_damageRect = m_paintInfo.rect;
    m_damageRect.moveBy(-m_paintOffset);
}

void InlineContentPainter::paintEllipsis(size_t lineIndex)
{
    if ((m_paintInfo.phase != PaintPhase::Foreground && m_paintInfo.phase != PaintPhase::TextClip) || root().style().usedVisibility() != Visibility::Visible)
        return;

    auto lineBox = InlineIterator::LineBox { InlineIterator::LineBoxIteratorModernPath { m_inlineContent, lineIndex } };
    if (!lineBox.hasEllipsis())
        return;
    EllipsisBoxPainter { lineBox, m_paintInfo, m_paintOffset, root().selectionForegroundColor(), root().selectionBackgroundColor() }.paint();
}

void InlineContentPainter::paintDisplayBox(const InlineDisplay::Box& box)
{
    auto hasDamage = [&](auto& box) {
        auto rect = enclosingLayoutRect(box.inkOverflow());
        root().flipForWritingMode(rect);
        // FIXME: This should test for intersection but horizontal ink overflow is miscomputed in a few cases (like with negative letter-spacing).
        return m_damageRect.maxY() > rect.y() && m_damageRect.y() < rect.maxY();
    };

    if (box.isFullyTruncated()) {
        // Fully truncated boxes are visually empty and they don't show their descendants either (unlike visibility property). 
        return;
    }

    if (box.isLineBreak())
        return;

    if (box.isInlineBox()) {
        if (!box.isVisible() || !hasDamage(box))
            return;

        auto inlineBoxPaintInfo = PaintInfo { m_paintInfo };
        inlineBoxPaintInfo.phase = m_paintInfo.phase == PaintPhase::ChildOutlines ? PaintPhase::Outline : m_paintInfo.phase;
        inlineBoxPaintInfo.outlineObjects = &m_outlineObjects;

        InlineBoxPainter { m_inlineContent, box, inlineBoxPaintInfo, m_paintOffset }.paint();
        return;
    }

    if (box.isText()) {
        auto hasVisibleDamage = box.text().length() && box.isVisible() && hasDamage(box);
        if (!hasVisibleDamage)
            return;

        ModernTextBoxPainter { m_inlineContent, box, m_paintInfo, m_paintOffset }.paint();
        return;
    }

    if (auto* renderer = dynamicDowncast<RenderBox>(box.layoutBox().rendererForIntegration()); renderer && renderer->isReplacedOrInlineBlock()) {
        if (m_paintInfo.shouldPaintWithinRoot(*renderer)) {
            // FIXME: Painting should not require a non-const renderer.
            const_cast<RenderBox*>(renderer)->paintAsInlineBlock(m_paintInfo, flippedContentOffsetIfNeeded(*renderer));
        }
    }
}

void InlineContentPainter::paint()
{
    auto layerPaintScope = LayerPaintScope { m_boxTree, m_inlineBoxWithLayer };
    auto lastBoxLineIndex = std::optional<size_t> { };

    auto paintLineEndingEllipsisIfApplicable = [&](std::optional<size_t> currentLineIndex) {
        // Since line ending ellipsis belongs to the line structure but we don't have the concept of painting the line itself
        // let's paint it when we are either at the end of the content or finished painting a line with ellipsis.
        // While normally ellipsis is on the last line, -webkit-line-clamp can make us put ellipsis on any line.
        if (m_inlineBoxWithLayer) {
            // Line ending ellipsis is never on the inline box (with layer).
            return;
        }
        if (lastBoxLineIndex && (!currentLineIndex || *lastBoxLineIndex != currentLineIndex))
            paintEllipsis(*lastBoxLineIndex);
    };

    for (auto& box : m_inlineContent.boxesForRect(m_damageRect)) {
        auto shouldPaintBoxForPhase = [&] {
            switch (m_paintInfo.phase) {
            case PaintPhase::ChildOutlines:
                return box.isNonRootInlineBox();
            case PaintPhase::SelfOutline:
                return box.isRootInlineBox();
            case PaintPhase::Outline:
                return box.isInlineBox();
            case PaintPhase::Mask:
                return box.isInlineBox();
            default:
                return true;
            }
        };

        if (shouldPaintBoxForPhase() && layerPaintScope.includes(box)) {
            paintLineEndingEllipsisIfApplicable(box.lineIndex());
            paintDisplayBox(box);
        }
        lastBoxLineIndex = box.lineIndex();
    }
    paintLineEndingEllipsisIfApplicable({ });

    for (auto& renderInline : m_outlineObjects)
        renderInline.paintOutline(m_paintInfo, m_paintOffset);
}

LayoutPoint InlineContentPainter::flippedContentOffsetIfNeeded(const RenderBox& childRenderer) const
{
    if (root().style().isFlippedBlocksWritingMode())
        return root().flipForWritingModeForChild(childRenderer, m_paintOffset);
    return m_paintOffset;
}

LayerPaintScope::LayerPaintScope(const BoxTree& boxTree, const RenderInline* inlineBoxWithLayer)
    : m_boxTree(boxTree)
    , m_inlineBoxWithLayer(inlineBoxWithLayer ? inlineBoxWithLayer->layoutBox() : nullptr)
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

    if (m_inlineBoxWithLayer == &box.layoutBox())
        return true;
    if (m_inlineBoxWithLayer && !isInside(box, *m_inlineBoxWithLayer))
        return false;
    if (m_currentExcludedInlineBox && isInside(box, *m_currentExcludedInlineBox))
        return false;

    m_currentExcludedInlineBox = nullptr;

    if (box.isRootInlineBox() || box.isText() || box.isLineBreak())
        return true;

    auto* renderer = dynamicDowncast<RenderLayerModelObject>(box.layoutBox().rendererForIntegration());
    bool hasSelfPaintingLayer = renderer && renderer->hasSelfPaintingLayer();

    if (hasSelfPaintingLayer && box.isNonRootInlineBox())
        m_currentExcludedInlineBox = &downcast<Layout::ElementBox>(box.layoutBox());

    return !hasSelfPaintingLayer;
}

}
}

