/*
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

#ifndef SVGTextLayoutAttributes_h
#define SVGTextLayoutAttributes_h

#if ENABLE(SVG)
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGTextLayoutAttributes {
public:
    SVGTextLayoutAttributes();

    void fillWithEmptyValues(unsigned length);
    void dump();

    Vector<float>& xValues() { return m_xValues; }
    const Vector<float>& xValues() const { return m_xValues; }

    Vector<float>& yValues() { return m_yValues; }
    const Vector<float>& yValues() const { return m_yValues; }

    Vector<float>& dxValues() { return m_dxValues; }
    const Vector<float>& dxValues() const { return m_dxValues; }

    Vector<float>& dyValues() { return m_dyValues; }
    const Vector<float>& dyValues() const { return m_dyValues; }

    Vector<float>& rotateValues() { return m_rotateValues; }
    const Vector<float>& rotateValues() const { return m_rotateValues; }

    static float emptyValue();

    struct CharacterData {
        CharacterData()
            : spansCharacters(0)
            , width(0)
            , height(0)
        {
        }

        // When multiple unicode characters map to a single glyph (eg. 'ffi' ligature)
        // 'spansCharacters' contains the number of characters this glyph spans.
        int spansCharacters;
        
        // The 'glyphName' / 'unicodeString' pair is needed for kerning calculations.
        String glyphName;
        String unicodeString;

        // 'width' and 'height' hold the size of this glyph/character.
        float width;
        float height;
    };

    Vector<CharacterData>& characterDataValues() { return m_characterDataValues; }
    const Vector<CharacterData>& characterDataValues() const { return m_characterDataValues; }

private:
    Vector<float> m_xValues;
    Vector<float> m_yValues;
    Vector<float> m_dxValues;
    Vector<float> m_dyValues;
    Vector<float> m_rotateValues;
    Vector<CharacterData> m_characterDataValues;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
