/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGPathTraversalStateBuilder.h"

#include "PathTraversalState.h"

namespace WebCore {

SVGPathTraversalStateBuilder::SVGPathTraversalStateBuilder(PathTraversalState& state, float desiredLength)
    : m_traversalState(state)
{
    m_traversalState.setDesiredLength(desiredLength);
}

void SVGPathTraversalStateBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode)
{
    m_traversalState.processPathElement(PathElement::Type::MoveToPoint, &targetPoint);
}

void SVGPathTraversalStateBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode)
{
    m_traversalState.processPathElement(PathElement::Type::AddLineToPoint, &targetPoint);
}

void SVGPathTraversalStateBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode)
{
    FloatPoint points[] = { point1, point2, targetPoint };

    m_traversalState.processPathElement(PathElement::Type::AddCurveToPoint, points);
}

void SVGPathTraversalStateBuilder::closePath()
{
    m_traversalState.processPathElement(PathElement::Type::CloseSubpath, nullptr);
}

bool SVGPathTraversalStateBuilder::continueConsuming()
{
    return !m_traversalState.success();
}

float SVGPathTraversalStateBuilder::totalLength() const
{
    return m_traversalState.totalLength();
}

FloatPoint SVGPathTraversalStateBuilder::currentPoint() const
{
    return m_traversalState.current();
}

}
