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

#ifndef KSVG_SVGMarkerElementImpl_H
#define KSVG_SVGMarkerElementImpl_H

#include "SVGLangSpaceImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

class KCanvasMarker;

namespace KDOM
{
    class DocumentImpl;
};

namespace KSVG
{
    class SVGAngleImpl;
    class SVGAnimatedAngleImpl;
    class SVGAnimatedLengthImpl;
    class SVGAnimatedEnumerationImpl;
    class SVGMarkerElementImpl : public SVGStyledElementImpl,
                                 public SVGLangSpaceImpl,
                                 public SVGExternalResourcesRequiredImpl,
                                 public SVGFitToViewBoxImpl
    {
    public:
        SVGMarkerElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGMarkerElementImpl();

        // 'SVGMarkerElement' functions
        SVGAnimatedLengthImpl *refX() const;
        SVGAnimatedLengthImpl *refY() const;
        SVGAnimatedEnumerationImpl *markerUnits() const;
        SVGAnimatedLengthImpl *markerWidth() const;
        SVGAnimatedLengthImpl *markerHeight() const;
        SVGAnimatedEnumerationImpl *orientType() const;
        SVGAnimatedAngleImpl *orientAngle() const;

        void setOrientToAuto();
        void setOrientToAngle(SVGAngleImpl *angle);

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);
    
        virtual bool rendererIsNeeded(khtml::RenderStyle *) { return true; }
        virtual khtml::RenderObject *createRenderer(RenderArena *arena, khtml::RenderStyle *style);
        virtual KCanvasMarker *canvasResource();

    private:
        mutable RefPtr<SVGAnimatedLengthImpl> m_refX;
        mutable RefPtr<SVGAnimatedLengthImpl> m_refY;
        mutable RefPtr<SVGAnimatedLengthImpl> m_markerWidth;
        mutable RefPtr<SVGAnimatedLengthImpl> m_markerHeight;
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_markerUnits;
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_orientType;
        mutable RefPtr<SVGAnimatedAngleImpl> m_orientAngle;
        KCanvasMarker *m_marker;
    };
};

#endif

// vim:ts=4:noet
