/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGPathData.h"

#include "Path.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SVGCircleElement.h"
#include "SVGEllipseElement.h"
#include "SVGLengthContext.h"
#include "SVGLineElement.h"
#include "SVGNames.h"
#include "SVGPathElement.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include "SVGPointList.h"
#include "SVGPolygonElement.h"
#include "SVGPolylineElement.h"
#include "SVGRectElement.h"
#include <wtf/HashMap.h>

namespace WebCore {

static Path pathFromCircleElement(const SVGElement& element)
{
    ASSERT(is<SVGCircleElement>(element));

    RenderElement* renderer = element.renderer();
    if (!renderer)
        return { };

    Path path;
    auto& style = renderer->style();
    SVGLengthContext lengthContext(&element);
    float r = lengthContext.valueForLength(style.svgStyle().r());
    if (r > 0) {
        float cx = lengthContext.valueForLength(style.svgStyle().cx(), SVGLengthMode::Width);
        float cy = lengthContext.valueForLength(style.svgStyle().cy(), SVGLengthMode::Height);
        path.addEllipse(FloatRect(cx - r, cy - r, r * 2, r * 2));
    }
    return path;
}

static Path pathFromEllipseElement(const SVGElement& element)
{
    RenderElement* renderer = element.renderer();
    if (!renderer)
        return { };

    auto& style = renderer->style();
    SVGLengthContext lengthContext(&element);
    float rx = lengthContext.valueForLength(style.svgStyle().rx(), SVGLengthMode::Width);
    if (rx <= 0)
        return { };

    float ry = lengthContext.valueForLength(style.svgStyle().ry(), SVGLengthMode::Height);
    if (ry <= 0)
        return { };

    Path path;
    float cx = lengthContext.valueForLength(style.svgStyle().cx(), SVGLengthMode::Width);
    float cy = lengthContext.valueForLength(style.svgStyle().cy(), SVGLengthMode::Height);
    path.addEllipse(FloatRect(cx - rx, cy - ry, rx * 2, ry * 2));
    return path;
}

static Path pathFromLineElement(const SVGElement& element)
{
    Path path;
    const auto& line = downcast<SVGLineElement>(element);

    SVGLengthContext lengthContext(&element);
    path.moveTo(FloatPoint(line.x1().value(lengthContext), line.y1().value(lengthContext)));
    path.addLineTo(FloatPoint(line.x2().value(lengthContext), line.y2().value(lengthContext)));
    return path;
}

static Path pathFromPathElement(const SVGElement& element)
{
    return downcast<SVGPathElement>(element).path();
}

static Path pathFromPolygonElement(const SVGElement& element)
{
    auto& points = downcast<SVGPolygonElement>(element).points().items();
    if (points.isEmpty())
        return { };

    Path path;
    path.moveTo(points.first()->value());

    unsigned size = points.size();
    for (unsigned i = 1; i < size; ++i)
        path.addLineTo(points.at(i)->value());

    path.closeSubpath();
    return path;
}

static Path pathFromPolylineElement(const SVGElement& element)
{
    auto& points = downcast<SVGPolylineElement>(element).points().items();
    if (points.isEmpty())
        return { };

    Path path;
    path.moveTo(points.first()->value());

    unsigned size = points.size();
    for (unsigned i = 1; i < size; ++i)
        path.addLineTo(points.at(i)->value());
    return path;
}

static Path pathFromRectElement(const SVGElement& element)
{
    RenderElement* renderer = element.renderer();
    if (!renderer)
        return { };

    auto& style = renderer->style();
    SVGLengthContext lengthContext(&element);
    float width = lengthContext.valueForLength(style.width(), SVGLengthMode::Width);
    if (width <= 0)
        return { };

    float height = lengthContext.valueForLength(style.height(), SVGLengthMode::Height);
    if (height <= 0)
        return { };

    Path path;
    float x = lengthContext.valueForLength(style.svgStyle().x(), SVGLengthMode::Width);
    float y = lengthContext.valueForLength(style.svgStyle().y(), SVGLengthMode::Height);
    float rx = lengthContext.valueForLength(style.svgStyle().rx(), SVGLengthMode::Width);
    float ry = lengthContext.valueForLength(style.svgStyle().ry(), SVGLengthMode::Height);
    bool hasRx = rx > 0;
    bool hasRy = ry > 0;
    if (hasRx || hasRy) {
        if (!hasRx)
            rx = ry;
        else if (!hasRy)
            ry = rx;
        // FIXME: We currently enforce using beziers here, as at least on CoreGraphics/Lion, as
        // the native method uses a different line dash origin, causing svg/custom/dashOrigin.svg to fail.
        // See bug https://bugs.webkit.org/show_bug.cgi?id=79932 which tracks this issue.
        path.addRoundedRect(FloatRect(x, y, width, height), FloatSize(rx, ry), Path::RoundedRectStrategy::PreferBezier);
        return path;
    }

    path.addRect(FloatRect(x, y, width, height));
    return path;
}

Path pathFromGraphicsElement(const SVGElement* element)
{
    ASSERT(element);

    typedef Path (*PathFromFunction)(const SVGElement&);
    static HashMap<AtomStringImpl*, PathFromFunction>* map = 0;
    if (!map) {
        map = new HashMap<AtomStringImpl*, PathFromFunction>;
        map->set(SVGNames::circleTag->localName().impl(), pathFromCircleElement);
        map->set(SVGNames::ellipseTag->localName().impl(), pathFromEllipseElement);
        map->set(SVGNames::lineTag->localName().impl(), pathFromLineElement);
        map->set(SVGNames::pathTag->localName().impl(), pathFromPathElement);
        map->set(SVGNames::polygonTag->localName().impl(), pathFromPolygonElement);
        map->set(SVGNames::polylineTag->localName().impl(), pathFromPolylineElement);
        map->set(SVGNames::rectTag->localName().impl(), pathFromRectElement);
    }

    if (PathFromFunction pathFromFunction = map->get(element->localName().impl()))
        return (*pathFromFunction)(*element);
    
    return { };
}

} // namespace WebCore
