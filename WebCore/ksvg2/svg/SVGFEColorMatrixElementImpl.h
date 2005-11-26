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

#ifndef KSVG_SVGFEColorMatrixElementImpl_H
#define KSVG_SVGFEColorMatrixElementImpl_H

#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedStringImpl;
    class SVGAnimatedNumberListImpl;
    class SVGAnimatedEnumerationImpl;

    class SVGFEColorMatrixElementImpl : public SVGFilterPrimitiveStandardAttributesImpl
    {
    public:
        SVGFEColorMatrixElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFEColorMatrixElementImpl();

        // 'SVGFEColorMatrixElement' functions
        SVGAnimatedStringImpl *in1() const;
        SVGAnimatedEnumerationImpl *type() const;
        SVGAnimatedNumberListImpl *values() const;

        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);
        
        virtual KCanvasFEColorMatrix *filterEffect() const;

    private:
        mutable SharedPtr<SVGAnimatedStringImpl> m_in1;
        mutable SharedPtr<SVGAnimatedEnumerationImpl> m_type;
        mutable SharedPtr<SVGAnimatedNumberListImpl> m_values;
        mutable KCanvasFEColorMatrix *m_filterEffect;
    };
};

#endif

// vim:ts=4:noet
