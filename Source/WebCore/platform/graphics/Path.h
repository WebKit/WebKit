/*
 * Copyright (C) 2003, 2006, 2009, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Path_h
#define Path_h

#include "FloatRect.h"
#include "WindRule.h"
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/Forward.h>

#if USE(CG)

#include <wtf/RetainPtr.h>
#include <CoreGraphics/CGPath.h>
typedef struct CGPath PlatformPath;

#elif USE(DIRECT2D)
#include "COMPtr.h"

interface ID2D1Geometry;
interface ID2D1GeometryGroup;
interface ID2D1PathGeometry;
interface ID2D1GeometrySink;

typedef ID2D1GeometryGroup PlatformPath;

#elif USE(CAIRO)

namespace WebCore {
class CairoPath;
}
typedef WebCore::CairoPath PlatformPath;

#elif USE(WINGDI)

namespace WebCore {
class PlatformPath;
}
typedef WebCore::PlatformPath PlatformPath;

#else

typedef void PlatformPath;

#endif

typedef PlatformPath* PlatformPathPtr;

namespace WTF {
class TextStream;
}

namespace WebCore {

    class AffineTransform;
    class FloatPoint;
    class FloatRoundedRect;
    class FloatSize;
    class GraphicsContext;
    class PathTraversalState;
    class RoundedRect;
    class StrokeStyleApplier;

    enum PathElementType {
        PathElementMoveToPoint, // The points member will contain 1 value.
        PathElementAddLineToPoint, // The points member will contain 1 value.
        PathElementAddQuadCurveToPoint, // The points member will contain 2 values.
        PathElementAddCurveToPoint, // The points member will contain 3 values.
        PathElementCloseSubpath // The points member will contain no values.
    };

    // The points in the structure are the same as those that would be used with the
    // add... method. For example, a line returns the endpoint, while a cubic returns
    // two tangent points and the endpoint.
    struct PathElement {
        PathElementType type;
        FloatPoint* points;
    };

    using PathApplierFunction = WTF::Function<void (const PathElement&)>;

    class Path {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        WEBCORE_EXPORT Path();
#if USE(CG)
        Path(RetainPtr<CGMutablePathRef>);
#endif
        WEBCORE_EXPORT ~Path();

        WEBCORE_EXPORT Path(const Path&);
        WEBCORE_EXPORT Path(Path&&);
        WEBCORE_EXPORT Path& operator=(const Path&);
        WEBCORE_EXPORT Path& operator=(Path&&);
        
        static Path polygonPathFromPoints(const Vector<FloatPoint>&);

        bool contains(const FloatPoint&, WindRule = WindRule::NonZero) const;
        bool strokeContains(StrokeStyleApplier*, const FloatPoint&) const;
        // fastBoundingRect() should equal or contain boundingRect(); boundingRect()
        // should perfectly bound the points within the path.
        FloatRect boundingRect() const;
        WEBCORE_EXPORT FloatRect fastBoundingRect() const;
        FloatRect strokeBoundingRect(StrokeStyleApplier* = 0) const;

        float length() const;
        PathTraversalState traversalStateAtLength(float length, bool& success) const;
        FloatPoint pointAtLength(float length, bool& success) const;
        float normalAngleAtLength(float length, bool& success) const;

        WEBCORE_EXPORT void clear();
        bool isNull() const { return !m_path; }
        bool isEmpty() const;
        // Gets the current point of the current path, which is conceptually the final point reached by the path so far.
        // Note the Path can be empty (isEmpty() == true) and still have a current point.
        bool hasCurrentPoint() const;
        FloatPoint currentPoint() const;

        WEBCORE_EXPORT void moveTo(const FloatPoint&);
        WEBCORE_EXPORT void addLineTo(const FloatPoint&);
        WEBCORE_EXPORT void addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint);
        WEBCORE_EXPORT void addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint);
        void addArcTo(const FloatPoint&, const FloatPoint&, float radius);
        WEBCORE_EXPORT void closeSubpath();

        void addArc(const FloatPoint&, float radius, float startAngle, float endAngle, bool anticlockwise);
        void addRect(const FloatRect&);
        void addEllipse(FloatPoint, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise);
        void addEllipse(const FloatRect&);

        enum RoundedRectStrategy {
            PreferNativeRoundedRect,
            PreferBezierRoundedRect
        };

        WEBCORE_EXPORT void addRoundedRect(const FloatRect&, const FloatSize& roundingRadii, RoundedRectStrategy = PreferNativeRoundedRect);
        WEBCORE_EXPORT void addRoundedRect(const FloatRoundedRect&, RoundedRectStrategy = PreferNativeRoundedRect);
        void addRoundedRect(const RoundedRect&);

        void addPath(const Path&, const AffineTransform&);

        void translate(const FloatSize&);

        // To keep Path() cheap, it does not allocate a PlatformPath immediately
        // meaning Path::platformPath() can return null.
#if USE(DIRECT2D)
        PlatformPathPtr platformPath() const { return m_path.get(); }
#else
        PlatformPathPtr platformPath() const { return m_path; }
#endif
        // ensurePlatformPath() will allocate a PlatformPath if it has not yet been and will never return null.
        WEBCORE_EXPORT PlatformPathPtr ensurePlatformPath();

        WEBCORE_EXPORT void apply(const PathApplierFunction&) const;
        void transform(const AffineTransform&);

        static float circleControlPoint()
        {
            // Approximation of control point positions on a bezier to simulate a quarter of a circle.
            // This is 1-kappa, where kappa = 4 * (sqrt(2) - 1) / 3
            return 0.447715;
        }

        void addBeziersForRoundedRect(const FloatRect&, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius);

#if USE(CG) || USE(DIRECT2D)
        void platformAddPathForRoundedRect(const FloatRect&, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius);
#endif

#if USE(DIRECT2D)
        ID2D1GeometrySink* activePath() const { return m_activePath.get(); }
        void appendGeometry(ID2D1Geometry*);
        void createGeometryWithFillMode(WindRule, COMPtr<ID2D1GeometryGroup>&) const;
        void drawDidComplete();

        HRESULT initializePathState();
        void openFigureAtCurrentPointIfNecessary();
#endif

#ifndef NDEBUG
        void dump() const;
#endif

    private:
#if USE(DIRECT2D)
        COMPtr<ID2D1GeometryGroup> m_path;
        COMPtr<ID2D1PathGeometry> m_activePathGeometry;
        COMPtr<ID2D1GeometrySink> m_activePath;
        bool m_doesHaveOpenFigure { false };
#else
        PlatformPathPtr m_path { nullptr };
#endif
    };

WTF::TextStream& operator<<(WTF::TextStream&, const Path&);

}

#endif
