/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009, 2010 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2010 Michael Lotz <mmlr@mlotz.ch>
 *
 * All rights reserved.
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

#include "config.h"
#include "Path.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "StrokeStyleApplier.h"
#include "TransformationMatrix.h"
#include <Bitmap.h>
#include <Shape.h>
#include <View.h>
#include <math.h>
#include <stdio.h>

namespace WebCore {

// A one pixel sized BBitmap for drawing into. Default high-color of BViews
// is black. So testing of any color channel for < 128 means the pixel was hit.
class HitTestBitmap {
public:
    HitTestBitmap()
        : m_bitmap(0)
        , m_view(0)
        , m_referenceCount(0)
    {
        // Do not create the bitmap here, since this object is initialized
        // statically, and BBitmaps need a running BApplication to work.
    }

    ~HitTestBitmap()
    {
    }

    void addReference()
    {
        m_referenceCount++;
    }

    void removeReference()
    {
        // The reference counting is needed to delete the BBitmap when
        // the last Path object is gone. The deletion needs to happen
        // here, and not in our destructor, because we are deleted on the
        // execution of global destructors, at which point the BApplication
        // is already gone, and deleting BBitmaps without a BApplication
        // invokes the debugger.
        m_referenceCount--;

        if (!m_referenceCount) {
            // m_bitmap being 0 simply means WebCore never performed any 
            // hit-tests on Paths.
            if (!m_bitmap)
                return;

            m_bitmap->Unlock();
            // Will also delete the BView attached to the bitmap:
            delete m_bitmap;
            m_bitmap = 0;
            m_view = 0;
        }
    }

    void init()
    {
        if (m_bitmap)
            return;

        m_bitmap = new BBitmap(BRect(0, 0, 0, 0), B_RGB32, true);
        // Keep the dummmy window locked at all times, so we don't need
        // to worry about it anymore.
        m_bitmap->Lock();
        // Add a BView which does any rendering.
        m_view = new BView(m_bitmap->Bounds(), "WebCore hi-test view", 0, 0);
        m_bitmap->AddChild(m_view);

        clearToWhite();
    }

    void clearToWhite()
    {
        memset(m_bitmap->Bits(), 255, m_bitmap->BitsLength());
    }

    void prepareHitTest(float x, float y)
    {
        clearToWhite();
        // The current pen location is used as the reference point for
        // where the shape is rendered in the view. Obviously Be thought this
        // was a neat idea for using BShapes as symbols, although it is
        // inconsistent with drawing other primitives. SetOrigin() should
        // be used instead, but this is cheaper:
        m_view->MovePenTo(-x, -y);
    }

    bool hitTest(BShape* shape, float x, float y, WindRule rule)
    {
        prepareHitTest(x, y);

        m_view->FillShape(shape);

        return hitTestPixel();
    }

    bool hitTest(BShape* shape, float x, float y, StrokeStyleApplier* applier)
    {
        prepareHitTest(x, y);

        GraphicsContext context(m_view);
        applier->strokeStyle(&context);
        m_view->StrokeShape(shape);

        return hitTestPixel();
    }

