/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGFETurbulenceElementImpl_H
#define KSVG_SVGFETurbulenceElementImpl_H

#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedIntegerImpl;
    class SVGAnimatedNumberImpl;
    class SVGAnimatedEnumerationImpl;

    class SVGFETurbulenceElementImpl : public SVGFilterPrimitiveStandardAttributesImpl
    {
    public:
        SVGFETurbulenceElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFETurbulenceElementImpl();

        // 'SVGFETurbulenceElement' functions
        SVGAnimatedNumberImpl *baseFrequencyX() const;
        SVGAnimatedNumberImpl *baseFrequencyY() const;
        SVGAnimatedIntegerImpl *numOctaves() const;
        SVGAnimatedNumberImpl *seed() const;
        SVGAnimatedEnumerationImpl *stitchTiles() const;
        SVGAnimatedEnumerationImpl *type() const;

        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        virtual KCanvasFETurbulence *filterEffect() const;

    private:
        mutable RefPtr<SVGAnimatedNumberImpl> m_baseFrequencyX;
        mutable RefPtr<SVGAnimatedNumberImpl> m_baseFrequencyY;
        mutable RefPtr<SVGAnimatedIntegerImpl> m_numOctaves;
        mutable RefPtr<SVGAnimatedNumberImpl> m_seed;
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_stitchTiles;
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_type;
        mutable KCanvasFETurbulence *m_filterEffect;
    };
};

#endif

// vim:ts=4:noet
