/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGPathElement_h
#define SVGPathElement_h

#if ENABLE(SVG)
#include "SVGAnimatedPathData.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"

namespace WebCore {

    class SVGPathSeg;
    class SVGPathSegArcAbs;
    class SVGPathSegArcRel;
    class SVGPathSegClosePath;
    class SVGPathSegLinetoAbs;
    class SVGPathSegLinetoRel;
    class SVGPathSegMovetoAbs;
    class SVGPathSegMovetoRel;
    class SVGPathSegCurvetoCubicAbs;
    class SVGPathSegCurvetoCubicRel;
    class SVGPathSegLinetoVerticalAbs;
    class SVGPathSegLinetoVerticalRel;
    class SVGPathSegLinetoHorizontalAbs;
    class SVGPathSegLinetoHorizontalRel;
    class SVGPathSegCurvetoQuadraticAbs;
    class SVGPathSegCurvetoQuadraticRel;
    class SVGPathSegCurvetoCubicSmoothAbs;
    class SVGPathSegCurvetoCubicSmoothRel;
    class SVGPathSegCurvetoQuadraticSmoothAbs;
    class SVGPathSegCurvetoQuadraticSmoothRel;
    class SVGPathElement : public SVGStyledTransformableElement,
                           public SVGTests,
                           public SVGLangSpace,
                           public SVGExternalResourcesRequired,
                           public SVGAnimatedPathData
    {
    public:
        SVGPathElement(const QualifiedName&, Document*);
        virtual ~SVGPathElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }
        float getTotalLength();
        FloatPoint getPointAtLength(float distance);
        unsigned long getPathSegAtLength(float distance);

        static SVGPathSegClosePath* createSVGPathSegClosePath();
        static SVGPathSegMovetoAbs* createSVGPathSegMovetoAbs(float x, float y);
        static SVGPathSegMovetoRel* createSVGPathSegMovetoRel(float x, float y);
        static SVGPathSegLinetoAbs* createSVGPathSegLinetoAbs(float x, float y);
        static SVGPathSegLinetoRel* createSVGPathSegLinetoRel(float x, float y);
        static SVGPathSegCurvetoCubicAbs* createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2);
        static SVGPathSegCurvetoCubicRel* createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2);
        static SVGPathSegCurvetoQuadraticAbs* createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
        static SVGPathSegCurvetoQuadraticRel* createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
        static SVGPathSegArcAbs* createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
        static SVGPathSegArcRel* createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
        static SVGPathSegLinetoHorizontalAbs* createSVGPathSegLinetoHorizontalAbs(float x);
        static SVGPathSegLinetoHorizontalRel* createSVGPathSegLinetoHorizontalRel(float x);
        static SVGPathSegLinetoVerticalAbs* createSVGPathSegLinetoVerticalAbs(float y);
        static SVGPathSegLinetoVerticalRel* createSVGPathSegLinetoVerticalRel(float y);
        static SVGPathSegCurvetoCubicSmoothAbs* createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2);
        static SVGPathSegCurvetoCubicSmoothRel* createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2);
        static SVGPathSegCurvetoQuadraticSmoothAbs* createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);
        static SVGPathSegCurvetoQuadraticSmoothRel* createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);

        // Derived from: 'SVGAnimatedPathData'
        virtual SVGPathSegList* pathSegList() const;
        virtual SVGPathSegList* normalizedPathSegList() const;
        virtual SVGPathSegList* animatedPathSegList() const;
        virtual SVGPathSegList* animatedNormalizedPathSegList() const;

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);

        virtual Path toPathData() const;

        virtual bool supportsMarkers() const { return true; }

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        mutable RefPtr<SVGPathSegList> m_pathSegList;

        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
 
        ANIMATED_PROPERTY_DECLARATIONS(SVGPathElement, float, float, PathLength, pathLength)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
