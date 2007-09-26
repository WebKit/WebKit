/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPatternElement.h"

#include "AffineTransform.h"
#include "Document.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PatternAttributes.h"
#include "RenderSVGContainer.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPaintServerPattern.h"
#include "SVGSVGElement.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include "SVGUnitTypes.h"

#include <math.h>
#include <wtf/OwnPtr.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

SVGPatternElement::SVGPatternElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , m_x(this, LengthModeWidth)
    , m_y(this, LengthModeHeight)
    , m_width(this, LengthModeWidth)
    , m_height(this, LengthModeHeight)
    , m_patternUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_patternContentUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_patternTransform(new SVGTransformList)
{
}

SVGPatternElement::~SVGPatternElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, int, Enumeration, enumeration, PatternUnits, patternUnits, SVGNames::patternUnitsAttr.localName(), m_patternUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, int, Enumeration, enumeration, PatternContentUnits, patternContentUnits, SVGNames::patternContentUnitsAttr.localName(), m_patternContentUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength, Length, length, X, x, SVGNames::xAttr.localName(), m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength, Length, length, Y, y, SVGNames::yAttr.localName(), m_y)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGLength, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height)
ANIMATED_PROPERTY_DEFINITIONS(SVGPatternElement, SVGTransformList*, TransformList, transformList, PatternTransform, patternTransform, SVGNames::patternTransformAttr.localName(), m_patternTransform.get())

void SVGPatternElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::patternUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setPatternUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setPatternUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::patternContentUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setPatternContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setPatternContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::patternTransformAttr) {
        SVGTransformList* patternTransforms = patternTransformBaseValue();
        if (!SVGTransformable::parseTransformAttribute(patternTransforms, attr->value())) {
            ExceptionCode ec = 0;
            patternTransforms->clear(ec);
        }
    } else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
        if (width().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for pattern attribute <width> is not allowed");
    } else if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
        if (width().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for pattern attribute <height> is not allowed");
    } else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGFitToViewBox::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGPatternElement::buildPattern(const FloatRect& targetRect) const
{
    PatternAttributes attributes = collectPatternProperties();

    // If we didn't find any pattern content, ignore the request.
    if (!attributes.patternContentElement())
        return;

    // Determine specified pattern size
    float xValue = narrowPrecisionToFloat(attributes.x());
    float yValue = narrowPrecisionToFloat(attributes.y());
    float widthValue = narrowPrecisionToFloat(attributes.width());
    float heightValue = narrowPrecisionToFloat(attributes.height());

    if (attributes.boundingBoxMode()) {
        xValue *= targetRect.width();
        yValue *= targetRect.height();
        widthValue *= targetRect.width();
        heightValue *= targetRect.height();
    }

    // As we're allocating buffers here, clip the buffer size to the target object size as upper boundary
    if (widthValue > targetRect.width())
        widthValue = targetRect.width();

    if (heightValue > targetRect.height())
        heightValue = targetRect.height();

    auto_ptr<ImageBuffer> patternImage = ImageBuffer::create(IntSize(lroundf(widthValue), lroundf(heightValue)), false);
    if (!patternImage.get())
        return;

    GraphicsContext* context = patternImage->context();
    ASSERT(context);
 
    if (attributes.boundingBoxModeContent()) {
        context->save();
        context->scale(FloatSize(targetRect.width(), targetRect.height()));
    }

    // Render subtree into ImageBuffer
    for (Node* n = attributes.patternContentElement()->firstChild(); n; n = n->nextSibling()) {
        SVGElement* elem = svg_dynamic_cast(n);
        if (!elem || !elem->isStyled())
            continue;

        SVGStyledElement* e = static_cast<SVGStyledElement*>(elem);
        RenderObject* item = e->renderer();
        if (!item)
            continue;

        ImageBuffer::renderSubtreeToImage(patternImage.get(), item);
    }

    if (attributes.boundingBoxModeContent())
        context->restore();

    m_resource->setPatternTransform(attributes.patternTransform());
    m_resource->setPatternBoundaries(FloatRect(xValue, yValue, widthValue, heightValue));
    m_resource->setTile(patternImage);
}

void SVGPatternElement::notifyAttributeChange() const
{
    if (!m_resource || !attached() || ownerDocument()->parsing())
        return;

    m_resource->invalidate();
    m_resource->repaintClients();
}

RenderObject* SVGPatternElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderSVGContainer* patternContainer = new (arena) RenderSVGContainer(this);
    patternContainer->setDrawsContents(false);
    return patternContainer;
}

SVGResource* SVGPatternElement::canvasResource()
{
    if (!m_resource)
        m_resource = new SVGPaintServerPattern(this);

    return m_resource.get();
}

PatternAttributes SVGPatternElement::collectPatternProperties() const
{
    PatternAttributes attributes;
    HashSet<const SVGPatternElement*> processedPatterns;

    const SVGPatternElement* current = this;
    while (current) {
        if (!attributes.hasX() && current->hasAttribute(SVGNames::xAttr))
            attributes.setX(current->x().valueAsPercentage());

        if (!attributes.hasY() && current->hasAttribute(SVGNames::yAttr))
            attributes.setY(current->y().valueAsPercentage());

        if (!attributes.hasWidth() && current->hasAttribute(SVGNames::widthAttr))
            attributes.setWidth(current->width().valueAsPercentage());

        if (!attributes.hasHeight() && current->hasAttribute(SVGNames::heightAttr))
            attributes.setHeight(current->height().valueAsPercentage());

        if (!attributes.hasBoundingBoxMode() && current->hasAttribute(SVGNames::patternUnitsAttr))
            attributes.setBoundingBoxMode(current->getAttribute(SVGNames::patternUnitsAttr) == "objectBoundingBox");

        if (!attributes.hasBoundingBoxModeContent() && current->hasAttribute(SVGNames::patternContentUnitsAttr))
            attributes.setBoundingBoxModeContent(current->getAttribute(SVGNames::patternContentUnitsAttr) == "objectBoundingBox");

        if (!attributes.hasPatternTransform() && current->hasAttribute(SVGNames::patternTransformAttr))
            attributes.setPatternTransform(current->patternTransform()->consolidate().matrix());

        if (!attributes.hasPatternContentElement() && current->hasChildNodes())
            attributes.setPatternContentElement(current);

        processedPatterns.add(current);

        // Respect xlink:href, take attributes from referenced element
        Node* refNode = ownerDocument()->getElementById(SVGURIReference::getTarget(current->href()));
        if (refNode && refNode->hasTagName(SVGNames::patternTag)) {
            current = static_cast<const SVGPatternElement*>(const_cast<const Node*>(refNode));

            // Cycle detection
            if (processedPatterns.contains(current))
                return PatternAttributes();
        } else
            current = 0;
    }

    return attributes;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
