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

#include "SVGTestsImpl.h"
#include "svgpathparser.h"
#include "SVGLangSpaceImpl.h"
#include "SVGStyledTransformableElementImpl.h"
#include "SVGAnimatedPathDataImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace KSVG
{
    class SVGPointImpl;
    class SVGPathSegImpl;
    class SVGPathSegArcAbsImpl;
    class SVGPathSegArcRelImpl;
    class SVGAnimatedNumberImpl;
    class SVGPathSegClosePathImpl;
    class SVGPathSegLinetoAbsImpl;
    class SVGPathSegLinetoRelImpl;
    class SVGPathSegMovetoAbsImpl;
    class SVGPathSegMovetoRelImpl;
    class SVGPathSegCurvetoCubicAbsImpl;
    class SVGPathSegCurvetoCubicRelImpl;
    class SVGPathSegLinetoVerticalAbsImpl;
    class SVGPathSegLinetoVerticalRelImpl;
    class SVGPathSegLinetoHorizontalAbsImpl;
    class SVGPathSegLinetoHorizontalRelImpl;
    class SVGPathSegCurvetoQuadraticAbsImpl;
    class SVGPathSegCurvetoQuadraticRelImpl;
    class SVGPathSegCurvetoCubicSmoothAbsImpl;
    class SVGPathSegCurvetoCubicSmoothRelImpl;
    class SVGPathSegCurvetoQuadraticSmoothAbsImpl;
    class SVGPathSegCurvetoQuadraticSmoothRelImpl;
    class SVGPathElementImpl : public SVGStyledTransformableElementImpl,
                               public SVGTestsImpl,
                               public SVGLangSpaceImpl,
                               public SVGExternalResourcesRequiredImpl,
                               public SVGAnimatedPathDataImpl,
                               public SVGPathParser
    {
    public:
        SVGPathElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGPathElementImpl();
        
        virtual bool isValid() const { return SVGTestsImpl::isValid(); }

        SVGAnimatedNumberImpl *pathLength() const;
        double getTotalLength();
        SVGPointImpl *getPointAtLength(double distance);
        unsigned long getPathSegAtLength(double distance);

        SVGPathSegClosePathImpl *createSVGPathSegClosePath();
        SVGPathSegMovetoAbsImpl *createSVGPathSegMovetoAbs(double x, double y, const SVGStyledElementImpl *context = 0);
        SVGPathSegMovetoRelImpl *createSVGPathSegMovetoRel(double x, double y, const SVGStyledElementImpl *context = 0);
        SVGPathSegLinetoAbsImpl *createSVGPathSegLinetoAbs(double x, double y, const SVGStyledElementImpl *context = 0);
        SVGPathSegLinetoRelImpl *createSVGPathSegLinetoRel(double x, double y, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoCubicAbsImpl *createSVGPathSegCurvetoCubicAbs(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoCubicRelImpl *createSVGPathSegCurvetoCubicRel(double x, double y, double x1, double y1, double x2, double y2, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoQuadraticAbsImpl *createSVGPathSegCurvetoQuadraticAbs(double x, double y, double x1, double y1, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoQuadraticRelImpl *createSVGPathSegCurvetoQuadraticRel(double x, double y, double x1, double y1, const SVGStyledElementImpl *context = 0);
        SVGPathSegArcAbsImpl *createSVGPathSegArcAbs(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElementImpl *context = 0);
        SVGPathSegArcRelImpl *createSVGPathSegArcRel(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, const SVGStyledElementImpl *context = 0);
        SVGPathSegLinetoHorizontalAbsImpl *createSVGPathSegLinetoHorizontalAbs(double x, const SVGStyledElementImpl *context = 0);
        SVGPathSegLinetoHorizontalRelImpl *createSVGPathSegLinetoHorizontalRel(double x, const SVGStyledElementImpl *context = 0);
        SVGPathSegLinetoVerticalAbsImpl *createSVGPathSegLinetoVerticalAbs(double y, const SVGStyledElementImpl *context = 0);
        SVGPathSegLinetoVerticalRelImpl *createSVGPathSegLinetoVerticalRel(double y, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoCubicSmoothAbsImpl *createSVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoCubicSmoothRelImpl *createSVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoQuadraticSmoothAbsImpl *createSVGPathSegCurvetoQuadraticSmoothAbs(double x, double y, const SVGStyledElementImpl *context = 0);
        SVGPathSegCurvetoQuadraticSmoothRelImpl *createSVGPathSegCurvetoQuadraticSmoothRel(double x, double y, const SVGStyledElementImpl *context = 0);

        // Derived from: 'SVGAnimatedPathData'
        virtual SVGPathSegListImpl *pathSegList() const;
        virtual SVGPathSegListImpl *normalizedPathSegList() const;
        virtual SVGPathSegListImpl *animatedPathSegList() const;
        virtual SVGPathSegListImpl *animatedNormalizedPathSegList() const;

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        virtual bool rendererIsNeeded(khtml::RenderStyle *) { return true; }
        virtual KCanvasPath* toPathData() const;

    private:
        mutable RefPtr<SVGPathSegListImpl> m_pathSegList;
        mutable RefPtr<SVGAnimatedNumberImpl> m_pathLength;

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
