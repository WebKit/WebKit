/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "SVGTextChunk.h"

#include "SVGInlineTextBox.h"
#include "SVGTextContentElement.h"
#include "SVGTextFragment.h"

namespace WebCore {

SVGTextChunk::SVGTextChunk(const Vector<SVGInlineTextBox*>& lineLayoutBoxes, unsigned first, unsigned limit)
{
    ASSERT(first < limit);
    ASSERT(limit <= lineLayoutBoxes.size());

    const SVGInlineTextBox* box = lineLayoutBoxes[first];
    const RenderStyle& style = box->renderer().style();
    const SVGRenderStyle& svgStyle = style.svgStyle();

    if (!style.isLeftToRightDirection())
        m_chunkStyle |= SVGTextChunk::RightToLeftText;

    if (style.isVerticalWritingMode())
        m_chunkStyle |= SVGTextChunk::VerticalText;
    
    switch (svgStyle.textAnchor()) {
    case TextAnchor::Start:
        break;
    case TextAnchor::Middle:
        m_chunkStyle |= MiddleAnchor;
        break;
    case TextAnchor::End:
        m_chunkStyle |= EndAnchor;
        break;
    }

    if (auto* textContentElement = SVGTextContentElement::elementFromRenderer(box->renderer().parent())) {
        SVGLengthContext lengthContext(textContentElement);
        m_desiredTextLength = textContentElement->specifiedTextLength().value(lengthContext);

        switch (textContentElement->lengthAdjust()) {
        case SVGLengthAdjustUnknown:
            break;
        case SVGLengthAdjustSpacing:
            m_chunkStyle |= LengthAdjustSpacing;
            break;
        case SVGLengthAdjustSpacingAndGlyphs:
            m_chunkStyle |= LengthAdjustSpacingAndGlyphs;
            break;
        }
    }

    for (unsigned i = first; i < limit; ++i)
        m_boxes.append(lineLayoutBoxes[i]);
}

unsigned SVGTextChunk::totalCharacters() const
{
    unsigned characters = 0;
    for (auto* box : m_boxes) {
        for (auto& fragment : box->textFragments())
            characters += fragment.length;
    }
    return characters;
}

float SVGTextChunk::totalLength() const
{
    const SVGTextFragment* firstFragment = nullptr;
    const SVGTextFragment* lastFragment = nullptr;

    for (auto* box : m_boxes) {
        auto& fragments = box->textFragments();
        if (fragments.size()) {
            firstFragment = &(*fragments.begin());
            break;
        }
    }

    for (auto it = m_boxes.rbegin(), end = m_boxes.rend(); it != end; ++it) {
        auto& fragments = (*it)->textFragments();
        if (fragments.size()) {
            lastFragment = &(*fragments.rbegin());
            break;
        }
    }

    ASSERT(!firstFragment == !lastFragment);
    if (!firstFragment)
        return 0;

    if (m_chunkStyle & VerticalText)
        return (lastFragment->y + lastFragment->height) - firstFragment->y;

    return (lastFragment->x + lastFragment->width) - firstFragment->x;
}

float SVGTextChunk::totalAnchorShift() const
{
    float length = totalLength();
    if (m_chunkStyle & MiddleAnchor)
        return -length / 2;
    if (m_chunkStyle & EndAnchor)
        return m_chunkStyle & RightToLeftText ? 0 : -length;
    return m_chunkStyle & RightToLeftText ? -length : 0;
}

void SVGTextChunk::layout(HashMap<SVGInlineTextBox*, AffineTransform>& textBoxTransformations) const
{
    if (hasDesiredTextLength()) {
        if (hasLengthAdjustSpacing())
            processTextLengthSpacingCorrection();
        else {
            ASSERT(hasLengthAdjustSpacingAndGlyphs());
            buildBoxTransformations(textBoxTransformations);
        }
    }

    if (hasTextAnchor())
        processTextAnchorCorrection();
}

void SVGTextChunk::processTextLengthSpacingCorrection() const
{
    float textLengthShift = (desiredTextLength() - totalLength()) / totalCharacters();
    bool isVerticalText = m_chunkStyle & VerticalText;
    unsigned atCharacter = 0;

    for (auto* box : m_boxes) {
        for (auto& fragment : box->textFragments()) {
            if (isVerticalText)
                fragment.y += textLengthShift * atCharacter;
            else
                fragment.x += textLengthShift * atCharacter;
            
            atCharacter += fragment.length;
        }
    }
}

void SVGTextChunk::buildBoxTransformations(HashMap<SVGInlineTextBox*, AffineTransform>& textBoxTransformations) const
{
    AffineTransform spacingAndGlyphsTransform;
    bool foundFirstFragment = false;

    for (auto* box : m_boxes) {
        if (!foundFirstFragment) {
            if (!boxSpacingAndGlyphsTransform(box, spacingAndGlyphsTransform))
                continue;
            foundFirstFragment = true;
        }

        textBoxTransformations.set(box, spacingAndGlyphsTransform);
    }
}

bool SVGTextChunk::boxSpacingAndGlyphsTransform(const SVGInlineTextBox* box, AffineTransform& spacingAndGlyphsTransform) const
{
    auto& fragments = box->textFragments();
    if (fragments.isEmpty())
        return false;

    const SVGTextFragment& fragment = fragments.first();
    float scale = desiredTextLength() / totalLength();

    spacingAndGlyphsTransform.translate(fragment.x, fragment.y);

    if (m_chunkStyle & VerticalText)
        spacingAndGlyphsTransform.scaleNonUniform(1, scale);
    else
        spacingAndGlyphsTransform.scaleNonUniform(scale, 1);

    spacingAndGlyphsTransform.translate(-fragment.x, -fragment.y);
    return true;
}

void SVGTextChunk::processTextAnchorCorrection() const
{
    float textAnchorShift = totalAnchorShift();
    bool isVerticalText = m_chunkStyle & VerticalText;

    for (auto* box : m_boxes) {
        for (auto& fragment : box->textFragments()) {
            if (isVerticalText)
                fragment.y += textAnchorShift;
            else
                fragment.x += textAnchorShift;
        }
    }
}

} // namespace WebCore
