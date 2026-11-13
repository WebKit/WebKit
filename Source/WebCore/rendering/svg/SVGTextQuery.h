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

// Answers the SVGTextContentElement queries from the result of the text layout: the text fragments and the metrics of the
// typographic characters. Characters are numbered in logical order, positions are in the user space of the text.
// https://w3c.github.io/svgwg/svg2-draft/text.html#InterfaceSVGTextContentElement
class SVGTextQuery {
public:
    explicit SVGTextQuery(const RenderElement&);

    unsigned numberOfCharacters() const { return m_numberOfCharacters; }
    float textLength() const;
    float subStringLength(unsigned startPosition, unsigned length) const;
    FloatPoint startPositionOfCharacter(unsigned position) const;
    FloatPoint endPositionOfCharacter(unsigned position) const;
    float rotationOfCharacter(unsigned position) const;
    FloatRect extentOfCharacter(unsigned position) const;
    int characterNumberAtPosition(const FloatPoint&) const;

private:
    struct TypographicCharacter {
        unsigned start { 0 };
        unsigned length { 0 };
        float advance { 0 };
        float spacing { 0 };
        float rotation { 0 };
        // Before the transform of the text fragment.
        FloatRect glyphCell { };
        FloatPoint startPosition { };
        FloatPoint endPosition { };
        // Characters that are not rendered, like the ones beyond the end of a text path, have no fragment.
        std::optional<unsigned> fragmentIndex { };
    };

    const TypographicCharacter* findTypographicCharacter(unsigned position) const;

    Vector<TypographicCharacter> m_typographicCharacters;
    Vector<AffineTransform> m_fragmentTransforms;
    unsigned m_numberOfCharacters { 0 };
};

} // namespace WebCore
