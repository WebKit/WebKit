/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *           (C) 2008 Rob Buis <buis@kde.org>
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
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGInlineText.h"

#include "FloatConversion.h"
#include "FloatQuad.h"
#include "RenderBlock.h"
#include "RenderSVGRoot.h"
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"
#include "VisiblePosition.h"

namespace WebCore {

static PassRefPtr<StringImpl> applySVGWhitespaceRules(PassRefPtr<StringImpl> string, bool preserveWhiteSpace)
{
    if (preserveWhiteSpace) {
        // Spec: When xml:space="preserve", the SVG user agent will do the following using a
        // copy of the original character data content. It will convert all newline and tab
        // characters into space characters. Then, it will draw all space characters, including
        // leading, trailing and multiple contiguous space characters.
        RefPtr<StringImpl> newString = string->replace('\t', ' ');
        newString = newString->replace('\n', ' ');
        newString = newString->replace('\r', ' ');
        return newString.release();
    }

    // Spec: When xml:space="default", the SVG user agent will do the following using a
    // copy of the original character data content. First, it will remove all newline
    // characters. Then it will convert all tab characters into space characters.
    // Then, it will strip off all leading and trailing space characters.
    // Then, all contiguous space characters will be consolidated.
    RefPtr<StringImpl> newString = string->replace('\n', StringImpl::empty());
    newString = newString->replace('\r', StringImpl::empty());
    newString = newString->replace('\t', ' ');
    return newString.release();
}

RenderSVGInlineText::RenderSVGInlineText(Node* n, PassRefPtr<StringImpl> string)
    : RenderText(n, applySVGWhitespaceRules(string, false))
{
}

void RenderSVGInlineText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);

    const RenderStyle* newStyle = style();
    if (!newStyle || newStyle->whiteSpace() != PRE)
        return;

    if (!oldStyle || oldStyle->whiteSpace() != PRE)
        setText(applySVGWhitespaceRules(originalText(), true), true);
}

InlineTextBox* RenderSVGInlineText::createTextBox()
{
    InlineTextBox* box = new (renderArena()) SVGInlineTextBox(this);
    box->setHasVirtualLogicalHeight();
    return box;
}

IntRect RenderSVGInlineText::localCaretRect(InlineBox*, int, int*)
{
    return IntRect();
}

IntRect RenderSVGInlineText::linesBoundingBox() const
{
    IntRect boundingBox;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        boundingBox.unite(box->calculateBoundaries());
    return boundingBox;
}

bool RenderSVGInlineText::characterStartsNewTextChunk(int position) const
{
    ASSERT(m_attributes.xValues().size() == textLength());
    ASSERT(m_attributes.yValues().size() == textLength());
    ASSERT(position >= 0);
    ASSERT(position < static_cast<int>(textLength()));

    // Each <textPath> element starts a new text chunk, regardless of any x/y values.
    if (!position && parent()->isSVGTextPath() && !previousSibling())
        return true;

    int currentPosition = 0;
    unsigned size = m_attributes.textMetricsValues().size();
    for (unsigned i = 0; i < size; ++i) {
        const SVGTextMetrics& metrics = m_attributes.textMetricsValues().at(i);

        // We found the desired character.
        if (currentPosition == position) {
            return m_attributes.xValues().at(position) != SVGTextLayoutAttributes::emptyValue()
                || m_attributes.yValues().at(position) != SVGTextLayoutAttributes::emptyValue();
        }

        currentPosition += metrics.length();
        if (currentPosition > position)
            break;
    }

    // The desired position is available in the x/y list, but not in the character data values list.
    // That means the previous character data described a single glyph, consisting of multiple unicode characters.
    // The consequence is that the desired character does not define a new absolute x/y position, even if present in the x/y test.
    // This code is tested by svg/W3C-SVG-1.1/text-text-06-t.svg (and described in detail, why this influences chunk detection).
    return false;
}

VisiblePosition RenderSVGInlineText::positionForPoint(const IntPoint& point)
{
    if (!firstTextBox() || !textLength())
        return createVisiblePosition(0, DOWNSTREAM);

    RenderStyle* style = this->style();
    ASSERT(style);
    int baseline = style->font().ascent();

    RenderBlock* containingBlock = this->containingBlock();
    ASSERT(containingBlock);

    // Map local point to absolute point, as the character origins stored in the text fragments use absolute coordinates.
    FloatPoint absolutePoint(point);
    absolutePoint.move(containingBlock->x(), containingBlock->y());

    float closestDistance = std::numeric_limits<float>::max();
    float closestDistancePosition = 0;
    const SVGTextFragment* closestDistanceFragment = 0;
    SVGInlineTextBox* closestDistanceBox = 0;

    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        ASSERT(box->isSVGInlineTextBox());
        SVGInlineTextBox* textBox = static_cast<SVGInlineTextBox*>(box);
        Vector<SVGTextFragment>& fragments = textBox->textFragments();

        unsigned textFragmentsSize = fragments.size();
        for (unsigned i = 0; i < textFragmentsSize; ++i) {
            const SVGTextFragment& fragment = fragments.at(i);
            FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height);
            if (!fragment.transform.isIdentity())
                fragmentRect = fragment.transform.mapRect(fragmentRect);

            float distance = powf(fragmentRect.x() - absolutePoint.x(), 2) +
                             powf(fragmentRect.y() + fragmentRect.height() / 2 - absolutePoint.y(), 2);

            if (distance < closestDistance) {
                closestDistance = distance;
                closestDistanceBox = textBox;
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

}

#endif // ENABLE(SVG)
