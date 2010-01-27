/*
 Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
 
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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEDisplacementMapElement.h"

#include "MappedAttribute.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_xChannelSelector(CHANNEL_A)
    , m_yChannelSelector(CHANNEL_A)
{
}

SVGFEDisplacementMapElement::~SVGFEDisplacementMapElement()
{
}

ChannelSelectorType SVGFEDisplacementMapElement::stringToChannel(const String& key)
{
    if (key == "R")
        return CHANNEL_R;
    else if (key == "G")
        return CHANNEL_G;
    else if (key == "B")
        return CHANNEL_B;
    else if (key == "A")
        return CHANNEL_A;

    return CHANNEL_UNKNOWN;
}

void SVGFEDisplacementMapElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::xChannelSelectorAttr)
        setXChannelSelectorBaseValue(stringToChannel(value));
    else if (attr->name() == SVGNames::yChannelSelectorAttr)
        setYChannelSelectorBaseValue(stringToChannel(value));
    else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::in2Attr)
        setIn2BaseValue(value);
    else if (attr->name() == SVGNames::scaleAttr)
        setScaleBaseValue(value.toFloat());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

void SVGFEDisplacementMapElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeXChannelSelector();
        synchronizeYChannelSelector();
        synchronizeIn1();
        synchronizeIn2();
        synchronizeScale();
        return;
    }

    if (attrName == SVGNames::xChannelSelectorAttr)
        synchronizeXChannelSelector();
    else if (attrName == SVGNames::yChannelSelectorAttr)
        synchronizeYChannelSelector();
    else if (attrName == SVGNames::inAttr)
        synchronizeIn1();
    else if (attrName == SVGNames::in2Attr)
        synchronizeIn2();
    else if (attrName == SVGNames::scaleAttr)
        synchronizeScale();
}

bool SVGFEDisplacementMapElement::build(SVGResourceFilter* filterResource)
{
    FilterEffect* input1 = filterResource->builder()->getEffectById(in1());
    FilterEffect* input2 = filterResource->builder()->getEffectById(in2());
    
    if (!input1 || !input2)
        return false;
        
    
    RefPtr<FilterEffect> effect = FEDisplacementMap::create(input1, input2, static_cast<ChannelSelectorType>(xChannelSelector()), 
                                        static_cast<ChannelSelectorType>(yChannelSelector()), scale());
    filterResource->addFilterEffect(this, effect.release());
    
    return true;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
