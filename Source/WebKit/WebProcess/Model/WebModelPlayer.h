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
#include <WebCore/Model.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerClient.h>
#include <WebCore/StageModeOperations.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>

OBJC_CLASS WKBridgeModelLoader;

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
    void update();

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
    void hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void isMuted(CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setIsMuted(bool, CompletionHandler<void(bool success)>&&) final;
    WebCore::ModelPlayerAccessibilityChildren accessibilityChildren() final;
#if PLATFORM(COCOA)
    std::optional<WebCore::TransformationMatrix> entityTransform() const final;
#endif
    void setEntityTransform(WebCore::TransformationMatrix) final;
    bool supportsTransform(WebCore::TransformationMatrix) final;
    bool supportsMouseInteraction() final;

    const MachSendRight* displayBuffer() const;
    WebCore::GraphicsLayerContentsDisplayDelegate* contentsDisplayDelegate();

    void setPlaybackRate(double, CompletionHandler<void(double effectivePlaybackRate)>&&) final;
    void setAutoplay(bool) final;
    void setLoop(bool) final;
    void setPaused(bool, CompletionHandler<void(bool succeeded)>&&) final;
    bool paused() const final;
    void play(bool);
    void simulate(float elapsedTime);
    double duration() const final;

    void ensureOnMainThreadWithProtectedThis(Function<void(Ref<WebModelPlayer>)>&& task);
    void setStageMode(WebCore::StageModeOperation) final;
    void notifyEntityTransformUpdated();
    void setEnvironmentMap(Ref<WebCore::SharedBuffer>&&) final;

    WeakPtr<WebCore::ModelPlayerClient> m_client;

    WebCore::ModelPlayerIdentifier m_id;
    RetainPtr<WKBridgeModelLoader> m_modelLoader;
    Vector<MachSendRight> m_displayBuffers;
    RefPtr<WebKit::Mesh> m_currentModel;
    RetainPtr<NSData> m_retainedData;
    WeakRef<WebCore::Page> m_page;
    mutable RefPtr<ModelDisplayBufferDisplayDelegate> m_contentsDisplayDelegate;
    uint32_t m_currentTexture { 0 };
    WebCore::StageModeOperation m_stageMode { WebCore::StageModeOperation::None };
    float m_currentScale { 1.f };
    bool m_didFinishLoading { false };
    enum class PauseState {
        None,
        Playing,
        Paused
    };
    PauseState m_pauseState { PauseState::None };
    std::optional<WebCore::LayoutPoint> m_currentPoint;
    std::optional<Ref<WebCore::SharedBuffer>> m_environmentMap;
    float m_yawAcceleration { 0.f };
    float m_pitchAcceleration { 0.f };
    float m_yaw { 0.f };
    float m_pitch { 0.f };
    float m_playbackRate { 1.0f };
    bool m_isLooping { false };
};

}

#endif
