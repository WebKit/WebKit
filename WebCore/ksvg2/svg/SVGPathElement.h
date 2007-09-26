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

#ifndef SVGPathElement_h
#define SVGPathElement_h

#if ENABLE(SVG)

#include "SVGAnimatedPathData.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGParserUtilities.h"
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
                           public SVGAnimatedPathData,
                           public SVGPathParser
    {
    public:
        SVGPathElement(const QualifiedName&, Document*);
        virtual ~SVGPathElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }
        float getTotalLength();
        FloatPoint getPointAtLength(float distance);
        unsigned long getPathSegAtLength(float distance);

        SVGPathSegClosePath* createSVGPathSegClosePath();
        SVGPathSegMovetoAbs* createSVGPathSegMovetoAbs(float x, float y);
        SVGPathSegMovetoRel* createSVGPathSegMovetoRel(float x, float y);
        SVGPathSegLinetoAbs* createSVGPathSegLinetoAbs(float x, float y);
        SVGPathSegLinetoRel* createSVGPathSegLinetoRel(float x, float y);
        SVGPathSegCurvetoCubicAbs* createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2);
        SVGPathSegCurvetoCubicRel* createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2);
        SVGPathSegCurvetoQuadraticAbs* createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
        SVGPathSegCurvetoQuadraticRel* createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
        SVGPathSegArcAbs* createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
        SVGPathSegArcRel* createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
        SVGPathSegLinetoHorizontalAbs* createSVGPathSegLinetoHorizontalAbs(float x);
        SVGPathSegLinetoHorizontalRel* createSVGPathSegLinetoHorizontalRel(float x);
        SVGPathSegLinetoVerticalAbs* createSVGPathSegLinetoVerticalAbs(float y);
        SVGPathSegLinetoVerticalRel* createSVGPathSegLinetoVerticalRel(float y);
        SVGPathSegCurvetoCubicSmoothAbs* createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2);
        SVGPathSegCurvetoCubicSmoothRel* createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2);
        SVGPathSegCurvetoQuadraticSmoothAbs* createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);
        SVGPathSegCurvetoQuadraticSmoothRel* createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);

        // Derived from: 'SVGAnimatedPathData'
        virtual SVGPathSegList* pathSegList() const;
        virtual SVGPathSegList* normalizedPathSegList() const;
        virtual SVGPathSegList* animatedPathSegList() const;
        virtual SVGPathSegList* animatedNormalizedPathSegList() const;

        virtual void parseMappedAttribute(MappedAttribute* attr);
        virtual void notifyAttributeChange() const;

        virtual Path toPathData() const;

        virtual bool supportsMarkers() const { return true; }

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        mutable RefPtr<SVGPathSegList> m_pathSegList;

        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
 
        ANIMATED_PROPERTY_DECLARATIONS(SVGPathElement, float, float, PathLength, pathLength)

        virtual void svgMoveTo(float x1, float y1, bool closed, bool abs = true);
        virtual void svgLineTo(float x1, float y1, bool abs = true);
        virtual void svgLineToHorizontal(float x, bool abs = true);
        virtual void svgLineToVertical(float y, bool abs = true);
        virtual void svgCurveToCubic(float x1, float y1, float x2, float y2, float x, float y, bool abs = true);
        virtual void svgCurveToCubicSmooth(float x, float y, float x2, float y2, bool abs = true);
        virtual void svgCurveToQuadratic(float x, float y, float x1, float y1, bool abs = true);
        virtual void svgCurveToQuadraticSmooth(float x, float y, bool abs = true);
        virtual void svgArcTo(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, bool abs = true);
        virtual void svgClosePath();
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
