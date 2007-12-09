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

#ifndef SVGFEGaussianBlurElement_h
#define SVGFEGaussianBlurElement_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEGaussianBlur.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore
{

    class SVGFEGaussianBlurElement : public SVGFilterPrimitiveStandardAttributes
    {
    public:
        SVGFEGaussianBlurElement(const QualifiedName&, Document*);
        virtual ~SVGFEGaussianBlurElement();

        void setStdDeviation(float stdDeviationX, float stdDeviationY);

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual SVGFEGaussianBlur* filterEffect(SVGResourceFilter*) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEGaussianBlurElement, String, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEGaussianBlurElement, float, float, StdDeviationX, stdDeviationX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEGaussianBlurElement, float, float, StdDeviationY, stdDeviationY)

        mutable SVGFEGaussianBlur* m_filterEffect;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
