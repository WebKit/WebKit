/*
    Copyright (C) 2007 Krzysztof Kowalczyk <kkowalczyk@gmail.com>
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005, 2007 Apple Inc. All Rights reserved.
                  2007 Alp Toker <alp@atoker.com>
                  2008 Dirk Schulze <krit@webkit.org>
                  2011, 2020 Igalia S.L.

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

#if USE(CAIRO)

#include "CairoUniquePtr.h"
#include "CairoUtilities.h"
#include "FloatRect.h"
#include "GraphicsContextCairo.h"
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Path::Path()
    : m_elements(Vector<PathElement>())
{
}

Path::Path(RefPtr<cairo_t>&& path)
    : m_path(WTFMove(path))
{
}

Path::~Path() = default;

Path::Path(Path&&) = default;

Path& Path::operator=(Path&&) = default;

Path::Path(const Path& other)
{
    if (other.isNull())
        return;

    cairo_t* cr = ensureCairoPath();
    cairo_matrix_t ctm;
    cairo_get_matrix(other.m_path.get(), &ctm);
    cairo_set_matrix(cr, &ctm);

    CairoUniquePtr<cairo_path_t> pathCopy(cairo_copy_path(other.m_path.get()));
    cairo_append_path(cr, pathCopy.get());
    m_elements = other.m_elements;
}

cairo_t* Path::ensureCairoPath()
{
    if (!m_path)
        m_path = adoptRef(cairo_create(adoptRef(cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1)).get()));
    return m_path.get();
}

Path& Path::operator=(const Path& other)
{
    if (&other == this)
        return *this;

    if (other.isNull()) {
        m_path = nullptr;
        m_elements = Vector<PathElement>();
        return *this;
    }

    clear();

    cairo_t* cr = ensureCairoPath();
    cairo_matrix_t ctm;
    cairo_get_matrix(other.m_path.get(), &ctm);
    cairo_set_matrix(cr, &ctm);

    CairoUniquePtr<cairo_path_t> pathCopy(cairo_copy_path(other.m_path.get()));
    cairo_append_path(cr, pathCopy.get());
    m_elements = other.m_elements;

    return *this;
}

void Path::clear()
{
    if (isNull())
        return;

    cairo_identity_matrix(m_path.get());
    cairo_new_path(m_path.get());
    m_elements = Vector<PathElement>();
}

bool Path::isEmptySlowCase() const
{
    return !cairo_has_current_point(m_path.get());
}

FloatPoint Path::currentPointSlowCase() const
{
    // FIXME: Is this the correct way?
    double x;
    double y;
    cairo_get_current_point(m_path.get(), &x, &y);
    return FloatPoint(x, y);
}

void Path::translate(const FloatSize& p)
{
    cairo_translate(ensureCairoPath(), -p.width(), -p.height());

    if (!m_elements)
        return;

    for (auto& element : m_elements.value()) {
        switch (element.type) {
        case PathElement::Type::MoveToPoint:
        case PathElement::Type::AddLineToPoint:
            element.points[0].move(p);
            break;
        case PathElement::Type::AddQuadCurveToPoint:
            element.points[0].move(p);
            element.points[1].move(p);
            break;
        case PathElement::Type::AddCurveToPoint:
            element.points[0].move(p);
            element.points[1].move(p);
            element.points[2].move(p);
            break;
        case PathElement::Type::CloseSubpath:
            break;
        }
    }
}

void Path::appendElement(PathElement::Type type, Vector<FloatPoint, 3>&& points)
{
    PathElement element;
    element.type = type;
    switch (type) {
    case PathElement::Type::MoveToPoint:
    case PathElement::Type::AddLineToPoint:
        element.points[0] = points[0];
        break;
    case PathElement::Type::AddQuadCurveToPoint:
        element.points[0] = points[0];
        element.points[1] = points[1];
        break;
    case PathElement::Type::AddCurveToPoint:
        element.points[0] = points[0];
        element.points[1] = points[1];
        element.points[2] = points[2];
        break;
    case PathElement::Type::CloseSubpath:
        break;
    }
    m_elements->append(WTFMove(element));
}

void Path::moveToSlowCase(const FloatPoint& p)
{
    cairo_move_to(ensureCairoPath(), p.x(), p.y());
    if (m_elements)
        appendElement(PathElement::Type::MoveToPoint, { p });
}

void Path::addLineToSlowCase(const FloatPoint& p)
{
    cairo_line_to(ensureCairoPath(), p.x(), p.y());
    if (m_elements)
        appendElement(PathElement::Type::AddLineToPoint, { p });
}

void Path::addRect(const FloatRect& rect)
{
    cairo_rectangle(ensureCairoPath(), rect.x(), rect.y(), rect.width(), rect.height());

    if (!m_elements)
        return;

    FloatPoint point(rect.location());
    appendElement(PathElement::Type::MoveToPoint, { point });
    point.move(rect.width(), 0);
    appendElement(PathElement::Type::AddLineToPoint, { point });
    point.move(0, rect.height());
    appendElement(PathElement::Type::AddLineToPoint, { point });
    point.move(-rect.width(), 0);
    appendElement(PathElement::Type::AddLineToPoint, { point });
    appendElement(PathElement::Type::CloseSubpath, { });
    if (cairo_has_current_point(m_path.get()))
        appendElement(PathElement::Type::MoveToPoint, { currentPointSlowCase() });
}

void Path::addQuadCurveToSlowCase(const FloatPoint& controlPoint, const FloatPoint& point)
{
    double x, y;
    double x1 = controlPoint.x();
    double y1 = controlPoint.y();
    double x2 = point.x();
    double y2 = point.y();
    cairo_t* cr = ensureCairoPath();
    cairo_get_current_point(cr, &x, &y);
    cairo_curve_to(cr,
        x  + 2.0 / 3.0 * (x1 - x),  y  + 2.0 / 3.0 * (y1 - y),
        x2 + 2.0 / 3.0 * (x1 - x2), y2 + 2.0 / 3.0 * (y1 - y2),
        x2, y2);
    if (m_elements)
        appendElement(PathElement::Type::AddQuadCurveToPoint, { controlPoint, point  });
}

void Path::addBezierCurveToSlowCase(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& controlPoint3)
{
    cairo_curve_to(ensureCairoPath(), controlPoint1.x(), controlPoint1.y(),
        controlPoint2.x(), controlPoint2.y(), controlPoint3.x(), controlPoint3.y());
    if (m_elements)
        appendElement(PathElement::Type::AddCurveToPoint, { controlPoint1, controlPoint2, controlPoint3 });
}

void Path::addArcSlowCase(const FloatPoint& p, float r, float startAngle, float endAngle, bool anticlockwise)
{
    m_elements = std::nullopt;
    cairo_t* cr = ensureCairoPath();
    float sweep = endAngle - startAngle;
    const float twoPI = 2 * piFloat;
    if ((sweep <= -twoPI || sweep >= twoPI)
        && ((anticlockwise && (endAngle < startAngle)) || (!anticlockwise && (startAngle < endAngle)))) {
        if (anticlockwise)
            cairo_arc_negative(cr, p.x(), p.y(), r, startAngle, startAngle - twoPI);
        else
            cairo_arc(cr, p.x(), p.y(), r, startAngle, startAngle + twoPI);
        cairo_new_sub_path(cr);
        cairo_arc(cr, p.x(), p.y(), r, endAngle, endAngle);
    } else {
        if (anticlockwise)
            cairo_arc_negative(cr, p.x(), p.y(), r, startAngle, endAngle);
        else
            cairo_arc(cr, p.x(), p.y(), r, startAngle, endAngle);
    }
}

static inline float areaOfTriangleFormedByPoints(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& p3)
{
    return p1.x() * (p2.y() - p3.y()) + p2.x() * (p3.y() - p1.y()) + p3.x() * (p1.y() - p2.y());
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    // FIXME: Why do we return if the path is empty? Can't a path start with an arc?
    if (isEmpty())
        return;

    double x0, y0;
    cairo_get_current_point(m_path.get(), &x0, &y0);
    FloatPoint p0(x0, y0);

    // Draw only a straight line to p1 if any of the points are equal or the radius is zero
    // or the points are collinear (triangle that the points form has area of zero value).
    if ((p1.x() == p0.x() && p1.y() == p0.y()) || (p1.x() == p2.x() && p1.y() == p2.y()) || !radius
        || !areaOfTriangleFormedByPoints(p0, p1, p2)) {
        cairo_line_to(m_path.get(), p1.x(), p1.y());
        if (m_elements)
            appendElement(PathElement::Type::AddLineToPoint, { p1 });
        return;
    }

    FloatPoint p1p0((p0.x() - p1.x()),(p0.y() - p1.y()));
    FloatPoint p1p2((p2.x() - p1.x()),(p2.y() - p1.y()));
    float p1p0_length = std::hypot(p1p0.x(), p1p0.y());
    float p1p2_length = std::hypot(p1p2.x(), p1p2.y());
    double cos_phi = (p1p0.x() * p1p2.x() + p1p0.y() * p1p2.y()) / (p1p0_length * p1p2_length);
    // all points on a line logic
    if (cos_phi == -1) {
        cairo_line_to(m_path.get(), p1.x(), p1.y());
        if (m_elements)
            appendElement(PathElement::Type::AddLineToPoint, { p1 });
        return;
    }
    if (cos_phi == 1) {
        // add infinite far away point
        unsigned int max_length = 65535;
        double factor_max = max_length / p1p0_length;
        FloatPoint ep((p0.x() + factor_max * p1p0.x()), (p0.y() + factor_max * p1p0.y()));
        cairo_line_to(m_path.get(), ep.x(), ep.y());
        if (m_elements)
            appendElement(PathElement::Type::AddLineToPoint, { ep });
        return;
    }

    m_elements = std::nullopt;
    float tangent = radius / tan(acos(cos_phi) / 2);
    float factor_p1p0 = tangent / p1p0_length;
    FloatPoint t_p1p0((p1.x() + factor_p1p0 * p1p0.x()), (p1.y() + factor_p1p0 * p1p0.y()));

    FloatPoint orth_p1p0(p1p0.y(), -p1p0.x());
    float orth_p1p0_length = std::hypot(orth_p1p0.x(), orth_p1p0.y());
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
    FloatPoint orth_p1p2((t_p1p2.x() - p.x()),(t_p1p2.y() - p.y()));
    float orth_p1p2_length = sqrtf(orth_p1p2.x() * orth_p1p2.x() + orth_p1p2.y() * orth_p1p2.y());
    float ea = acos(orth_p1p2.x() / orth_p1p2_length);
    if (orth_p1p2.y() < 0)
        ea = 2 * piDouble - ea;
    if ((sa > ea) && ((sa - ea) < piDouble))
        anticlockwise = true;
    if ((sa < ea) && ((ea - sa) > piDouble))
        anticlockwise = true;

    cairo_line_to(m_path.get(), t_p1p0.x(), t_p1p0.y());

    addArc(p, radius, sa, ea, anticlockwise);
}

void Path::addEllipse(FloatPoint point, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise)
{
    m_elements = std::nullopt;
    cairo_t* cr = ensureCairoPath();
    cairo_save(cr);
    cairo_translate(cr, point.x(), point.y());
    cairo_rotate(cr, rotation);
    cairo_scale(cr, radiusX, radiusY);

    if (anticlockwise)
        cairo_arc_negative(cr, 0, 0, 1, startAngle, endAngle);
    else
        cairo_arc(cr, 0, 0, 1, startAngle, endAngle);

    cairo_restore(cr);
}

void Path::addEllipse(const FloatRect& rect)
{
    m_elements = std::nullopt;
    cairo_t* cr = ensureCairoPath();
    cairo_save(cr);
    float yRadius = .5 * rect.height();
    float xRadius = .5 * rect.width();
    cairo_translate(cr, rect.x() + xRadius, rect.y() + yRadius);
    cairo_scale(cr, xRadius, yRadius);
    cairo_arc(cr, 0., 0., 1., 0., 2 * piDouble);
    cairo_restore(cr);
}

void Path::addPath(const Path& path, const AffineTransform& transform)
{
    if (path.isNull())
        return;

    cairo_matrix_t matrix = toCairoMatrix(transform);
    if (cairo_matrix_invert(&matrix) != CAIRO_STATUS_SUCCESS)
        return;

    m_elements = std::nullopt;

    cairo_t* cr = path.cairoPath();
    cairo_save(cr);
    cairo_transform(cr, &matrix);
    CairoUniquePtr<cairo_path_t> pathCopy(cairo_copy_path(cr));
    cairo_restore(cr);
    cairo_append_path(ensureCairoPath(), pathCopy.get());
}

void Path::closeSubpath()
{
    cairo_close_path(ensureCairoPath());
    if (m_elements) {
        appendElement(PathElement::Type::CloseSubpath, { });
        if (cairo_has_current_point(m_path.get()))
            appendElement(PathElement::Type::MoveToPoint, { currentPointSlowCase() });
    }
}

FloatRect Path::boundingRectSlowCase() const
{
    double x0, x1, y0, y1;
    if (m_elements && m_elements.value().size() == 1 && m_elements.value()[0].type == PathElement::Type::MoveToPoint) {
        FloatPoint p = m_elements.value()[0].points[0];
        return FloatRect(p.x(), p.y(), 0, 0);
    }
    cairo_path_extents(m_path.get(), &x0, &y0, &x1, &y1);
    return FloatRect(x0, y0, x1 - x0, y1 - y0);
}

FloatRect Path::strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    // Should this be isEmpty() or can an empty path have a non-zero origin?
    if (isNull())
        return FloatRect();

    if (strokeStyleApplier) {
        GraphicsContextCairo gc(m_path.get());
        strokeStyleApplier(gc);
    }

    double x0, x1, y0, y1;
    cairo_stroke_extents(m_path.get(), &x0, &y0, &x1, &y1);
    return FloatRect(x0, y0, x1 - x0, y1 - y0);
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    if (isNull() || !std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;

    cairo_fill_rule_t cur = cairo_get_fill_rule(m_path.get());
    cairo_set_fill_rule(m_path.get(), rule == WindRule::EvenOdd ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    bool contains = cairo_in_fill(m_path.get(), point.x(), point.y());
    cairo_set_fill_rule(m_path.get(), cur);
    return contains;
}

bool Path::strokeContains(const FloatPoint& point, const Function<void(GraphicsContext&)>& strokeStyleApplier) const
{
    ASSERT(strokeStyleApplier);

    if (isNull())
        return false;

    {
        GraphicsContextCairo graphicsContext(m_path.get());
        strokeStyleApplier(graphicsContext);
    }

    return cairo_in_stroke(m_path.get(), point.x(), point.y());
}

void Path::applySlowCase(const PathApplierFunction& function) const
{
    if (m_elements) {
        for (const auto& element : m_elements.value())
            function(element);
        return;
    }

    CairoUniquePtr<cairo_path_t> pathCopy(cairo_copy_path(m_path.get()));
    cairo_path_data_t* data;
    PathElement pathElement;

    for (int i = 0; i < pathCopy->num_data; i += pathCopy->data[i].header.length) {
        data = &pathCopy->data[i];
        switch (data->header.type) {
        case CAIRO_PATH_MOVE_TO:
            pathElement.type = PathElement::Type::MoveToPoint;
            pathElement.points[0] = FloatPoint(data[1].point.x, data[1].point.y);
            function(pathElement);
            break;
        case CAIRO_PATH_LINE_TO:
            pathElement.type = PathElement::Type::AddLineToPoint;
            pathElement.points[0] = FloatPoint(data[1].point.x, data[1].point.y);
            function(pathElement);
            break;
        case CAIRO_PATH_CURVE_TO:
            pathElement.type = PathElement::Type::AddCurveToPoint;
            pathElement.points[0] = FloatPoint(data[1].point.x, data[1].point.y);
            pathElement.points[1] = FloatPoint(data[2].point.x, data[2].point.y);
            pathElement.points[2] = FloatPoint(data[3].point.x, data[3].point.y);
            function(pathElement);
            break;
        case CAIRO_PATH_CLOSE_PATH:
            pathElement.type = PathElement::Type::CloseSubpath;
            function(pathElement);
            break;
        }
    }
}

FloatRect Path::fastBoundingRectSlowCase() const
{
    return boundingRect();
}

void Path::transform(const AffineTransform& transform)
{
    cairo_matrix_t matrix = toCairoMatrix(transform);
    cairo_matrix_invert(&matrix);
    cairo_transform(ensureCairoPath(), &matrix);

    if (!m_elements)
        return;

    for (auto& element : m_elements.value()) {
        switch (element.type) {
        case PathElement::Type::MoveToPoint:
        case PathElement::Type::AddLineToPoint:
            element.points[0] = transform.mapPoint(element.points[0]);
            break;
        case PathElement::Type::AddQuadCurveToPoint:
            element.points[0] = transform.mapPoint(element.points[0]);
            element.points[1] = transform.mapPoint(element.points[1]);
            break;
        case PathElement::Type::AddCurveToPoint:
            element.points[0] = transform.mapPoint(element.points[0]);
            element.points[1] = transform.mapPoint(element.points[1]);
            element.points[2] = transform.mapPoint(element.points[2]);
            break;
        case PathElement::Type::CloseSubpath:
            break;
        }
    }
}

bool Path::isNull() const
{
    return !m_path;
}

} // namespace WebCore

#endif // USE(CAIRO)
