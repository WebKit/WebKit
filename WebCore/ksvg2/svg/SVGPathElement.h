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

#ifndef KSVG_SVGPathElementImpl_H
#define KSVG_SVGPathElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "svgpathparser.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGAnimatedPathData.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    class SVGPoint;
    class SVGPathSeg;
    class SVGPathSegArcAbs;
    class SVGPathSegArcRel;
    class SVGAnimatedNumber;
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
        SVGPathElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGPathElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        SVGAnimatedNumber *pathLength() const;
        double getTotalLength();
        SVGPoint *getPointAtLength(double distance);
        unsigned long getPathSegAtLength(double distance);

        SVGPathSegClosePath *createSVGPathSegClosePath();
        SVGPathSegMovetoAbs *createSVGPathSegMovetoAbs(double x, double y, const SVGStyledElement *context = 0);
        SVGPathSegMovetoRel *createSVGPathSegMovetoRel(double x, double y, const SVGStyledElement *context = 0);
        SVGPathSegLinetoAbs *createSVGPathSegLinetoAbs(double x, double y, const SVGStyledElement *context = 0);
        SVGPathSegLinetoRel *createSVGPathSegLinetoRel(double x, double y, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoCubicAbs *createSVGPathSegCurvetoCubicAbs(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoCubicRel *createSVGPathSegCurvetoCubicRel(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoQuadraticAbs *createSVGPathSegCurvetoQuadraticAbs(double x, double y, double x1, double y1, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoQuadraticRel *createSVGPathSegCurvetoQuadraticRel(double x, double y, double x1, double y1, const SVGStyledElement *context = 0);
        SVGPathSegArcAbs *createSVGPathSegArcAbs(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElement *context = 0);
        SVGPathSegArcRel *createSVGPathSegArcRel(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElement *context = 0);
        SVGPathSegLinetoHorizontalAbs *createSVGPathSegLinetoHorizontalAbs(double x, const SVGStyledElement *context = 0);
        SVGPathSegLinetoHorizontalRel *createSVGPathSegLinetoHorizontalRel(double x, const SVGStyledElement *context = 0);
        SVGPathSegLinetoVerticalAbs *createSVGPathSegLinetoVerticalAbs(double y, const SVGStyledElement *context = 0);
        SVGPathSegLinetoVerticalRel *createSVGPathSegLinetoVerticalRel(double y, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoCubicSmoothAbs *createSVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoCubicSmoothRel *createSVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoQuadraticSmoothAbs *createSVGPathSegCurvetoQuadraticSmoothAbs(double x, double y, const SVGStyledElement *context = 0);
        SVGPathSegCurvetoQuadraticSmoothRel *createSVGPathSegCurvetoQuadraticSmoothRel(double x, double y, const SVGStyledElement *context = 0);

        // Derived from: 'SVGAnimatedPathData'
        virtual SVGPathSegList *pathSegList() const;
        virtual SVGPathSegList *normalizedPathSegList() const;
        virtual SVGPathSegList *animatedPathSegList() const;
        virtual SVGPathSegList *animatedNormalizedPathSegList() const;

        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual bool rendererIsNeeded(RenderStyle *style) { return StyledElement::rendererIsNeeded(style); }
        virtual KCanvasPath* toPathData() const;

    private:
        mutable RefPtr<SVGPathSegList> m_pathSegList;
        mutable RefPtr<SVGAnimatedNumber> m_pathLength;

        virtual void svgMoveTo(double x1, double y1, bool closed, bool abs = true);
        virtual void svgLineTo(double x1, double y1, bool abs = true);
        virtual void svgLineToHorizontal(double x, bool abs = true);
        virtual void svgLineToVertical(double y, bool abs = true);
        virtual void svgCurveToCubic(double x1, double y1, double x2, double y2, double x, double y, bool abs = true);
        virtual void svgCurveToCubicSmooth(double x, double y, double x2, double y2, bool abs = true);
        virtual void svgCurveToQuadratic(double x, double y, double x1, double y1, bool abs = true);
        virtual void svgCurveToQuadraticSmooth(double x, double y, bool abs = true);
        virtual void svgArcTo(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, bool abs = true);
        virtual void svgClosePath();
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
