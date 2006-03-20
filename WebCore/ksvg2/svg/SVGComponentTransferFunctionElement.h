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

#ifndef KSVG_SVGComponentTransferFunctionElementImpl_H
#define KSVG_SVGComponentTransferFunctionElementImpl_H
#if SVG_SUPPORT

#include "SVGElement.h"
#include "KCanvasFilters.h"

namespace WebCore
{
    class SVGAnimatedNumber;
    class SVGAnimatedNumberList;
    class SVGAnimatedEnumeration;

    class SVGComponentTransferFunctionElement : public SVGElement
    {
    public:
        SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGComponentTransferFunctionElement();

        // 'SVGComponentTransferFunctionElement' functions
        SVGAnimatedEnumeration *type() const;
        SVGAnimatedNumberList *tableValues() const;
        SVGAnimatedNumber *slope() const;
        SVGAnimatedNumber *intercept() const;
        SVGAnimatedNumber *amplitude() const;
        SVGAnimatedNumber *exponent() const;
        SVGAnimatedNumber *offset() const;

        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute *attr);
        
        KCComponentTransferFunction transferFunction() const;

    private:
        mutable RefPtr<SVGAnimatedEnumeration> m_type;
        mutable RefPtr<SVGAnimatedNumberList> m_tableValues;
        mutable RefPtr<SVGAnimatedNumber> m_slope;
        mutable RefPtr<SVGAnimatedNumber> m_intercept;
        mutable RefPtr<SVGAnimatedNumber> m_amplitude;
        mutable RefPtr<SVGAnimatedNumber> m_exponent;
        mutable RefPtr<SVGAnimatedNumber> m_offset;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
