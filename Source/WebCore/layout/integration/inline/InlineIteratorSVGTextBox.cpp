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
#include "InlineIteratorSVGTextBox.h"

#include "LayoutIntegrationLineLayout.h"
#include "RenderSVGText.h"
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"
#include "SVGTextBoxPainter.h"
#include "SVGTextFragment.h"

namespace WebCore {
namespace InlineIterator {

SVGTextBox::SVGTextBox(PathVariant&& path)
    : TextBox(WTFMove(path))
{
}

FloatRect SVGTextBox::calculateBoundariesIncludingSVGTransform() const
{
    FloatRect textRect;

    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    float baseline = renderer().scaledFont().metricsOfPrimaryFont().ascent() / scalingFactor;
    for (auto& fragment : textFragments()) {
        auto fragmentRect = FloatRect { fragment.x, fragment.y - baseline, fragment.width, fragment.height };

        AffineTransform fragmentTransform;
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            fragmentRect = fragmentTransform.mapRect(fragmentRect);

        textRect.unite(fragmentRect);
    }
    return textRect;
}

LayoutRect SVGTextBox::localSelectionRect(unsigned start, unsigned end) const
{
    auto [clampedStart, clampedEnd] = selectableRange().clamp(start, end);

    if (clampedStart >= clampedEnd)
        return LayoutRect();

    auto& style = renderer().style();

    AffineTransform fragmentTransform;
    FloatRect selectionRect;
    unsigned fragmentStartPosition = 0;
    unsigned fragmentEndPosition = 0;

    for (auto& fragment : textFragments()) {
        fragmentStartPosition = clampedStart;
        fragmentEndPosition = clampedEnd;
        if (!mapStartEndPositionsIntoFragmentCoordinates(this->start(), fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        FloatRect fragmentRect = selectionRectForTextFragment(renderer(), direction(), fragment, fragmentStartPosition, fragmentEndPosition, style);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            fragmentRect = fragmentTransform.mapRect(fragmentRect);

        selectionRect.unite(fragmentRect);
    }

    return enclosingIntRect(selectionRect);
}

const Vector<SVGTextFragment>& SVGTextBox::textFragments() const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) -> const Vector<SVGTextFragment>& {
        return path.svgTextFragments();
    });
}

const SVGInlineTextBox* SVGTextBox::legacyInlineBox() const
{
    return downcast<SVGInlineTextBox>(TextBox::legacyInlineBox());
}

SVGTextBoxIterator::SVGTextBoxIterator(Box::PathVariant&& path)
    : TextBoxIterator(WTFMove(path))
{
}

SVGTextBoxIterator::SVGTextBoxIterator(const Box& box)
    : TextBoxIterator(box)
{
}

SVGTextBoxIterator firstSVGTextBoxFor(const RenderSVGInlineText& text)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(text)) {
        auto box = lineLayout->textBoxesFor(text);
        if (!box)
            return { };
        return { *box };
    }

    return { BoxLegacyPath { text.firstLegacyTextBox() } };
}

BoxRange<SVGTextBoxIterator> svgTextBoxesFor(const RenderSVGInlineText& text)
{
    return { firstSVGTextBoxFor(text) };
}

SVGTextBoxIterator svgTextBoxFor(const SVGInlineTextBox* box)
{
    return { BoxLegacyPath { box } };
}

SVGTextBoxIterator svgTextBoxFor(const LayoutIntegration::InlineContent& inlineContent, size_t boxIndex)
{
    auto& box = inlineContent.displayContent().boxes[boxIndex];
    if (!box.isText() || !box.layoutBox().rendererForIntegration()->isRenderSVGInlineText())
        return { };
    return { BoxModernPath { inlineContent, boxIndex } };
}

SVGTextBox::Key makeKey(const SVGTextBox& textBox)
{
    return { &textBox.renderer(), textBox.start() };
}

BoxRange<BoxIterator> boxesFor(const RenderSVGText& svgText)
{
    if (auto* lineLayout = svgText.inlineLayout())
        return { BoxIterator { *lineLayout->firstRootInlineBox() } };

    return { BoxIterator { BoxLegacyPath { svgText.legacyRootBox() } } };
}

}
}
