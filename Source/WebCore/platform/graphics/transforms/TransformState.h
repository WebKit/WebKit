/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#pragma once

#include "AffineTransform.h"
#include "FloatPoint.h"
#include "FloatQuad.h"
#include "LayoutSize.h"
#include "TransformationMatrix.h"
#include <optional>

namespace WTF {
class TextStream;
}

namespace WebCore {

class TransformState {
public:
    enum TransformDirection { ApplyTransformDirection, UnapplyInverseTransformDirection };
    enum TransformAccumulation { FlattenTransform, AccumulateTransform };
    enum TransformMatrixTracking { DoNotTrackTransformMatrix, TrackSVGCTMMatrix, TrackSVGScreenCTMMatrix };

    TransformState(TransformDirection mappingDirection, const FloatPoint& p, const FloatQuad& quad)
        : m_inputPoint(p)
        , m_inputQuad(quad)
        , m_direction(mappingDirection)
    {
    }
    
    TransformState(TransformDirection mappingDirection, const FloatPoint& p)
        : m_inputPoint(p)
        , m_direction(mappingDirection)
    {
    }
    
    TransformState(TransformDirection mappingDirection, const FloatQuad& quad, std::optional<FloatQuad> secondaryQuad = std::nullopt)
        : m_inputQuad(quad)
        , m_inputSecondaryQuad(secondaryQuad)
        , m_direction(mappingDirection)
    {
    }
    
    TransformState(const TransformState& other) { *this = other; }

    WEBCORE_EXPORT TransformState& operator=(const TransformState&);

    WEBCORE_EXPORT void reset(const FloatQuad&, const std::optional<FloatQuad>& secondaryQuad = std::nullopt);

    void setSecondaryQuadInMappedSpace(const std::optional<FloatQuad>&);

    void setTransformMatrixTracking(TransformMatrixTracking tracking) { m_tracking = tracking; }
    TransformMatrixTracking transformMatrixTracking() const { return m_tracking; }

    void move(LayoutUnit x, LayoutUnit y, TransformAccumulation accumulate = FlattenTransform)
    {
        move(LayoutSize(x, y), accumulate);
    }

    void move(const LayoutSize&, TransformAccumulation = FlattenTransform);
    void applyTransform(const AffineTransform& transformFromContainer, TransformAccumulation = FlattenTransform, bool* wasClamped = nullptr);
    WEBCORE_EXPORT void applyTransform(const TransformationMatrix& transformFromContainer, TransformAccumulation = FlattenTransform, bool* wasClamped = nullptr);

    bool isMappingSecondaryQuad() const { return m_inputSecondaryQuad.has_value(); }

    // Return the point or quad mapped through the current transform
    FloatPoint mappedPoint() const;
    WEBCORE_EXPORT FloatQuad mappedQuad(bool* wasClamped = nullptr) const;
    WEBCORE_EXPORT std::optional<FloatQuad> mappedSecondaryQuad(bool* wasClamped = nullptr) const;

    LayoutSize accumulatedOffset() const { return m_accumulatedOffset; }
    TransformationMatrix* accumulatedTransform() const { return m_accumulatedTransform.get(); }
    std::unique_ptr<TransformationMatrix> releaseTrackedTransform();
    TransformDirection direction() const { return m_direction; }

    void flatten();

private:
    void translateTransform(const LayoutSize&, TransformDirection);
    void applyAccumulatedOffset();


    bool shouldFlattenBefore(TransformAccumulation accumulate = FlattenTransform);
    bool shouldFlattenAfter(TransformAccumulation accumulate = FlattenTransform);
    
    TransformDirection inverseDirection() const;

    void mapQuad(FloatQuad&, TransformDirection, bool* wasClamped = nullptr) const;

    FloatPoint m_inputPoint;
    FloatQuad m_inputQuad;
    std::optional<FloatQuad> m_inputSecondaryQuad;

    // We only allocate the transform if we need to
    std::unique_ptr<TransformationMatrix> m_accumulatedTransform;
    LayoutSize m_accumulatedOffset;
    TransformMatrixTracking m_tracking { DoNotTrackTransformMatrix };
    TransformDirection m_direction;
};

inline TransformState::TransformDirection TransformState::inverseDirection() const
{
    return m_direction == ApplyTransformDirection ? UnapplyInverseTransformDirection : ApplyTransformDirection;
}

WTF::TextStream& operator<<(WTF::TextStream&, const TransformState&);

} // namespace WebCore
