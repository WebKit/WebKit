/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PathOperation.h"

#include "GeometryUtilities.h"
#include "SVGElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathData.h"
#include "SVGPathElement.h"

namespace WebCore {

Ref<ReferencePathOperation> ReferencePathOperation::create(const String& url, const AtomString& fragment, const RefPtr<SVGElement> element)
{
    return adoptRef(*new ReferencePathOperation(url, fragment, element));
}

Ref<ReferencePathOperation> ReferencePathOperation::create(std::optional<Path>&& path)
{
    return adoptRef(*new ReferencePathOperation(WTFMove(path)));
}

ReferencePathOperation::ReferencePathOperation(const String& url, const AtomString& fragment, const RefPtr<SVGElement> element)
    : PathOperation(Reference)
    , m_url(url)
    , m_fragment(fragment)
    , m_element(element)
{
    if (is<SVGPathElement>(m_element) || is<SVGGeometryElement>(m_element))
        m_path = pathFromGraphicsElement(m_element.get());
}

ReferencePathOperation::ReferencePathOperation(std::optional<Path>&& path)
    : PathOperation(Reference)
    , m_path(WTFMove(path))
{
}

const SVGElement* ReferencePathOperation::element() const
{
    return m_element.get();
}

Ref<RayPathOperation> RayPathOperation::create(float angle, Size size, bool isContaining, FloatRect&& containingBlockBoundingRect, FloatPoint&& position)
{
    return adoptRef(*new RayPathOperation(angle, size, isContaining, WTFMove(containingBlockBoundingRect), WTFMove(position)));
}

double RayPathOperation::lengthForPath() const
{
    auto boundingBox = m_containingBlockBoundingRect;
    auto distances = distanceOfPointToSidesOfRect(boundingBox, m_position);
    
    switch (m_size) {
    case Size::ClosestSide:
        return std::min( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
    case Size::FarthestSide:
        return std::max( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
    case Size::FarthestCorner:
        return std::sqrt(std::pow(std::max(distances.left(), distances.right()), 2) + std::pow(std::max(distances.top(), distances.bottom()), 2));
    case Size::ClosestCorner:
        return std::sqrt(std::pow(std::min(distances.left(), distances.right()), 2) + std::pow(std::min(distances.top(), distances.bottom()), 2));
    case Size::Sides:
        return lengthOfRayIntersectionWithBoundingBox(boundingBox, std::make_pair(m_position, m_angle));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

double RayPathOperation::lengthForContainPath(const FloatRect& elementRect, double computedPathLength, const FloatPoint& anchor, const OffsetRotation rotation) const
{
    // Construct vertices of element for determining if they are within the path length
    // Anchor point as origin
    auto vertices = verticesForBox(elementRect, anchor);
    
    // Rotate vertices depending on offset rotate or angle
    if (!rotation.hasAuto()) {
        auto deg = toRelatedAcuteAngle(toPositiveAngle(m_angle - rotation.angle()));
        auto angle = deg2rad(deg);
        std::for_each(vertices.begin(), vertices.end(), [angle] (FloatPoint& p) {
            p.rotate(angle);
        });
    }
    
    Vector<std::pair<double, double>, 4> bounds;
    
    for (const auto& p : vertices) {
        // Use equation for circle (offset distance + x)^2 + y^2 <= r^2 to find offset distance that satisfies equation
        // If no solution for above equation, must minimally increase it, otherwise clamp such that
        // every point is within path
        double discriminant = computedPathLength * computedPathLength - p.y() * p.y();
        if (discriminant < 0) {
            // Need to minimally increase path length
            break;
        }
        bounds.append(std::make_pair(-p.x() - std::sqrt(discriminant), -p.x() + std::sqrt(discriminant)));
    }
    
    if (vertices.size() == bounds.size()) {
        auto lowerBound = std::max_element(bounds.begin(), bounds.end(),
            [] (std::pair<double, double> const lhs, std::pair<double, double> const rhs) {
            return lhs.first < rhs.first;
        })->first;
        
        auto upperBound = std::min_element(bounds.begin(), bounds.end(),
            [] (std::pair<double, double> const lhs, std::pair<double, double> const rhs) {
            return lhs.second < rhs.second;
        })->second;
        
        if (lowerBound <= upperBound)
            return std::max(lowerBound, std::min(upperBound, computedPathLength));
    }
    
    // TODO: Implement minimally increasing path length to allow all vertices to be within such a path length
    return computedPathLength;
}

const std::optional<Path> RayPathOperation::getPath(const FloatRect& referenceRect, FloatPoint anchor, OffsetRotation rotation) const
{
    Path path;
    if (m_containingBlockBoundingRect.isZero())
        return std::nullopt;
    double length = lengthForPath();
    if (m_isContaining)
        length = lengthForContainPath(referenceRect, length, anchor, rotation);
    auto radians = deg2rad(toPositiveAngle(m_angle) - 90.0);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);
    path.addLineTo(point);
    return path;
}

} // namespace WebCore
