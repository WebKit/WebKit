/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
#include "FilterEffect.h"
#include "SVGFEFuncAElement.h"
#include "SVGFEFuncBElement.h"
#include "SVGFEFuncGElement.h"
#include "SVGFEFuncRElement.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEComponentTransferElement);

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEComponentTransferElement, SVGNames::inAttr, In1, in1)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEComponentTransferElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEComponentTransferElement::SVGFEComponentTransferElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feComponentTransferTag));
    registerAnimatedPropertiesForSVGFEComponentTransferElement();
}

Ref<SVGFEComponentTransferElement> SVGFEComponentTransferElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEComponentTransferElement(tagName, document));
}

void SVGFEComponentTransferElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

RefPtr<FilterEffect> SVGFEComponentTransferElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());
    
    if (!input1)
        return nullptr;

    ComponentTransferFunction red;
    ComponentTransferFunction green;
    ComponentTransferFunction blue;
    ComponentTransferFunction alpha;

    for (auto& child : childrenOfType<SVGElement>(*this)) {
        if (is<SVGFEFuncRElement>(child))
            red = downcast<SVGFEFuncRElement>(child).transferFunction();
        else if (is<SVGFEFuncGElement>(child))
            green = downcast<SVGFEFuncGElement>(child).transferFunction();
        else if (is<SVGFEFuncBElement>(child))
            blue = downcast<SVGFEFuncBElement>(child).transferFunction();
        else if (is<SVGFEFuncAElement>(child))
            alpha = downcast<SVGFEFuncAElement>(child).transferFunction();
    }
    
    RefPtr<FilterEffect> effect = FEComponentTransfer::create(filter, red, green, blue, alpha);
    effect->inputEffects().append(input1);
    return effect;
}

}
