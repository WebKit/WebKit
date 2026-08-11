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

#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <WebCore/ModelPlayer.h>
#import <WebCore/ModelPlayerAnimationState.h>
#import <WebCore/ModelPlayerClient.h>
#import <WebCore/ModelPlayerIdentifier.h>
#import <WebCore/NodeIdentifier.h>
#import <WebCore/StageModeOperations.h>
#import <wtf/Compiler.h>
#import <wtf/HashMap.h>

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

    struct NodeAnimationState {
        WebCore::ModelPlayerAnimationState playbackState;
        std::optional<Seconds> pendingCurrentTime;
        std::optional<MonotonicTime> clockTimestampOfLastCurrentTimeSet;
    };
    NodeAnimationState& ensureAnimationState(WebCore::NodeIdentifier);
    NodeAnimationState* animationStateIfExists(WebCore::NodeIdentifier);
    const NodeAnimationState* animationStateIfExists(WebCore::NodeIdentifier) const;

    // Messages
    void didCreateLayer(WebCore::LayerHostingContextIdentifier);
    void didFinishLoading(WebCore::NodeIdentifier, const WebCore::FloatPoint3D&, const WebCore::FloatPoint3D&);
    void didConvertModelData(Ref<WebCore::SharedBuffer>&&, const String& convertedMIMEType);
    void didFailLoading(WebCore::NodeIdentifier);
    void didUpdateEntityTransform(WebCore::NodeIdentifier, const WebCore::TransformationMatrix&);
#if ENABLE(SPATIAL_PORTAL)
    void didUpdatePortalTransform(const WebCore::TransformationMatrix&);
#endif
    void didUpdateAnimationPlaybackState(WebCore::NodeIdentifier, bool isPaused, double playbackRate, Seconds duration, Seconds currentTime, MonotonicTime clockTimestamp);
    void didFinishEnvironmentMapLoading(bool succeeded);

    // WebCore::ModelPlayer overrides.
    WebCore::ModelPlayerIdentifier identifier() const final { return m_id; }
    std::optional<WebCore::ModelPlayerAnimationState> currentAnimationState(WebCore::NodeIdentifier) const final;
    std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> currentTransformState(WebCore::NodeIdentifier) const final;
    void load(WebCore::NodeIdentifier, WebCore::Model&, WebCore::LayoutSize, bool) final;
    void unload(WebCore::NodeIdentifier) final;
    void reload(WebCore::NodeIdentifier, WebCore::Model&, WebCore::LayoutSize, WebCore::ModelPlayerAnimationState&, std::unique_ptr<WebCore::ModelPlayerTransformState>&&) final;
    void visibilityStateDidChange() final;
    void sizeDidChange(WebCore::LayoutSize) final;
    void configureGraphicsLayer(WebCore::GraphicsLayer&, WebCore::ModelPlayerGraphicsLayerConfiguration&&) final;
    void handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime) final;
    std::optional<WebCore::FloatPoint3D> boundingBoxCenter(WebCore::NodeIdentifier) const final;
    std::optional<WebCore::FloatPoint3D> boundingBoxExtents(WebCore::NodeIdentifier) const final;
    std::optional<WebCore::TransformationMatrix> entityTransform(WebCore::NodeIdentifier) const final;
    void setEntityTransform(WebCore::NodeIdentifier, WebCore::TransformationMatrix) final;
    bool supportsTransform(WebCore::TransformationMatrix) final;
    void enterFullscreen() final;
    void getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&&) final;
    void setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&&) final;
    void isPlayingAnimation(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setAnimationIsPlaying(WebCore::NodeIdentifier, bool, CompletionHandler<void(bool success)>&&) final;
    void isLoopingAnimation(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void setIsLoopingAnimation(WebCore::NodeIdentifier, bool, CompletionHandler<void(bool success)>&&) final;
    void animationDuration(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void animationCurrentTime(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&&) final;
    void setAnimationCurrentTime(WebCore::NodeIdentifier, Seconds, CompletionHandler<void(bool success)>&&) final;
    WebCore::ModelPlayerAccessibilityChildren accessibilityChildren() final;
    void setAutoplay(WebCore::NodeIdentifier, bool) final;
    void setLoop(WebCore::NodeIdentifier, bool) final;
    void setPlaybackRate(WebCore::NodeIdentifier, double, CompletionHandler<void(double effectivePlaybackRate)>&&) final;
    double duration(WebCore::NodeIdentifier) const final;
    bool paused(WebCore::NodeIdentifier) const final;
    void setPaused(WebCore::NodeIdentifier, bool, CompletionHandler<void(bool succeeded)>&&) final;
    Seconds currentTime(WebCore::NodeIdentifier) const final;
    void setCurrentTime(WebCore::NodeIdentifier, Seconds, CompletionHandler<void()>&&) final;
    void setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data) final;
    void setHasPortal(bool) final;
#if ENABLE(SPATIAL_PORTAL)
    void setPortalTransform(WebCore::PortalTransformKind) final;
    void setPortalAction(WebCore::PortalActionKind) final;
#endif
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
#if ENABLE(SPATIAL_PORTAL)
    WebCore::PortalTransformKind m_portalTransform { WebCore::PortalTransformKind::Auto };
    WebCore::PortalActionKind m_portalAction { WebCore::PortalActionKind::None };
#endif
    WebCore::StageModeOperation m_stageModeOperation { WebCore::StageModeOperation::None };
    HashMap<WebCore::NodeIdentifier, NodeAnimationState> m_animationStates;
    SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
};

}

#endif // ENABLE(MODEL_PROCESS)
