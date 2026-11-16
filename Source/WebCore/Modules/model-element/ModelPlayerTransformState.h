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

#pragma once

#include <WebCore/StageModeOperations.h>

namespace WebCore {

class FloatPoint3D;
class TransformationMatrix;

class WEBCORE_EXPORT ModelPlayerTransformState {
public:
    virtual ~ModelPlayerTransformState() = default;
    virtual std::unique_ptr<ModelPlayerTransformState> clone() const = 0;

    virtual std::optional<TransformationMatrix> entityTransform() const = 0;
    virtual void setEntityTransform(TransformationMatrix) = 0;
    virtual bool isEntityTransformSupported(const TransformationMatrix&) const = 0;

    virtual std::optional<FloatPoint3D> boundingBoxCenter() const = 0;
    virtual std::optional<FloatPoint3D> boundingBoxExtents() const = 0;

    virtual void invalidateTransform() = 0;

#if ENABLE(MODEL_ELEMENT_PORTAL)
    virtual bool hasPortal() const = 0;
    virtual void setHasPortal(bool) = 0;
#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
    virtual StageModeOperation stageMode() const = 0;
    virtual void setStageMode(StageModeOperation) = 0;
#endif
};

} // namespace WebCore
