/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#include "SVGFETurbulenceElement.h"

#include "MappedAttribute.h"
#include "SVGParserUtilities.h"
#include "SVGResourceFilter.h"

namespace WebCore {

char SVGBaseFrequencyXIdentifier[] = "SVGBaseFrequencyX";
char SVGBaseFrequencyYIdentifier[] = "SVGBaseFrequencyY";

SVGFETurbulenceElement::SVGFETurbulenceElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_baseFrequencyX(this, SVGNames::baseFrequencyAttr)
    , m_baseFrequencyY(this, SVGNames::baseFrequencyAttr)
    , m_numOctaves(this, SVGNames::numOctavesAttr, 1)
    , m_seed(this, SVGNames::seedAttr)
    , m_stitchTiles(this, SVGNames::stitchTilesAttr, SVG_STITCHTYPE_NOSTITCH)
    , m_type(this, SVGNames::typeAttr, FETURBULENCE_TYPE_TURBULENCE)
{
}

SVGFETurbulenceElement::~SVGFETurbulenceElement()
{
}

void SVGFETurbulenceElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::typeAttr) {
        if (value == "fractalNoise")
            setTypeBaseValue(FETURBULENCE_TYPE_FRACTALNOISE);
        else if (value == "turbulence")
            setTypeBaseValue(FETURBULENCE_TYPE_TURBULENCE);
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
        setNumOctavesBaseValue(value.toUIntStrict());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

bool SVGFETurbulenceElement::build(SVGResourceFilter* filterResource)
{
    RefPtr<FilterEffect> effect = FETurbulence::create(static_cast<TurbulanceType>(type()), baseFrequencyX(), 
                                        baseFrequencyY(), numOctaves(), seed(), stitchTiles() == SVG_STITCHTYPE_STITCH);
    filterResource->addFilterEffect(this, effect.release());

    return true;
}

}

#endif // ENABLE(SVG)
