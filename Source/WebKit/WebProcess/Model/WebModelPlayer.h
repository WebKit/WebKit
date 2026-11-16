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
#include <WebCore/NodeIdentifier.h>
#include <WebCore/PlatformDynamicRangeLimit.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/StageModeOperations.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Observer.h>
#include <wtf/RetainPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/URL.h>
#include <wtf/UniqueRef.h>

OBJC_CLASS WKBridgeModelLoader;
OBJC_CLASS WKStageModeOrbitSimulator;

namespace WebModel {
struct ImageAsset;
}

namespace WebCore {
class ColorSpace;
class FloatSize;
class GraphicsLayerContentsDisplayDelegate;
class ImageBuffer;
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
    bool isWebModelPlayerInstance() const final { return true; }
    void scheduleUpdateIfNeeded();
    void releaseModelResources();

private:
    WebModelPlayer(WebCore::Page&, WebCore::ModelPlayerClient&);

    struct HostedModel : public CanMakeCheckedPtr<HostedModel> {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(HostedModel);
        WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(HostedModel);

        enum class PauseState : uint8_t { None, Playing, Paused };

        ~HostedModel();

        bool isPlaying() const { return pauseState == PauseState::Playing; }

#if ENABLE(SPATIAL_PORTAL)
        WebCore::TransformationMatrix anchorPlacementTransform(const HostedModel& anchor);
#endif

        RetainPtr<WKBridgeModelLoader> loader;
        RetainPtr<NSData> retainedData;

        HashMap<String, WebModel::TypedResourceId> meshIDs;
        HashMap<String, WebModel::TypedResourceId> materialIDs;
        HashMap<String, WebModel::TypedResourceId> textureIDs;

        bool didFinishLoading { false };
        bool pendingClientFinishLoadingNotification { false };

        bool autoplay { false };
        PauseState pauseState { PauseState::None };
        double playbackRate { 1.0 };
        bool isLooping { false };

        std::optional<WebCore::ModelPlayerAnimationState> animationStateToRestore;
#if ENABLE(SPATIAL_PORTAL)
        std::optional<WebCore::TransformationMatrix> childTransform;
        std::optional<WebCore::NodeIdentifier> anchorNode;
        String anchorPlacementName;
        std::optional<WebCore::TransformationMatrix> anchorCorrection;
#endif
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
        RefPtr<WebCore::Model> cachedModelSource;
#endif
    };

    HostedModel& ensureHostedModel(WebCore::NodeIdentifier);
    HostedModel* hostedModel(WebCore::NodeIdentifier);
    const HostedModel* hostedModel(WebCore::NodeIdentifier) const;
    bool ensureModelBacking(WebCore::LayoutSize);
    void loadHostedModel(WebCore::NodeIdentifier, WebCore::Model&, WebCore::LayoutSize);
    void setLoaderCallbacks(WebCore::NodeIdentifier, WKBridgeModelLoader *);
    void removeHostedModelResources(WebCore::NodeIdentifier, HostedModel&);
    bool hasAnyLoadedChild() const;

    void updateScene();

    // ModelPlayer finals.
    void load(WebCore::NodeIdentifier, WebCore::Model&, WebCore::LayoutSize, bool) final;
    void unload(WebCore::NodeIdentifier) final;
    void sizeDidChange(WebCore::LayoutSize) final;
    void configureGraphicsLayer(WebCore::GraphicsLayer&, WebCore::ModelPlayerGraphicsLayerConfiguration&&) final;
    void adoptContentsDisplayDelegateFrom(WebCore::ModelPlayer&) final;
    RefPtr<WebCore::ImageBuffer> snapshotCurrentFrame(const WebCore::FloatSize& deviceSize, const WebCore::ColorSpace&) final;
    void enterFullscreen() final;
    void handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime) final;
    void handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime) final;
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
#if PLATFORM(COCOA)
    std::optional<WebCore::TransformationMatrix> entityTransform(WebCore::NodeIdentifier) const final;
#endif
    void setEntityTransform(WebCore::NodeIdentifier, WebCore::TransformationMatrix) final;
    bool supportsTransform(WebCore::TransformationMatrix) final;
    bool supportsMouseInteraction() final;
    void visibilityStateDidChange() final;
    void reload(WebCore::NodeIdentifier, WebCore::Model&, WebCore::LayoutSize, WebCore::ModelPlayerAnimationState&, std::unique_ptr<WebCore::ModelPlayerTransformState>&&) final;
    std::optional<WebCore::ModelPlayerAnimationState> currentAnimationState(WebCore::NodeIdentifier) const final;
    std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> currentTransformState(WebCore::NodeIdentifier) const final;

    std::pair<WebCore::FloatPoint3D, WebCore::FloatPoint3D> boundingBoxCenterAndExtents(WebCore::NodeIdentifier) const;

    const MachSendRight* displayBuffer() const;
    WebCore::GraphicsLayerContentsDisplayDelegate* contentsDisplayDelegate();

    void setPlaybackRate(WebCore::NodeIdentifier, double, CompletionHandler<void(double effectivePlaybackRate)>&&) final;
    void setAutoplay(WebCore::NodeIdentifier, bool) final;
    void setLoop(WebCore::NodeIdentifier, bool) final;
    void setPaused(WebCore::NodeIdentifier, bool, CompletionHandler<void(bool succeeded)>&&) final;
    bool paused(WebCore::NodeIdentifier) const final;
    Seconds currentTime(WebCore::NodeIdentifier) const final;
    void setCurrentTime(WebCore::NodeIdentifier, Seconds, CompletionHandler<void()>&&) final;
    void play(WebCore::NodeIdentifier, HostedModel&, bool);
    void updateMeshPlayState();
    bool simulate(float elapsedTime);
    double duration(WebCore::NodeIdentifier) const final;

    void ensureOnMainThreadWithProtectedThis(Function<void(Ref<WebModelPlayer>)>&& task);
    void startUpdateLoopIfNeeded();
    void update();
    bool render();
    void scheduleDisplayUpdate();
    void notifyClientDidFinishLoading();

    void setStageMode(WebCore::StageModeOperation) final;
    void notifyEntityTransformUpdated();
    void setEnvironmentMap(Ref<WebCore::SharedBuffer>&&, const URL&) final;
