/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ModelProcessModelPlayerTransformState.h"

#if ENABLE(MODEL_PROCESS)

#include "Logging.h"
#include <CoreRE/CoreRE.h>

namespace WebKit {

std::unique_ptr<ModelProcessModelPlayerTransformState> ModelProcessModelPlayerTransformState::create(std::optional<WebCore::TransformationMatrix> entityTransform, std::optional<WebCore::FloatPoint3D> boundingBoxCenter, std::optional<WebCore::FloatPoint3D> boundingBoxExtents, bool hasPortal, WebCore::StageModeOperation stageModeOperation)
{
    return makeUnique<ModelProcessModelPlayerTransformState>(entityTransform, boundingBoxCenter, boundingBoxExtents, hasPortal, stageModeOperation);
}

ModelProcessModelPlayerTransformState::ModelProcessModelPlayerTransformState(std::optional<WebCore::TransformationMatrix> entityTransform, std::optional<WebCore::FloatPoint3D> boundingBoxCenter, std::optional<WebCore::FloatPoint3D> boundingBoxExtents, bool hasPortal, WebCore::StageModeOperation stageModeOperation)
    : m_entityTransform(entityTransform)
    , m_boundingBoxCenter(boundingBoxCenter)
    , m_boundingBoxExtents(boundingBoxExtents)
    , m_hasPortal(hasPortal)
    , m_stageModeOperation(stageModeOperation)
{
}

std::unique_ptr<WebCore::ModelPlayerTransformState> ModelProcessModelPlayerTransformState::clone() const
{
    return makeUnique<ModelProcessModelPlayerTransformState>(m_entityTransform, m_boundingBoxCenter, m_boundingBoxExtents, m_hasPortal, m_stageModeOperation);
}

static bool areSameSignAndAlmostEqual(float a, float b, float tolerance)
{
    if (a * b < 0)
        return false;

    float absA = std::abs(a);
    float absB = std::abs(b);
    return std::abs(absA - absB) < tolerance * std::min(absA, absB);
}

bool ModelProcessModelPlayerTransformState::transformSupported(const WebCore::TransformationMatrix& transform)
{
    constexpr float tolerance = 1e-5f;

    RESRT srt = REMakeSRTFromMatrix(transform);

    // Scale must be uniform across all 3 axis
    if (!areSameSignAndAlmostEqual(simd_reduce_max(srt.scale), simd_reduce_min(srt.scale), tolerance)) {
        RELEASE_LOG_ERROR(ModelElement, "Rejecting non-uniform scaling %.05f %.05f %.05f", srt.scale[0], srt.scale[1], srt.scale[2]);
        return false;
    }

    // Matrix must be a SRT (scale/rotation/translation) matrix - no shear.
    // RESRT itself is already clean of shear, so we just need to see if the input is the same as the cleaned RESRT
    simd_float4x4 noShearMatrix = RESRTMatrix(srt);
    if (!simd_almost_equal_elements(transform, noShearMatrix, tolerance)) {
        RELEASE_LOG_ERROR(ModelElement, "Rejecting shear matrix");
        return false;
    }

    return true;
}

void ModelProcessModelPlayerTransformState::setEntityTransform(WebCore::TransformationMatrix entityTransform)
{
    ASSERT(m_stageModeOperation == WebCore::StageModeOperation::None);
    if (transformSupported(entityTransform))
        m_entityTransform = entityTransform;
}

bool ModelProcessModelPlayerTransformState::isEntityTransformSupported(const WebCore::TransformationMatrix& transform) const
{
    return transformSupported(transform);
}

void ModelProcessModelPlayerTransformState::setHasPortal(bool hasPortal)
{
    if (m_hasPortal == hasPortal)
        return;

    m_hasPortal = hasPortal;
    invalidateTransform();
}

void ModelProcessModelPlayerTransformState::setStageMode(WebCore::StageModeOperation stageModeOperation)
{
    if (m_stageModeOperation == stageModeOperation)
        return;

    m_stageModeOperation = stageModeOperation;
    invalidateTransform();
}

void ModelProcessModelPlayerTransformState::invalidateTransform()
{
    // FIXME: recalculate entity transform
    // Invalidate m_entityTransform for now so the entityTransform can be recomputed on reload.
    m_entityTransform = std::nullopt;
}

}

#endif
