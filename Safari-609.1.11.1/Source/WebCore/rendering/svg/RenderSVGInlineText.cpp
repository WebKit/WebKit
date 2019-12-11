/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderSVGInlineText.h"

#include "CSSFontSelector.h"
#include "FloatConversion.h"
#include "FloatQuad.h"
#include "RenderBlock.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "SVGInlineTextBox.h"
#include "SVGRenderingContext.h"
#include "SVGRootInlineBox.h"
#include "StyleFontSizeFunctions.h"
#include "StyleResolver.h"
#include "VisiblePosition.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGInlineText);

static String applySVGWhitespaceRules(const String& string, bool preserveWhiteSpace)
{
    String newString = string;
    if (preserveWhiteSpace) {
        // Spec: When xml:space="preserve", the SVG user agent will do the following using a
        // copy of the original character data content. It will convert all newline and tab
        // characters into space characters. Then, it will draw all space characters, including
        // leading, trailing and multiple contiguous space characters.
        newString.replace('\t', ' ');
        newString.replace('\n', ' ');
        newString.replace('\r', ' ');
        return newString;
    }

    // Spec: When xml:space="default", the SVG user agent will do the following using a
    // copy of the original character data content. First, it will remove all newline
    // characters. Then it will convert all tab characters into space characters.
    // Then, it will strip off all leading and trailing space characters.
    // Then, all contiguous space characters will be consolidated.
    newString.replace('\n', emptyString());
    newString.replace('\r', emptyString());
    newString.replace('\t', ' ');
    return newString;
}

RenderSVGInlineText::RenderSVGInlineText(Text& textNode, const String& string)
    : RenderText(textNode, applySVGWhitespaceRules(string, false))
    , m_scalingFactor(1)
    , m_layoutAttributes(*this)
{
}

String RenderSVGInlineText::originalText() const
{
    return textNode().data();
}

void RenderSVGInlineText::setRenderedText(const String& text)
{
    RenderText::setRenderedText(text);
    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this))
        textAncestor->subtreeTextDidChange(this);
}

void RenderSVGInlineText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);
    updateScaledFont();

    bool newPreserves = style().whiteSpace() == WhiteSpace::Pre;
    bool oldPreserves = oldStyle ? oldStyle->whiteSpace() == WhiteSpace::Pre : false;
    if (oldPreserves && !newPreserves) {
        setText(applySVGWhitespaceRules(originalText(), false), true);
        return;
    }

    if (!oldPreserves && newPreserves) {
        setText(applySVGWhitespaceRules(originalText(), true), true);
        return;
    }

    if (diff != StyleDifference::Layout)
        return;

    // The text metrics may be influenced by style changes.
    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(*this))
        textAncestor->subtreeStyleDidChange(this);
}

std::unique_ptr<InlineTextBox> RenderSVGInlineText::createTextBox()
{
    auto box = makeUnique<SVGInlineTextBox>(*this);
    box->setHasVirtualLogicalHeight();
    return box;
}

LayoutRect RenderSVGInlineText::localCaretRect(InlineBox* box, unsigned caretOffset, LayoutUnit*)
{
    if (!is<InlineTextBox>(box))
        return LayoutRect();

    auto& textBox = downcast<InlineTextBox>(*box);
    if (caretOffset < textBox.start() || caretOffset > textBox.start() + textBox.len())
        return LayoutRect();

    // Use the edge of the selection rect to determine the caret rect.
    if (caretOffset < textBox.start() + textBox.len()) {
        LayoutRect rect = textBox.localSelectionRect(caretOffset, caretOffset + 1);
        LayoutUnit x = textBox.isLeftToRightDirection() ? rect.x() : rect.maxX();
        return LayoutRect(x, rect.y(), caretWidth, rect.height());
    }

    LayoutRect rect = textBox.localSelectionRect(caretOffset - 1, caretOffset);
    LayoutUnit x = textBox.isLeftToRightDirection() ? rect.maxX() : rect.x();
    return LayoutRect(x, rect.y(), caretWidth, rect.height());
}

FloatRect RenderSVGInlineText::floatLinesBoundingBox() const
{
    FloatRect boundingBox;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        boundingBox.unite(box->calculateBoundaries());
    return boundingBox;
}

IntRect RenderSVGInlineText::linesBoundingBox() const
{
    return enclosingIntRect(floatLinesBoundingBox());
}

