/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>
   Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
    
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "SVGFontFaceElement.h"
#include "CString.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "FontCache.h"
#include "FontPlatformData.h"
#include "SimpleFontData.h"
#include "SVGDefinitionSrcElement.h"
#include "SVGFontElement.h"
#include "SVGFontFaceSrcElement.h"
#include "SVGGlyphElement.h"
#include "SVGNames.h"

#include <math.h>

namespace WebCore {

using namespace SVGNames;

SVGFontFaceElement::SVGFontFaceElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_fontFaceRule(new CSSFontFaceRule(0))
    , m_styleDeclaration(new CSSMutableStyleDeclaration)
{
    m_styleDeclaration->setParent(document()->mappedElementSheet());
    m_styleDeclaration->setStrictParsing(true);
    m_fontFaceRule->setDeclaration(m_styleDeclaration.get());
    document()->mappedElementSheet()->append(m_fontFaceRule);
}

SVGFontFaceElement::~SVGFontFaceElement()
{
}

static inline void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, int>* propertyNameToIdMap, const QualifiedName& attrName, const char* cssPropertyName = 0)
{
    int propertyId = 0;
    if (cssPropertyName)
        propertyId = getPropertyID(cssPropertyName, strlen(cssPropertyName));
    else {
        CString propertyName = attrName.localName().domString().utf8();
        propertyId = getPropertyID(propertyName.data(), propertyName.length());
    }
    if (propertyId < 1)
        fprintf(stderr, "Failed to find property: %s\n", attrName.localName().deprecatedString().ascii());
    ASSERT(propertyId > 0);
    propertyNameToIdMap->set(attrName.localName().impl(), propertyId);
}

static int cssPropertyIdForSVGAttributeName(const QualifiedName& attrName)
{
    if (!attrName.namespaceURI().isNull())
        return 0;
    
    static HashMap<AtomicStringImpl*, int>* propertyNameToIdMap = 0;
    if (!propertyNameToIdMap) {
        propertyNameToIdMap = new HashMap<AtomicStringImpl*, int>;
        // This is a list of all @font-face CSS properties which are exposed as SVG XML attributes
        // Those commented out are not yet supported by WebCore's style system
        //mapAttributeToCSSProperty(propertyNameToIdMap, accent_heightAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, alphabeticAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, ascentAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, bboxAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, cap_heightAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, descentAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_familyAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_sizeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_stretchAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_styleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_variantAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_weightAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, hangingAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, ideographicAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, mathematicalAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, overline_positionAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, overline_thicknessAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, panose_1Attr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, slopeAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, stemhAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, stemvAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, strikethrough_positionAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, strikethrough_thicknessAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, underline_positionAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, underline_thicknessAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, unicode_rangeAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, units_per_emAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_alphabeticAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_hangingAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_ideographicAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_mathematicalAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, widthsAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, x_heightAttr);
    }
    
    return propertyNameToIdMap->get(attrName.localName().impl());
}

void SVGFontFaceElement::parseMappedAttribute(MappedAttribute* attr)
{    
    int propId = cssPropertyIdForSVGAttributeName(attr->name());
    if (propId > 0) {
        m_styleDeclaration->setProperty(propId, attr->value(), false);
        rebuildFontFace();
        return;
    }
    
    SVGElement::parseMappedAttribute(attr);
}

unsigned SVGFontFaceElement::unitsPerEm() const
{
    AtomicString value(getAttribute(units_per_emAttr));
    if (value.isEmpty())
        return 1000;

    return static_cast<unsigned>(ceilf(value.toFloat()));
}

int SVGFontFaceElement::xHeight() const
{
    AtomicString value(getAttribute(x_heightAttr));
    if (value.isEmpty())
        return 0;

    return static_cast<int>(ceilf(value.toFloat()));
}

float SVGFontFaceElement::horizontalOriginX() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: The X-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing horizontally oriented text. (Note that the origin applies to all glyphs in the font.)
    // If the attribute is not specified, the effect is as if a value of "0" were specified.
    AtomicString value(m_fontElement->getAttribute(horiz_origin_xAttr));
    if (value.isEmpty())
        return 0.0f;

    return value.toFloat();
}

float SVGFontFaceElement::horizontalOriginY() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: The Y-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing horizontally oriented text. (Note that the origin applies to all glyphs in the font.)
    // If the attribute is not specified, the effect is as if a value of "0" were specified.
    AtomicString value(m_fontElement->getAttribute(horiz_origin_yAttr));
    if (value.isEmpty())
        return 0.0f;

    return value.toFloat();
}

float SVGFontFaceElement::horizontalAdvanceX() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: The default horizontal advance after rendering a glyph in horizontal orientation. Glyph
    // widths are required to be non-negative, even if the glyph is typically rendered right-to-left,
    // as in Hebrew and Arabic scripts.
    AtomicString value(m_fontElement->getAttribute(horiz_adv_xAttr));
    if (value.isEmpty())
        return 0.0f;

    return value.toFloat();
}

float SVGFontFaceElement::verticalOriginX() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: The default X-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing vertically oriented text. If the attribute is not specified, the effect is as if the attribute
    // were set to half of the effective value of attribute horiz-adv-x.
    AtomicString value(m_fontElement->getAttribute(vert_origin_xAttr));
    if (value.isEmpty())
        return horizontalAdvanceX() / 2.0f;

    return value.toFloat();
}

