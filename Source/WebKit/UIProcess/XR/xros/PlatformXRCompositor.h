/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 */

#pragma once

#if ENABLE(WEBXR) && USE(COMPOSITORXR)

#import "PlatformXRCoordinator.h"
#import "XRDeviceIdentifier.h"
#import <CompositorServices/CompositorServices.h>
#import <Metal/Metal.h>
#import <WebCore/PageIdentifier.h>
#import <WebCore/SecurityOriginData.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/threads/BinarySemaphore.h>

@class UIView;
@class WKSpatialGestureRecognizer;
@class WKXRTrackingManager;
#if HAVE(SPATIAL_CONTROLLERS)
@class WKXRControllerManager;
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
@class _UINonHostingVisibilityPropagationView;
#if ENABLE(GPU_PROCESS)
@class _UILayerHostView;
#endif
#endif

namespace WebKit {
class CompositorCoordinator;
class WebPageProxy;

enum class PlatformXRSessionEndReason : uint8_t;
}

namespace WebCore {
struct XRCanvasConfiguration;
}

namespace WebKit {

class CompositorCoordinator final : public PlatformXRCoordinator {
    WTF_MAKE_TZONE_ALLOCATED(CompositorCoordinator);
public:
    virtual ~CompositorCoordinator() { }

    static CompositorCoordinator* singleton();
    static bool isCompositorServicesAvailable();

    void getPrimaryDeviceInfo(WebPageProxy&, DeviceInfoCallback&&) override;
    void requestPermissionOnSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& /* granted */, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, const PlatformXR::Device::FeatureList& /* requiredFeaturesRequested */, const PlatformXR::Device::FeatureList& /* optionalFeaturesRequested */, FeatureListCallback&&) override;

    void startSession(WebPageProxy&, WeakPtr<PlatformXRCoordinatorSessionEventClient>&&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, std::optional<WebCore::XRCanvasConfiguration>&&) override;
    void endSessionIfExists(WebPageProxy&) override;

    void scheduleAnimationFrame(WebPageProxy&, std::optional<PlatformXR::RequestData>&&, PlatformXR::Device::RequestFrameCallback&&) override;
    void submitFrame(WebPageProxy&) override;

private:
    friend class WTF::LazyNeverDestroyed<CompositorCoordinator>;

    CompositorCoordinator();

    bool processEvent();
    void update();
    void render(cp_frame_t, cp_drawable_t, NSTimeInterval);
    void terminateSession(WebPageProxy&, PlatformXRSessionEndReason);
    void currentSessionHasEnded();
    void setPaused(bool);
    void setBackgrounded(bool);
    void updateVisibilityStateIfNeeded();

    void didCompleteSessionSetup(WebPageProxy&, WebCore::PageIdentifier, const WebCore::SecurityOriginData&, const PlatformXR::Device::FeatureList&, UIViewController *);
    void setUpDepthTextures();

    void sessionRequestIsCancelled();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void setUpVisibilityPropagationViews(WebPageProxy&, UIView *);
    void cleanUpVisibilityPropagationViewsIfNeeded();
#endif
#if ENABLE(WEBXR_LAYERS)
    void createCompositionLayer(PlatformXR::CompositionLayerType, WebCore::IntSize, PlatformXR::LayerLayout, PlatformXRCoordinator::CreateCompositionLayerCallback&&) override { }
#endif

    void getSupportedFeatures(WebPageProxy&);
    PlatformXR::Device::FeatureList filterOutUnsupportedFeatures(const PlatformXR::Device::FeatureList&) const;

    std::optional<size_t> indexOfReusableTextures(id<MTLTexture>, id<MTLTexture>);

    RetainPtr<cp_layer_renderer_t> m_cpLayer;
    RetainPtr<WKXRTrackingManager> m_xrTrackingManager;
    RetainPtr<WKSpatialGestureRecognizer> m_spatialGestureRecognizer;
    std::optional<XRDeviceIdentifier> m_headsetIdentifier;
    PlatformXR::Device::FeatureList m_supportedVRFeatures;
    PlatformXR::Device::FeatureList m_supportedARFeatures;
    std::optional<WebCore::PageIdentifier> m_sessionStartPendingPageIdentifier;
    std::optional<WebCore::PageIdentifier> m_sessionPageIdentifier;
    WeakPtr<WebPageProxy> m_sessionPage;
    WeakPtr<PlatformXRCoordinatorSessionEventClient> m_sessionEventClient;
    PlatformXR::Device::RequestFrameCallback m_onFrameUpdate;
    PlatformXR::DepthRange m_depthRange = { 0.1f, 1000.0f };
    std::unique_ptr<BinarySemaphore> m_renderSemaphore;
    RefPtr<Thread> m_updateThread;
    std::atomic<bool> m_terminationPending { false };
    bool m_paused { false };
    bool m_backgrounded { false };
    PlatformXR::VisibilityState m_visibilityState { PlatformXR::VisibilityState::Visible };
    RetainPtr<id> m_applicationDidEnterBackgroundObserver;
    RetainPtr<id> m_applicationDidBecomeActiveObserver;
#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    std::optional<WebCore::SecurityOriginData> m_lastSecurityOriginGrantedHandTracking;
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    RetainPtr<_UINonHostingVisibilityPropagationView> m_visibilityPropagationViewForWebProcess;
#if ENABLE(GPU_PROCESS)
    RetainPtr<_UILayerHostView> m_visibilityPropagationViewForGPUProcess;
#endif // ENABLE(GPU_PROCESS)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)
    bool m_foveationEnabled { false };
    bool m_layeredModeEnabled { false };
    bool m_forwardDepthAvailable { false };
    Vector<std::tuple<WeakObjCPtr<id<MTLTexture>>, WeakObjCPtr<id<MTLTexture>>>> m_compositorTextures;
#if HAVE(SPATIAL_CONTROLLERS)
    RetainPtr<WKXRControllerManager> m_controllerManager;
#endif
};

}

#endif // ENABLE(WEBXR) && USE(COMPOSITORXR)
