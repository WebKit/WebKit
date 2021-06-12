/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

    TransformState(TransformDirection mappingDirection, const FloatPoint& p, const FloatQuad& quad)
        : m_lastPlanarPoint(p)
        , m_lastPlanarQuad(quad)
        , m_accumulatingTransform(false)
        , m_mapPoint(true)
        , m_mapQuad(true)
        , m_direction(mappingDirection)
    {
    }
    
    TransformState(TransformDirection mappingDirection, const FloatPoint& p)
        : m_lastPlanarPoint(p)
        , m_accumulatingTransform(false)
        , m_mapPoint(true)
        , m_mapQuad(false)
        , m_direction(mappingDirection)
    {
    }
    
    TransformState(TransformDirection mappingDirection, const FloatQuad& quad)
        : m_lastPlanarQuad(quad)
        , m_accumulatingTransform(false)
        , m_mapPoint(false)
        , m_mapQuad(true)
        , m_direction(mappingDirection)
    {
    }
    
    TransformState(const TransformState& other) { *this = other; }

    TransformState& operator=(const TransformState&);

    void setQuad(const FloatQuad& quad)
    {
        // We must be in a flattened state (no accumulated offset) when setting this quad.
        ASSERT(m_accumulatedOffset == LayoutSize());
        m_lastPlanarQuad = quad;
    }

    void setSecondaryQuad(const std::optional<FloatQuad>& quad)
    {
        // We must be in a flattened state (no accumulated offset) when setting this secondary quad.
        ASSERT(m_accumulatedOffset == LayoutSize());
        m_lastPlanarSecondaryQuad = quad;
    }

    void setLastPlanarSecondaryQuad(const std::optional<FloatQuad>&);

    void move(LayoutUnit x, LayoutUnit y, TransformAccumulation accumulate = FlattenTransform)
    {
        move(LayoutSize(x, y), accumulate);
    }

    void move(const LayoutSize&, TransformAccumulation = FlattenTransform);
    void applyTransform(const AffineTransform& transformFromContainer, TransformAccumulation = FlattenTransform, bool* wasClamped = nullptr);
    void applyTransform(const TransformationMatrix& transformFromContainer, TransformAccumulation = FlattenTransform, bool* wasClamped = nullptr);
    void flatten(bool* wasClamped = nullptr);

    // Return the coords of the point or quad in the last flattened layer
    FloatPoint lastPlanarPoint() const { return m_lastPlanarPoint; }
    FloatQuad lastPlanarQuad() const { return m_lastPlanarQuad; }
    std::optional<FloatQuad> lastPlanarSecondaryQuad() const { return m_lastPlanarSecondaryQuad; }
    bool isMappingSecondaryQuad() const { return m_lastPlanarSecondaryQuad.has_value(); }

    // Return the point or quad mapped through the current transform
    FloatPoint mappedPoint(bool* wasClamped = nullptr) const;
    FloatQuad mappedQuad(bool* wasClamped = nullptr) const;
    std::optional<FloatQuad> mappedSecondaryQuad(bool* wasClamped = nullptr) const;

    TransformationMatrix* accumulatedTransform() const { return m_accumulatedTransform.get(); }

private:
    void translateTransform(const LayoutSize&);
    void translateMappedCoordinates(const LayoutSize&);
    void flattenWithTransform(const TransformationMatrix&, bool* wasClamped);
    void applyAccumulatedOffset();
    
    TransformDirection direction() const { return m_direction; }
    TransformDirection inverseDirection() const;

    void mapQuad(FloatQuad&, TransformDirection, bool* clamped = nullptr) const;
    
    FloatPoint m_lastPlanarPoint;
    FloatQuad m_lastPlanarQuad;
    std::optional<FloatQuad> m_lastPlanarSecondaryQuad;

    // We only allocate the transform if we need to
    std::unique_ptr<TransformationMatrix> m_accumulatedTransform;
    LayoutSize m_accumulatedOffset;
    bool m_accumulatingTransform;
    bool m_mapPoint;
    bool m_mapQuad;
    TransformDirection m_direction;
};

inline TransformState::TransformDirection TransformState::inverseDirection() const
{
    return m_direction == ApplyTransformDirection ? UnapplyInverseTransformDirection : ApplyTransformDirection;
}

WTF::TextStream& operator<<(WTF::TextStream&, const TransformState&);

} // namespace WebCore
