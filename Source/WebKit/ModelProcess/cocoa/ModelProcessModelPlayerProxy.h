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

#include "Connection.h"
#include "LayerHostingContext.h"
#include "MessageReceiver.h"
#include "SharedPreferencesForWebProcess.h"
#include <CoreRE/CoreRE.h>
#include <WebCore/Color.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerAnimationState.h>
#include <WebCore/ModelPlayerIdentifier.h>
#include <WebCore/StageModeOperations.h>
#include <WebCore/TransformationMatrix.h>
#include <WebKitAdditions/REPtr.h>
#include <WebKitAdditions/REModelLoaderClient.h>
#include <simd/simd.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS WKModelProcessModelLayer;
OBJC_CLASS WKModelProcessModelPlayerProxyObjCAdapter;
OBJC_CLASS WKRKEntity;
OBJC_CLASS WKStageModeInteractionDriver;

namespace WebCore {
class Model;
class REModel;
class REModelLoader;
}

namespace WebKit {

class ModelProcessModelPlayerManagerProxy;

class ModelProcessModelPlayerProxy final
    : public WebCore::ModelPlayer
    , public WebCore::REModelLoaderClient
    , private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(ModelProcessModelPlayerProxy);
public:
    static Ref<ModelProcessModelPlayerProxy> create(ModelProcessModelPlayerManagerProxy&, WebCore::ModelPlayerIdentifier, Ref<IPC::Connection>&&, const std::optional<String>&, std::optional<int> debugEntityMemoryLimit);
    ~ModelProcessModelPlayerProxy();

    void ref() const final { WebCore::ModelPlayer::ref(); }
    void deref() const final { WebCore::ModelPlayer::deref(); }

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    void invalidate();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    template<typename T> void send(T&& message);

    void unloadModelTimerFired();
    void updateTransformAfterLayout();
    void updateOpacity();
    void startAnimating();
    void animationPlaybackStateDidUpdate();

    // Messages
    void createLayer();
    void loadModel(Ref<WebCore::Model>&&, WebCore::LayoutSize);
    void reloadModel(Ref<WebCore::Model>&&, WebCore::LayoutSize, std::optional<WebCore::TransformationMatrix> transformToRestore, std::optional<WebCore::ModelPlayerAnimationState> animationStateToRestore);
    void modelVisibilityDidChange(bool isVisible);

    // WebCore::REModelLoaderClient overrides.
    void didFinishLoading(WebCore::REModelLoader&, Ref<WebCore::REModel>) final;
    void didFailLoading(WebCore::REModelLoader&, const WebCore::ResourceError&) final;

    // WebCore::ModelPlayer overrides.
    WebCore::ModelPlayerIdentifier identifier() const final { return m_id; }
    void load(WebCore::Model&, WebCore::LayoutSize) final;
    void sizeDidChange(WebCore::LayoutSize) final;
    void configureGraphicsLayer(WebCore::GraphicsLayer&, WebCore::ModelPlayerGraphicsLayerConfiguration&&) final;
    void setEntityTransform(WebCore::TransformationMatrix) final;
    void enterFullscreen() final;
    bool supportsMouseInteraction() final;
    bool supportsDragging() final;
    void setInteractionEnabled(bool) final;
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
    void resetModelTransformAfterDrag() final;
    void stageModeInteractionDidUpdateModel();
    void animateModelToFitPortal(CompletionHandler<void(bool)>&&) final;

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    void ensureImmersivePresentation(CompletionHandler<void(std::optional<WebCore::LayerHostingContextIdentifier>)>&&) final;
    void exitImmersivePresentation(CompletionHandler<void()>&&) final;
#endif

    USING_CAN_MAKE_WEAKPTR(WebCore::REModelLoaderClient);

    void disableUnloadDelayForTesting() { m_unloadDelayDisabledForTesting = true; }
    static uint64_t objectCountForTesting() { return gObjectCountForTesting; }

private:
    ModelProcessModelPlayerProxy(ModelProcessModelPlayerManagerProxy&, WebCore::ModelPlayerIdentifier, Ref<IPC::Connection>&&, const std::optional<String>&, std::optional<int> debugEntityMemoryLimit);

    void computeTransform(bool);
    void updateTransform();
    void applyEnvironmentMapDataAndRelease(CompletionHandler<void()>&&);
    void applyStageModeOperationToDriver();
    bool stageModeInteractionInProgress() const;
    void updateTransformSRT();
    void notifyModelPlayerOfEntityTransformChange();
    void applyDefaultIBL();
    void updateForCurrentStageMode();
    std::optional<WebCore::LayerHostingContextIdentifier> layerHostingContextIdentifier();

    WebCore::ModelPlayerIdentifier m_id;
    bool m_isVisible { true };
    Ref<IPC::Connection> m_webProcessConnection;
    WeakPtr<ModelProcessModelPlayerManagerProxy> m_manager;

    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    RetainPtr<WKModelProcessModelLayer> m_layer;
    RefPtr<WebCore::REModelLoader> m_loader;
    RetainPtr<WKRKEntity> m_modelRKEntity;
    REPtr<RESceneRef> m_scene;
    REPtr<REEntityRef> m_hostingEntity;
    REPtr<REEntityRef> m_containerEntity;
    RetainPtr<WKModelProcessModelPlayerProxyObjCAdapter> m_objCAdapter;

    simd_float3 m_originalBoundingBoxCenter { simd_make_float3(0, 0, 0) };
    simd_float3 m_originalBoundingBoxExtents { simd_make_float3(0, 0, 0) };
    float m_pitch { 0 };
    float m_yaw { 0 };

    RESRT m_transformSRT; // SRT=Scaling/Rotation/Translation. This is stricter than a WebCore::TransformationMatrix.
    bool m_transformNeedsUpdateAfterNextLayout { false };

    bool m_autoplay { false };
    bool m_loop { false };
    double m_playbackRate { 1.0 };

    RefPtr<WebCore::SharedBuffer> m_transientEnvironmentMapData;
    bool m_hasPortal { true };

    // For interactions
    RetainPtr<WKStageModeInteractionDriver> m_stageModeInteractionDriver;
    WebCore::StageModeOperation m_stageModeOperation { WebCore::StageModeOperation::None };

    std::optional<String> m_attributionTaskID;
    std::optional<int> m_debugEntityMemoryLimit;
    std::optional<WebCore::TransformationMatrix> m_entityTransformToRestore;
    std::optional<WebCore::ModelPlayerAnimationState> m_animationStateToRestore;
    RunLoop::Timer m_unloadModelTimer;

    // For testing
    bool m_unloadDelayDisabledForTesting { false };
    static uint64_t gObjectCountForTesting;

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    bool m_immersivePresentation { false };
    WebCore::LayoutSize m_layoutSize { };
    Vector<CompletionHandler<void(bool)>> m_modelLoadedCallbacks;

    void triggerModelLoadedCallbacks(bool);
    void ensureModelLoaded(CompletionHandler<void(bool)>&&);
    void setImmersivePresentation(bool);
#endif
};

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
