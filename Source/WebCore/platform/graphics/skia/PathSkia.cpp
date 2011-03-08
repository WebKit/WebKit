// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "Path.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "ImageBuffer.h"
#include "StrokeStyleApplier.h"

#include "SkPath.h"
#include "SkRegion.h"
#include "SkiaUtils.h"

#include <wtf/MathExtras.h>

namespace WebCore {

Path::Path()
{
    m_path = new SkPath;
}

Path::Path(const Path& other)
{
    m_path = new SkPath(*other.m_path);
}

Path::~Path()
{
    delete m_path;
}

Path& Path::operator=(const Path& other)
{
    *m_path = *other.m_path;
    return *this;
}

bool Path::isEmpty() const
{
    return m_path->isEmpty();
}

bool Path::hasCurrentPoint() const
{
    return m_path->getPoints(NULL, 0) != 0;
}

FloatPoint Path::currentPoint() const 
{
    // FIXME: return current point of subpath.
    float quietNaN = std::numeric_limits<float>::quiet_NaN();
    return FloatPoint(quietNaN, quietNaN);
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    return SkPathContainsPoint(m_path, point,
      rule == RULE_NONZERO ? SkPath::kWinding_FillType : SkPath::kEvenOdd_FillType);
}

void Path::translate(const FloatSize& size)
{
    m_path->offset(WebCoreFloatToSkScalar(size.width()), WebCoreFloatToSkScalar(size.height()));
}

FloatRect Path::boundingRect() const
{
    return m_path->getBounds();
}

void Path::moveTo(const FloatPoint& point)
{
    m_path->moveTo(point);
}

void Path::addLineTo(const FloatPoint& point)
{
    m_path->lineTo(point);
}

void Path::addQuadCurveTo(const FloatPoint& cp, const FloatPoint& ep)
{
    m_path->quadTo(cp, ep);
}

void Path::addBezierCurveTo(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& ep)
{
    m_path->cubicTo(p1, p2, ep);
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    m_path->arcTo(p1, p2, WebCoreFloatToSkScalar(radius));
}

void Path::closeSubpath()
{
    m_path->close();
}

void Path::addArc(const FloatPoint& p, float r, float sa, float ea, bool anticlockwise) {
    SkScalar cx = WebCoreFloatToSkScalar(p.x());
    SkScalar cy = WebCoreFloatToSkScalar(p.y());
    SkScalar radius = WebCoreFloatToSkScalar(r);
    SkScalar s360 = SkIntToScalar(360);

    SkRect oval;
    oval.set(cx - radius, cy - radius, cx + radius, cy + radius);

    float sweep = ea - sa;
    SkScalar startDegrees = WebCoreFloatToSkScalar(sa * 180 / piFloat);
    SkScalar sweepDegrees = WebCoreFloatToSkScalar(sweep * 180 / piFloat);
    // Check for a circle.
    if (sweepDegrees >= s360 || sweepDegrees <= -s360) {
        // Move to the start position (0 sweep means we add a single point).
        m_path->arcTo(oval, startDegrees, 0, false);
        // Draw the circle.
        m_path->addOval(oval, anticlockwise ?
            SkPath::kCCW_Direction : SkPath::kCW_Direction);
        // Force a moveTo the end position.
        m_path->arcTo(oval, startDegrees + sweepDegrees, 0, true);
    } else {
        // Counterclockwise arcs should be drawn with negative sweeps, while
        // clockwise arcs should be drawn with positive sweeps. Check to see
        // if the situation is reversed and correct it by adding or subtracting
        // a full circle
        if (anticlockwise && sweepDegrees > 0) {
            sweepDegrees -= s360;
        } else if (!anticlockwise && sweepDegrees < 0) {
            sweepDegrees += s360;
        }

        m_path->arcTo(oval, startDegrees, sweepDegrees, false);
    }
}

void Path::addRect(const FloatRect& rect)
{
    m_path->addRect(rect);
}

void Path::addEllipse(const FloatRect& rect)
{
    m_path->addOval(rect);
}

void Path::clear()
{
    m_path->reset();
}

static FloatPoint* convertPathPoints(FloatPoint dst[], const SkPoint src[], int count)
{
    for (int i = 0; i < count; i++) {
        dst[i].setX(SkScalarToFloat(src[i].fX));
        dst[i].setY(SkScalarToFloat(src[i].fY));
    }
    return dst;
}

void Path::apply(void* info, PathApplierFunction function) const
{
    SkPath::Iter iter(*m_path, false);
    SkPoint pts[4];
    PathElement pathElement;
    FloatPoint pathPoints[3];

    for (;;) {
        switch (iter.next(pts)) {
        case SkPath::kMove_Verb:
            pathElement.type = PathElementMoveToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[0], 1);
            break;
        case SkPath::kLine_Verb:
            pathElement.type = PathElementAddLineToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[1], 1);
            break;
        case SkPath::kQuad_Verb:
            pathElement.type = PathElementAddQuadCurveToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[1], 2);
            break;
        case SkPath::kCubic_Verb:
            pathElement.type = PathElementAddCurveToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[1], 3);
            break;
        case SkPath::kClose_Verb:
            pathElement.type = PathElementCloseSubpath;
            pathElement.points = convertPathPoints(pathPoints, 0, 0);
            break;
        case SkPath::kDone_Verb:
            return;
        }
        function(info, &pathElement);
    }
}

void Path::transform(const AffineTransform& xform)
{
    m_path->transform(xform);
}

FloatRect Path::strokeBoundingRect(StrokeStyleApplier* applier) const
{
    GraphicsContext* scratch = scratchContext();
    scratch->save();

    if (applier)
        applier->strokeStyle(scratch);

    SkPaint paint;
    scratch->platformContext()->setupPaintForStroking(&paint, 0, 0);
    SkPath boundingPath;
    paint.getFillPath(*platformPath(), &boundingPath);

    FloatRect r = boundingPath.getBounds();
    scratch->restore();
    return r;
}

bool Path::strokeContains(StrokeStyleApplier* applier, const FloatPoint& point) const
{
    ASSERT(applier);
    GraphicsContext* scratch = scratchContext();
    scratch->save();

    applier->strokeStyle(scratch);

    SkPaint paint;
    scratch->platformContext()->setupPaintForStroking(&paint, 0, 0);
    SkPath strokePath;
    paint.getFillPath(*platformPath(), &strokePath);
    bool contains = SkPathContainsPoint(&strokePath, point,
                                        SkPath::kWinding_FillType);

    scratch->restore();
    return contains;
}
} // namespace WebCore
