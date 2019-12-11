/*
 * Copyright (C) 2011 Leo Yang <leoyang@webkit.org>
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

#if ENABLE(SVG_FONTS)
#include "SVGAltGlyphItemElement.h"

#include "ElementIterator.h"
#include "SVGGlyphRefElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAltGlyphItemElement);

inline SVGAltGlyphItemElement::SVGAltGlyphItemElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::altGlyphItemTag));
}

Ref<SVGAltGlyphItemElement> SVGAltGlyphItemElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGAltGlyphItemElement(tagName, document));
}

bool SVGAltGlyphItemElement::hasValidGlyphElements(Vector<String>& glyphNames) const
{
    // Spec: http://www.w3.org/TR/SVG/text.html#AltGlyphItemElement
    // The ‘altGlyphItem’ element defines a candidate set of possible glyph substitutions.
    // The first ‘altGlyphItem’ element whose referenced glyphs are all available is chosen.
    // Its glyphs are rendered instead of the character(s) that are inside of the referencing
    // ‘altGlyph’ element.
    //
    // Here we fill glyphNames and return true only if all referenced glyphs are valid and
    // there is at least one glyph.

    for (auto& glyphRef : childrenOfType<SVGGlyphRefElement>(*this)) {
        String referredGlyphName;
        if (glyphRef.hasValidGlyphElement(referredGlyphName))
            glyphNames.append(referredGlyphName);
        else {
            glyphNames.clear();
            return false;
        }
    }
    return !glyphNames.isEmpty();
}

}

#endif
