/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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
#include "SVGViewElement.h"

#include "Attribute.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include "SVGStringList.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_BOOLEAN(SVGViewElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)
DEFINE_ANIMATED_RECT(SVGViewElement, SVGNames::viewBoxAttr, ViewBox, viewBox)
DEFINE_ANIMATED_PRESERVEASPECTRATIO(SVGViewElement, SVGNames::preserveAspectRatioAttr, PreserveAspectRatio, preserveAspectRatio)

inline SVGViewElement::SVGViewElement(const QualifiedName& tagName, Document* document)
    : SVGStyledElement(tagName, document)
    , m_viewTarget(SVGNames::viewTargetAttr)
{
    ASSERT(hasTagName(SVGNames::viewTag));
}

PassRefPtr<SVGViewElement> SVGViewElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGViewElement(tagName, document));
}

bool SVGViewElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        SVGZoomAndPan::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::viewTargetAttr);
    }
    return supportedAttributes.contains(attrName);
}

void SVGViewElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGStyledElement::parseMappedAttribute(attr);
        return;
    }

    if (attr->name() == SVGNames::viewTargetAttr) {
        viewTarget().reset(attr->value());
        return;
    }

    if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
        return;
    if (SVGFitToViewBox::parseMappedAttribute(document(), attr))
        return;
    if (SVGZoomAndPan::parseMappedAttribute(attr))
        return;

    ASSERT_NOT_REACHED();
}

void SVGViewElement::synchronizeProperty(const QualifiedName& attrName)
{
    if (attrName == anyQName()) {
        synchronizeExternalResourcesRequired();
        SVGFitToViewBox::synchronizeProperties(attrName);
        SVGStyledElement::synchronizeProperty(attrName);
        return;
    }

    if (!isSupportedAttribute(attrName)) {
        SVGStyledElement::synchronizeProperty(attrName);
        return;
    }

    if (attrName == SVGNames::viewTargetAttr)
        return;

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        synchronizeExternalResourcesRequired();
        return;
    }

    if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        SVGFitToViewBox::synchronizeProperties(attrName);
        return;
    }

    ASSERT_NOT_REACHED();
}

AttributeToPropertyTypeMap& SVGViewElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGViewElement::fillAttributeToPropertyTypeMap()
{
    AttributeToPropertyTypeMap& attributeToPropertyTypeMap = this->attributeToPropertyTypeMap();

    SVGStyledElement::fillPassedAttributeToPropertyTypeMap(attributeToPropertyTypeMap);
    attributeToPropertyTypeMap.set(SVGNames::viewBoxAttr, AnimatedRect);
    attributeToPropertyTypeMap.set(SVGNames::preserveAspectRatioAttr, AnimatedPreserveAspectRatio);
}

}

#endif // ENABLE(SVG)
