/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
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

#ifndef SVGGlyphElement_h
#define SVGGlyphElement_h

#if ENABLE(SVG_FONTS)
#include "Path.h"
#include "SVGStyledElement.h"

#include <limits>
#include <wtf/Forward.h>

namespace WebCore {

class SVGFontData;

// Describe a SVG <glyph> element
struct SVGGlyphIdentifier {
    enum Orientation {
        Vertical,
        Horizontal,
        Both
    };

    // SVG Font depends on exactly this order.
    enum ArabicForm {
        None = 0,
        Isolated,
        Terminal,
        Initial,
        Medial
    };

    SVGGlyphIdentifier()
        : isValid(false)
        , orientation(Both)
        , arabicForm(None)
        , priority(0)
        , nameLength(0)
        , horizontalAdvanceX(0.0f)
        , verticalOriginX(0.0f)
        , verticalOriginY(0.0f)
        , verticalAdvanceY(0.0f)
    {
    }

    // Used to mark our float properties as "to be inherited from SVGFontData"
    static float inheritedValue()
    {
        static float s_inheritedValue = std::numeric_limits<float>::infinity();
        return s_inheritedValue;
    }

    bool operator==(const SVGGlyphIdentifier& other) const
    {
        return isValid == other.isValid
            && orientation == other.orientation
            && arabicForm == other.arabicForm
            && glyphName == other.glyphName
            && horizontalAdvanceX == other.horizontalAdvanceX
            && verticalOriginX == other.verticalOriginX
            && verticalOriginY == other.verticalOriginY
            && verticalAdvanceY == other.verticalAdvanceY
            && languages == other.languages;
    }

    bool isValid : 1;

    unsigned orientation : 2; // Orientation
    unsigned arabicForm : 3; // ArabicForm
    int priority;
    size_t nameLength;
    String glyphName;

    float horizontalAdvanceX;
    float verticalOriginX;
    float verticalOriginY;
    float verticalAdvanceY;

    Path pathData;
    Vector<String> languages;
};

class SVGGlyphElement : public SVGStyledElement {
public:
    static PassRefPtr<SVGGlyphElement> create(const QualifiedName&, Document*);

    SVGGlyphIdentifier buildGlyphIdentifier() const;

    // Helper function used by SVGFont
    static void inheritUnspecifiedAttributes(SVGGlyphIdentifier&, const SVGFontData*);
    static String querySVGFontLanguage(const SVGElement*);

    // Helper function shared between SVGGlyphElement & SVGMissingGlyphElement
    static SVGGlyphIdentifier buildGenericGlyphIdentifier(const SVGElement*);

private:
    SVGGlyphElement(const QualifiedName&, Document*);

    virtual void parseMappedAttribute(Attribute*);
    virtual void fillAttributeToPropertyTypeMap();
    virtual AttributeToPropertyTypeMap& attributeToPropertyTypeMap();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual bool rendererIsNeeded(RenderStyle*) { return false; }

    void invalidateGlyphCache();
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
#endif
