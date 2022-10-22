/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "SVGFEComponentTransferElement.h"

#include "ElementIterator.h"
#include "ElementName.h"
#include "FEComponentTransfer.h"
#include "SVGComponentTransferFunctionElement.h"
#include "SVGComponentTransferFunctionElementInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEComponentTransferElement);

inline SVGFEComponentTransferElement::SVGFEComponentTransferElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feComponentTransferTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEComponentTransferElement::m_in1>();
    });
}

Ref<SVGFEComponentTransferElement> SVGFEComponentTransferElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEComponentTransferElement(tagName, document));
}

void SVGFEComponentTransferElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEComponentTransferElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEComponentTransferElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    ComponentTransferFunctions functions;

    for (auto& child : childrenOfType<SVGComponentTransferFunctionElement>(*this))
        functions[child.channel()] = child.transferFunction();

    return FEComponentTransfer::create(WTFMove(functions));
}

static bool isRelevantTransferFunctionElement(const Element& child)
{
    auto name = child.elementName();

    ASSERT(is<SVGComponentTransferFunctionElement>(child));

    for (auto laterSibling = child.nextElementSibling(); laterSibling; laterSibling = laterSibling->nextElementSibling()) {
        if (laterSibling->elementName() == name)
            return false;
    }

    return true;
}

bool SVGFEComponentTransferElement::setFilterEffectAttributeFromChild(FilterEffect& effect, const Element& childElement, const QualifiedName& attrName)
{
    ASSERT(isRelevantTransferFunctionElement(childElement));

    if (!is<SVGComponentTransferFunctionElement>(childElement)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    auto& feComponentTransfer = downcast<FEComponentTransfer>(effect);
    auto& child = downcast<SVGComponentTransferFunctionElement>(childElement);

    if (attrName == SVGNames::typeAttr)
        return feComponentTransfer.setType(child.channel(), child.type());

    if (attrName == SVGNames::slopeAttr)
        return feComponentTransfer.setSlope(child.channel(), child.slope());

    if (attrName == SVGNames::interceptAttr)
        return feComponentTransfer.setIntercept(child.channel(), child.intercept());

    if (attrName == SVGNames::amplitudeAttr)
        return feComponentTransfer.setAmplitude(child.channel(), child.amplitude());

    if (attrName == SVGNames::exponentAttr)
        return feComponentTransfer.setExponent(child.channel(), child.exponent());

    if (attrName == SVGNames::offsetAttr)
        return feComponentTransfer.setOffset(child.channel(), child.offset());

    if (attrName == SVGNames::tableValuesAttr)
        return feComponentTransfer.setTableValues(child.channel(), child.tableValues());

    return false;
}

void SVGFEComponentTransferElement::transferFunctionAttributeChanged(SVGComponentTransferFunctionElement& child, const QualifiedName& attrName)
{
    ASSERT(child.parentNode() == this);

    if (!isRelevantTransferFunctionElement(child))
        return;

    primitiveAttributeOnChildChanged(child, attrName);
}

} // namespace WebCore
