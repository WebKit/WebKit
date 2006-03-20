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
#if SVG_SUPPORT

#include "SVGLangSpace.h"
#include "SVGFitToViewBox.h"
#include "SVGStyledElement.h"
#include "SVGExternalResourcesRequired.h"

class KCanvasMarker;

namespace WebCore
{
    class Document;
};

namespace WebCore
{
    class SVGAngle;
    class SVGAnimatedAngle;
    class SVGAnimatedLength;
    class SVGAnimatedEnumeration;
    class SVGMarkerElement : public SVGStyledElement,
                                 public SVGLangSpace,
                                 public SVGExternalResourcesRequired,
                                 public SVGFitToViewBox
    {
    public:
        SVGMarkerElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGMarkerElement();

        // 'SVGMarkerElement' functions
        SVGAnimatedLength *refX() const;
        SVGAnimatedLength *refY() const;
        SVGAnimatedEnumeration *markerUnits() const;
        SVGAnimatedLength *markerWidth() const;
        SVGAnimatedLength *markerHeight() const;
        SVGAnimatedEnumeration *orientType() const;
        SVGAnimatedAngle *orientAngle() const;

        void setOrientToAuto();
        void setOrientToAngle(SVGAngle *angle);

        virtual void parseMappedAttribute(MappedAttribute *attr);
    
        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);
        virtual KCanvasMarker *canvasResource();

    private:
        mutable RefPtr<SVGAnimatedLength> m_refX;
        mutable RefPtr<SVGAnimatedLength> m_refY;
        mutable RefPtr<SVGAnimatedLength> m_markerWidth;
        mutable RefPtr<SVGAnimatedLength> m_markerHeight;
        mutable RefPtr<SVGAnimatedEnumeration> m_markerUnits;
        mutable RefPtr<SVGAnimatedEnumeration> m_orientType;
        mutable RefPtr<SVGAnimatedAngle> m_orientAngle;
        KCanvasMarker *m_marker;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
