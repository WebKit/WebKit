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
#include "RenderSVGResource.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceMasker.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"
#include "SVGElementRareData.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGResourceFilter.h"
#include "SVGSVGElement.h"
#include <wtf/Assertions.h>

namespace WebCore {

using namespace SVGNames;

void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, int>* propertyNameToIdMap, const QualifiedName& attrName)
{
    int propertyId = cssPropertyID(attrName.localName());
    ASSERT(propertyId > 0);
    propertyNameToIdMap->set(attrName.localName().impl(), propertyId);
}

SVGStyledElement::SVGStyledElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
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
        mapAttributeToCSSProperty(propertyNameToIdMap, SVGNames::colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolationAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolation_filtersAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_profileAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_renderingAttr); 
        mapAttributeToCSSProperty(propertyNameToIdMap, cursorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, SVGNames::directionAttr);
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
        setClassNameBaseValue(attr->value());
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

    return (attrName == idAttributeName() || attrName == HTMLNames::styleAttr); 
}

void SVGStyledElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGElement::svgAttributeChanged(attrName);

    if (attrName.matches(HTMLNames::classAttr))
        classAttributeChanged(className());

    // If we're the child of a resource element, be sure to invalidate it.
    invalidateResourcesInAncestorChain();

    // If the element is using resources, invalidate them.
    invalidateResources();

    // Invalidate all SVGElementInstances associated with us
    SVGElementInstance::invalidateAllInstancesOfElement(this);
}

void SVGStyledElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGElement::synchronizeProperty(attrName);

    if (attrName == anyQName() || attrName.matches(HTMLNames::classAttr))
        synchronizeClassName();
}

void SVGStyledElement::invalidateResources()
{
    RenderObject* object = renderer();
    if (!object)
        return;

    const SVGRenderStyle* svgStyle = object->style()->svgStyle();
    Document* document = this->document();

    if (document->parsing())
        return;

#if ENABLE(FILTERS)
    SVGResourceFilter* filter = getFilterById(document, svgStyle->filter(), object);
    if (filter)
        filter->invalidate();
#endif

    if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(document, svgStyle->maskElement()))
        masker->invalidateClient(object);

    if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(document, svgStyle->clipPath()))
        clipper->invalidateClient(object);
}

void SVGStyledElement::invalidateResourcesInAncestorChain() const
{
    Node* node = parentNode();
    while (node) {
        if (!node->isSVGElement())
            break;

        SVGElement* element = static_cast<SVGElement*>(node);
        if (SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(element->isStyled() ? element : 0))
            styledElement->invalidateCanvasResources();

        node = node->parentNode();
    }
}

void SVGStyledElement::invalidateCanvasResources()
{
    RenderObject* object = renderer();
    if (!object)
        return;

    if (object->isSVGResource())
        object->toRenderSVGResource()->invalidateClients();

    // The following lines will be removed soon, once all resources are handled by renderers.
    if (SVGResource* resource = canvasResource(object))
        resource->invalidate();
}

void SVGStyledElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

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

    QualifiedName attributeName(nullAtom, name, nullAtom);
    Attribute* attr = mappedAttributes()->getAttributeItem(attributeName);
    if (!attr || !attr->isMappedAttribute() || !attr->style())
        return 0;

    MappedAttribute* cssSVGAttr = static_cast<MappedAttribute*>(attr);
    // This function returns a pointer to a CSSValue which can be mutated from JavaScript.
    // If the associated MappedAttribute uses the same CSSMappedAttributeDeclaration
    // as StyledElement's mappedAttributeDecls cache, create a new CSSMappedAttributeDeclaration
    // before returning so that any modifications to the CSSValue will not affect other attributes.
    MappedAttributeEntry entry;
    mapToEntry(attributeName, entry);
    if (getMappedAttributeDecl(entry, cssSVGAttr) == cssSVGAttr->decl()) {
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

bool SVGStyledElement::instanceUpdatesBlocked() const
{
    return hasRareSVGData() && rareSVGData()->instanceUpdatesBlocked();
}

void SVGStyledElement::setInstanceUpdatesBlocked(bool value)
{
    if (hasRareSVGData())
        rareSVGData()->setInstanceUpdatesBlocked(value);
}

}

#endif // ENABLE(SVG)
