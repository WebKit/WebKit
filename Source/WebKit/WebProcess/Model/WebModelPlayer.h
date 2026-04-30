/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS_MODEL)

#include "ModelTypes.h"
#include <WebCore/Color.h>
#include <WebCore/Model.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerAnimationState.h>
#include <WebCore/ModelPlayerClient.h>
#include <WebCore/PlatformDynamicRangeLimit.h>
#include <WebCore/StageModeOperations.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>

OBJC_CLASS WKBridgeModelLoader;
OBJC_CLASS WKStageModeOrbitSimulator;

namespace WebModel {
struct ImageAsset;
}

namespace WebCore {
class GraphicsLayerContentsDisplayDelegate;
class ModelPlayerClient;
class Page;
}

namespace WebKit {
class Mesh;
class ModelDisplayBufferDisplayDelegate;
}

namespace WebKit {

class WebModelPlayer final : public WebCore::ModelPlayer {
public:
    static Ref<WebModelPlayer> create(WebCore::Page&, WebCore::ModelPlayerClient&);
    virtual ~WebModelPlayer();

    WebCore::ModelPlayerIdentifier identifier() const final;
    bool isPlaceholder() const final;
    void scheduleUpdateIfNeeded();

private:
    WebModelPlayer(WebCore::Page&, WebCore::ModelPlayerClient&);

    void updateScene();

    // ModelPlayer finals.
    void load(WebCore::Model&, WebCore::LayoutSize) final;
    void sizeDidChange(WebCore::LayoutSize) final;
    void configureGraphicsLayer(WebCore::GraphicsLayer&, WebCore::ModelPlayerGraphicsLayerConfiguration&&) final;
    void enterFullscreen() final;
    void handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime) final;
    void getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&&) final;
    void setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&&) final;
    void isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&) final;
    void isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&) final;
    void animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&) final;
    WebCore::ModelPlayerAccessibilityChildren accessibilityChildren() final;
#if PLATFORM(COCOA)
    std::optional<WebCore::TransformationMatrix> entityTransform() const final;
#endif
    void setEntityTransform(WebCore::TransformationMatrix) final;
    bool supportsTransform(WebCore::TransformationMatrix) final;
    bool supportsMouseInteraction() final;
    void visibilityStateDidChange() final;
    void reload(WebCore::Model&, WebCore::LayoutSize, WebCore::ModelPlayerAnimationState&, std::unique_ptr<WebCore::ModelPlayerTransformState>&&) final;
    std::optional<WebCore::ModelPlayerAnimationState> currentAnimationState() const final;
    std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> currentTransformState() const final;

    const MachSendRight* displayBuffer() const;
    WebCore::GraphicsLayerContentsDisplayDelegate* contentsDisplayDelegate();

    void setPlaybackRate(double, CompletionHandler<void(double effectivePlaybackRate)>&&) final;
    void setAutoplay(bool) final;
    void setLoop(bool) final;
    void setPaused(bool, CompletionHandler<void(bool succeeded)>&&) final;
    bool paused() const final;
    Seconds currentTime() const final;
    void setCurrentTime(Seconds, CompletionHandler<void()>&&) final;
    void play(bool);
    bool simulate(float elapsedTime);
    double duration() const final;

    void ensureOnMainThreadWithProtectedThis(Function<void(Ref<WebModelPlayer>)>&& task);
    void startUpdateLoopIfNeeded();
    void update();
    bool render();
    void scheduleDisplayUpdate();

    void setStageMode(WebCore::StageModeOperation) final;
    void notifyEntityTransformUpdated();
    void setEnvironmentMap(Ref<WebCore::SharedBuffer>&&) final;

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    void setDynamicRangeLimit(WebCore::PlatformDynamicRangeLimit, float currentEDRHeadroom, bool suppressEDR) final;
    std::optional<double> getEffectiveDynamicRangeLimitValue() const final;
    float computeContentsHeadroom();
    void updateContentsHeadroom();
#endif

    WeakPtr<WebCore::ModelPlayerClient> m_client;

    WebCore::ModelPlayerIdentifier m_id;
    RetainPtr<WKBridgeModelLoader> m_modelLoader;
    Vector<MachSendRight> m_displayBuffers;
    RefPtr<WebKit::Mesh> m_currentModel;
    RetainPtr<NSData> m_retainedData;
    WeakRef<WebCore::Page> m_page;
    mutable RefPtr<ModelDisplayBufferDisplayDelegate> m_contentsDisplayDelegate;
    WeakPtr<WebCore::GraphicsLayer> m_graphicsLayer;
    uint32_t m_renderTextureIndex { 0 };
    uint32_t m_displayTextureIndex { 0 };
    WebCore::StageModeOperation m_stageMode { WebCore::StageModeOperation::None };
    std::optional<WebCore::Color> m_backgroundColor;
    WebCore::IntSize m_currentPixelSize;
    bool m_didFinishLoading { false };
    enum class PauseState {
        None,
        Playing,
        Paused
    };
    PauseState m_pauseState { PauseState::None };
    std::optional<WebCore::LayoutPoint> m_initialPoint;
    std::optional<Ref<WebCore::SharedBuffer>> m_environmentMap;
    RetainPtr<WKStageModeOrbitSimulator> m_orbitSimulator;
    MonotonicTime m_lastUpdateTime;
    std::optional<WebCore::ModelPlayerAnimationState> m_cachedAnimationState;
    std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> m_cachedTransformState;
    float m_playbackRate { 1.0f };
    bool m_isLooping { false };

    bool m_isUpdateLoopRunning { false };
    bool m_isUpdateScheduled { false };
    bool m_isUpdating { false };
    bool m_needsEntityTransformNotification { false };

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    float m_currentEDRHeadroom { 1.f };
    bool m_suppressEDR { false };
    WebCore::PlatformDynamicRangeLimit m_dynamicRangeLimit { WebCore::PlatformDynamicRangeLimit::initialValue() };
#endif
};

}

#endif
