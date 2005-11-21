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

#ifndef KSVG_SVGFEMergeNodeElementImpl_H
#define KSVG_SVGFEMergeNodeElementImpl_H

#include <SVGElementImpl.h>

namespace KSVG
{
    class SVGAnimatedStringImpl;
    class SVGFEMergeNodeElementImpl : public SVGElementImpl
    {
    public:
        SVGFEMergeNodeElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFEMergeNodeElementImpl();

        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        // 'SVGFEMergeNodeElement' functions
        SVGAnimatedStringImpl *in1() const;

    private:
        mutable SVGAnimatedStringImpl *m_in1;
    };
};

#endif

// vim:ts=4:noet
