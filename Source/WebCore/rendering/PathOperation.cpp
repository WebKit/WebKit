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
{
    if (is<SVGPathElement>(element) || is<SVGGeometryElement>(element))
        m_path = pathFromGraphicsElement(*element);
}

ReferencePathOperation::ReferencePathOperation(std::optional<Path>&& path)
    : PathOperation(Reference)
    , m_path(WTFMove(path))
{
}

Ref<RayPathOperation> RayPathOperation::create(float angle, Size size, bool isContaining, LengthPoint&& position, CSSBoxType referenceBox)
{
    return adoptRef(*new RayPathOperation(angle, size, isContaining, WTFMove(position), referenceBox));
}

Ref<PathOperation> RayPathOperation::clone() const
{
    auto position = m_position;
    return adoptRef(*new RayPathOperation(m_angle, m_size, m_isContaining, WTFMove(position), m_referenceBox));
}

bool RayPathOperation::canBlend(const PathOperation& to) const
{
    if (auto* toRayPathOperation = dynamicDowncast<RayPathOperation>(to))
        return m_size == toRayPathOperation->size() && m_isContaining == toRayPathOperation->isContaining() && m_referenceBox == toRayPathOperation->referenceBox();
    return false;
}

RefPtr<PathOperation> RayPathOperation::blend(const PathOperation* to, const BlendingContext& context) const
{
    auto& toRayPathOperation = downcast<RayPathOperation>(*to);
    return RayPathOperation::create(WebCore::blend(m_angle, toRayPathOperation.angle(), context), m_size, m_isContaining, WebCore::blend(m_position, toRayPathOperation.position(), context), m_referenceBox);
}

const std::optional<Path> RayPathOperation::getPath(const TransformOperationData& data) const
{
    return MotionPath::computePathForRay(*this, data);
}

} // namespace WebCore
