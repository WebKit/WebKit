/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#include "SVGFontFaceElement.h"

#include "CSSFontFaceSrcValue.h"
#include "CSSFontSelector.h"
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "Document.h"
#include "ElementChildIteratorInlines.h"
#include "FontCascade.h"
#include "Logging.h"
#include "MutableStyleProperties.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFontElement.h"
#include "SVGFontFaceSrcElement.h"
#include "SVGGlyphElement.h"
#include "SVGNames.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFontFaceElement);

using namespace SVGNames;

inline SVGFontFaceElement::SVGFontFaceElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , m_fontFaceRule(StyleRuleFontFace::create(MutableStyleProperties::create(HTMLStandardMode)))
{
    LOG(Fonts, "SVGFontFaceElement %p ctor", this);
    ASSERT(hasTagName(font_faceTag));
}

SVGFontFaceElement::~SVGFontFaceElement()
{
    LOG(Fonts, "SVGFontFaceElement %p dtor", this);
}

Ref<SVGFontFaceElement> SVGFontFaceElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFontFaceElement(tagName, document));
}

void SVGFontFaceElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    CSSPropertyID propertyId = cssPropertyIdForSVGAttributeName(name);
    if (propertyId > 0) {
        // FIXME: Parse using the @font-face descriptor grammars, not the property grammars.
        auto& properties = m_fontFaceRule->mutableProperties();
        bool valueChanged = properties.setProperty(propertyId, newValue);

        if (valueChanged) {
            // The above parser is designed for the font-face properties, not descriptors, and the properties accept the global keywords, but descriptors don't.
            // Rather than invasively modifying the parser for the properties to have a special mode, we can simply detect the error condition after-the-fact and
            // avoid it explicitly.
            if (auto parsedValue = properties.propertyAsValueID(propertyId)) {
                if (isCSSWideKeyword(*parsedValue))
                    properties.removeProperty(propertyId);
            }
        }

        rebuildFontFace();
        return;
    }

    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

unsigned SVGFontFaceElement::unitsPerEm() const
{
    const AtomString& valueString = attributeWithoutSynchronization(units_per_emAttr);
    if (valueString.isEmpty())
        return FontMetrics::defaultUnitsPerEm;

    auto value = static_cast<unsigned>(ceilf(valueString.toFloat()));
    if (!value)
        return FontMetrics::defaultUnitsPerEm;

    return value;
}

int SVGFontFaceElement::xHeight() const
{
    return static_cast<int>(ceilf(attributeWithoutSynchronization(x_heightAttr).toFloat()));
}

int SVGFontFaceElement::capHeight() const
{
    return static_cast<int>(ceilf(attributeWithoutSynchronization(cap_heightAttr).toFloat()));
}

float SVGFontFaceElement::horizontalOriginX() const
{
    RefPtr fontElement = m_fontElement.get();
    if (!fontElement)
        return 0.0f;

    // Spec: The X-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing horizontally oriented text. (Note that the origin applies to all glyphs in the font.)
    // If the attribute is not specified, the effect is as if a value of "0" were specified.
    return fontElement->attributeWithoutSynchronization(horiz_origin_xAttr).toFloat();
}

float SVGFontFaceElement::horizontalOriginY() const
{
    RefPtr fontElement = m_fontElement.get();
    if (!fontElement)
        return 0.0f;

    // Spec: The Y-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing horizontally oriented text. (Note that the origin applies to all glyphs in the font.)
    // If the attribute is not specified, the effect is as if a value of "0" were specified.
    return fontElement->attributeWithoutSynchronization(horiz_origin_yAttr).toFloat();
}

float SVGFontFaceElement::horizontalAdvanceX() const
{
    RefPtr fontElement = m_fontElement.get();
    if (!fontElement)
        return 0.0f;

    // Spec: The default horizontal advance after rendering a glyph in horizontal orientation. Glyph
    // widths are required to be non-negative, even if the glyph is typically rendered right-to-left,
    // as in Hebrew and Arabic scripts.
    return fontElement->attributeWithoutSynchronization(horiz_adv_xAttr).toFloat();
}

float SVGFontFaceElement::verticalOriginX() const
{
    RefPtr fontElement = m_fontElement.get();
    if (!fontElement)
        return 0.0f;

    // Spec: The default X-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing vertically oriented text. If the attribute is not specified, the effect is as if the attribute
    // were set to half of the effective value of attribute horiz-adv-x.
    const AtomString& value = fontElement->attributeWithoutSynchronization(vert_origin_xAttr);
    if (value.isEmpty())
        return horizontalAdvanceX() / 2.0f;

    return value.toFloat();
}

float SVGFontFaceElement::verticalOriginY() const
{
    RefPtr fontElement = m_fontElement.get();
    if (!fontElement)
        return 0.0f;

    // Spec: The default Y-coordinate in the font coordinate system of the origin of a glyph to be used when
    // drawing vertically oriented text. If the attribute is not specified, the effect is as if the attribute
    // were set to the position specified by the font's ascent attribute.             
    const AtomString& value = fontElement->attributeWithoutSynchronization(vert_origin_yAttr);
    if (value.isEmpty())
        return ascent();

    return value.toFloat();
}

