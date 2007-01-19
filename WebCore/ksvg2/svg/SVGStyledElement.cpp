/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGStyledElement.h"

#include "Attr.h"
#include "cssstyleselector.h"
#include "Document.h"
#include "HTMLNames.h"
#include "ksvgcssproperties.h"
#include "PlatformString.h"
#include "RenderView.h"
#include "RenderPath.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGSVGElement.h"

#include <wtf/Assertions.h>

namespace WebCore {

// Defined in CSSGrammar.y, but not in any header, so just declare it here for now.
int getPropertyID(const char* str, int len);

using namespace SVGNames;

SVGStyledElement::SVGStyledElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
{
}

SVGStyledElement::~SVGStyledElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGStyledElement, String, String, string, ClassName, className, HTMLNames::classAttr.localName(), m_className)

RenderObject* SVGStyledElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    // The path data is set upon the first layout() call.
    return new (arena) RenderPath(style, this);
}

static inline int cssPropertyIdForName(const char* propertyName, int propertyLength)
{
    int propertyId = getPropertyID(propertyName, propertyLength);
    if (propertyId == 0)
        propertyId = SVG::getSVGCSSPropertyID(propertyName, propertyLength);
    return propertyId;
}

static inline void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, int>* propertyNameToIdMap, const QualifiedName& attrName, const char* cssPropertyName = 0)
{
    int propertyId = 0;
    if (cssPropertyName)
        propertyId = cssPropertyIdForName(cssPropertyName, strlen(cssPropertyName));
    else {
        DeprecatedString propertyName = attrName.localName().deprecatedString();
        propertyId = cssPropertyIdForName(propertyName.ascii(), propertyName.length());
    }
    ASSERT(propertyId > 0);
    propertyNameToIdMap->set(attrName.localName().impl(), propertyId);
}

int SVGStyledElement::cssPropertyIdForSVGAttributeName(const QualifiedName& attrName)
{
    if (!attrName.namespaceURI().isNull())
        return 0;
    
    static HashMap<AtomicStringImpl*, int>* propertyNameToIdMap = 0;
    if (!propertyNameToIdMap) {
        propertyNameToIdMap = new HashMap<AtomicStringImpl*, int>;
        // This is a list of all base CSS and SVG CSS properties which are exposed as SVG XML attributes
        mapAttributeToCSSProperty(propertyNameToIdMap, alignment_baselineAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, baseline_shiftAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clipAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clip_pathAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clip_ruleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolation_filtersAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_profileAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, cursorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, directionAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, displayAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, dominant_baselineAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, enable_backgroundAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fillAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fill_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fill_ruleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, filterAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, flood_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, flood_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_familyAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_sizeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_stretchAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_styleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_variantAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_weightAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, glyph_orientation_horizontalAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, glyph_orientation_verticalAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, image_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, kerningAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, letter_spacingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, lighting_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_endAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_midAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_startAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, maskAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, overflowAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, pointer_eventsAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, shape_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stop_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stop_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, strokeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_dasharrayAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_dashoffsetAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_linecapAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_linejoinAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_miterlimitAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_widthAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_anchorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_decorationAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, unicode_bidiAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, visibilityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, word_spacingAttr);
    }
    
    return propertyNameToIdMap->get(attrName.localName().impl());
}

bool SVGStyledElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (SVGStyledElement::cssPropertyIdForSVGAttributeName(attrName) > 0) {
        result = eSVG;
        return false;
    }
    return SVGElement::mapToEntry(attrName, result);
}

void SVGStyledElement::parseMappedAttribute(MappedAttribute* attr)
{
    // NOTE: Any subclass which overrides parseMappedAttribute for a property handled by
    // cssPropertyIdForSVGAttributeName will also have to override mapToEntry to disable the default eSVG mapping
    int propId = SVGStyledElement::cssPropertyIdForSVGAttributeName(attr->name());
    if (propId > 0) {
        addCSSProperty(attr, propId, attr->value());
        setChanged();
        return;
    }
    
    // id and class are handled by StyledElement
    SVGElement::parseMappedAttribute(attr);
}

void SVGStyledElement::notifyAttributeChange() const
{
    SVGDocumentExtensions* extensions = document()->accessSVGExtensions();
    if (!extensions)
        return;

    // In case we're referenced by a <use> element, we have element instances registered
    // to us in the SVGDocumentExtensions. If notifyAttributeChange() is called, we need
    // to recursively update all children including ourselves.
    updateElementInstance(extensions);
}

void SVGStyledElement::updateElementInstance(SVGDocumentExtensions* extensions) const
{
    SVGStyledElement* nonConstThis = const_cast<SVGStyledElement*>(this);
    HashSet<SVGElementInstance*>* set = extensions->instancesForElement(nonConstThis);
    if (!set || set->isEmpty())
        return;

    HashSet<SVGElementInstance*>::const_iterator it = set->begin();
    const HashSet<SVGElementInstance*>::const_iterator end = set->end();

    for (; it != end; ++it)
        (*it)->updateInstance(nonConstThis);

    if (hasChildNodes()) {
        for (Node* child = firstChild(); child; child = child->nextSibling()) {
            SVGElement* element = svg_dynamic_cast(child);
            if (!element || !element->isStyled())
                continue;

            SVGStyledElement* styled = static_cast<SVGStyledElement*>(element);
            styled->updateElementInstance(extensions);
        }
    }
}

void SVGStyledElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    // FIXME: Eventually subclasses from SVGElement should implement
    // attributeChanged() instead of notifyAttributeChange()
    // This is a quick fix to allow dynamic updates of SVG elements
    // but will result in slower dynamic-update performance than necessary.
    SVGElement::attributeChanged(attr, preserveDecls);
    notifyAttributeChange();
}

RenderView* SVGStyledElement::view() const
{
    return static_cast<RenderView*>(document()->renderer());
}

void SVGStyledElement::rebuildRenderer() const
{
    if (!renderer() || !renderer()->isRenderPath())
        return;
    
    RenderPath* renderPath = static_cast<RenderPath*>(renderer());    
    SVGElement* parentElement = svg_dynamic_cast(parentNode());
    if (parentElement && parentElement->renderer() && parentElement->isStyled()
        && parentElement->childShouldCreateRenderer(const_cast<SVGStyledElement*>(this)))
        renderPath->setNeedsLayout(true);
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
