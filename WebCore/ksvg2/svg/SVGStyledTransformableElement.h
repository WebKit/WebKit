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
#if SVG_SUPPORT

#include "SVGStyledLocatableElement.h"
#include "SVGTransformable.h"

class QMatrix;

namespace WebCore
{
    class Node;
    class StringImpl;
    class Attribute;
};

namespace WebCore
{
    class SVGMatrix;
    class SVGTransformList;
    class SVGAnimatedTransformList;
    class SVGStyledTransformableElement : public SVGStyledLocatableElement, public SVGTransformable
    {
    public:
        SVGStyledTransformableElement(const QualifiedName&, Document*);
        virtual ~SVGStyledTransformableElement();
        
        virtual bool isStyledTransformable() const { return true; }

        // 'SVGTransformable' functions
        virtual SVGAnimatedTransformList *transform() const;
        virtual SVGMatrix *localMatrix() const;

        // Derived from: 'SVGLocatable'
        virtual SVGMatrix *getCTM() const;
        virtual SVGMatrix *getScreenCTM() const;
        virtual SVGElement *nearestViewportElement() const;
        virtual SVGElement *farthestViewportElement() const;

        virtual FloatRect getBBox() const;
        virtual SVGMatrix *getTransformToElement(SVGElement *element) const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

        void updateLocalTransform(SVGTransformList *localTransforms);
        
        virtual void attach();

    protected:
        mutable RefPtr<SVGMatrix> m_localMatrix;
        mutable RefPtr<SVGAnimatedTransformList> m_transform;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
