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

#ifndef SVGFEOffsetElement_h
#define SVGFEOffsetElement_h
#ifdef SVG_SUPPORT

#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGFEOffset.h"

namespace WebCore
{

    class SVGFEOffsetElement : public SVGFilterPrimitiveStandardAttributes
    {
    public:
        SVGFEOffsetElement(const QualifiedName&, Document*);
        virtual ~SVGFEOffsetElement();

        // 'SVGFEOffsetElement' functions
        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual SVGFEOffset *filterEffect() const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEOffsetElement, String, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEOffsetElement, double, double, Dx, dx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEOffsetElement, double, double, Dy, dy)
        mutable SVGFEOffset *m_filterEffect;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
