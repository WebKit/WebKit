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
#ifdef SVG_SUPPORT

#include "KCanvasResources.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"

namespace WebCore
{
    class Document;
    class SVGAngle;
    class SVGAngle;
    class SVGLength;
    class SVGMarkerElement : public SVGStyledElement,
                                 public SVGLangSpace,
                                 public SVGExternalResourcesRequired,
                                 public SVGFitToViewBox
    {
    public:
        SVGMarkerElement(const QualifiedName&, Document*);
        virtual ~SVGMarkerElement();

        // 'SVGMarkerElement' functions
        void setOrientToAuto();
        void setOrientToAngle(SVGAngle *angle);

        virtual void parseMappedAttribute(MappedAttribute *attr);
    
        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject *createRenderer(RenderArena *arena, RenderStyle *style);
        virtual KCanvasMarker *canvasResource();

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired) 
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGFitToViewBox, FloatRect, ViewBox, viewBox)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGFitToViewBox, SVGPreserveAspectRatio*, PreserveAspectRatio, preserveAspectRatio)

        ANIMATED_PROPERTY_DECLARATIONS(SVGMarkerElement, SVGLength*, RefPtr<SVGLength>, RefX, refX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMarkerElement, SVGLength*, RefPtr<SVGLength>, RefY, refY)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMarkerElement, SVGLength*, RefPtr<SVGLength>, MarkerWidth, markerWidth)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMarkerElement, SVGLength*, RefPtr<SVGLength>, MarkerHeight, markerHeight)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMarkerElement, int, int, MarkerUnits, markerUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMarkerElement, int, int, OrientType, orientType)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMarkerElement, SVGAngle*, RefPtr<SVGAngle>, OrientAngle, orientAngle)

        KCanvasMarker *m_marker;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
