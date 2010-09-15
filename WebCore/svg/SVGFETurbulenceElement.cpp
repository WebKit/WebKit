/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFETurbulenceElement.h"

#include "Attribute.h"
#include "SVGParserUtilities.h"

namespace WebCore {

char SVGBaseFrequencyXIdentifier[] = "SVGBaseFrequencyX";
char SVGBaseFrequencyYIdentifier[] = "SVGBaseFrequencyY";

inline SVGFETurbulenceElement::SVGFETurbulenceElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_numOctaves(1)
    , m_stitchTiles(SVG_STITCHTYPE_NOSTITCH)
    , m_type(FETURBULENCE_TYPE_TURBULENCE)
{
}

PassRefPtr<SVGFETurbulenceElement> SVGFETurbulenceElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFETurbulenceElement(tagName, document));
}

void SVGFETurbulenceElement::parseMappedAttribute(Attribute* attr)
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
        else if (value == "noStitch")
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

void SVGFETurbulenceElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
    
    if (attrName == SVGNames::baseFrequencyAttr
        || attrName == SVGNames::numOctavesAttr
        || attrName == SVGNames::seedAttr
        || attrName == SVGNames::stitchTilesAttr
        || attrName == SVGNames::typeAttr)
        invalidate();
}

void SVGFETurbulenceElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeType();
        synchronizeStitchTiles();
        synchronizeBaseFrequencyX();
        synchronizeBaseFrequencyY();
        synchronizeSeed();
        synchronizeNumOctaves();
        return;
    }

    if (attrName == SVGNames::typeAttr)
        synchronizeType();
    else if (attrName == SVGNames::stitchTilesAttr)
        synchronizeStitchTiles();
    else if (attrName == SVGNames::baseFrequencyAttr) {
        synchronizeBaseFrequencyX();
        synchronizeBaseFrequencyY();
    } else if (attrName == SVGNames::seedAttr)
        synchronizeSeed();
    else if (attrName == SVGNames::numOctavesAttr)
        synchronizeNumOctaves();
}

PassRefPtr<FilterEffect> SVGFETurbulenceElement::build(SVGFilterBuilder*)
{
    if (baseFrequencyX() < 0 || baseFrequencyY() < 0)
        return 0;

    return FETurbulence::create(static_cast<TurbulanceType>(type()), baseFrequencyX(), 
                baseFrequencyY(), numOctaves(), seed(), stitchTiles() == SVG_STITCHTYPE_STITCH);
}

}

#endif // ENABLE(SVG)
