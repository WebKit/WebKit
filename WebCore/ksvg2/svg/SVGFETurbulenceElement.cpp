/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGFETurbulenceElement.h"

#include "KRenderingDevice.h"

namespace WebCore {

SVGFETurbulenceElement::SVGFETurbulenceElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_baseFrequencyX(0.0)
    , m_baseFrequencyY(0.0)
    , m_numOctaves(0)
    , m_seed(0.0)
    , m_stitchTiles(0)
    , m_type(0)
    , m_filterEffect(0)
{
}

SVGFETurbulenceElement::~SVGFETurbulenceElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, double, Number, number, BaseFrequencyX, baseFrequencyX, "baseFrequencyX", m_baseFrequencyX)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, double, Number, number, BaseFrequencyY, baseFrequencyY, "baseFrequencyY", m_baseFrequencyY)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, double, Number, number, Seed, seed, SVGNames::seedAttr.localName(), m_seed)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, long, Integer, integer, NumOctaves, numOctaves, SVGNames::numOctavesAttr.localName(), m_numOctaves)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, int, Enumeration, enumeration, StitchTiles, stitchTiles, SVGNames::stitchTilesAttr.localName(), m_stitchTiles)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, int, Enumeration, enumeration, Type, type, SVGNames::typeAttr.localName(), m_type)

void SVGFETurbulenceElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::typeAttr) {
        if (value == "fractalNoise")
            setTypeBaseValue(SVG_TURBULENCE_TYPE_FRACTALNOISE);
        else if (value == "turbulence")
            setTypeBaseValue(SVG_TURBULENCE_TYPE_TURBULENCE);
    } else if (attr->name() == SVGNames::stitchTilesAttr) {
        if (value == "stitch")
            setStitchTilesBaseValue(SVG_STITCHTYPE_STITCH);
        else if (value == "nostitch")
            setStitchTilesBaseValue(SVG_STITCHTYPE_NOSTITCH);
    } else if (attr->name() == SVGNames::baseFrequencyAttr) {
        Vector<String> numbers = value.split(' ');
        setBaseFrequencyXBaseValue(numbers[0].toDouble());
        if (numbers.size() == 1)
            setBaseFrequencyYBaseValue(numbers[0].toDouble());
        else
            setBaseFrequencyYBaseValue(numbers[1].toDouble());
    } else if (attr->name() == SVGNames::seedAttr)
        setSeedBaseValue(value.toDouble());
    else if (attr->name() == SVGNames::numOctavesAttr)
        setNumOctavesBaseValue(value.deprecatedString().toUInt());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFETurbulence* SVGFETurbulenceElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<SVGFETurbulence*>(renderingDevice()->createFilterEffect(FE_TURBULENCE));
    if (!m_filterEffect)
        return 0;
    
    m_filterEffect->setType((SVGTurbulanceType) type());
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setBaseFrequencyX(baseFrequencyX());
    m_filterEffect->setBaseFrequencyY(baseFrequencyY());
    m_filterEffect->setNumOctaves(numOctaves());
    m_filterEffect->setSeed(seed());
    m_filterEffect->setStitchTiles(stitchTiles() == SVG_STITCHTYPE_STITCH);
    return m_filterEffect;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

