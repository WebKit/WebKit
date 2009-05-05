/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>

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

#if ENABLE(SVG)
#include "SVGStyledElement.h"

#include "Attr.h"
#include "CSSParser.h"
#include "CSSStyleSelector.h"
#include "CString.h"
#include "Document.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "PlatformString.h"
#include "RenderObject.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGResource.h"
#include "SVGSVGElement.h"
#include <wtf/Assertions.h>

namespace WebCore {

using namespace SVGNames;

char SVGStyledElementIdentifier[] = "SVGStyledElement";
static HashSet<const SVGStyledElement*>* gElementsWithInstanceUpdatesBlocked = 0;

SVGStyledElement::SVGStyledElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_className(this, HTMLNames::classAttr)
{
}

SVGStyledElement::~SVGStyledElement()
{
    SVGResource::removeClient(this);
}

bool SVGStyledElement::rendererIsNeeded(RenderStyle* style)
{
    // http://www.w3.org/TR/SVG/extend.html#PrivateData
    // Prevent anything other than SVG renderers from appearing in our render tree
    // Spec: SVG allows inclusion of elements from foreign namespaces anywhere
    // with the SVG content. In general, the SVG user agent will include the unknown
    // elements in the DOM but will otherwise ignore unknown elements. 
    if (!parentNode() || parentNode()->isSVGElement())
        return StyledElement::rendererIsNeeded(style);

    return false;
}

static void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, int>* propertyNameToIdMap, const QualifiedName& attrName)
{
    int propertyId = cssPropertyID(attrName.localName());
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
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolationAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolation_filtersAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_profileAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_renderingAttr); 
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
        mapAttributeToCSSProperty(propertyNameToIdMap, writing_modeAttr);
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
    const QualifiedName& attrName = attr->name();
    // NOTE: Any subclass which overrides parseMappedAttribute for a property handled by
    // cssPropertyIdForSVGAttributeName will also have to override mapToEntry to disable the default eSVG mapping
    int propId = SVGStyledElement::cssPropertyIdForSVGAttributeName(attrName);
    if (propId > 0) {
        addCSSProperty(attr, propId, attr->value());
        setNeedsStyleRecalc();
        return;
    }
    
    // SVG animation has currently requires special storage of values so we set
    // the className here.  svgAttributeChanged actually causes the resulting
    // style updates (instead of StyledElement::parseMappedAttribute). We don't
    // tell StyledElement about the change to avoid parsing the class list twice
    if (attrName.matches(HTMLNames::classAttr))
        setClassName(attr->value());
    else
        // id is handled by StyledElement which SVGElement inherits from
        SVGElement::parseMappedAttribute(attr);
}

bool SVGStyledElement::isKnownAttribute(const QualifiedName& attrName)
{
    // Recognize all style related SVG CSS properties
    int propId = SVGStyledElement::cssPropertyIdForSVGAttributeName(attrName);
    if (propId > 0)
        return true;

    return (attrName == HTMLNames::idAttr || attrName == HTMLNames::styleAttr); 
}

void SVGStyledElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGElement::svgAttributeChanged(attrName);

    if (attrName.matches(HTMLNames::classAttr))
        classAttributeChanged(className());

    // If we're the child of a resource element, be sure to invalidate it.
    invalidateResourcesInAncestorChain();

    // Invalidate all SVGElementInstances associated with us
    SVGElementInstance::invalidateAllInstancesOfElement(this);
}

void SVGStyledElement::invalidateResourcesInAncestorChain() const
{
    Node* node = parentNode();
    while (node) {
        if (!node->isSVGElement())
            break;

        SVGElement* element = static_cast<SVGElement*>(node);
        if (SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(element->isStyled() ? element : 0)) {
            if (SVGResource* resource = styledElement->canvasResource())
                resource->invalidate();
        }

        node = node->parentNode();
    }
}

void SVGStyledElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (document()->parsing())
        return;

    // Invalidate all SVGElementInstances associated with us
    SVGElementInstance::invalidateAllInstancesOfElement(this);
}

PassRefPtr<RenderStyle> SVGStyledElement::resolveStyle(RenderStyle* parentStyle)
{
    if (renderer())
        return renderer()->style();
    return document()->styleSelector()->styleForElement(this, parentStyle);
}

PassRefPtr<CSSValue> SVGStyledElement::getPresentationAttribute(const String& name)
{
    if (!mappedAttributes())
        return 0;

    Attribute* attr = mappedAttributes()->getAttributeItem(QualifiedName(nullAtom, name, nullAtom));
    if (!attr || !attr->isMappedAttribute() || !attr->style())
        return 0;

    MappedAttribute* cssSVGAttr = static_cast<MappedAttribute*>(attr);

    // FIXME: Is it possible that the style will not be shared at the time this
    // is called, but a later addition to the DOM will make it shared?
    if (!cssSVGAttr->style()->hasOneRef()) {
        cssSVGAttr->setDecl(0);
        int propId = SVGStyledElement::cssPropertyIdForSVGAttributeName(cssSVGAttr->name());
        addCSSProperty(cssSVGAttr, propId, cssSVGAttr->value());
    }

    return cssSVGAttr->style()->getPropertyCSSValue(name);
}

void SVGStyledElement::detach()
{
    SVGResource::removeClient(this);
    SVGElement::detach();
}

void SVGStyledElement::setInstanceUpdatesBlocked(bool blockUpdates)
{
    if (blockUpdates) {
        if (!gElementsWithInstanceUpdatesBlocked)
            gElementsWithInstanceUpdatesBlocked = new HashSet<const SVGStyledElement*>;
        gElementsWithInstanceUpdatesBlocked->add(this);
    } else {
        ASSERT(gElementsWithInstanceUpdatesBlocked);
        ASSERT(gElementsWithInstanceUpdatesBlocked->contains(this));
        gElementsWithInstanceUpdatesBlocked->remove(this);
    }
}
    
}

#endif // ENABLE(SVG)
