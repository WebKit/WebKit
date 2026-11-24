/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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
#include "SVGTextQuery.h"

#include "FloatConversion.h"
#include "FontCascadeInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineIteratorSVGTextBox.h"
#include "RenderBlockFlow.h"
#include "RenderElementInlines.h"
#include "RenderObjectStyle.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "SVGTextFragment.h"
#include <wtf/MathExtras.h>

namespace WebCore {

static inline InlineIterator::InlineBoxIterator inlineBoxForRenderer(const RenderElement& renderer)
{
    if (CheckedPtr renderBlock = dynamicDowncast<RenderBlockFlow>(renderer)) {
        // If we're given a block element, it has to be a RenderSVGText.
        ASSERT(is<RenderSVGText>(*renderBlock));
        return InlineIterator::firstRootInlineBoxFor(*renderBlock);
    }

    if (renderer.isInlineBox())
        return InlineIterator::lineLeftmostInlineBoxFor(downcast<RenderBoxModelObject>(renderer));

    ASSERT_NOT_REACHED();
    return { };
}

static Vector<InlineIterator::SVGTextBoxIterator> collectTextBoxesInLogicalOrder(const RenderElement& renderer)
{
    Vector<InlineIterator::SVGTextBoxIterator> textBoxes;
    auto inlineBox = inlineBoxForRenderer(renderer);
    if (!inlineBox)
        return textBoxes;

    auto isInRenderer = [&](const RenderObject& text) {
        for (CheckedPtr ancestor = text.parent(); ancestor; ancestor = ancestor->parent()) {
            // Skip generated content.
            if (!ancestor->element())
                return false;

            if (ancestor == &renderer)
                return true;
        }
        return false;
    };

    InlineIterator::LineLogicalOrderCache orderCache;
    auto box = InlineIterator::firstLeafOnLineInLogicalOrder(inlineBox->lineBox(), orderCache);
    for (; box; box = InlineIterator::nextLeafOnLineInLogicalOrder(box, orderCache)) {
        if (box->isSVGText() && isInRenderer(box->renderer()))
            textBoxes.append(*box);
    }

    return textBoxes;
}

SVGTextQuery::SVGTextQuery(const RenderElement& renderer)
{
    for (auto& textBox : collectTextBoxesInLogicalOrder(renderer)) {
        appendTypographicCharacters(*textBox);
        m_numberOfCharacters += textBox->length();
    }
    ASSERT(std::ranges::is_sorted(m_typographicCharacters, { }, &TypographicCharacter::firstCharacterIndex));
}

void SVGTextQuery::appendTypographicCharacters(const InlineIterator::SVGTextBox& textBox)
{
    CheckedRef textRenderer = textBox.renderer();
    auto& metrics = textRenderer->textMetrics();

    // The characters of all text boxes are numbered together in logical order. m_numberOfCharacters counts the characters of
    // the previous text boxes, and offset is a position in the text of the renderer, where this text box starts at textBox.start().
    auto characterIndexAt = [&](unsigned offset) {
        return m_numberOfCharacters + offset - textBox.start();
    };

    unsigned offset = 0;
    unsigned metricsIndex = 0;
    auto appendHiddenCharacters = [&](unsigned endOffset) {
        for (; offset < endOffset && metricsIndex < metrics.size(); ++metricsIndex) {
            if (offset >= textBox.start())
                m_typographicCharacters.append({ .firstCharacterIndex = characterIndexAt(offset), .characterCount = metrics[metricsIndex].length });
            offset += metrics[metricsIndex].length;
        }
    };

    auto ascent = textRenderer->scaledFont().metricsOfPrimaryFont().ascent() / textRenderer->scalingFactor();
    auto isVertical = textRenderer->writingMode().isVertical();
    auto isInlineFlipped = textBox.isInlineFlipped();
    for (auto& fragment : textBox.textFragments()) {
        appendHiddenCharacters(fragment.characterOffset);
        ASSERT(metricsIndex == fragment.metricsListOffset);

        AffineTransform transform;
        fragment.buildFragmentTransform(transform);

        auto rotation = narrowPrecisionToFloat(rad2deg(atan2(fragment.transform.b(), fragment.transform.a())));

        // The positions follow the rotate attribute and the text path, not the orientation of the glyph.
        auto positionTransform = transform;
        if (isVertical) {
            auto positionFragment = fragment;
            positionFragment.transform = AffineTransform::makeRotation(rotation - fragment.orientationAngle);
            positionFragment.buildFragmentTransform(positionTransform);
        }

        auto isSideways = isVertical && fmodf(fragment.orientationAngle, 180);
        auto lengthAdjustScale = static_cast<float>(isVertical ? fragment.lengthAdjustTransform.d() : fragment.lengthAdjustTransform.a());
        auto inlineLength = isVertical ? fragment.height : fragment.width;
        auto origin = isVertical ? FloatPoint { fragment.x - fragment.baselineShift, fragment.y } : FloatPoint { fragment.x, fragment.y + fragment.baselineShift };

        auto inlineVector = [&](float length) {
            return isVertical ? FloatSize { 0, length } : FloatSize { length, 0 };
        };

        auto fragmentEnd = fragment.characterOffset + fragment.length;
        auto firstTypographicCharacterIndex = m_typographicCharacters.size();
        auto inlineOffset = 0.f;
        for (; offset < fragmentEnd && metricsIndex < metrics.size(); ++metricsIndex) {
            auto& entry = metrics[metricsIndex];
            auto advance = isVertical && !isSideways ? entry.height : entry.width;
            auto cellStart = isInlineFlipped ? inlineLength - inlineOffset - advance : inlineOffset;

            // "The inline-base direction position of the alignment point is on the start-edge of the glyph."
            // https://w3c.github.io/svgwg/svg2-draft/text.html#TermAlignmentPoint
            auto alignmentPoint = origin + inlineVector(isInlineFlipped ? cellStart + advance : cellStart);

            m_typographicCharacters.append({
                .firstCharacterIndex = characterIndexAt(offset),
                .characterCount = entry.length,
                .advance = advance * lengthAdjustScale,
                .spacing = offset + entry.length >= fragmentEnd ? fragment.spacing : 0,
                .rotation = rotation,
                .glyphCell = isVertical ? FloatRect { fragment.x, fragment.y - ascent + cellStart, entry.width, entry.height } : FloatRect { fragment.x + cellStart, fragment.y - ascent, advance, entry.height },
                .startPosition = positionTransform.mapPoint(alignmentPoint),
                .endPosition = positionTransform.mapPoint(alignmentPoint + inlineVector(isInlineFlipped ? -advance : advance)),
            });

            inlineOffset += advance;
            offset += entry.length;
        }

        m_fragments.append({
            .transform = transform,
            .firstTypographicCharacterIndex = static_cast<unsigned>(firstTypographicCharacterIndex),
            .typographicCharacterCount = static_cast<unsigned>(m_typographicCharacters.size() - firstTypographicCharacterIndex),
        });
    }

    appendHiddenCharacters(textBox.start() + textBox.length());
}

// Spec: "While charnum < last and the character at index charnum does not correspond to a typographic character:
// Set charnum to charnum + 1."
// https://w3c.github.io/svgwg/svg2-draft/text.html#TermFindTypographicCharacterForCharacter
std::optional<size_t> SVGTextQuery::findTypographicCharacter(unsigned characterIndex) const
{
    ASSERT(characterIndex < m_numberOfCharacters);
    size_t next = std::ranges::upper_bound(m_typographicCharacters, characterIndex, { }, &TypographicCharacter::firstCharacterIndex) - m_typographicCharacters.begin();
    if (next) {
        auto& previous = m_typographicCharacters[next - 1];
        if (characterIndex < previous.firstCharacterIndex + previous.characterCount)
            return next - 1;
    }
    if (next < m_typographicCharacters.size())
        return next;
    return std::nullopt;
}

const SVGTextQuery::Fragment* SVGTextQuery::fragmentContaining(size_t typographicCharacterIndex) const
{
    size_t next = std::ranges::upper_bound(m_fragments, typographicCharacterIndex, { }, &Fragment::firstTypographicCharacterIndex) - m_fragments.begin();
    if (!next)
        return nullptr;
    auto& fragment = m_fragments[next - 1];
    return typographicCharacterIndex < fragment.firstTypographicCharacterIndex + fragment.typographicCharacterCount ? &fragment : nullptr;
}

// Spec: "Let length be the value that would be returned if the getSubStringLength method were called on this element,
// passing 0 and count as arguments."
// https://w3c.github.io/svgwg/svg2-draft/text.html#__svg__SVGTextContentElement__getComputedTextLength
float SVGTextQuery::textLength() const
{
    return subStringLength(0, m_numberOfCharacters);
}

// Spec: "If the character corresponds to a typographic character and it is the first character in document order
// to correspond to that typographic character, then: Add the advance of the typographic character to length, adjusted
// for any font kerning in effect. If the letter-spacing or word-spacing properties contributed space just after the
// typographic character, then add that space to length."
// https://w3c.github.io/svgwg/svg2-draft/text.html#__svg__SVGTextContentElement__getSubStringLength
float SVGTextQuery::subStringLength(unsigned characterIndex, unsigned characterCount) const
{
    // The caller clamps characterCount, so the sum cannot overflow.
    ASSERT(characterIndex <= m_numberOfCharacters && characterCount <= m_numberOfCharacters - characterIndex);
    auto endIndex = characterIndex + characterCount;
    auto subStringLength = 0.f;
    auto first = std::ranges::lower_bound(m_typographicCharacters, characterIndex, { }, &TypographicCharacter::firstCharacterIndex) - m_typographicCharacters.begin();
    for (auto& character : m_typographicCharacters.subspan(first)) {
        if (character.firstCharacterIndex >= endIndex)
            break;
        subStringLength += character.advance + character.spacing;
    }
    return subStringLength;
}

// Spec: "Let p be the alignment point of the typographic character that correspond to the character at index charnum,
// in the coordinate system of the current element."
// https://w3c.github.io/svgwg/svg2-draft/text.html#__svg__SVGTextContentElement__getStartPositionOfChar
FloatPoint SVGTextQuery::startPositionOfCharacter(unsigned characterIndex) const
{
    auto index = findTypographicCharacter(characterIndex);
    return index ? m_typographicCharacters[*index].startPosition : FloatPoint { };
}

FloatPoint SVGTextQuery::endPositionOfCharacter(unsigned characterIndex) const
{
    auto index = findTypographicCharacter(characterIndex);
    return index ? m_typographicCharacters[*index].endPosition : FloatPoint { };
}

float SVGTextQuery::rotationOfCharacter(unsigned characterIndex) const
{
    auto index = findTypographicCharacter(characterIndex);
    return index ? m_typographicCharacters[*index].rotation : 0;
}

// Spec: "Let rect be the rectangle that forms the tightest bounding box around quad in the current element's coordinate system."
// https://w3c.github.io/svgwg/svg2-draft/text.html#__svg__SVGTextContentElement__getExtentOfChar
FloatRect SVGTextQuery::extentOfCharacter(unsigned characterIndex) const
{
    auto index = findTypographicCharacter(characterIndex);
    if (!index)
        return { };
    auto* fragment = fragmentContaining(*index);
    return fragment ? fragment->transform.mapRect(m_typographicCharacters[*index].glyphCell) : FloatRect { };
}

// Spec: "If the character at index charnum corresponds to a typographic character and it is the first character in document order
// to correspond to that typographic character, and point in this element's coordinate system is within the glyph cell for
// the typographic character, then set result to charnum."
// https://w3c.github.io/svgwg/svg2-draft/text.html#__svg__SVGTextContentElement__getCharNumAtPosition
int SVGTextQuery::characterNumberAtPosition(const FloatPoint& position) const
{
    auto isInGlyphCell = [](const FloatRect& glyphCell, const FloatPoint& point) {
        return point.x() >= glyphCell.x() && point.x() < glyphCell.maxX() && point.y() >= glyphCell.y() && point.y() < glyphCell.maxY();
    };

    auto result = -1;
    for (auto& fragment : m_fragments) {
        auto inverse = fragment.transform.inverse();
        if (!inverse)
            continue;

        auto positionInFragment = inverse->mapPoint(position);
        for (auto& character : m_typographicCharacters.subspan(fragment.firstTypographicCharacterIndex, fragment.typographicCharacterCount)) {
            if (isInGlyphCell(character.glyphCell, positionInFragment))
                result = character.firstCharacterIndex;
        }
    }
    return result;
}

} // namespace WebCore
