/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "ModelPlayer.h"
#include "ModelPlayerAnimationState.h"

namespace WebCore {

class PlaceholderModelPlayer final : public ModelPlayer {
public:
    static Ref<PlaceholderModelPlayer> create(bool suspended, const ModelPlayerAnimationState&, std::unique_ptr<ModelPlayerTransformState>&&);
    virtual ~PlaceholderModelPlayer();

private:
    PlaceholderModelPlayer(bool suspended, const ModelPlayerAnimationState&, std::unique_ptr<ModelPlayerTransformState>&&);

    // ModelPlayer overrides.
    ModelPlayerIdentifier identifier() const final { return m_id; }
    bool isPlaceholder() const final { return true; }
    std::optional<ModelPlayerAnimationState> currentAnimationState(NodeIdentifier) const final;
    std::optional<std::unique_ptr<ModelPlayerTransformState>> currentTransformState(NodeIdentifier) const final;
    void NODELETE load(NodeIdentifier, Model&, LayoutSize, bool) final;
    void NODELETE reload(NodeIdentifier, Model&, LayoutSize, ModelPlayerAnimationState&, std::unique_ptr<ModelPlayerTransformState>&&) final;

#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)
    std::optional<FloatPoint3D> boundingBoxCenter(NodeIdentifier) const final;
    std::optional<FloatPoint3D> boundingBoxExtents(NodeIdentifier) const final;
#endif

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)
    std::optional<TransformationMatrix> entityTransform(NodeIdentifier) const final;
    void setEntityTransform(NodeIdentifier, TransformationMatrix) final;
    bool supportsTransform(TransformationMatrix) final;
#endif

#if ENABLE(MODEL_ELEMENT_ANIMATIONS_CONTROL)
    void setAutoplay(NodeIdentifier, bool) final;
    void setLoop(NodeIdentifier, bool) final;
    void setPlaybackRate(NodeIdentifier, double playbackRate, CompletionHandler<void(double effectivePlaybackRate)>&&) final;
    double duration(NodeIdentifier) const final;
    bool paused(NodeIdentifier) const final;
    void setPaused(NodeIdentifier, bool, CompletionHandler<void(bool succeeded)>&&) final;
    Seconds currentTime(NodeIdentifier) const final;
    void setCurrentTime(NodeIdentifier, Seconds, CompletionHandler<void()>&&) final;
#endif

#if ENABLE(MODEL_ELEMENT_PORTAL)
    void setHasPortal(bool) final;
#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
    void setStageMode(WebCore::StageModeOperation) final;
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    void ensureImmersivePresentation(CompletionHandler<void(std::optional<LayerHostingContextIdentifier>)>&&) final;
    void exitImmersivePresentation(CompletionHandler<void()>&&) final;
#endif

    // Empty implementation
    void NODELETE configureGraphicsLayer(GraphicsLayer&, ModelPlayerGraphicsLayerConfiguration&&) final;
    void NODELETE sizeDidChange(LayoutSize) final;
    void NODELETE enterFullscreen() final;
    void NODELETE handleMouseDown(const LayoutPoint&, MonotonicTime) final;
    void NODELETE handleMouseMove(const LayoutPoint&, MonotonicTime) final;
    void NODELETE handleMouseUp(const LayoutPoint&, MonotonicTime) final;

    void getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&&) final;
    void setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&&) final;
    void isPlayingAnimation(NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setAnimationIsPlaying(NodeIdentifier, bool, CompletionHandler<void(bool success)>&&) final;
    void isLoopingAnimation(NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setIsLoopingAnimation(NodeIdentifier, bool, CompletionHandler<void(bool success)>&&) final;
    void animationDuration(NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void animationCurrentTime(NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void setAnimationCurrentTime(NodeIdentifier, Seconds, CompletionHandler<void(bool success)>&&) final;
#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
    ModelPlayerAccessibilityChildren accessibilityChildren() final;
#endif

    std::optional<bool> m_lastPausedStateIfSuspended;
    ModelPlayerAnimationState m_animationState;
    std::unique_ptr<ModelPlayerTransformState> m_transformState;
    ModelPlayerIdentifier m_id;
};

} // namespace WebCore
