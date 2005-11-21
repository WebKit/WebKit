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

#ifndef KSVG_SVGStyledTransformableElementImpl_H
#define KSVG_SVGStyledTransformableElementImpl_H

#include "SVGStyledLocatableElementImpl.h"
#include "SVGTransformableImpl.h"

class QMatrix;

namespace KDOM
{
    class NodeImpl;
    class DOMStringImpl;
    class AttributeImpl;
};

namespace KSVG
{
    class SVGMatrixImpl;
    class SVGTransformListImpl;
    class SVGAnimatedTransformListImpl;
    class SVGStyledTransformableElementImpl : public SVGStyledLocatableElementImpl, public SVGTransformableImpl
    {
    public:
        SVGStyledTransformableElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGStyledTransformableElementImpl();
        
        virtual bool isStyledTransformable() const { return true; }

        // 'SVGTransformable' functions
        virtual SVGAnimatedTransformListImpl *transform() const;
        virtual SVGMatrixImpl *localMatrix() const;

        // Derived from: 'SVGLocatable'
        virtual SVGMatrixImpl *getCTM() const;
        virtual SVGMatrixImpl *getScreenCTM() const;
        virtual SVGElementImpl *nearestViewportElement() const;
        virtual SVGElementImpl *farthestViewportElement() const;

        virtual SVGRectImpl *getBBox() const;
        virtual SVGMatrixImpl *getTransformToElement(SVGElementImpl *element) const;

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        void updateLocalTransform(SVGTransformListImpl *localTransforms);
        
        virtual void attach();

    protected:
        mutable SVGMatrixImpl *m_localMatrix;
        mutable SVGAnimatedTransformListImpl *m_transform;
    };
};

#endif

// vim:ts=4:noet
