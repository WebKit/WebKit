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

#ifndef KSVG_SVGFECompositeElementImpl_H
#define KSVG_SVGFECompositeElementImpl_H
#if SVG_SUPPORT

#include "SVGFilterPrimitiveStandardAttributes.h"
#include "KCanvasFilters.h"

namespace WebCore
{
    class SVGAnimatedString;
    class SVGAnimatedNumber;
    class SVGAnimatedEnumeration;

    class SVGFECompositeElement : public SVGFilterPrimitiveStandardAttributes
    {
    public:
        SVGFECompositeElement(const QualifiedName&, Document*);
        virtual ~SVGFECompositeElement();

        // 'SVGFECompositeElement' functions
        SVGAnimatedString *in1() const;
        SVGAnimatedString *in2() const;
        SVGAnimatedEnumeration *_operator() const;
        SVGAnimatedNumber *k1() const;
        SVGAnimatedNumber *k2() const;
        SVGAnimatedNumber *k3() const;
        SVGAnimatedNumber *k4() const;

        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual KCanvasFEComposite *filterEffect() const;

    private:
        mutable RefPtr<SVGAnimatedString> m_in1;
        mutable RefPtr<SVGAnimatedString> m_in2;
        mutable RefPtr<SVGAnimatedEnumeration> m_operator;
        mutable RefPtr<SVGAnimatedNumber> m_k1;
        mutable RefPtr<SVGAnimatedNumber> m_k2;
        mutable RefPtr<SVGAnimatedNumber> m_k3;
        mutable RefPtr<SVGAnimatedNumber> m_k4;
        mutable KCanvasFEComposite *m_filterEffect;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
