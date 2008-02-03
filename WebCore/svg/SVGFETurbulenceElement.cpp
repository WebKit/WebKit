/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFETurbulenceElement.h"

#include "SVGParserUtilities.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFETurbulenceElement::SVGFETurbulenceElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_baseFrequencyX(0.0f)
    , m_baseFrequencyY(0.0f)
    , m_numOctaves(1)
    , m_seed(0.0f)
    , m_stitchTiles(SVG_STITCHTYPE_NOSTITCH)
    , m_type(SVG_TURBULENCE_TYPE_TURBULENCE)
    , m_filterEffect(0)
{
}

SVGFETurbulenceElement::~SVGFETurbulenceElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, float, Number, number, BaseFrequencyX, baseFrequencyX, "baseFrequencyX", m_baseFrequencyX)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, float, Number, number, BaseFrequencyY, baseFrequencyY, "baseFrequencyY", m_baseFrequencyY)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, float, Number, number, Seed, seed, SVGNames::seedAttr, m_seed)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, long, Integer, integer, NumOctaves, numOctaves, SVGNames::numOctavesAttr, m_numOctaves)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, int, Enumeration, enumeration, StitchTiles, stitchTiles, SVGNames::stitchTilesAttr, m_stitchTiles)
ANIMATED_PROPERTY_DEFINITIONS(SVGFETurbulenceElement, int, Enumeration, enumeration, Type, type, SVGNames::typeAttr, m_type)

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
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setBaseFrequencyXBaseValue(x);
            setBaseFrequencyYBaseValue(y);
        }
    } else if (attr->name() == SVGNames::seedAttr)
        setSeedBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::numOctavesAttr)
        setNumOctavesBaseValue(value.deprecatedString().toUInt());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFETurbulence* SVGFETurbulenceElement::filterEffect(SVGResourceFilter* filter) const
{
    
    if (!m_filterEffect)
        m_filterEffect = new SVGFETurbulence(filter);
    
    m_filterEffect->setType((SVGTurbulanceType) type());
    m_filterEffect->setBaseFrequencyX(baseFrequencyX());
    m_filterEffect->setBaseFrequencyY(baseFrequencyY());
    m_filterEffect->setNumOctaves(numOctaves());
    m_filterEffect->setSeed(seed());
    m_filterEffect->setStitchTiles(stitchTiles() == SVG_STITCHTYPE_STITCH);

    setStandardAttributes(m_filterEffect); 
    return m_filterEffect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
