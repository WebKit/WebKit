/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#pragma once

#include "AffineTransform.h"
#include "FloatRect.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderElement;

namespace InlineIterator {
class SVGTextBox;
}

// Answers the SVGTextContentElement queries from the result of the text layout: the text fragments and the metrics of the
// typographic characters. Character indices are in logical order, positions are in the user space of the text.
// https://w3c.github.io/svgwg/svg2-draft/text.html#InterfaceSVGTextContentElement
class SVGTextQuery {
public:
    SVGTextQuery() = default;
    explicit SVGTextQuery(const RenderElement&);

    unsigned numberOfCharacters() const { return m_numberOfCharacters; }
    float textLength() const;
    float subStringLength(unsigned characterIndex, unsigned characterCount) const;
    FloatPoint startPositionOfCharacter(unsigned characterIndex) const;
    FloatPoint endPositionOfCharacter(unsigned characterIndex) const;
    float rotationOfCharacter(unsigned characterIndex) const;
    FloatRect extentOfCharacter(unsigned characterIndex) const;
    int characterNumberAtPosition(const FloatPoint&) const;

private:
    // The unit the queries work on, a grapheme cluster or the characters of a ligature, drawn as one or more glyphs.
    // https://w3c.github.io/svgwg/svg2-draft/text.html#TermTypographicCharacterUnit
    struct TypographicCharacter {
        // The characters of the text it spans.
        unsigned firstCharacterIndex { 0 };
        unsigned characterCount { 0 };
        // Length in the inline direction, without the letter and word spacing after it.
        float advance { 0 };
        float spacing { 0 };
        float rotation { 0 };
        // The box of the glyph, before the transform of the text fragment, which maps it the way the glyph is drawn.
        FloatRect glyphCell { };
        // The alignment points at the start and the end of the advance, in user space. Unlike the glyph cell, they follow
        // the rotate attribute and the text path but not the orientation of the glyph.
        FloatPoint startPosition { };
        FloatPoint endPosition { };
    };

    // The typographic characters of a text fragment and their transform.
    struct Fragment {
        AffineTransform transform;
        unsigned firstTypographicCharacterIndex { 0 };
        unsigned typographicCharacterCount { 0 };
    };

    void appendTypographicCharacters(const InlineIterator::SVGTextBox&);
    std::optional<size_t> findTypographicCharacter(unsigned characterIndex) const;
    const Fragment* fragmentContaining(size_t typographicCharacterIndex) const;

    // Sorted by firstCharacterIndex.
    Vector<TypographicCharacter> m_typographicCharacters;
    // Sorted by firstTypographicCharacterIndex.
    Vector<Fragment> m_fragments;
    unsigned m_numberOfCharacters { 0 };
};

} // namespace WebCore