float SVGFontFaceElement::verticalAdvanceY() const
{
    RefPtr fontElement = m_fontElement.get();
    if (!fontElement)
        return 0.0f;

    // Spec: The default vertical advance after rendering a glyph in vertical orientation. If the attribute is
    // not specified, the effect is as if a value equivalent of one em were specified (see units-per-em).                    
    const AtomString& value = fontElement->attributeWithoutSynchronization(vert_adv_yAttr);
       if (value.isEmpty())
        return 1.0f;

    return value.toFloat();
}

int SVGFontFaceElement::ascent() const
{
    // Spec: Same syntax and semantics as the 'ascent' descriptor within an @font-face rule. The maximum
    // unaccented height of the font within the font coordinate system. If the attribute is not specified,
    // the effect is as if the attribute were set to the difference between the units-per-em value and the
    // vert-origin-y value for the corresponding font.
    const AtomString& ascentValue = attributeWithoutSynchronization(ascentAttr);
    if (!ascentValue.isEmpty())
        return static_cast<int>(ceilf(ascentValue.toFloat()));

    if (RefPtr fontElement = m_fontElement.get()) {
        const AtomString& vertOriginY = fontElement->attributeWithoutSynchronization(vert_origin_yAttr);
        if (!vertOriginY.isEmpty())
            return static_cast<int>(unitsPerEm()) - static_cast<int>(ceilf(vertOriginY.toFloat()));
    }

    // Match Batiks default value
    return static_cast<int>(ceilf(unitsPerEm() * 0.8f));
}

int SVGFontFaceElement::descent() const
{
    // Spec: Same syntax and semantics as the 'descent' descriptor within an @font-face rule. The maximum
    // unaccented depth of the font within the font coordinate system. If the attribute is not specified,
    // the effect is as if the attribute were set to the vert-origin-y value for the corresponding font.
    const AtomString& descentValue = attributeWithoutSynchronization(descentAttr);
    if (!descentValue.isEmpty()) {
        // 14 different W3C SVG 1.1 testcases use a negative descent value,
        // where a positive was meant to be used  Including:
        // animate-elem-24-t.svg, fonts-elem-01-t.svg, fonts-elem-02-t.svg (and 11 others)
        int descent = static_cast<int>(ceilf(descentValue.toFloat()));
        return descent < 0 ? -descent : descent;
    }

    if (RefPtr fontElement = m_fontElement.get()) {
        const AtomString& vertOriginY = fontElement->attributeWithoutSynchronization(vert_origin_yAttr);
        if (!vertOriginY.isEmpty())
            return static_cast<int>(ceilf(vertOriginY.toFloat()));
    }

    // Match Batiks default value
    return static_cast<int>(ceilf(unitsPerEm() * 0.2f));
}

String SVGFontFaceElement::fontFamily() const
{
    return m_fontFaceRule->properties().getPropertyValue(CSSPropertyFontFamily);
}

SVGFontElement* SVGFontFaceElement::associatedFontElement() const
{
    ASSERT(parentNode() == m_fontElement);
    ASSERT(!parentNode() || is<SVGFontElement>(*parentNode()));
    return m_fontElement.get();
}

void SVGFontFaceElement::rebuildFontFace()
{
    if (!isConnected()) {
        ASSERT(!m_fontElement);
        return;
    }

    // we currently ignore all but the first src element, alternatively we could concat them
    auto srcElement = childrenOfType<SVGFontFaceSrcElement>(*this).first();

    m_fontElement = dynamicDowncast<SVGFontElement>(*parentNode());
    bool describesParentFont = !!m_fontElement;

    RefPtr<CSSValueList> list;
    if (m_fontElement)
        list = CSSValueList::createCommaSeparated(CSSFontFaceSrcLocalValue::create(AtomString { fontFamily() }));
    else if (srcElement)
        list = srcElement->createSrcValue();

    if (!list || !list->length())
        return;

    // Parse in-memory CSS rules
    m_fontFaceRule->mutableProperties().addParsedProperty(CSSProperty(CSSPropertySrc, list));

    if (describesParentFont) {
        // Traverse parsed CSS values and associate CSSFontFaceSrcLocalValue elements with ourselves.
        if (auto* srcList = downcast<CSSValueList>(m_fontFaceRule->properties().getPropertyCSSValue(CSSPropertySrc).get())) {
            for (auto& item : *srcList)
                downcast<CSSFontFaceSrcLocalValue>(const_cast<CSSValue&>(item)).setSVGFontFaceElement(*this);
        }
    }

    document().styleScope().didChangeStyleSheetEnvironment();
}

Node::InsertedIntoAncestorResult SVGFontFaceElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument) {
        ASSERT(!m_fontElement);
        return InsertedIntoAncestorResult::Done;
    }
    document().accessSVGExtensions().registerSVGFontFaceElement(*this);

    rebuildFontFace();
    return result;
}

void SVGFontFaceElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (removalType.disconnectedFromDocument) {
        m_fontElement = nullptr;
        document().accessSVGExtensions().unregisterSVGFontFaceElement(*this);
        auto& fontFaceSet = document().fontSelector().cssFontFaceSet();
        if (auto* fontFace = fontFaceSet.lookUpByCSSConnection(m_fontFaceRule))
            fontFaceSet.remove(*fontFace);
        m_fontFaceRule->mutableProperties().clear();

        document().styleScope().didChangeStyleSheetEnvironment();
    } else
        ASSERT(!m_fontElement);
}

void SVGFontFaceElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);
    rebuildFontFace();
}

} // namespace WebCore
