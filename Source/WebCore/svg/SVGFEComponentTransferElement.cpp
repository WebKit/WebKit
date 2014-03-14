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

#if ENABLE(FILTERS)
#include "SVGFEComponentTransferElement.h"

#include "Attr.h"
#include "ElementIterator.h"
#include "FilterEffect.h"
#include "SVGFEFuncAElement.h"
#include "SVGFEFuncBElement.h"
#include "SVGFEFuncGElement.h"
#include "SVGFEFuncRElement.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"

namespace WebCore {

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

PassRefPtr<SVGFEComponentTransferElement> SVGFEComponentTransferElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new SVGFEComponentTransferElement(tagName, document));
}

bool SVGFEComponentTransferElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        supportedAttributes.add(SVGNames::inAttr);
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEComponentTransferElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFEComponentTransferElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(in1());
    
    if (!input1)
        return nullptr;

    ComponentTransferFunction red;
    ComponentTransferFunction green;
    ComponentTransferFunction blue;
    ComponentTransferFunction alpha;

    for (auto& child : childrenOfType<SVGElement>(*this)) {
        if (isSVGFEFuncRElement(child))
            red = toSVGFEFuncRElement(child).transferFunction();
        else if (isSVGFEFuncGElement(child))
            green = toSVGFEFuncGElement(child).transferFunction();
        else if (isSVGFEFuncBElement(child))
            blue = toSVGFEFuncBElement(child).transferFunction();
        else if (isSVGFEFuncAElement(child))
            alpha = toSVGFEFuncAElement(child).transferFunction();
    }
    
    RefPtr<FilterEffect> effect = FEComponentTransfer::create(filter, red, green, blue, alpha);
    effect->inputEffects().append(input1);
    return effect.release();
}

}

#endif
