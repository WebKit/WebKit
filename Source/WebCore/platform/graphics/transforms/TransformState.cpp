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

#include "config.h"
#include "TransformState.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TransformState& TransformState::operator=(const TransformState& other)
{
    m_accumulatedOffset = other.m_accumulatedOffset;
    m_tracking = other.m_tracking;
    m_mapPoint = other.m_mapPoint;
    m_mapQuad = other.m_mapQuad;
    if (m_mapPoint)
        m_lastPlanarPoint = other.m_lastPlanarPoint;
    if (m_mapQuad) {
        m_lastPlanarQuad = other.m_lastPlanarQuad;
        m_lastPlanarSecondaryQuad = other.m_lastPlanarSecondaryQuad;
    }
    m_accumulatingTransform = other.m_accumulatingTransform;
    m_useCSS3DTransformInterop = other.m_useCSS3DTransformInterop;
    m_direction = other.m_direction;
    
    m_accumulatedTransform = nullptr;

    if (other.m_accumulatedTransform)
        m_accumulatedTransform = makeUnique<TransformationMatrix>(*other.m_accumulatedTransform);
        
    m_trackedTransform = nullptr;
    if (other.m_trackedTransform)
        m_trackedTransform = makeUnique<TransformationMatrix>(*other.m_trackedTransform);

    return *this;
}

void TransformState::translateTransform(const LayoutSize& offset)
{
    if (m_direction == ApplyTransformDirection)
        m_accumulatedTransform->translateRight(offset.width(), offset.height());
    else
        m_accumulatedTransform->translate(offset.width(), offset.height());
}

void TransformState::translateMappedCoordinates(const LayoutSize& offset)
{
    LayoutSize adjustedOffset = (m_direction == ApplyTransformDirection) ? offset : -offset;
    if (m_mapPoint)
        m_lastPlanarPoint.move(adjustedOffset);
    if (m_mapQuad) {
        m_lastPlanarQuad.move(adjustedOffset);
        if (m_lastPlanarSecondaryQuad)
            m_lastPlanarSecondaryQuad->move(adjustedOffset);
    }
    if (m_tracking != DoNotTrackTransformMatrix) {
        if (!m_trackedTransform)
            m_trackedTransform = makeUnique<TransformationMatrix>();
        if (m_direction == ApplyTransformDirection)
            m_trackedTransform->translateRight(offset.width(), offset.height());
        else
            m_trackedTransform->translate(offset.width(), offset.height());
    }
}

void TransformState::move(const LayoutSize& offset, TransformAccumulation accumulate)
{
    if (shouldFlattenBefore(accumulate))
        flatten();

    if (accumulate == FlattenTransform && !m_accumulatedTransform && m_tracking == DoNotTrackTransformMatrix)
        m_accumulatedOffset += offset;
    else {
        applyAccumulatedOffset();
        if (m_accumulatingTransform && m_accumulatedTransform) {
            // If we're accumulating into an existing transform, apply the translation.
            translateTransform(offset);

            // Then flatten if necessary.
            if (shouldFlattenAfter(accumulate))
                flatten();
        } else
            // Just move the point and/or quad.
            translateMappedCoordinates(offset);
    }
}

void TransformState::applyAccumulatedOffset()
{
    LayoutSize offset = m_accumulatedOffset;
    m_accumulatedOffset = LayoutSize();
    if (!offset.isZero()) {
        if (m_accumulatedTransform) {
            translateTransform(offset);
            flatten();
        } else
            translateMappedCoordinates(offset);
    }
}

bool TransformState::shouldFlattenBefore(TransformAccumulation accumulate)
{
    if (m_useCSS3DTransformInterop)
        return accumulate == FlattenTransform && m_direction != ApplyTransformDirection;
    return false;
}

bool TransformState::shouldFlattenAfter(TransformAccumulation accumulate)
{
    if (m_useCSS3DTransformInterop)
        return accumulate == FlattenTransform && m_direction == ApplyTransformDirection;
    return accumulate == FlattenTransform;
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

    applyAccumulatedOffset();

    if (shouldFlattenBefore(accumulate))
        flatten();

    // If we have an accumulated transform from last time, multiply in this transform
    if (m_accumulatedTransform) {
        if (m_direction == ApplyTransformDirection)
            m_accumulatedTransform = makeUnique<TransformationMatrix>(transformFromContainer * *m_accumulatedTransform);
        else
            m_accumulatedTransform->multiply(transformFromContainer);
    }

    if (shouldFlattenAfter(accumulate)) {
        const TransformationMatrix* finalTransform = m_accumulatedTransform ? m_accumulatedTransform.get() : &transformFromContainer;
        flattenWithTransform(*finalTransform, wasClamped);
    } else if (!m_accumulatedTransform) {
        m_accumulatedTransform = makeUnique<TransformationMatrix>(transformFromContainer);
        m_accumulatingTransform = true;
    }
}

