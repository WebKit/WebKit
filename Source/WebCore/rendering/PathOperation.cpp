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

#include "AnimationUtilities.h"
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

Ref<PathOperation> ReferencePathOperation::clone() const
{
    if (auto path = this->path()) {
        auto pathCopy = *path;
        return adoptRef(*new ReferencePathOperation(WTFMove(pathCopy)));
    }
    return adoptRef(*new ReferencePathOperation(std::nullopt));
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

Ref<RayPathOperation> RayPathOperation::create(float angle, Size size, bool isContaining, LengthPoint&& startingPosition, FloatRect&& containingBlockBoundingRect, FloatPoint&& usedStartingPosition, FloatPoint&& offsetFromContainer)
{
    return adoptRef(*new RayPathOperation(angle, size, isContaining, WTFMove(startingPosition), WTFMove(containingBlockBoundingRect), WTFMove(usedStartingPosition), WTFMove(offsetFromContainer)));
}

Ref<PathOperation> RayPathOperation::clone() const
{
    auto startingPosition = m_startingPosition;
    auto containingBlockBoundingRect = m_containingBlockBoundingRect;
    auto usedStartingPosition = m_usedStartingPosition;
    auto offsetFromContainer = m_offsetFromContainer;
    return adoptRef(*new RayPathOperation(m_angle, m_size, m_isContaining, WTFMove(startingPosition), WTFMove(containingBlockBoundingRect), WTFMove(usedStartingPosition), WTFMove(offsetFromContainer)));
}

bool RayPathOperation::canBlend(const PathOperation& to) const
{
    if (auto* toRayPathOperation = dynamicDowncast<RayPathOperation>(to))
        return m_size == toRayPathOperation->size() && m_isContaining == toRayPathOperation->isContaining();
    return false;
}

RefPtr<PathOperation> RayPathOperation::blend(const PathOperation* to, const BlendingContext& context) const
{
    ASSERT(is<RayPathOperation>(to));
    auto& toRayPathOperation = downcast<RayPathOperation>(*to);
    return RayPathOperation::create(WebCore::blend(m_angle, toRayPathOperation.angle(), context), m_size, m_isContaining, WebCore::blend(m_startingPosition, toRayPathOperation.startingPosition(), context));
}

FloatPoint RayPathOperation::currentOffset() const
{
    return FloatPoint(m_usedStartingPosition - m_offsetFromContainer);
}

double RayPathOperation::lengthForPath() const
{
    auto boundingBox = m_containingBlockBoundingRect;
    auto distances = distanceOfPointToSidesOfRect(boundingBox, m_usedStartingPosition);
    
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
        return lengthOfRayIntersectionWithBoundingBox(boundingBox, std::make_pair(m_usedStartingPosition, m_angle));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

double RayPathOperation::lengthForContainPath(const FloatRect& elementRect, double computedPathLength) const
{
    return std::max(0.0, computedPathLength - (std::max(elementRect.width(), elementRect.height()) / 2));
}

const std::optional<Path> RayPathOperation::getPath(const FloatRect& referenceRect) const
{
    if (m_containingBlockBoundingRect.isZero())
        return std::nullopt;

    double length = lengthForPath();
    if (m_isContaining)
        length = lengthForContainPath(referenceRect, length);

    auto radians = deg2rad(toPositiveAngle(m_angle) - 90.0);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);

    Path path;
    path.moveTo(currentOffset());
    path.addLineTo(currentOffset() + point);
    return path;
}

} // namespace WebCore