#if ENABLE(SPATIAL_PORTAL)
    void disableEnvironmentMap() final;
    void enableSystemEnvironmentMap() final;
#endif
    String environmentMapForTesting() const final;
    void pushEnvironmentMapToModel();
    WebCore::StageModeOperation effectiveStageMode() const;
#if ENABLE(SPATIAL_PORTAL)
    void setPortalTransform(const WebCore::UsedPortalTransform&) final;
    void setPortalAction(WebCore::PortalActionKind) final;
    bool isSpatialPortal() const { return !!m_portalTransform; }
    void notifyPortalTransformUpdated();
    void setAnchor(WebCore::NodeIdentifier, std::optional<WebCore::NodeIdentifier> anchorNode, const String& placement) final;
    bool isAnchoredTo(const HostedModel&, WebCore::NodeIdentifier ancestorNode) const;
    void pushPortalChildTransform(WebCore::NodeIdentifier);
    void pushAnchoredPortalChildTransforms(WebCore::NodeIdentifier anchorNode);
#endif

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    void setDynamicRangeLimit(WebCore::PlatformDynamicRangeLimit, float currentEDRHeadroom, bool suppressEDR) final;
    std::optional<double> getEffectiveDynamicRangeLimitValue() const final;
    void updateContentsHeadroom();
    void updateScreenHeadroom(float currentEDRHeadroom, bool suppressEDR);
    void updateScreenHeadroomFromPage();
    void dynamicRangeLimitDidChange();
#endif

    WeakPtr<WebCore::ModelPlayerClient> m_client;

    WebCore::ModelPlayerIdentifier m_id;
    HashMap<WebCore::NodeIdentifier, UniqueRef<HostedModel>> m_hostedModels;
    Vector<MachSendRight> m_displayBuffers;
    RefPtr<WebKit::Mesh> m_currentModel;
    WeakPtr<WebCore::Page> m_page;
    mutable RefPtr<ModelDisplayBufferDisplayDelegate> m_contentsDisplayDelegate;
    WeakPtr<WebCore::GraphicsLayer> m_graphicsLayer;
    uint32_t m_renderTextureIndex { 0 };
    uint32_t m_displayTextureIndex { 0 };
    bool m_hasRenderedFrame { false };
    WebCore::StageModeOperation m_stageMode { WebCore::StageModeOperation::None };
    WebCore::IntSize m_currentPixelSize;
    std::optional<WebCore::LayoutPoint> m_initialPoint;
    std::optional<Ref<WebCore::SharedBuffer>> m_environmentMap;
    URL m_environmentMapURL;
    WebCore::EnvironmentMapKind m_environmentMapKind { WebCore::EnvironmentMapKind::Default };
    bool m_environmentMapPushedToCurrentModel { false };
    const RetainPtr<WKStageModeOrbitSimulator> m_orbitSimulator;
    MonotonicTime m_lastUpdateTime;
#if ENABLE(SPATIAL_PORTAL)
    std::optional<WebCore::UsedPortalTransform> m_portalTransform;
    WebCore::PortalActionKind m_portalAction { WebCore::PortalActionKind::None };
#endif

    bool m_isUpdateLoopRunning { false };
    bool m_isUpdateScheduled { false };
    bool m_isUpdating { false };
    bool m_needsEntityTransformNotification { false };

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    using ScreenPropertiesChangedObserver = Observer<void(WebCore::PlatformDisplayID)>;
    const RefPtr<ScreenPropertiesChangedObserver> m_screenPropertiesChangedObserver;
    WebCore::LayoutSize m_lastLayoutSize;
    float m_currentEDRHeadroom { 1.f };
    float m_lastSentContentsHeadroom { -1.f };
    bool m_suppressEDR { false };
    WebCore::PlatformDynamicRangeLimit m_dynamicRangeLimit { WebCore::PlatformDynamicRangeLimit::standard() };
    bool m_usingStandardDynamicRange { false };
#endif
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebModelPlayer)
static bool isType(const WebCore::ModelPlayer& player) { return player.isWebModelPlayerInstance(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
