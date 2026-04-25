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

#include "config.h"
#include "TransformState.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TransformState& TransformState::operator=(const TransformState& other)
{
    m_accumulatedOffset = other.m_accumulatedOffset;
    m_tracking = other.m_tracking;
    m_inputPoint = other.m_inputPoint;
    m_inputQuad = other.m_inputQuad;
    m_inputSecondaryQuad = other.m_inputSecondaryQuad;
    m_direction = other.m_direction;
    
    m_accumulatedTransform = nullptr;

    if (other.m_accumulatedTransform)
        m_accumulatedTransform = makeUnique<TransformationMatrix>(*other.m_accumulatedTransform);

    return *this;
}

void TransformState::translateTransform(const LayoutSize& offset, TransformDirection direction)
{
    if (direction == ApplyTransformDirection)
        m_accumulatedTransform->translateRight(offset.width(), offset.height());
    else
        m_accumulatedTransform->translate(offset.width(), offset.height());
}

void TransformState::move(const LayoutSize& offset, TransformAccumulation accumulate)
{
    if (shouldFlattenBefore(accumulate))
        flatten();

    if (m_accumulatedTransform) {
        // If we're accumulating into an existing transform, apply the translation.
        translateTransform(offset, m_direction);
    } else
        m_accumulatedOffset += offset;

    if (shouldFlattenAfter(accumulate))
        flatten();
}

bool TransformState::shouldFlattenBefore(TransformAccumulation accumulate)
{
    return accumulate == FlattenTransform && m_direction != ApplyTransformDirection;
}

bool TransformState::shouldFlattenAfter(TransformAccumulation accumulate)
{
    return accumulate == FlattenTransform && m_direction == ApplyTransformDirection;
}

// FIXME: We transform AffineTransform to TransformationMatrix. This is rather inefficient.
void TransformState::applyTransform(const AffineTransform& transformFromContainer, TransformAccumulation accumulate, bool* wasClamped)
{
    applyTransform(transformFromContainer.toTransformationMatrix(), accumulate, wasClamped);
}

void TransformState::applyTransform(const TransformationMatrix& transformFromContainer, TransformAccumulation accumulate, bool* wasClamped)
{
    if (wasClamped)
        *wasClamped = false;

    if (transformFromContainer.isIntegerTranslation()) {
        move(LayoutSize(transformFromContainer.e(), transformFromContainer.f()), accumulate);
        return;
    }

    if (shouldFlattenBefore(accumulate))
        flatten();

    if (!m_accumulatedTransform) {
        m_accumulatedTransform = makeUnique<TransformationMatrix>(m_accumulatedOffset.width(), m_accumulatedOffset.height());
        m_accumulatedOffset = { };
    }
    ASSERT(m_accumulatedOffset.isZero());

    if (m_direction == ApplyTransformDirection)
        *m_accumulatedTransform = transformFromContainer * *m_accumulatedTransform;
    else
        *m_accumulatedTransform *= transformFromContainer;


    if (shouldFlattenAfter(accumulate))
        flatten();
}

void TransformState::flatten()
{
    if (m_accumulatedTransform)
        m_accumulatedTransform->flatten();
}

FloatPoint TransformState::mappedPoint() const
{
    FloatPoint point = m_inputPoint;
    point.move((m_direction == ApplyTransformDirection) ? m_accumulatedOffset : -m_accumulatedOffset);
    if (!m_accumulatedTransform)
        return point;

    if (m_direction == ApplyTransformDirection)
        return m_accumulatedTransform->mapPoint(point);

    return valueOrDefault(m_accumulatedTransform->inverse()).projectPoint(point);
}

FloatQuad TransformState::mappedQuad(bool* wasClamped) const
{
    if (wasClamped)
        *wasClamped = false;

    FloatQuad quad = m_inputQuad;
    mapQuad(quad, m_direction, wasClamped);
    return quad;
}

std::optional<FloatQuad> TransformState::mappedSecondaryQuad(bool* wasClamped) const
{
    if (wasClamped)
        *wasClamped = false;

    if (!m_inputSecondaryQuad)
        return std::nullopt;

    FloatQuad quad = *m_inputSecondaryQuad;
    mapQuad(quad, m_direction, wasClamped);
    return quad;
}

void TransformState::reset(const FloatQuad& quad, const std::optional<FloatQuad>& secondaryQuad)
{
    m_inputQuad = quad;
    m_inputSecondaryQuad = secondaryQuad;
    m_accumulatedTransform = nullptr;
    m_accumulatedOffset = { };

}

void TransformState::setSecondaryQuadInMappedSpace(const std::optional<FloatQuad>& quad)
{
    if (!quad) {
        m_inputSecondaryQuad = std::nullopt;
        return;
    }
    
    // Map the quad back through any transform or offset back into the last flattening coordinate space.
    FloatQuad backMappedQuad(*quad);
    mapQuad(backMappedQuad, inverseDirection());
    m_inputSecondaryQuad = backMappedQuad;
}

void TransformState::mapQuad(FloatQuad& quad, TransformDirection direction, bool* wasClamped) const
{
    quad.move((direction == ApplyTransformDirection) ? m_accumulatedOffset : -m_accumulatedOffset);
    if (!m_accumulatedTransform)
        return;

    if (direction == ApplyTransformDirection) {
        quad = m_accumulatedTransform->mapQuad(quad);
        return;
    }

    quad = valueOrDefault(m_accumulatedTransform->inverse()).projectQuad(quad, wasClamped);
}

std::unique_ptr<TransformationMatrix> TransformState::releaseTrackedTransform()
{
    if (m_accumulatedTransform)
        return makeUnique<TransformationMatrix>(*m_accumulatedTransform);
    else
        return makeUnique<TransformationMatrix>(m_accumulatedOffset.width(), m_accumulatedOffset.height());
}

TextStream& operator<<(TextStream& ts, const TransformState& state)
{
    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    multilineStream.dumpProperty("mapped point"_s, state.mappedPoint());
    multilineStream.dumpProperty("mapped quad"_s, state.mappedQuad());

    if (state.mappedSecondaryQuad())
        multilineStream.dumpProperty("mapped secondary quad"_s, *state.mappedSecondaryQuad());

    if (state.accumulatedTransform())
        multilineStream.dumpProperty("accumulated transform"_s, ValueOrNull(state.accumulatedTransform()));
    else
        multilineStream.dumpProperty("accumulated offset"_s, state.accumulatedOffset());

    {
        TextStream::GroupScope scope(ts);
        ts << "TransformState "_s << multilineStream.release();
    }
    return ts;
}

} // namespace WebCore
