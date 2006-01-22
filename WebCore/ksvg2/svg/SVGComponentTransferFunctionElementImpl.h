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

#include "SVGElementImpl.h"
#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedNumberImpl;
    class SVGAnimatedNumberListImpl;
    class SVGAnimatedEnumerationImpl;

    class SVGComponentTransferFunctionElementImpl : public SVGElementImpl
    {
    public:
        SVGComponentTransferFunctionElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGComponentTransferFunctionElementImpl();

        // 'SVGComponentTransferFunctionElement' functions
        SVGAnimatedEnumerationImpl *type() const;
        SVGAnimatedNumberListImpl *tableValues() const;
        SVGAnimatedNumberImpl *slope() const;
        SVGAnimatedNumberImpl *intercept() const;
        SVGAnimatedNumberImpl *amplitude() const;
        SVGAnimatedNumberImpl *exponent() const;
        SVGAnimatedNumberImpl *offset() const;

        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);
        
        KCComponentTransferFunction transferFunction() const;

    private:
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_type;
        mutable RefPtr<SVGAnimatedNumberListImpl> m_tableValues;
        mutable RefPtr<SVGAnimatedNumberImpl> m_slope;
        mutable RefPtr<SVGAnimatedNumberImpl> m_intercept;
        mutable RefPtr<SVGAnimatedNumberImpl> m_amplitude;
        mutable RefPtr<SVGAnimatedNumberImpl> m_exponent;
        mutable RefPtr<SVGAnimatedNumberImpl> m_offset;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
