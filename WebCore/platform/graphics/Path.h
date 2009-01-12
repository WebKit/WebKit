/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 *               2006 Rob Buis <buis@kde.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include <algorithm>

#if PLATFORM(CG)
typedef struct CGPath PlatformPath;
#elif PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QPainterPath;
QT_END_NAMESPACE
typedef QPainterPath PlatformPath;
#elif PLATFORM(WX) && USE(WXGC)
class wxGraphicsPath;
typedef wxGraphicsPath PlatformPath;
#elif PLATFORM(CAIRO)
namespace WebCore {
    struct CairoPath;
}
typedef WebCore::CairoPath PlatformPath;
#elif PLATFORM(SKIA)
class SkPath;
typedef SkPath PlatformPath;
#else
typedef void PlatformPath;
#endif

namespace WebCore {

    class FloatPoint;
    class FloatRect;
    class FloatSize;
    class GraphicsContext;
    class String;
    class StrokeStyleApplier;
    class TransformationMatrix;

    enum WindRule {
        RULE_NONZERO = 0,
        RULE_EVENODD = 1
    };

    enum PathElementType {
        PathElementMoveToPoint,
        PathElementAddLineToPoint,
        PathElementAddQuadCurveToPoint,
        PathElementAddCurveToPoint,
        PathElementCloseSubpath
    };

    struct PathElement {
        PathElementType type;
        FloatPoint* points;
    };

    typedef void (*PathApplierFunction)(void* info, const PathElement*);

    class Path {
    public:
        Path();
        ~Path();

        Path(const Path&);
        Path& operator=(const Path&);

        void swap(Path& other) { std::swap(m_path, other.m_path); }

        bool contains(const FloatPoint&, WindRule rule = RULE_NONZERO) const;
        bool strokeContains(StrokeStyleApplier*, const FloatPoint&) const;
        FloatRect boundingRect() const;
        FloatRect strokeBoundingRect(StrokeStyleApplier* = 0);
        
        float length();
        FloatPoint pointAtLength(float length, bool& ok);
        float normalAngleAtLength(float length, bool& ok);

        void clear();
        bool isEmpty() const;

        void moveTo(const FloatPoint&);
        void addLineTo(const FloatPoint&);
        void addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint);
        void addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint);
        void addArcTo(const FloatPoint&, const FloatPoint&, float radius);
        void closeSubpath();

        void addArc(const FloatPoint&, float radius, float startAngle, float endAngle, bool anticlockwise);
        void addRect(const FloatRect&);
        void addEllipse(const FloatRect&);

        void translate(const FloatSize&);

        String debugString() const;

        PlatformPath* platformPath() const { return m_path; }

        static Path createRoundedRectangle(const FloatRect&, const FloatSize& roundingRadii);
        static Path createRoundedRectangle(const FloatRect&, const FloatSize& topLeftRadius, const FloatSize& topRightRadius, const FloatSize& bottomLeftRadius, const FloatSize& bottomRightRadius);
        static Path createRectangle(const FloatRect&);
        static Path createEllipse(const FloatPoint& center, float rx, float ry);
        static Path createCircle(const FloatPoint& center, float r);
        static Path createLine(const FloatPoint&, const FloatPoint&);

        void apply(void* info, PathApplierFunction) const;
        void transform(const TransformationMatrix&);

    private:
        PlatformPath* m_path;
    };

}

#endif
