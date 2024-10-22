/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Google Inc. All rights reserved.
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

#include "RenderSVGInlineText.h"
#include "RenderStyleInlines.h"
#include "SVGInlineTextBoxInlines.h"
#include "SVGRenderStyle.h"
#include "SVGTextContentElement.h"
#include "SVGTextFragment.h"

namespace WebCore {

SVGTextChunk::SVGTextChunk(const Vector<InlineIterator::SVGTextBoxIterator>& lineLayoutBoxes, unsigned first, unsigned limit, SVGTextFragmentMap& fragmentMap)
{
    ASSERT(first < limit);
    ASSERT(limit <= lineLayoutBoxes.size());

    auto firstBox = lineLayoutBoxes[first];
    const RenderStyle& style = firstBox->renderer().style();
    const SVGRenderStyle& svgStyle = style.svgStyle();

    if (style.writingMode().isBidiRTL())
        m_chunkStyle |= SVGTextChunk::RightToLeftText;

    if (style.writingMode().isVertical())
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

    if (RefPtr textContentElement = SVGTextContentElement::elementFromRenderer(firstBox->renderer().parent())) {
        SVGLengthContext lengthContext(textContentElement.get());
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

    for (auto box : lineLayoutBoxes.subspan(first, limit - first)) {
        auto it = fragmentMap.find(makeKey(*box));
        if (it == fragmentMap.end())
            continue;
        m_boxes.append({ box, it->value });
    }
}

unsigned SVGTextChunk::totalCharacters() const
{
    unsigned characters = 0;
    for (auto& box : m_boxes) {
        for (auto& fragment : box.fragments)
            characters += fragment.length;
    }
    return characters;
}

float SVGTextChunk::totalLength() const
{
    const SVGTextFragment* firstFragment = nullptr;
    const SVGTextFragment* lastFragment = nullptr;

    for (auto& box : m_boxes) {
        if (box.fragments.size()) {
            firstFragment = &box.fragments.first();
            break;
        }
    }

    for (auto& box : makeReversedRange(m_boxes)) {
        if (box.fragments.size()) {
            lastFragment = &box.fragments.last();
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

void SVGTextChunk::layout(SVGChunkTransformMap& textBoxTransformations) const
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
    float textLengthShift = 0;
    if (totalCharacters() > 1)
        textLengthShift = (desiredTextLength() - totalLength()) / (totalCharacters() - 1);

    bool isVerticalText = m_chunkStyle & VerticalText;
    unsigned atCharacter = 0;

    for (auto& box : m_boxes) {
        for (auto& fragment : box.fragments) {
            if (isVerticalText)
                fragment.y += textLengthShift * atCharacter;
            else
                fragment.x += textLengthShift * atCharacter;

            atCharacter += fragment.length;
        }
    }
}

void SVGTextChunk::buildBoxTransformations(SVGChunkTransformMap& textBoxTransformations) const
{
    AffineTransform spacingAndGlyphsTransform;
    bool foundFirstFragment = false;

    for (auto& box : m_boxes) {
        if (!foundFirstFragment) {
            if (!boxSpacingAndGlyphsTransform(box.fragments, spacingAndGlyphsTransform))
                continue;
            foundFirstFragment = true;
        }

        textBoxTransformations.set(makeKey(*box.box), spacingAndGlyphsTransform);
    }
}

bool SVGTextChunk::boxSpacingAndGlyphsTransform(const Vector<SVGTextFragment>& fragments, AffineTransform& spacingAndGlyphsTransform) const
{
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

    for (auto& box : m_boxes) {
        for (auto& fragment : box.fragments) {
            if (isVerticalText)
                fragment.y += textAnchorShift;
            else
                fragment.x += textAnchorShift;
        }
    }
}

} // namespace WebCore
