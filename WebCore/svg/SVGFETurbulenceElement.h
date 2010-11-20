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

#ifndef SVGFETurbulenceElement_h
#define SVGFETurbulenceElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "FETurbulence.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

enum SVGStitchOptions {
    SVG_STITCHTYPE_UNKNOWN  = 0,
    SVG_STITCHTYPE_STITCH   = 1,
    SVG_STITCHTYPE_NOSTITCH = 2
};

class SVGFETurbulenceElement : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFETurbulenceElement> create(const QualifiedName&, Document*);

private:
    SVGFETurbulenceElement(const QualifiedName&, Document*);

    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void synchronizeProperty(const QualifiedName&);
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*);

    static const AtomicString& baseFrequencyXIdentifier();
    static const AtomicString& baseFrequencyYIdentifier();

    DECLARE_ANIMATED_STATIC_PROPERTY_MULTIPLE_WRAPPERS_NEW(SVGFETurbulenceElement, SVGNames::baseFrequencyAttr, baseFrequencyXIdentifier(), float, BaseFrequencyX, baseFrequencyX)
    DECLARE_ANIMATED_STATIC_PROPERTY_MULTIPLE_WRAPPERS_NEW(SVGFETurbulenceElement, SVGNames::baseFrequencyAttr, baseFrequencyYIdentifier(), float, BaseFrequencyY, baseFrequencyY)
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFETurbulenceElement, SVGNames::numOctavesAttr, long, NumOctaves, numOctaves)
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFETurbulenceElement, SVGNames::seedAttr, float, Seed, seed)
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFETurbulenceElement, SVGNames::stitchTilesAttr, int, StitchTiles, stitchTiles)
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFETurbulenceElement, SVGNames::typeAttr, int, Type, type)
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
