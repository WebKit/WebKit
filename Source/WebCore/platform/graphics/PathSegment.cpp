/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PathSegment.h"

#include "PathElement.h"
#include "PathImpl.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

PathSegment::PathSegment(Data&& data)
    : m_data(WTFMove(data))
{
}

FloatPoint PathSegment::calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const
{
    return WTF::switchOn(m_data, [&](auto& data) {
        return data.calculateEndPoint(currentPoint, lastMoveToPoint);
    });
}

std::optional<FloatPoint> PathSegment::tryGetEndPointWithoutContext() const
{
    return WTF::switchOn(m_data, [&](auto& data) {
        return data.tryGetEndPointWithoutContext();
    });
}

void PathSegment::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    WTF::switchOn(m_data, [&](auto& data) {
        data.extendFastBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
    });
}

void PathSegment::extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    WTF::switchOn(m_data, [&](auto& data) {
        data.extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
    });
}

bool PathSegment::canApplyElements() const
{
    return WTF::switchOn(m_data, [&](auto& data) {
        return data.canApplyElements;
    });
}

bool PathSegment::applyElements(const PathElementApplier& applier) const
{
    return WTF::switchOn(m_data, [&](auto& data) -> bool {
        if constexpr (std::decay_t<decltype(data)>::canApplyElements) {
            data.applyElements(applier);
            return true;
        }
        return false;
    });
}

bool PathSegment::canTransform() const
{
    return WTF::switchOn(m_data, [&](auto& data) {
        return data.canTransform;
    });
}

bool PathSegment::transform(const AffineTransform& transform)
{
    return WTF::switchOn(m_data, [&](auto& data) {
        if constexpr (std::decay_t<decltype(data)>::canTransform) {
            data.transform(transform);
            return true;
        }
        return false;
    });
}

TextStream& operator<<(TextStream& ts, const PathSegment& segment)
{
    return WTF::switchOn(segment.data(), [&](auto& data) -> TextStream& {
        return ts << data;
    });
}

} // namespace WebCore
