/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGLinearGradientElement.h"

#include "Document.h"
#include "FloatPoint.h"
#include "LinearGradientAttributes.h"
#include "RenderSVGResourceLinearGradient.h"
#include "SVGLengthValue.h"
#include "SVGNames.h"
#include "SVGUnitTypes.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGLinearGradientElement);

inline SVGLinearGradientElement::SVGLinearGradientElement(const QualifiedName& tagName, Document& document)
    : SVGGradientElement(tagName, document)
{
    // Spec: If the x2 attribute is not specified, the effect is as if a value of "100%" were specified.
    ASSERT(hasTagName(SVGNames::linearGradientTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::x1Attr, &SVGLinearGradientElement::m_x1>();
        PropertyRegistry::registerProperty<SVGNames::y1Attr, &SVGLinearGradientElement::m_y1>();
        PropertyRegistry::registerProperty<SVGNames::x2Attr, &SVGLinearGradientElement::m_x2>();
        PropertyRegistry::registerProperty<SVGNames::y2Attr, &SVGLinearGradientElement::m_y2>();
    });
}

Ref<SVGLinearGradientElement> SVGLinearGradientElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGLinearGradientElement(tagName, document));
}

void SVGLinearGradientElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::x1Attr)
        m_x1->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::y1Attr)
        m_y1->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::x2Attr)
        m_x2->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::y2Attr)
        m_y2->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));

    reportAttributeParsingError(parseError, name, value);

    SVGGradientElement::parseAttribute(name, value);
}

void SVGLinearGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        updateRelativeLengthsInformation();
        if (RenderObject* object = renderer())
            object->setNeedsLayout();
        return;
    }

    SVGGradientElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGLinearGradientElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGResourceLinearGradient>(*this, WTFMove(style));
}

static void setGradientAttributes(SVGGradientElement& element, LinearGradientAttributes& attributes, bool isLinear = true)
{
    if (!attributes.hasSpreadMethod() && element.hasAttribute(SVGNames::spreadMethodAttr))
        attributes.setSpreadMethod(element.spreadMethod());

    if (!attributes.hasGradientUnits() && element.hasAttribute(SVGNames::gradientUnitsAttr))
        attributes.setGradientUnits(element.gradientUnits());

    if (!attributes.hasGradientTransform() && element.hasAttribute(SVGNames::gradientTransformAttr))
        attributes.setGradientTransform(element.gradientTransform().concatenate());

    if (!attributes.hasStops())
        attributes.setStops(element.buildStops());

    if (isLinear) {
        SVGLinearGradientElement& linear = downcast<SVGLinearGradientElement>(element);

        if (!attributes.hasX1() && element.hasAttribute(SVGNames::x1Attr))
            attributes.setX1(linear.x1());

        if (!attributes.hasY1() && element.hasAttribute(SVGNames::y1Attr))
            attributes.setY1(linear.y1());

        if (!attributes.hasX2() && element.hasAttribute(SVGNames::x2Attr))
            attributes.setX2(linear.x2());

        if (!attributes.hasY2() && element.hasAttribute(SVGNames::y2Attr))
            attributes.setY2(linear.y2());
    }
}

bool SVGLinearGradientElement::collectGradientAttributes(LinearGradientAttributes& attributes)
{
    if (!renderer())
        return false;

    HashSet<Ref<SVGGradientElement>> processedGradients;
    Ref<SVGGradientElement> current { *this };

    setGradientAttributes(current.get(), attributes);
    processedGradients.add(current.copyRef());

    while (true) {
        // Respect xlink:href, take attributes from referenced element
        auto target = SVGURIReference::targetElementFromIRIString(current->href(), treeScope());
        if (is<SVGGradientElement>(target.element)) {
            current = downcast<SVGGradientElement>(*target.element);

            // Cycle detection
            if (processedGradients.contains(current))
                return true;

            if (!current->renderer())
                return false;

            setGradientAttributes(current.get(), attributes, current->hasTagName(SVGNames::linearGradientTag));
            processedGradients.add(current.copyRef());
        } else
            return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGLinearGradientElement::selfHasRelativeLengths() const
{
    return x1().isRelative()
        || y1().isRelative()
        || x2().isRelative()
        || y2().isRelative();
}

}
