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

#ifndef KSVG_SVGFEOffsetElementImpl_H
#define KSVG_SVGFEOffsetElementImpl_H

#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedStringImpl;
    class SVGAnimatedNumberImpl;

    class SVGFEOffsetElementImpl : public SVGFilterPrimitiveStandardAttributesImpl
    {
    public:
        SVGFEOffsetElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFEOffsetElementImpl();

        // 'SVGFEOffsetElement' functions
        SVGAnimatedStringImpl *in1() const;
        SVGAnimatedNumberImpl *dx() const;
        SVGAnimatedNumberImpl *dy() const;

        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        virtual KCanvasFEOffset *filterEffect() const;

    private:
        mutable RefPtr<SVGAnimatedStringImpl> m_in1;
        mutable RefPtr<SVGAnimatedNumberImpl> m_dx;
        mutable RefPtr<SVGAnimatedNumberImpl> m_dy;
        mutable KCanvasFEOffset *m_filterEffect;
    };
};

#endif

// vim:ts=4:noet
