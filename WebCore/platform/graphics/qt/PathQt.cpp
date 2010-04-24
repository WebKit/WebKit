/*
 * Copyright (C) 2006 Zack Rusin   <zack@kde.org>
 *               2006 Rob Buis     <buis@kde.org>
 *               2009, 2010 Dirk Schulze <krit@webkit.org>
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
#include "ImageBuffer.h"
#include "PlatformString.h"
#include "StrokeStyleApplier.h"
#include <QPainterPath>
#include <QTransform>
#include <QString>
#include <wtf/OwnPtr.h>

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

namespace WebCore {

Path::Path()
{
}

Path::~Path()
{
}

Path::Path(const Path& other)
    : m_path(other.m_path)
{
}

Path& Path::operator=(const Path& other)
{
    m_path = other.m_path;
    return *this;
}

// Check whether a point is on the border
bool isPointOnPathBorder(const QPolygonF& border, const QPointF& p)
{
    QPointF p1 = border.at(0);
    QPointF p2;

    for (int i = 1; i < border.size(); ++i) {
        p2 = border.at(i);
        //  (x1<=x<=x2||x1=>x>=x2) && (y1<=y<=y2||y1=>y>=y2)  && (y2-y1)(x-x1) == (y-y1)(x2-x1)
        //  In which, (y2-y1)(x-x1) == (y-y1)(x2-x1) is from (y2-y1)/(x2-x1) == (y-y1)/(x-x1)
        //  it want to check the slope between p1 and p2 is same with slope between p and p1,
        //  if so then the three points lie on the same line.
        //  In which, (x1<=x<=x2||x1=>x>=x2) && (y1<=y<=y2||y1=>y>=y2) want to make sure p is
        //  between p1 and p2, not outside.
        if (((p.x() <= p1.x() && p.x() >= p2.x()) || (p.x() >= p1.x() && p.x() <= p2.x()))
            && ((p.y() <= p1.y() && p.y() >= p2.y()) || (p.y() >= p1.y() && p.y() <= p2.y()))
            && (p2.y() - p1.y()) * (p.x() - p1.x()) == (p.y() - p1.y()) * (p2.x() - p1.x())) {
            return true;
        }
        p1 = p2;
    }
    return false;
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    Qt::FillRule savedRule = m_path.fillRule();
    const_cast<QPainterPath*>(&m_path)->setFillRule(rule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);

    bool contains = m_path.contains(point);
    
    if (!contains) {
        // check whether the point is on the border
        contains = isPointOnPathBorder(m_path.toFillPolygon(), point);
    }

    const_cast<QPainterPath*>(&m_path)->setFillRule(savedRule);
    return contains;
}

bool Path::strokeContains(StrokeStyleApplier* applier, const FloatPoint& point) const
{
    ASSERT(applier);

    // FIXME: We should try to use a 'shared Context' instead of creating a new ImageBuffer
    // on each call.
    OwnPtr<ImageBuffer> scratchImage = ImageBuffer::create(IntSize(1, 1));
    GraphicsContext* gc = scratchImage->context();
    QPainterPathStroker stroke;
    applier->strokeStyle(gc);

    QPen pen = gc->pen();
    stroke.setWidth(pen.widthF());
    stroke.setCapStyle(pen.capStyle());
    stroke.setJoinStyle(pen.joinStyle());
    stroke.setMiterLimit(pen.miterLimit());
    stroke.setDashPattern(pen.dashPattern());
    stroke.setDashOffset(pen.dashOffset());

    return stroke.createStroke(m_path).contains(point);
}

void Path::translate(const FloatSize& size)
{
    QTransform matrix;
    matrix.translate(size.width(), size.height());
    m_path = m_path * matrix;
}

FloatRect Path::boundingRect() const
{
    return m_path.boundingRect();
}

FloatRect Path::strokeBoundingRect(StrokeStyleApplier* applier)
{
    // FIXME: We should try to use a 'shared Context' instead of creating a new ImageBuffer
    // on each call.
    OwnPtr<ImageBuffer> scratchImage = ImageBuffer::create(IntSize(1, 1));
    GraphicsContext* gc = scratchImage->context();
    QPainterPathStroker stroke;
    if (applier) {
        applier->strokeStyle(gc);

        QPen pen = gc->pen();
        stroke.setWidth(pen.widthF());
        stroke.setCapStyle(pen.capStyle());
        stroke.setJoinStyle(pen.joinStyle());
        stroke.setMiterLimit(pen.miterLimit());
        stroke.setDashPattern(pen.dashPattern());
        stroke.setDashOffset(pen.dashOffset());
    }
    return stroke.createStroke(m_path).boundingRect();
}

void Path::moveTo(const FloatPoint& point)
{
    m_path.moveTo(point);
}

void Path::addLineTo(const FloatPoint& p)
{
    m_path.lineTo(p);
}

void Path::addQuadCurveTo(const FloatPoint& cp, const FloatPoint& p)
{
    m_path.quadTo(cp, p);
}

void Path::addBezierCurveTo(const FloatPoint& cp1, const FloatPoint& cp2, const FloatPoint& p)
{
    m_path.cubicTo(cp1, cp2, p);
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    FloatPoint p0(m_path.currentPosition());

    if ((p1.x() == p0.x() && p1.y() == p0.y()) || (p1.x() == p2.x() && p1.y() == p2.y()) || radius == 0.f) {
        m_path.lineTo(p1);
        return;
    }

    FloatPoint p1p0((p0.x() - p1.x()), (p0.y() - p1.y()));
    FloatPoint p1p2((p2.x() - p1.x()), (p2.y() - p1.y()));
    float p1p0_length = sqrtf(p1p0.x() * p1p0.x() + p1p0.y() * p1p0.y());
    float p1p2_length = sqrtf(p1p2.x() * p1p2.x() + p1p2.y() * p1p2.y());

    double cos_phi = (p1p0.x() * p1p2.x() + p1p0.y() * p1p2.y()) / (p1p0_length * p1p2_length);
    // all points on a line logic
    if (cos_phi == -1) {
        m_path.lineTo(p1);
        return;
    }
    if (cos_phi == 1) {
        // add infinite far away point
        unsigned int max_length = 65535;
        double factor_max = max_length / p1p0_length;
        FloatPoint ep((p0.x() + factor_max * p1p0.x()), (p0.y() + factor_max * p1p0.y()));
        m_path.lineTo(ep);
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

    m_path.lineTo(t_p1p0);

    addArc(p, radius, sa, ea, anticlockwise);
}

void Path::closeSubpath()
{
    m_path.closeSubpath();
}

#define DEGREES(t) ((t) * 180.0 / M_PI)
void Path::addArc(const FloatPoint& p, float r, float sar, float ear, bool anticlockwise)
{
    qreal xc = p.x();
    qreal yc = p.y();
    qreal radius = r;


    //### HACK
    // In Qt we don't switch the coordinate system for degrees
    // and still use the 0,0 as bottom left for degrees so we need
    // to switch
    sar = -sar;
    ear = -ear;
    anticlockwise = !anticlockwise;
    //end hack

    float sa = DEGREES(sar);
    float ea = DEGREES(ear);

    double span = 0;

    double xs = xc - radius;
    double ys = yc - radius;
    double width  = radius*2;
    double height = radius*2;

    if (!anticlockwise && (ea < sa))
        span += 360;
    else if (anticlockwise && (sa < ea))
        span -= 360;

    // this is also due to switched coordinate system
    // we would end up with a 0 span instead of 360
    if (!(qFuzzyCompare(span + (ea - sa) + 1, 1.0) &&
          qFuzzyCompare(qAbs(span), 360.0))) {
        span += ea - sa;
    }

    // connect to the previous point by a straight line
    m_path.lineTo(QPointF(xc + radius  * cos(sar),
                          yc - radius  * sin(sar)));

    m_path.arcTo(xs, ys, width, height, sa, span);

}

void Path::addRect(const FloatRect& r)
{
    m_path.addRect(r.x(), r.y(), r.width(), r.height());
}

void Path::addEllipse(const FloatRect& r)
{
    m_path.addEllipse(r.x(), r.y(), r.width(), r.height());
}

void Path::clear()
{
    m_path = QPainterPath();
}

bool Path::isEmpty() const
{
    // Don't use QPainterPath::isEmpty(), as that also returns true if there's only
    // one initial MoveTo element in the path.
    return !m_path.elementCount();
}

bool Path::hasCurrentPoint() const
{
    return !isEmpty();
}

String Path::debugString() const
{
    QString ret;
    for (int i = 0; i < m_path.elementCount(); ++i) {
        const QPainterPath::Element &cur = m_path.elementAt(i);

        switch (cur.type) {
            case QPainterPath::MoveToElement:
                ret += QString(QLatin1String("M%1,%2 ")).arg(cur.x, 0, 'f', 2).arg(cur.y, 0, 'f', 2);
                break;
            case QPainterPath::LineToElement:
                ret += QString(QLatin1String("L%1,%2 ")).arg(cur.x, 0, 'f', 2).arg(cur.y, 0, 'f', 2);
                break;
            case QPainterPath::CurveToElement:
            {
                const QPainterPath::Element &c1 = m_path.elementAt(i + 1);
                const QPainterPath::Element &c2 = m_path.elementAt(i + 2);

                Q_ASSERT(c1.type == QPainterPath::CurveToDataElement);
                Q_ASSERT(c2.type == QPainterPath::CurveToDataElement);

                ret += QString(QLatin1String("C%1,%2,%3,%4,%5,%6 ")).arg(cur.x, 0, 'f', 2).arg(cur.y, 0, 'f', 2).arg(c1.x, 0, 'f', 2)
                                                                    .arg(c1.y, 0, 'f', 2).arg(c2.x, 0, 'f', 2).arg(c2.y, 0, 'f', 2);
                i += 2;
                break;
            }
            case QPainterPath::CurveToDataElement:
                Q_ASSERT(false);
                break;
        }
    }

    return ret.trimmed();
}

void Path::apply(void* info, PathApplierFunction function) const
{
    PathElement pelement;
    FloatPoint points[3];
    pelement.points = points;
    for (int i = 0; i < m_path.elementCount(); ++i) {
        const QPainterPath::Element& cur = m_path.elementAt(i);

        switch (cur.type) {
            case QPainterPath::MoveToElement:
                pelement.type = PathElementMoveToPoint;
                pelement.points[0] = QPointF(cur);
                function(info, &pelement);
                break;
            case QPainterPath::LineToElement:
                pelement.type = PathElementAddLineToPoint;
                pelement.points[0] = QPointF(cur);
                function(info, &pelement);
                break;
            case QPainterPath::CurveToElement:
            {
                const QPainterPath::Element& c1 = m_path.elementAt(i + 1);
                const QPainterPath::Element& c2 = m_path.elementAt(i + 2);

                Q_ASSERT(c1.type == QPainterPath::CurveToDataElement);
                Q_ASSERT(c2.type == QPainterPath::CurveToDataElement);

                pelement.type = PathElementAddCurveToPoint;
                pelement.points[0] = QPointF(cur);
                pelement.points[1] = QPointF(c1);
                pelement.points[2] = QPointF(c2);
                function(info, &pelement);

                i += 2;
                break;
            }
            case QPainterPath::CurveToDataElement:
                Q_ASSERT(false);
        }
    }
}

void Path::transform(const AffineTransform& transform)
{
    m_path = QTransform(transform).map(m_path);
}

}

// vim: ts=4 sw=4 et
