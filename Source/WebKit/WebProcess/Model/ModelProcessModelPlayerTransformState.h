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

#if ENABLE(MODEL_PROCESS)

#include <WebCore/FloatPoint3D.h>
#include <WebCore/ModelPlayerTransformState.h>
#include <WebCore/TransformationMatrix.h>

namespace WebKit {

class ModelProcessModelPlayerTransformState final : public WebCore::ModelPlayerTransformState {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ModelProcessModelPlayerTransformState);

public:
    static std::unique_ptr<ModelProcessModelPlayerTransformState> create(std::optional<WebCore::TransformationMatrix> entityTransform, std::optional<WebCore::FloatPoint3D> boundingBoxCenter, std::optional<WebCore::FloatPoint3D> boundingBoxExtents, bool hasPortal, WebCore::StageModeOperation);

    ModelProcessModelPlayerTransformState(std::optional<WebCore::TransformationMatrix> entityTransform, std::optional<WebCore::FloatPoint3D> boundingBoxCenter, std::optional<WebCore::FloatPoint3D> boundingBoxExtents, bool hasPortal, WebCore::StageModeOperation);
    virtual ~ModelProcessModelPlayerTransformState() = default;

    static bool transformSupported(const WebCore::TransformationMatrix&);

private:
    // ModelPlayerTransformState overrides
    std::unique_ptr<WebCore::ModelPlayerTransformState> clone() const final;
    std::optional<WebCore::TransformationMatrix> entityTransform() const final { return m_entityTransform; }
    void setEntityTransform(WebCore::TransformationMatrix) final;
    bool isEntityTransformSupported(const WebCore::TransformationMatrix&) const final;
    std::optional<WebCore::FloatPoint3D> boundingBoxCenter() const final { return m_boundingBoxCenter; }
    std::optional<WebCore::FloatPoint3D> boundingBoxExtents() const final { return m_boundingBoxExtents; }
    bool hasPortal() const final { return m_hasPortal; }
    void setHasPortal(bool) final;
    WebCore::StageModeOperation stageMode() const final { return m_stageModeOperation; }
    void setStageMode(WebCore::StageModeOperation) final;
    void invalidateTransform() final;

    std::optional<WebCore::TransformationMatrix> m_entityTransform;
    std::optional<WebCore::FloatPoint3D> m_boundingBoxCenter;
    std::optional<WebCore::FloatPoint3D> m_boundingBoxExtents;
    bool m_hasPortal { true };
    WebCore::StageModeOperation m_stageModeOperation { WebCore::StageModeOperation::None };
};

}

#endif
