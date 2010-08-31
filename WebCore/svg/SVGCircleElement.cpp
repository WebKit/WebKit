/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGCircleElement.h"

#include "Attribute.h"
#include "FloatPoint.h"
#include "RenderPath.h"
#include "RenderSVGResource.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

inline SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document* document)
    : SVGStyledTransformableElement(tagName, document)
    , m_cx(LengthModeWidth)
    , m_cy(LengthModeHeight)
    , m_r(LengthModeOther)
{
}

PassRefPtr<SVGCircleElement> SVGCircleElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGCircleElement(tagName, document));
}

void SVGCircleElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength(LengthModeWidth, attr->value()));       
    else if (attr->name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::rAttr) {
        setRBaseValue(SVGLength(LengthModeOther, attr->value()));
        if (rBaseValue().value(this) < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for circle <r> is not allowed");
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGCircleElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    bool isLengthAttribute = attrName == SVGNames::cxAttr
                          || attrName == SVGNames::cyAttr
                          || attrName == SVGNames::rAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    RenderPath* renderer = static_cast<RenderPath*>(this->renderer());
    if (!renderer)
        return;

    if (SVGStyledTransformableElement::isKnownAttribute(attrName)) {
        renderer->setNeedsTransformUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    if (isLengthAttribute) {
        renderer->setNeedsPathUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    if (SVGTests::isKnownAttribute(attrName)
        || SVGLangSpace::isKnownAttribute(attrName)
        || SVGExternalResourcesRequired::isKnownAttribute(attrName))
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
}

void SVGCircleElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeCx();
        synchronizeCy();
        synchronizeR();
        synchronizeExternalResourcesRequired();
        return;
    }

    if (attrName == SVGNames::cxAttr)
        synchronizeCx();
    else if (attrName == SVGNames::cyAttr)
        synchronizeCy();
    else if (attrName == SVGNames::rAttr)
        synchronizeR();
    else if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
}

Path SVGCircleElement::toPathData() const
{
    return Path::createCircle(FloatPoint(cx().value(this), cy().value(this)), r().value(this));
}

bool SVGCircleElement::selfHasRelativeLengths() const
{
    return cx().isRelative()
        || cy().isRelative()
        || r().isRelative();
}
 
}

#endif // ENABLE(SVG)
