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

#include "FontCascadeInlines.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlowInlines.h"
#include "RenderSVGText.h"
#include "SVGTextBoxPainter.h"
#include "SVGTextFragment.h"

namespace WebCore {
namespace InlineIterator {

SVGTextBox::SVGTextBox(const LayoutIntegration::InlineContent& inlineContent, size_t boxIndex)
    : TextBox(inlineContent, boxIndex)
{
}

FloatRect SVGTextBox::calculateBoundariesIncludingSVGTransform() const
{
    return calculateBoundariesIncludingSVGTransform(protect(renderer()), textFragments());
}

FloatRect SVGTextBox::calculateBoundariesIncludingSVGTransform(const RenderSVGInlineText& text, std::span<const SVGTextFragment> fragments)
{
    FloatRect textRect;

    float scalingFactor = text.scalingFactor();
    ASSERT(scalingFactor);

    float baseline = protect(text.scaledFont())->metricsOfPrimaryFont().ascent() / scalingFactor;
    for (auto& fragment : fragments) {
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

    CheckedRef style = renderer().style();

    AffineTransform fragmentTransform;
    FloatRect selectionRect;
    unsigned fragmentStartPosition = 0;
    unsigned fragmentEndPosition = 0;

    for (auto& fragment : textFragments()) {
        fragmentStartPosition = clampedStart;
        fragmentEndPosition = clampedEnd;
        if (!mapStartEndPositionsIntoFragmentCoordinates(this->start(), fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        FloatRect fragmentRect = selectionRectForTextFragment(protect(renderer()), direction(), fragment, fragmentStartPosition, fragmentEndPosition, style);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            fragmentRect = fragmentTransform.mapRect(fragmentRect);

        selectionRect.unite(fragmentRect);
    }

    return enclosingIntRect(selectionRect);
}

const Vector<SVGTextFragment>& SVGTextBox::textFragments() const
{
    return inlineContent().svgTextFragments(boxIndex());
}

SVGTextBoxIterator::SVGTextBoxIterator(const Box& box)
    : TextBoxIterator(box)
{
}

SVGTextBoxIterator firstSVGTextBoxFor(const RenderSVGInlineText& text)
{
    if (CheckedPtr lineLayout = LayoutIntegration::LineLayout::containing(text)) {
        auto box = lineLayout->textBoxesFor(text);
        if (!box)
            return { };
        return { *box };
    }
    return { };
}

BoxRange<SVGTextBoxIterator> svgTextBoxesFor(const RenderSVGInlineText& text)
{
    return { firstSVGTextBoxFor(text) };
}

SVGTextBoxIterator svgTextBoxFor(const LayoutIntegration::InlineContent& inlineContent, size_t boxIndex)
{
    auto& box = inlineContent.displayContent().boxes[boxIndex];
    if (!box.isText() || !box.layoutBox().rendererForIntegration()->isRenderSVGInlineText())
        return { };
    return { Box { inlineContent, boxIndex } };
}

BoxRange<BoxIterator> boxesFor(const RenderSVGText& svgText)
{
    if (CheckedPtr lineLayout = svgText.inlineLayout())
        return { BoxIterator { *lineLayout->firstRootInlineBox() } };
    return { BoxIterator { } };
}

BoxIterator lastBoxFor(const RenderSVGText& svgText)
{
    if (CheckedPtr lineLayout = svgText.inlineLayout())
        return lineLayout->lastBox();
    return { };
}

}
}
