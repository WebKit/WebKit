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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEDisplacementMapElement.h"

#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_xChannelSelector(SVG_CHANNEL_A)
    , m_yChannelSelector(SVG_CHANNEL_A)
    , m_scale(0.0f)
    , m_filterEffect(0)
{
}

SVGFEDisplacementMapElement::~SVGFEDisplacementMapElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEDisplacementMapElement, String, String, string, In1, in1, SVGNames::inAttr, m_in1)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDisplacementMapElement, String, String, string, In2, in2, SVGNames::in2Attr, m_in2)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDisplacementMapElement, int, Enumeration, enumeration, XChannelSelector, xChannelSelector, SVGNames::xChannelSelectorAttr, m_xChannelSelector)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDisplacementMapElement, int, Enumeration, enumeration, YChannelSelector, yChannelSelector, SVGNames::yChannelSelectorAttr, m_yChannelSelector)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEDisplacementMapElement, float, Number, number, Scale, scale, SVGNames::scaleAttr, m_scale)

SVGChannelSelectorType SVGFEDisplacementMapElement::stringToChannel(const String& key)
{
    if (key == "R")
        return SVG_CHANNEL_R;
    else if (key == "G")
        return SVG_CHANNEL_G;
    else if (key == "B")
        return SVG_CHANNEL_B;
    else if (key == "A")
        return SVG_CHANNEL_A;

    return SVG_CHANNEL_UNKNOWN;
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

SVGFEDisplacementMap* SVGFEDisplacementMapElement::filterEffect(SVGResourceFilter* filter) const
{
    if (!m_filterEffect)
        m_filterEffect = new SVGFEDisplacementMap(filter);

    m_filterEffect->setXChannelSelector((SVGChannelSelectorType) xChannelSelector());
    m_filterEffect->setYChannelSelector((SVGChannelSelectorType) yChannelSelector());
    m_filterEffect->setIn(in1());
    m_filterEffect->setIn2(in2());
    m_filterEffect->setScale(scale());

    setStandardAttributes(m_filterEffect);
    return m_filterEffect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
