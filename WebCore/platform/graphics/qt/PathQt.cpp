/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 *               2006 Rob Buis   <buis@kde.org>
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

#include "FloatRect.h"
#include "PlatformString.h"
#include "AffineTransform.h"
#include <QPainterPath>
#include <QMatrix>
#include <QString>

#define _USE_MATH_DEFINES
#include <math.h>

namespace WebCore {

Path::Path()
    : m_path(new QPainterPath())
{
}

Path::~Path()
{
    delete m_path;
}

Path::Path(const Path& other)
    : m_path(new QPainterPath(*other.platformPath()))
{
}

Path& Path::operator=(const Path& other)
{
    if (&other != this) {
        delete m_path;
        m_path = new QPainterPath(*other.platformPath());
    }

    return *this;
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    Qt::FillRule savedRule = m_path->fillRule();
    m_path->setFillRule(rule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);

    bool contains = m_path->contains(point);

    m_path->setFillRule(savedRule);
    return contains;
}

void Path::translate(const FloatSize& size)
{
    QMatrix matrix;
    matrix.translate(size.width(), size.height());
    *m_path = (*m_path) * matrix;
}

FloatRect Path::boundingRect() const
{
    return m_path->boundingRect();
}

void Path::moveTo(const FloatPoint& point)
{
    m_path->moveTo(point);
}

void Path::addLineTo(const FloatPoint& p)
{
    m_path->lineTo(p);
}

void Path::addQuadCurveTo(const FloatPoint& cp, const FloatPoint& p)
{
    m_path->quadTo(cp, p);
}

void Path::addBezierCurveTo(const FloatPoint& cp1, const FloatPoint& cp2, const FloatPoint& p)
{
    m_path->cubicTo(cp1, cp2, p);
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    //FIXME: busted
    qWarning("arcTo is busted");
    m_path->arcTo(p1.x(), p1.y(), p2.x(), p2.y(), radius, 90);
}

void Path::closeSubpath()
{
    m_path->closeSubpath();
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
    if (!(qFuzzyCompare(span + (ea - sa), 0.0) &&
          qFuzzyCompare(qAbs(span), 360.0))) {
        span += ea - sa;
    }

    m_path->moveTo(QPointF(xc + radius  * cos(sar),
                          yc - radius  * sin(sar)));

    m_path->arcTo(xs, ys, width, height, sa, span);
}

void Path::addRect(const FloatRect& r)
{
    m_path->addRect(r.x(), r.y(), r.width(), r.height());
}

void Path::addEllipse(const FloatRect& r)
{
    m_path->addEllipse(r.x(), r.y(), r.width(), r.height());
}

void Path::clear()
{
    *m_path = QPainterPath();
}

bool Path::isEmpty() const
{
    return m_path->isEmpty();
}

String Path::debugString() const
{
    QString ret;
    for (int i = 0; i < m_path->elementCount(); ++i) {
        const QPainterPath::Element &cur = m_path->elementAt(i);

        switch (cur.type) {
            case QPainterPath::MoveToElement:
                ret += QString("M %1 %2").arg(cur.x).arg(cur.y);
                break;
            case QPainterPath::LineToElement:
                ret += QString("L %1 %2").arg(cur.x).arg(cur.y);
                break;
            case QPainterPath::CurveToElement:
            {
                const QPainterPath::Element &c1 = m_path->elementAt(i + 1);
                const QPainterPath::Element &c2 = m_path->elementAt(i + 2);

                Q_ASSERT(c1.type == QPainterPath::CurveToDataElement);
                Q_ASSERT(c2.type == QPainterPath::CurveToDataElement);

                ret += QString("C %1 %2 %3 %4 %5 %6").arg(cur.x).arg(cur.y).arg(c1.x).arg(c1.y).arg(c2.x).arg(c2.y);

                i += 2;
                break;
            }
            case QPainterPath::CurveToDataElement:
                Q_ASSERT(false);
                break;
        }
    }

    return ret;
}

void Path::apply(void* info, PathApplierFunction function) const
{
    PathElement pelement;
    FloatPoint points[3];
    pelement.points = points;
    for (int i = 0; i < m_path->elementCount(); ++i) {
        const QPainterPath::Element& cur = m_path->elementAt(i);

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
                const QPainterPath::Element& c1 = m_path->elementAt(i + 1);
                const QPainterPath::Element& c2 = m_path->elementAt(i + 2);

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
    if (m_path) {
        QMatrix mat = transform;
        QPainterPath temp = mat.map(*m_path);
        delete m_path;
        m_path = new QPainterPath(temp);
    }
}

}

// vim: ts=4 sw=4 et
