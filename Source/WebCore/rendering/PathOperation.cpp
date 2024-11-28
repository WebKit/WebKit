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
#include "CSSRayValue.h"
#include "SVGElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathData.h"
#include "SVGPathElement.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {

PathOperation::~PathOperation() = default;

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
    : PathOperation(Type::Reference)
    , m_url(url)
    , m_fragment(fragment)
{
    if (is<SVGPathElement>(element) || is<SVGGeometryElement>(element))
        m_path = pathFromGraphicsElement(*element);
}

ReferencePathOperation::ReferencePathOperation(std::optional<Path>&& path)
    : PathOperation(Type::Reference)
    , m_path(WTFMove(path))
{
}

// MARK: - ShapePathOperation

Ref<ShapePathOperation> ShapePathOperation::create(Style::BasicShape shape, CSSBoxType referenceBox)
{
    return adoptRef(*new ShapePathOperation(WTFMove(shape), referenceBox));
}

Ref<PathOperation> ShapePathOperation::clone() const
{
    return adoptRef(*new ShapePathOperation(m_shape, m_referenceBox));
}

bool ShapePathOperation::canBlend(const PathOperation& to) const
{
    RefPtr toOperation = dynamicDowncast<ShapePathOperation>(to);
    return toOperation && WebCore::Style::canBlend(m_shape, toOperation->m_shape);
}

RefPtr<PathOperation> ShapePathOperation::blend(const PathOperation* to, const BlendingContext& context) const
{
    Ref toShapePathOperation = downcast<ShapePathOperation>(*to);
    return ShapePathOperation::create(WebCore::Style::blend(m_shape, toShapePathOperation->m_shape, context));
}

std::optional<Path> ShapePathOperation::getPath(const TransformOperationData& data) const
{
    return MotionPath::computePathForShape(*this, data);
}

// MARK: - BoxPathOperation

Ref<BoxPathOperation> BoxPathOperation::create(CSSBoxType referenceBox)
{
    return adoptRef(*new BoxPathOperation(referenceBox));
}

Ref<PathOperation> BoxPathOperation::clone() const
{
    return adoptRef(*new BoxPathOperation(referenceBox()));
}

std::optional<Path> BoxPathOperation::getPath(const TransformOperationData& data) const
{
    return MotionPath::computePathForBox(*this, data);
}

// MARK: - RayPathOperation

Ref<RayPathOperation> RayPathOperation::create(Style::RayFunction ray, CSSBoxType referenceBox)
{
    return adoptRef(*new RayPathOperation(WTFMove(ray), referenceBox));
}

Ref<PathOperation> RayPathOperation::clone() const
{
    return adoptRef(*new RayPathOperation(m_ray, m_referenceBox));
}

bool RayPathOperation::canBlend(const PathOperation& to) const
{
    RefPtr toRayPathOperation = dynamicDowncast<RayPathOperation>(to);
    return toRayPathOperation && Style::canBlend(m_ray, toRayPathOperation->m_ray) && m_referenceBox == toRayPathOperation->referenceBox();
}

RefPtr<PathOperation> RayPathOperation::blend(const PathOperation* to, const BlendingContext& context) const
{
    Ref toRayPathOperation = downcast<RayPathOperation>(*to);
    return RayPathOperation::create(Style::blend(m_ray, toRayPathOperation->m_ray, context), m_referenceBox);
}

std::optional<Path> RayPathOperation::getPath(const TransformOperationData& data) const
{
    return MotionPath::computePathForRay(*this, data);
}

} // namespace WebCore
