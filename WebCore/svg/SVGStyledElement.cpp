/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "SVGStyledElement.h"

#include "Attr.h"
#include "CSSParser.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "HTMLNames.h"
#include "PlatformString.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMasker.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"
#include "SVGElementRareData.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "SVGUseElement.h"
#include <wtf/Assertions.h>

namespace WebCore {

using namespace SVGNames;

void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, int>* propertyNameToIdMap, const QualifiedName& attrName)
{
    int propertyId = cssPropertyID(attrName.localName());
    ASSERT(propertyId > 0);
    propertyNameToIdMap->set(attrName.localName().impl(), propertyId);
}

SVGStyledElement::SVGStyledElement(const QualifiedName& tagName, Document* document)
    : SVGElement(tagName, document)
{
}

SVGStyledElement::~SVGStyledElement()
{
}

String SVGStyledElement::title() const
{
    // According to spec, we should not return titles when hovering over root <svg> elements (those
    // <title> elements are the title of the document, not a tooltip) so we instantly return.
    if (hasTagName(SVGNames::svgTag)) {
        const SVGSVGElement* svg = static_cast<const SVGSVGElement*>(this);
        if (svg->isOutermostSVG())
            return String();
    }
    
    // Walk up the tree, to find out whether we're inside a <use> shadow tree, to find the right title.
    Node* parent = const_cast<SVGStyledElement*>(this);
    while (parent) {
        if (!parent->isShadowNode()) {
            parent = parent->parentNode();
            continue;
        }
        
        // Get the <use> element.
        ContainerNode* shadowParent = parent->shadowParentNode();
        if (shadowParent && shadowParent->isSVGElement() && shadowParent->hasTagName(SVGNames::useTag)) {
            SVGUseElement* useElement = static_cast<SVGUseElement*>(shadowParent);
            // If the <use> title is not empty we found the title to use.
            String useTitle(useElement->title());
            if (useTitle.isEmpty())
                break;
            return useTitle;
        }
        parent = parent->parentNode();
    }
    
    // If we aren't an instance in a <use> or the <use> title was not found, then find the first
    // <title> child of this element.
    Element* titleElement = firstElementChild();
    for (; titleElement; titleElement = titleElement->nextElementSibling()) {
        if (titleElement->hasTagName(SVGNames::titleTag) && titleElement->isSVGElement())
            break;
    }

    // If a title child was found, return the text contents.
    if (titleElement)
        return titleElement->innerText();
    
    // Otherwise return a null/empty string.
    return String();
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
        mapAttributeToCSSProperty(propertyNameToIdMap, vector_effectAttr);
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

void SVGStyledElement::parseMappedAttribute(Attribute* attr)
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
    return isIdAttributeName(attrName);
}

void SVGStyledElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGElement::svgAttributeChanged(attrName);

    if (attrName.matches(HTMLNames::classAttr))
        classAttributeChanged(className());

    RenderObject* object = renderer();

    if (isIdAttributeName(attrName)) {
        // Notify resources about id changes, this is important as we cache resources by id in SVGDocumentExtensions
        if (object && object->isSVGResourceContainer())
            object->toRenderSVGResourceContainer()->idChanged();
    }

    // Invalidate all SVGElementInstances associated with us
    SVGElementInstance::invalidateAllInstancesOfElement(this);
}

void SVGStyledElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGElement::synchronizeProperty(attrName);

    if (attrName == anyQName() || attrName.matches(HTMLNames::classAttr))
        synchronizeClassName();
}

void SVGStyledElement::attach()
{
    SVGElement::attach();

    if (RenderObject* object = renderer())
        object->updateFromElement();
}

void SVGStyledElement::insertedIntoDocument()
{
    SVGElement::insertedIntoDocument();
    updateRelativeLengthsInformation();
}

void SVGStyledElement::removedFromDocument()
{
    updateRelativeLengthsInformation(false, this);
    SVGElement::removedFromDocument();
}

void SVGStyledElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    // Invalidate all SVGElementInstances associated with us
    if (!changedByParser)
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
    if (!attributeMap())
        return 0;

    QualifiedName attributeName(nullAtom, name, nullAtom);
    Attribute* attr = attributeMap()->getAttributeItem(attributeName);
    if (!attr || !attr->isMappedAttribute() || !attr->style())
        return 0;

    Attribute* cssSVGAttr = attr;
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

bool SVGStyledElement::instanceUpdatesBlocked() const
{
    return hasRareSVGData() && rareSVGData()->instanceUpdatesBlocked();
}

void SVGStyledElement::setInstanceUpdatesBlocked(bool value)
{
    if (hasRareSVGData())
        rareSVGData()->setInstanceUpdatesBlocked(value);
}

AffineTransform SVGStyledElement::localCoordinateSpaceTransform(SVGLocatable::CTMScope) const
{
    // To be overriden by SVGStyledLocatableElement/SVGStyledTransformableElement (or as special case SVGTextElement)
    ASSERT_NOT_REACHED();
    return AffineTransform();
}

void SVGStyledElement::updateRelativeLengthsInformation(bool hasRelativeLengths, SVGStyledElement* element)
{
    // If we're not yet in a document, this function will be called again from insertedIntoDocument(). Do nothing now.
    if (!inDocument())
        return;

    // An element wants to notify us that its own relative lengths state changed.
    // Register it in the relative length map, and register us in the parent relative length map.
    // Register the parent in the grandparents map, etc. Repeat procedure until the root of the SVG tree.

    if (hasRelativeLengths)
        m_elementsWithRelativeLengths.add(element);
    else {
        if (!m_elementsWithRelativeLengths.contains(element)) {
            // We were never registered. Do nothing.
            return;
        }

        m_elementsWithRelativeLengths.remove(element);
    }

    // Find first styled parent node, and notify it that we've changed our relative length state.
    ContainerNode* node = parentNode();
    while (node) {
        if (!node->isSVGElement())
            break;

        SVGElement* element = static_cast<SVGElement*>(node);
        if (!element->isStyled()) {
            node = node->parentNode();
            continue;
        }

        // Register us in the parent element map.
        static_cast<SVGStyledElement*>(element)->updateRelativeLengthsInformation(hasRelativeLengths, this);
        break;
    }
}

}

#endif // ENABLE(SVG)