void TransformState::flatten(bool* wasClamped)
{
    if (wasClamped)
        *wasClamped = false;

    applyAccumulatedOffset();

    if (!m_accumulatedTransform) {
        m_accumulatingTransform = false;
        return;
    }
    
    flattenWithTransform(*m_accumulatedTransform, wasClamped);
}

FloatPoint TransformState::mappedPoint(bool* wasClamped) const
{
    if (wasClamped)
        *wasClamped = false;

    FloatPoint point = m_lastPlanarPoint;
    point.move((m_direction == ApplyTransformDirection) ? m_accumulatedOffset : -m_accumulatedOffset);
    if (!m_accumulatedTransform)
        return point;

    if (m_direction == ApplyTransformDirection)
        return m_accumulatedTransform->mapPoint(point);

    return valueOrDefault(m_accumulatedTransform->inverse()).projectPoint(point, wasClamped);
}

FloatQuad TransformState::mappedQuad(bool* wasClamped) const
{
    if (wasClamped)
        *wasClamped = false;

    FloatQuad quad = m_lastPlanarQuad;
    mapQuad(quad, m_direction, wasClamped);
    return quad;
}

std::optional<FloatQuad> TransformState::mappedSecondaryQuad(bool* wasClamped) const
{
    if (wasClamped)
        *wasClamped = false;

    if (!m_lastPlanarSecondaryQuad)
        return std::nullopt;

    FloatQuad quad = *m_lastPlanarSecondaryQuad;
    mapQuad(quad, m_direction, wasClamped);
    return quad;
}

void TransformState::setLastPlanarSecondaryQuad(const std::optional<FloatQuad>& quad)
{
    if (!quad) {
        m_lastPlanarSecondaryQuad = std::nullopt;
        return;
    }
    
    // Map the quad back through any transform or offset back into the last flattening coordinate space.
    FloatQuad backMappedQuad(*quad);
    mapQuad(backMappedQuad, inverseDirection());
    m_lastPlanarSecondaryQuad = backMappedQuad;
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

void TransformState::flattenWithTransform(const TransformationMatrix& t, bool* wasClamped)
{
    if (m_direction == ApplyTransformDirection) {
        if (m_mapPoint)
            m_lastPlanarPoint = t.mapPoint(m_lastPlanarPoint);
        if (m_mapQuad) {
            m_lastPlanarQuad = t.mapQuad(m_lastPlanarQuad);
            if (m_lastPlanarSecondaryQuad)
                m_lastPlanarSecondaryQuad = t.mapQuad(*m_lastPlanarSecondaryQuad);
        }
    } else {
        TransformationMatrix inverseTransform = valueOrDefault(t.inverse());
        if (m_mapPoint)
            m_lastPlanarPoint = inverseTransform.projectPoint(m_lastPlanarPoint);
        if (m_mapQuad) {
            m_lastPlanarQuad = inverseTransform.projectQuad(m_lastPlanarQuad, wasClamped);
            if (m_lastPlanarSecondaryQuad)
                m_lastPlanarSecondaryQuad = inverseTransform.projectQuad(*m_lastPlanarSecondaryQuad, wasClamped);
        }
    }

    if (m_trackedTransform)
        *m_trackedTransform = (m_direction == ApplyTransformDirection) ? (t * *m_trackedTransform) : (*m_trackedTransform * t);
    else if (m_tracking != DoNotTrackTransformMatrix)
        m_trackedTransform = makeUnique<TransformationMatrix>(t);

    // We could throw away m_accumulatedTransform if we wanted to here, but that
    // would cause thrash when traversing hierarchies with alternating
    // preserve-3d and flat elements.
    if (m_accumulatedTransform)
        m_accumulatedTransform->makeIdentity();
    m_accumulatingTransform = false;
}

TextStream& operator<<(TextStream& ts, const TransformState& state)
{
    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    multilineStream.dumpProperty("last planar point", state.lastPlanarPoint());
    multilineStream.dumpProperty("last planar quad", state.lastPlanarQuad());

    if (state.lastPlanarSecondaryQuad())
        multilineStream.dumpProperty("last planar secondary quad", *state.lastPlanarSecondaryQuad());

    if (state.accumulatedTransform())
        multilineStream.dumpProperty("accumulated transform", ValueOrNull(state.accumulatedTransform()));

    {
        TextStream::GroupScope scope(ts);
        ts << "TransformState " << multilineStream.release();
    }
    return ts;
}

} // namespace WebCore
