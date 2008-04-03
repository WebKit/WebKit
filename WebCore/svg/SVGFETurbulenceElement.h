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

#ifndef SVGFETurbulenceElement_h
#define SVGFETurbulenceElement_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFETurbulence.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore
{
    enum SVGStitchOptions {
        SVG_STITCHTYPE_UNKNOWN  = 0,
        SVG_STITCHTYPE_STITCH   = 1,
        SVG_STITCHTYPE_NOSTITCH = 2
    };

    class SVGFETurbulenceElement : public SVGFilterPrimitiveStandardAttributes
    {
    public:
        SVGFETurbulenceElement(const QualifiedName&, Document*);
        virtual ~SVGFETurbulenceElement();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual SVGFETurbulence* filterEffect(SVGResourceFilter*) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFETurbulenceElement, float, float, BaseFrequencyX, baseFrequencyX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFETurbulenceElement, float, float, BaseFrequencyY, baseFrequencyY)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFETurbulenceElement, long, long, NumOctaves, numOctaves)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFETurbulenceElement, float, float, Seed, seed)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFETurbulenceElement, int, int, StitchTiles, stitchTiles)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFETurbulenceElement, int, int, Type, type)

        mutable SVGFETurbulence* m_filterEffect;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