float SVGFontFaceElement::verticalOriginY() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: The default Y-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing vertically oriented text. If the attribute is not specified, the effect is as if the attribute
    // were set to the position specified by the font's ascent attribute.             
    AtomicString value(m_fontElement->getAttribute(vert_origin_yAttr));
    if (value.isEmpty())
        return ascent();

    return value.toFloat();
}

float SVGFontFaceElement::verticalAdvanceY() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: The default vertical advance after rendering a glyph in vertical orientation. If the attribute is
    // not specified, the effect is as if a value equivalent of one em were specified (see units-per-em).                    
    AtomicString value(m_fontElement->getAttribute(vert_adv_yAttr));
       if (value.isEmpty())
        return 1.0f;

    return value.toFloat();
}

int SVGFontFaceElement::ascent() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: Same syntax and semantics as the 'ascent' descriptor within an @font-face rule. The maximum
    // unaccented height of the font within the font coordinate system. If the attribute is not specified,
    // the effect is as if the attribute were set to the difference between the units-per-em value and the
    // vert-origin-y value for the corresponding font.
    AtomicString value(m_fontElement->getAttribute(ascentAttr));
    if (!value.isEmpty())
        return static_cast<int>(ceilf(value.toFloat()));

    value = m_fontElement->getAttribute(vert_origin_yAttr);
    if (!value.isEmpty())
        return static_cast<int>(unitsPerEm()) - static_cast<int>(ceilf(value.toFloat()));

    // Match Batiks default value
    return static_cast<int>(ceilf(unitsPerEm() * 0.8f));
}

int SVGFontFaceElement::descent() const
{
    if (!m_fontElement)
        return 0.0f;

    // Spec: Same syntax and semantics as the 'descent' descriptor within an @font-face rule. The maximum
    // unaccented depth of the font within the font coordinate system. If the attribute is not specified,
    // the effect is as if the attribute were set to the vert-origin-y value for the corresponding font.
    AtomicString value(m_fontElement->getAttribute(descentAttr));
    if (!value.isEmpty()) {
        // Some testcases use a negative descent value, where a positive was meant to be used :(
        int descent = static_cast<int>(ceilf(value.toFloat()));
        return descent < 0 ? -descent : descent;
    }

    value = m_fontElement->getAttribute(vert_origin_yAttr);
    if (!value.isEmpty())
        return static_cast<int>(ceilf(value.toFloat()));

    // Match Batiks default value
    return static_cast<int>(ceilf(unitsPerEm() * 0.2f));
}

String SVGFontFaceElement::fontFamily() const
{
    return m_styleDeclaration->getPropertyValue(CSS_PROP_FONT_FAMILY);
}

SVGFontElement* SVGFontFaceElement::associatedFontElement() const
{
    return m_fontElement.get();
}

void SVGFontFaceElement::rebuildFontFace()
{
    // Ignore changes until we live in the tree
    if (!parentNode())
        return;

    // we currently ignore all but the first src element, alternatively we could concat them
    SVGFontFaceSrcElement* srcElement = 0;
    SVGDefinitionSrcElement* definitionSrc = 0;

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(font_face_srcTag) && !srcElement)
            srcElement = static_cast<SVGFontFaceSrcElement*>(child);
        else if (child->hasTagName(definition_srcTag) && !definitionSrc)
            definitionSrc = static_cast<SVGDefinitionSrcElement*>(child);
    }

#if 0
    // @font-face (CSSFontFace) does not yet support definition-src, as soon as it does this code should do the trick!
    if (definitionSrc)
        m_styleDeclaration->setProperty(CSS_PROP_DEFINITION_SRC, definitionSrc->getAttribute(XLinkNames::hrefAttr), false);
#endif

    bool describesParentFont = parentNode()->hasTagName(fontTag);
    RefPtr<CSSValueList> list;

    if (describesParentFont) {
        m_fontElement = static_cast<SVGFontElement*>(parentNode());

        list = new CSSValueList;
        list->append(new CSSFontFaceSrcValue(fontFamily(), true));
    } else if (srcElement)
        list = srcElement->srcValue();

    if (!list)
        return;

    // Parse in-memory CSS rules
    CSSProperty srcProperty(CSS_PROP_SRC, list);
    const CSSProperty* srcPropertyRef = &srcProperty;
    m_styleDeclaration->addParsedProperties(&srcPropertyRef, 1);

    if (describesParentFont) {    
        // Traverse parsed CSS values and associate CSSFontFaceSrcValue elements with ourselves.
        RefPtr<CSSValue> src = m_styleDeclaration->getPropertyCSSValue(CSS_PROP_SRC);
        CSSValueList* srcList = static_cast<CSSValueList*>(src.get());

        unsigned srcLength = srcList ? srcList->length() : 0;
        for (unsigned i = 0; i < srcLength; i++) {
            if (CSSFontFaceSrcValue* item = static_cast<CSSFontFaceSrcValue*>(srcList->item(i)))
                item->setSVGFontFaceElement(this);
        }
    }

    document()->updateStyleSelector();
}

void SVGFontFaceElement::insertedIntoDocument()
{
    rebuildFontFace();
}

void SVGFontFaceElement::childrenChanged(bool changedByParser)
{
    SVGElement::childrenChanged(changedByParser);
    rebuildFontFace();
}

}

#endif // ENABLE(SVG_FONTS)