bool RenderSVGInlineText::characterStartsNewTextChunk(int position) const
{
    ASSERT(position >= 0);
    ASSERT(position < static_cast<int>(text().length()));

    // Each <textPath> element starts a new text chunk, regardless of any x/y values.
    if (!position && parent()->isSVGTextPath() && !previousSibling())
        return true;

    const SVGCharacterDataMap::const_iterator it = m_layoutAttributes.characterDataMap().find(static_cast<unsigned>(position + 1));
    if (it == m_layoutAttributes.characterDataMap().end())
        return false;

    return it->value.x != SVGTextLayoutAttributes::emptyValue() || it->value.y != SVGTextLayoutAttributes::emptyValue();
}

VisiblePosition RenderSVGInlineText::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer*)
{
    if (!firstTextBox() || text().isEmpty())
        return createVisiblePosition(0, DOWNSTREAM);

    float baseline = m_scaledFont.fontMetrics().floatAscent();

    RenderBlock* containingBlock = this->containingBlock();
    ASSERT(containingBlock);

    // Map local point to absolute point, as the character origins stored in the text fragments use absolute coordinates.
    FloatPoint absolutePoint(point);
    absolutePoint.moveBy(containingBlock->location());

    float closestDistance = std::numeric_limits<float>::max();
    float closestDistancePosition = 0;
    const SVGTextFragment* closestDistanceFragment = nullptr;
    SVGInlineTextBox* closestDistanceBox = nullptr;

    AffineTransform fragmentTransform;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        if (!is<SVGInlineTextBox>(*box))
            continue;

        auto& textBox = downcast<SVGInlineTextBox>(*box);
        Vector<SVGTextFragment>& fragments = textBox.textFragments();

        unsigned textFragmentsSize = fragments.size();
        for (unsigned i = 0; i < textFragmentsSize; ++i) {
            const SVGTextFragment& fragment = fragments.at(i);
            FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height);
            fragment.buildFragmentTransform(fragmentTransform);
            if (!fragmentTransform.isIdentity())
                fragmentRect = fragmentTransform.mapRect(fragmentRect);

            float distance = powf(fragmentRect.x() - absolutePoint.x(), 2) +
                             powf(fragmentRect.y() + fragmentRect.height() / 2 - absolutePoint.y(), 2);

            if (distance < closestDistance) {
                closestDistance = distance;
                closestDistanceBox = &textBox;
                closestDistanceFragment = &fragment;
                closestDistancePosition = fragmentRect.x();
            }
        }
    }

    if (!closestDistanceFragment)
        return createVisiblePosition(0, DOWNSTREAM);

    int offset = closestDistanceBox->offsetForPositionInFragment(*closestDistanceFragment, absolutePoint.x() - closestDistancePosition, true);
    return createVisiblePosition(offset + closestDistanceBox->start(), offset > 0 ? VP_UPSTREAM_IF_POSSIBLE : DOWNSTREAM);
}

void RenderSVGInlineText::updateScaledFont()
{
    computeNewScaledFontForStyle(*this, style(), m_scalingFactor, m_scaledFont);
}

void RenderSVGInlineText::computeNewScaledFontForStyle(const RenderObject& renderer, const RenderStyle& style, float& scalingFactor, FontCascade& scaledFont)
{
    // Alter font-size to the right on-screen value to avoid scaling the glyphs themselves, except when GeometricPrecision is specified
    scalingFactor = SVGRenderingContext::calculateScreenFontSizeScalingFactor(renderer);
    if (!scalingFactor || style.fontDescription().textRenderingMode() == TextRenderingMode::GeometricPrecision) {
        scalingFactor = 1;
        scaledFont = style.fontCascade();
        return;
    }

    auto fontDescription = style.fontDescription();

    // FIXME: We need to better handle the case when we compute very small fonts below (below 1pt).
    fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSizeForSVGInlineText(fontDescription.computedSize(), fontDescription.isAbsoluteSize(), scalingFactor, renderer.document()));

    // SVG controls its own glyph orientation, so don't allow writing-mode
    // to affect it.
    if (fontDescription.orientation() != FontOrientation::Horizontal)
        fontDescription.setOrientation(FontOrientation::Horizontal);

    scaledFont = FontCascade(WTFMove(fontDescription), 0, 0);
    scaledFont.update(&renderer.document().fontSelector());
}

}
