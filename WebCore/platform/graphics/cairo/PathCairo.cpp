/*
    Copyright (C) 2007 Krzysztof Kowalczyk <kkowalczyk@gmail.com>
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005, 2007 Apple Inc. All Rights reserved.
                  2007 Alp Toker <alp@atoker.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "Path.h"

#include "AffineTransform.h"
#include "CairoPath.h"
#include "FloatRect.h"
#include "NotImplemented.h"
#include "PlatformString.h"

#include <cairo.h>
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

Path::Path()
    : m_path(new CairoPath())
{
}

Path::~Path()
{
    delete m_path;
}

Path::Path(const Path& other)
    : m_path(new CairoPath())
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_path_t* p = cairo_copy_path(other.platformPath()->m_cr);
    cairo_append_path(cr, p);
    cairo_path_destroy(p);
}

Path& Path::operator=(const Path& other)
{
    if (&other == this)
        return *this;

    clear();
    cairo_t* cr = platformPath()->m_cr;
    cairo_path_t* p = cairo_copy_path(other.platformPath()->m_cr);
    cairo_append_path(cr, p);
    cairo_path_destroy(p);
    return *this;
}

void Path::clear()
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_new_path(cr);
}

bool Path::isEmpty() const
{
    // FIXME: if/when the patch to get current pt return status is applied
    // double dx,dy;
    // return cairo_get_current_point(cr, &dx, &dy);

    cairo_t* cr = platformPath()->m_cr;
    cairo_path_t* p = cairo_copy_path(cr);
    bool hasData = p->num_data;
    cairo_path_destroy(p);
    return !hasData;
}

void Path::translate(const FloatSize& p)
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_translate(cr, p.width(), p.height());
}

void Path::moveTo(const FloatPoint& p)
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_move_to(cr, p.x(), p.y());
}

void Path::addLineTo(const FloatPoint& p)
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_line_to(cr, p.x(), p.y());
}

void Path::addRect(const FloatRect& rect)
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
}

/*
 * inspired by libsvg-cairo
 */
void Path::addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& point)
{
    cairo_t* cr = platformPath()->m_cr;
    double x, y;
    double x1 = controlPoint.x();
    double y1 = controlPoint.y();
    double x2 = point.x();
    double y2 = point.y();
    cairo_get_current_point(cr, &x, &y);
    cairo_curve_to(cr,
                   x  + 2.0 / 3.0 * (x1 - x),  y  + 2.0 / 3.0 * (y1 - y),
                   x2 + 2.0 / 3.0 * (x1 - x2), y2 + 2.0 / 3.0 * (y1 - y2),
                   x2, y2);
}

void Path::addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& controlPoint3)
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_curve_to(cr, controlPoint1.x(), controlPoint1.y(),
                   controlPoint2.x(), controlPoint2.y(),
                   controlPoint3.x(), controlPoint3.y());
}

void Path::addArc(const FloatPoint& p, float r, float sa, float ea, bool anticlockwise)
{
    // http://bugs.webkit.org/show_bug.cgi?id=16449
    // cairo_arc() functions hang or crash when passed inf as radius or start/end angle
    if (!isfinite(r) || !isfinite(sa) || !isfinite(ea))
        return;

    cairo_t* cr = platformPath()->m_cr;
    if (anticlockwise)
        cairo_arc_negative(cr, p.x(), p.y(), r, sa, ea);
    else
        cairo_arc(cr, p.x(), p.y(), r, sa, ea);
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    // FIXME: cairo_arc_to not yet in cairo see cairo.h
    // cairo_arc_to(m_cr, p1.x(), p1.y(), p2.x(), p2.y());
    notImplemented();
}

void Path::addEllipse(const FloatRect& rect)
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_save(cr);
    float yRadius = .5 * rect.height();
    float xRadius = .5 * rect.width();
    cairo_translate(cr, rect.x() + xRadius, rect.y() + yRadius);
    cairo_scale(cr, xRadius, yRadius);
    cairo_arc(cr, 0., 0., 1., 0., 2 * piDouble);
    cairo_restore(cr);
}

void Path::closeSubpath()
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_close_path(cr);
}

FloatRect Path::boundingRect() const
{
    cairo_t* cr = platformPath()->m_cr;
    double x0, x1, y0, y1;
    cairo_fill_extents(cr, &x0, &y0, &x1, &y1);
    return FloatRect(x0, y0, x1 - x0, y1 - y0);
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    if (!boundingRect().contains(point))
        return false;

    cairo_t* cr = platformPath()->m_cr;
    cairo_fill_rule_t cur = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, rule == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    bool contains = cairo_in_fill(cr, point.x(), point.y());
    cairo_set_fill_rule(cr, cur);
    return contains;
}

void Path::apply(void* info, PathApplierFunction function) const
{
    cairo_t* cr = platformPath()->m_cr;
    cairo_path_t* path = cairo_copy_path(cr);
    cairo_path_data_t* data;
    PathElement pelement;
    FloatPoint points[3];
    pelement.points = points;

    for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
        data = &path->data[i];
        switch (data->header.type) {
        case CAIRO_PATH_MOVE_TO:
            pelement.type = PathElementMoveToPoint;
            pelement.points[0] = FloatPoint(data[1].point.x,data[1].point.y);
            function(info, &pelement);
            break;
        case CAIRO_PATH_LINE_TO:
            pelement.type = PathElementAddLineToPoint;
            pelement.points[0] = FloatPoint(data[1].point.x,data[1].point.y);
            function(info, &pelement);
            break;
        case CAIRO_PATH_CURVE_TO:
            pelement.type = PathElementAddCurveToPoint;
            pelement.points[0] = FloatPoint(data[1].point.x,data[1].point.y);
            pelement.points[1] = FloatPoint(data[2].point.x,data[2].point.y);
            pelement.points[2] = FloatPoint(data[3].point.x,data[3].point.y);
            function(info, &pelement);
            break;
        case CAIRO_PATH_CLOSE_PATH:
            pelement.type = PathElementCloseSubpath;
            function(info, &pelement);
            break;
        }
    }
    cairo_path_destroy(path);
}

void Path::transform(const AffineTransform& trans)
{
    cairo_t* m_cr = platformPath()->m_cr;
    cairo_matrix_t c_matrix = cairo_matrix_t(trans);
    cairo_transform(m_cr, &c_matrix);
}

String Path::debugString() const
{
    String string = "";
    cairo_path_t* path = cairo_copy_path(platformPath()->m_cr);
    cairo_path_data_t* data;

    if (!path->num_data )
        string = "EMPTY";

    for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
        data = &path->data[i];
        switch (data->header.type) {
        case CAIRO_PATH_MOVE_TO:
            string += String::format("M %.2f,%.2f",
                                      data[1].point.x, data[1].point.y);
            break;
        case CAIRO_PATH_LINE_TO:
            string += String::format("L %.2f,%.2f",
                                      data[1].point.x, data[1].point.y);
            break;
        case CAIRO_PATH_CURVE_TO:
            string += String::format("C %.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
                                      data[1].point.x, data[1].point.y,
                                      data[2].point.x, data[2].point.y,
                                      data[3].point.x, data[3].point.y);
            break;
        case CAIRO_PATH_CLOSE_PATH:
            string += "X";
            break;
        }
    }
    cairo_path_destroy(path);
    return string;
}

} // namespace WebCore
