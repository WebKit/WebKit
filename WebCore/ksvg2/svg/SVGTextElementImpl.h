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

#ifndef KSVG_SVGTextElementImpl_H
#define KSVG_SVGTextElementImpl_H

#include "SVGTextPositioningElementImpl.h"
#include "SVGTransformableImpl.h"

namespace KSVG
{
    class SVGTextElementImpl : public SVGTextPositioningElementImpl,
                               public SVGTransformableImpl
    {
    public:
        SVGTextElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGTextElementImpl();

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

         // 'SVGTextElement' functions
        virtual SVGElementImpl *nearestViewportElement() const;
        virtual SVGElementImpl *farthestViewportElement() const;

        virtual SVGRectImpl *getBBox() const;
        virtual SVGMatrixImpl *getCTM() const;
        virtual SVGMatrixImpl *getScreenCTM() const;
        virtual SVGMatrixImpl *getTransformToElement(SVGElementImpl *element) const { return 0; }

        virtual bool rendererIsNeeded(khtml::RenderStyle *) { return true; }
        virtual khtml::RenderObject *createRenderer(RenderArena *arena, khtml::RenderStyle *style);
        virtual bool childShouldCreateRenderer(DOM::NodeImpl *) const;

        virtual SVGAnimatedTransformListImpl *transform() const { return 0; }
        virtual SVGMatrixImpl *localMatrix() const { return 0; }
        
        // FIXME: HACK
        virtual void updateLocalTransform(SVGTransformListImpl *localTransforms) { };
    };
};

#endif

// vim:ts=4:noet
