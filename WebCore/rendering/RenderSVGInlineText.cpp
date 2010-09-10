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

RenderSVGInlineText::RenderSVGInlineText(Node* n, PassRefPtr<StringImpl> str) 
    : RenderText(n, str)
{
}

void RenderSVGInlineText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);
    
    // FIXME: Should optimize this.
    // SVG text is always transformed.
    if (RefPtr<StringImpl> textToTransform = originalText())
        setText(textToTransform.release(), true);
}

InlineTextBox* RenderSVGInlineText::createTextBox()
{
    InlineTextBox* box = new (renderArena()) SVGInlineTextBox(this);
    box->setHasVirtualHeight();
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

    int currentPosition = 0;
    unsigned size = m_attributes.characterDataValues().size();
    for (unsigned i = 0; i < size; ++i) {
        const SVGTextLayoutAttributes::CharacterData& data = m_attributes.characterDataValues().at(i);

        // We found the desired character.
        if (currentPosition == position) {
            if (isVerticalWritingMode(style()->svgStyle()))
                return m_attributes.yValues().at(position) != SVGTextLayoutAttributes::emptyValue();

            return m_attributes.xValues().at(position) != SVGTextLayoutAttributes::emptyValue();
        }

        currentPosition += data.spansCharacters;
        if (currentPosition > position)
            break;
    }

    // The desired position is available in the x/y list, but not in the character data values list.
    // That means the previous character data described a single glyph, consisting of multiple unicode characters.
    // The consequence is that the desired character does not define a new absolute x/y position, even if present in the x/y test.
    // This code is tested by svg/W3C-SVG-1.1/text-text-06-t.svg (and described in detail, why this influences chunk detection).
    ASSERT(currentPosition > position);
    return false;
}

}

#endif // ENABLE(SVG)
