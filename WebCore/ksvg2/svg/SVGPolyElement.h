/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGPolyElement_h
#define SVGPolyElement_h

#if ENABLE(SVG)

#include "SVGAnimatedPoints.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGParserUtilities.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"

namespace WebCore
{
    class SVGPolyElement :  public SVGStyledTransformableElement,
                            public SVGTests,
                            public SVGLangSpace,
                            public SVGExternalResourcesRequired,
                            public SVGAnimatedPoints,
                            public SVGPolyParser
    {
    public:
        SVGPolyElement(const QualifiedName&, Document*);
        virtual ~SVGPolyElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        // Derived from: 'SVGAnimatedPoints'
        virtual SVGPointList* points() const;
        virtual SVGPointList* animatedPoints() const;

        virtual void parseMappedAttribute(MappedAttribute* attr); 
        virtual void notifyAttributeChange() const;

        virtual bool rendererIsNeeded(RenderStyle* style) { return StyledElement::rendererIsNeeded(style); }
        virtual bool supportsMarkers() const { return true; }

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        mutable bool m_ignoreAttributeChanges;
        mutable RefPtr<SVGPointList> m_points;

        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)

        virtual void svgPolyTo(double x1, double y1, int nr) const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