    bool hitTestPixel() const
    {
        // Make sure the app_server has rendered everything already.
        m_view->Sync();
        // Bitmap is white before drawing, anti-aliasing threshold is mid-grey,
        // then the pixel is considered within the black shape. Theoretically,
        // it would be enough to test one color channel, but since the Haiku
        // app_server renders all shapes with LCD sub-pixel anti-aliasing, the
        // color channels can in fact differ at edge pixels.
        const uint8* bits = reinterpret_cast<const uint8*>(m_bitmap->Bits());
        return (static_cast<uint16>(bits[0]) + bits[1] + bits[2]) / 3 < 128;
    }

private:
    BBitmap* m_bitmap;
    BView* m_view;
    int m_referenceCount;
};

// Reuse the same hit test bitmap for all paths. Initialization is lazy, and
// needs to be, since BBitmaps need a running BApplication. If WebCore ever
// runs each Document in it's own thread, we shall re-use the internal bitmap's
// lock to synchronize access. Since the pointer is likely only sitting above
// one document at a time, it seems unlikely to be a problem anyway.
static HitTestBitmap gHitTestBitmap;

// #pragma mark - Path

Path::Path()
    : m_path(new BShape())
{
    gHitTestBitmap.addReference();
}

Path::Path(const Path& other)
    : m_path(new BShape(*other.platformPath()))
{
    gHitTestBitmap.addReference();
}

Path::Path(Path&& other)
{
    m_path = other.m_path;
    other.m_path = nullptr;
    gHitTestBitmap.addReference();
}

Path::~Path()
{
    gHitTestBitmap.removeReference();
    delete m_path;
}

Path& Path::operator=(const Path& other)
{
    if (&other != this) {
        m_path->Clear();
        m_path->AddShape(other.platformPath());
    }

    return *this;
}

Path& Path::operator=(Path&& other)
{
    if (this == &other)
        return *this;
    if (m_path)
        delete m_path;
    m_path = other.m_path;
    other.m_path = nullptr;
    return *this;
}

FloatPoint Path::currentPointSlowCase() const
{
    return m_path->CurrentPosition();
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    gHitTestBitmap.init();
    return gHitTestBitmap.hitTest(m_path, point.x(), point.y(), rule);
}

bool Path::strokeContains(StrokeStyleApplier& applier, const FloatPoint& point) const
{
    gHitTestBitmap.init();
    return gHitTestBitmap.hitTest(m_path, point.x(), point.y(), &applier);
}

void Path::translate(const FloatSize& size)
{
    // BShapeIterator allows us to modify the path data "in place"
    class TranslateIterator : public BShapeIterator {
    public:
        TranslateIterator(const FloatSize& size)
            : m_size(size)
        {
        }
        virtual status_t IterateMoveTo(BPoint* point)
        {
            point->x += m_size.width();
            point->y += m_size.height();
            return B_OK;
        }

        virtual status_t IterateLineTo(int32 lineCount, BPoint* linePts)
        {
            while (lineCount--) {
                linePts->x += m_size.width();
                linePts->y += m_size.height();
                linePts++;
            }
            return B_OK;
        }

        virtual status_t IterateBezierTo(int32 bezierCount, BPoint* bezierPts)
        {
            while (bezierCount--) {
                bezierPts[0].x += m_size.width();
                bezierPts[0].y += m_size.height();
                bezierPts[1].x += m_size.width();
                bezierPts[1].y += m_size.height();
                bezierPts[2].x += m_size.width();
                bezierPts[2].y += m_size.height();
                bezierPts += 3;
            }
            return B_OK;
        }

        virtual status_t IterateArcTo(float& rx, float& ry,
        	float& angle, bool largeArc, bool counterClockWise, BPoint& point)
        {
            point.x += m_size.width();
            point.y += m_size.height();

            return B_OK;
        }

        virtual status_t IterateClose()
        {
            return B_OK;
        }

    private:
        const FloatSize& m_size;
    } translateIterator(size);

    translateIterator.Iterate(m_path);
}

FloatRect Path::boundingRectSlowCase() const
{
    return m_path->Bounds();
}

void Path::moveToSlowCase(const FloatPoint& p)
{
    m_path->MoveTo(p);
}

void Path::addLineToSlowCase(const FloatPoint& p)
{
    m_path->LineTo(p);
}

void Path::addQuadCurveToSlowCase(const FloatPoint& cp, const FloatPoint& p)
{
    BPoint control = cp;

    BPoint points[3];
    points[0] = control;
    points[0].x += (control.x - points[0].x) * (2.0 / 3.0);
    points[0].y += (control.y - points[0].y) * (2.0 / 3.0);

    points[1] = p;
    points[1].x += (control.x - points[1].x) * (2.0 / 3.0);
    points[1].y += (control.y - points[1].y) * (2.0 / 3.0);

    points[2] = p;
    m_path->BezierTo(points);
}

void Path::addBezierCurveToSlowCase(const FloatPoint& cp1, const FloatPoint& cp2, const FloatPoint& p)
{
    BPoint points[3];
    points[0] = cp1;
    points[1] = cp2;
    points[2] = p;
    m_path->BezierTo(points);
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    if (isEmpty())
        return;

    FloatPoint p0(m_path->CurrentPosition());

    if ((p1.x() == p0.x() && p1.y() == p0.y()) || (p1.x() == p2.x() && p1.y() == p2.y()) || radius == 0.f) {
        m_path->LineTo(p1);
        return;
    }

    FloatPoint p1p0((p0.x() - p1.x()), (p0.y() - p1.y()));
    FloatPoint p1p2((p2.x() - p1.x()), (p2.y() - p1.y()));
    float p1p0_length = sqrtf(p1p0.x() * p1p0.x() + p1p0.y() * p1p0.y());
    float p1p2_length = sqrtf(p1p2.x() * p1p2.x() + p1p2.y() * p1p2.y());

    double cos_phi = (p1p0.x() * p1p2.x() + p1p0.y() * p1p2.y()) / (p1p0_length * p1p2_length);
    // all points on a line logic
    if (cos_phi == -1) {
        m_path->LineTo(p1);
        return;
    }
    if (cos_phi == 1) {
        // add infinite far away point
        unsigned int max_length = 65535;
        double factor_max = max_length / p1p0_length;
        FloatPoint ep((p0.x() + factor_max * p1p0.x()), (p0.y() + factor_max * p1p0.y()));
        m_path->LineTo(ep);
        return;
    }

    float tangent = radius / tan(acos(cos_phi) / 2);
    float factor_p1p0 = tangent / p1p0_length;
    FloatPoint t_p1p0((p1.x() + factor_p1p0 * p1p0.x()), (p1.y() + factor_p1p0 * p1p0.y()));

    FloatPoint orth_p1p0(p1p0.y(), -p1p0.x());
    float orth_p1p0_length = sqrt(orth_p1p0.x() * orth_p1p0.x() + orth_p1p0.y() * orth_p1p0.y());
    float factor_ra = radius / orth_p1p0_length;

    // angle between orth_p1p0 and p1p2 to get the right vector orthographic to p1p0
    double cos_alpha = (orth_p1p0.x() * p1p2.x() + orth_p1p0.y() * p1p2.y()) / (orth_p1p0_length * p1p2_length);
    if (cos_alpha < 0.f)
        orth_p1p0 = FloatPoint(-orth_p1p0.x(), -orth_p1p0.y());

    FloatPoint p((t_p1p0.x() + factor_ra * orth_p1p0.x()), (t_p1p0.y() + factor_ra * orth_p1p0.y()));

    // calculate angles for addArc
    orth_p1p0 = FloatPoint(-orth_p1p0.x(), -orth_p1p0.y());
    float sa = acos(orth_p1p0.x() / orth_p1p0_length);
    if (orth_p1p0.y() < 0.f)
        sa = 2 * piDouble - sa;

    // anticlockwise logic
    bool anticlockwise = false;

    float factor_p1p2 = tangent / p1p2_length;
    FloatPoint t_p1p2((p1.x() + factor_p1p2 * p1p2.x()), (p1.y() + factor_p1p2 * p1p2.y()));
    FloatPoint orth_p1p2((t_p1p2.x() - p.x()), (t_p1p2.y() - p.y()));
    float orth_p1p2_length = sqrtf(orth_p1p2.x() * orth_p1p2.x() + orth_p1p2.y() * orth_p1p2.y());
    float ea = acos(orth_p1p2.x() / orth_p1p2_length);
    if (orth_p1p2.y() < 0)
        ea = 2 * piDouble - ea;
    if ((sa > ea) && ((sa - ea) < piDouble))
        anticlockwise = true;
    if ((sa < ea) && ((ea - sa) > piDouble))
        anticlockwise = true;

    m_path->LineTo(t_p1p0);

    addArc(p, radius, sa, ea, anticlockwise);
}

void Path::addPath(const Path&, const AffineTransform&)
{
    // FIXME: This should probably be very similar to Path::transform.
    notImplemented();
}

void Path::closeSubpath()
{
    m_path->Close();
}

void Path::addArcSlowCase(const FloatPoint& center, float radius,
       float startAngleRadiants, float endAngleRadiants, bool anticlockwise)
{
       // Compute start and end positions
       float startX = center.x() + radius * cos(startAngleRadiants);
      float startY = center.y() + radius * sin(startAngleRadiants);
      float endX   = center.x() + radius * cos(endAngleRadiants);
       float endY   = center.y() + radius * sin(endAngleRadiants);

       // Handle special case of ellipse (the code below isn't stable in that case it seems ?)
       if ((int)startX == (int)endX && (int)startY == (int)endY)
       {
               addEllipse(FloatRect(center.x() - radius, center.y() - radius,
                       radius * 2, radius * 2));
               return;
       }

       // Decide if we are drawing a "large" arc (more than PI rad)
       bool large = anticlockwise;
       float coverage = fmodf(endAngleRadiants - startAngleRadiants, 2 * M_PI);
       if (coverage < 0)
               coverage += 2 * M_PI;
       if (coverage >= M_PI)
               large = !anticlockwise;

       // Draw the radius or whatever line is needed to get to the start point
       // (or teleport there if there was no previous position)
       if (hasCurrentPoint())
               m_path->LineTo(BPoint(startX, startY));
       else
               m_path->MoveTo(BPoint(startX, startY));

       // And finally, draw the arc itself
       m_path->ArcTo(radius, radius, startAngleRadiants, large, anticlockwise,
               BPoint(endX, endY));
}


void Path::addRect(const FloatRect& r)
{
    m_path->MoveTo(BPoint(r.x(), r.y()));
    m_path->LineTo(BPoint(r.maxX(), r.y()));
    m_path->LineTo(BPoint(r.maxX(), r.maxY()));
    m_path->LineTo(BPoint(r.x(), r.maxY()));
    m_path->Close();
}


void Path::addEllipse(FloatPoint p, float radiusX, float radiusY, float rotation,
    float startAngle, float endAngle, bool anticlockwise) {
    notImplemented();
}


void Path::addEllipse(const FloatRect& r)
{
    BPoint points[3];
    const float radiusH = r.width() / 2;
    const float radiusV = r.height() / 2;
    const float middleH = r.x() + radiusH;
    const float middleV = r.y() + radiusV;
    const float kRadiusBezierScale = 0.552284;

    m_path->MoveTo(BPoint(middleH, r.y()));
    points[0].x = middleH + kRadiusBezierScale * radiusH;
    points[0].y = r.y();
    points[1].x = r.maxX();
    points[1].y = middleV - kRadiusBezierScale * radiusV;
    points[2].x = r.maxX();
    points[2].y = middleV;
    m_path->BezierTo(points);
    points[0].x = r.maxX();
    points[0].y = middleV + kRadiusBezierScale * radiusV;
    points[1].x = middleH + kRadiusBezierScale * radiusH;
    points[1].y = r.maxY();
    points[2].x = middleH;
    points[2].y = r.maxY();
    m_path->BezierTo(points);
    points[0].x = middleH - kRadiusBezierScale * radiusH;
    points[0].y = r.maxY();
    points[1].x = r.x();
    points[1].y = middleV + kRadiusBezierScale * radiusV;
    points[2].x = r.x();
    points[2].y = middleV;
    m_path->BezierTo(points);
    points[0].x = r.x();
    points[0].y = middleV - kRadiusBezierScale * radiusV;
    points[1].x = middleH - kRadiusBezierScale * radiusH;
    points[1].y = r.y();
    points[2].x = middleH;
    points[2].y = r.y();
    m_path->BezierTo(points);
    m_path->Close();
}

void Path::clear()
{
    m_path->Clear();
}


bool Path::isEmptySlowCase() const
{
    return !m_path->Bounds().IsValid();
}


void Path::applySlowCase(const PathApplierFunction& function) const
{
    class ApplyIterator : public BShapeIterator {
    public:
        ApplyIterator(const PathApplierFunction& function)
            : m_function(function)
        {
        }

        virtual status_t IterateMoveTo(BPoint* point)
        {
            PathElement pathElement;
            pathElement.type = PathElement::Type::MoveToPoint;
            pathElement.points[0] = point[0];
            m_function(pathElement);
            return B_OK;
        }

        virtual status_t IterateLineTo(int32 lineCount, BPoint* linePts)
        {
            PathElement pathElement;
            pathElement.type = PathElement::Type::AddLineToPoint;
            while (lineCount--) {
                pathElement.points[0] = linePts[0];
                m_function(pathElement);
                linePts++;
            }
            return B_OK;
        }

        virtual status_t IterateBezierTo(int32 bezierCount, BPoint* bezierPts)
        {
            PathElement pathElement;
            pathElement.type = PathElement::Type::AddCurveToPoint;
            while (bezierCount--) {
                pathElement.points[0] = bezierPts[0];
            	pathElement.points[1] = bezierPts[1];
                pathElement.points[2] = bezierPts[2];
                m_function(pathElement);
                bezierPts += 3;
            }
            return B_OK;
        }

        virtual status_t IterateArcTo(float& rx, float& ry,
            float& angle, bool largeArc, bool counterClockWise, BPoint& point)
        {
            // FIXME: This doesn't seem to be supported by WebCore.
            // Maybe we are supposed to convert arc into cubic curve
            // segments when adding them to a path?

            return B_OK;
        }

        virtual status_t IterateClose()
        {
            PathElement pathElement;
            pathElement.type = PathElement::Type::CloseSubpath;
            m_function(pathElement);
            return B_OK;
        }

    private:
        const PathApplierFunction& m_function;
    } applyIterator(function);

    applyIterator.Iterate(m_path);
}

void Path::transform(const AffineTransform& transform)
{
    // BShapeIterator allows us to modify the path data "in place"
    class TransformIterator : public BShapeIterator {
    public:
        TransformIterator(const AffineTransform& transform)
            : m_transform(transform)
        {
        }
        virtual status_t IterateMoveTo(BPoint* point)
        {
            *point = m_transform.mapPoint(*point);
            return B_OK;
        }

        virtual status_t IterateLineTo(int32 lineCount, BPoint* linePts)
        {
            while (lineCount--) {
                *linePts = m_transform.mapPoint(*linePts);
                linePts++;
            }
            return B_OK;
        }

        virtual status_t IterateBezierTo(int32 bezierCount, BPoint* bezierPts)
        {
            while (bezierCount--) {
                bezierPts[0] = m_transform.mapPoint(bezierPts[0]);
                bezierPts[1] = m_transform.mapPoint(bezierPts[1]);
                bezierPts[2] = m_transform.mapPoint(bezierPts[2]);
                bezierPts += 3;
            }
            return B_OK;
        }

        virtual status_t IterateArcTo(float& rx, float& ry,
        	float& angle, bool largeArc, bool counterClockWise, BPoint& point)
        {
            point = m_transform.mapPoint(point);
            rx *= m_transform.a();
            ry *= m_transform.d();
            // FIXME: rotate angle...

            return B_OK;
        }

        virtual status_t IterateClose()
        {
            return B_OK;
        }

    private:
        const AffineTransform& m_transform;
    } transformIterator(transform);

    transformIterator.Iterate(m_path);
}

FloatRect Path::strokeBoundingRect(StrokeStyleApplier* applier) const
{
    notImplemented();
    return m_path->Bounds();
}


FloatRect Path::fastBoundingRectSlowCase() const
{
    return m_path->Bounds();
}


bool Path::isNull() const
{
    return !m_path;
}

} // namespace WebCore

