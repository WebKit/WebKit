/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS)

#import "ModelIdentifier.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <WebCore/ModelPlayer.h>
#import <WebCore/ModelPlayerAnimationState.h>
#import <WebCore/ModelPlayerClient.h>
#import <WebCore/ModelPlayerIdentifier.h>
#import <WebCore/StageModeOperations.h>
#import <wtf/Compiler.h>

namespace WebKit {

class ModelProcessModelPlayer
    : public WebCore::ModelPlayer
    , public IPC::MessageReceiver {
public:
    static Ref<ModelProcessModelPlayer> create(WebCore::ModelPlayerIdentifier, WebPage&, WebCore::ModelPlayerClient&);
    virtual ~ModelProcessModelPlayer();

    void ref() const final { WebCore::ModelPlayer::ref(); }
    void deref() const final { WebCore::ModelPlayer::deref(); }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier() { return m_layerHostingContextIdentifier; };
    void didUnload();

    void disableUnloadDelayForTesting();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return WebProcess::singleton().sharedPreferencesForWebProcess(); }
    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcessValue() const { return WebProcess::singleton().sharedPreferencesForWebProcessValue(); }

private:
    explicit ModelProcessModelPlayer(WebCore::ModelPlayerIdentifier, WebPage&, WebCore::ModelPlayerClient&);

    WebPage* page() { return m_page.get(); }
    WebCore::ModelPlayerClient* client() { return m_client.get(); }

    template<typename T> void send(T&& message);
    template<typename T, typename C> void sendWithAsyncReply(T&& message, C&& completionHandler);

    bool modelProcessEnabled() const;

    // Messages
    void didCreateLayer(WebCore::LayerHostingContextIdentifier);
    void didFinishLoading(const WebCore::FloatPoint3D&, const WebCore::FloatPoint3D&);
    void didFailLoading();
    void didUpdateEntityTransform(const WebCore::TransformationMatrix&);
    void didUpdateAnimationPlaybackState(bool isPaused, double playbackRate, Seconds duration, Seconds currentTime, MonotonicTime clockTimestamp);
    void didFinishEnvironmentMapLoading(bool succeeded);

    // WebCore::ModelPlayer overrides.
    WebCore::ModelPlayerIdentifier identifier() const final { return m_id; }
    std::optional<WebCore::ModelPlayerAnimationState> currentAnimationState() const final;
    std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> currentTransformState() const final;
    void load(WebCore::Model&, WebCore::LayoutSize) final;
    void reload(WebCore::Model&, WebCore::LayoutSize, WebCore::ModelPlayerAnimationState&, std::unique_ptr<WebCore::ModelPlayerTransformState>&&) final;
    void visibilityStateDidChange() final;
    void sizeDidChange(WebCore::LayoutSize) final;
    void configureGraphicsLayer(WebCore::GraphicsLayer&, WebCore::ModelPlayerGraphicsLayerConfiguration&&) final;
    void handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime) final;
    std::optional<WebCore::FloatPoint3D> boundingBoxCenter() const final;
    std::optional<WebCore::FloatPoint3D> boundingBoxExtents() const final;
    std::optional<WebCore::TransformationMatrix> entityTransform() const final;
    void setEntityTransform(WebCore::TransformationMatrix) final;
    bool supportsTransform(WebCore::TransformationMatrix) final;
    void enterFullscreen() final;
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
    void setAutoplay(bool) final;
    void setLoop(bool) final;
    void setPlaybackRate(double, CompletionHandler<void(double effectivePlaybackRate)>&&) final;
    double duration() const final;
    bool paused() const final;
    void setPaused(bool, CompletionHandler<void(bool succeeded)>&&) final;
    Seconds currentTime() const final;
    void setCurrentTime(Seconds, CompletionHandler<void()>&&) final;
    void setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data) final;
    void setHasPortal(bool) final;
    void setStageMode(WebCore::StageModeOperation) final;
    void beginStageModeTransform(const WebCore::TransformationMatrix&) final;
    void updateStageModeTransform(const WebCore::TransformationMatrix&) final;
    void endStageModeInteraction() final;
    void animateModelToFitPortal(CompletionHandler<void(bool)>&&) final;
    void resetModelTransformAfterDrag() final;

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    void ensureImmersivePresentation(CompletionHandler<void(std::optional<WebCore::LayerHostingContextIdentifier>)>&&) final;
    void exitImmersivePresentation(CompletionHandler<void()>&&) final;
#endif

    WebCore::ModelPlayerIdentifier m_id;
    WeakPtr<WebPage> m_page;
    WeakPtr<WebCore::ModelPlayerClient> m_client;

    std::optional<WebCore::LayerHostingContextIdentifier> m_layerHostingContextIdentifier;

    std::optional<WebCore::TransformationMatrix> m_entityTransform;
    std::optional<WebCore::FloatPoint3D> m_boundingBoxCenter;
    std::optional<WebCore::FloatPoint3D> m_boundingBoxExtents;
    bool m_hasPortal { true };
    WebCore::StageModeOperation m_stageModeOperation { WebCore::StageModeOperation::None };
    double m_requestedPlaybackRate { 1.0 };
    std::optional<Seconds> m_pendingCurrentTime;
    std::optional<MonotonicTime> m_clockTimestampOfLastCurrentTimeSet;
    WebCore::ModelPlayerAnimationState m_animationState;
    SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
};

}

#endif // ENABLE(MODEL_PROCESS)
