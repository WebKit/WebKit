/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 */

#import "config.h"

#if ENABLE(WEBXR) && USE(COMPOSITORXR)
#import "PlatformXRCompositor.h"

#define PS_ACKNOWLEDGE_SYSTEM_GRAPH_IMPORT_DEPRECATED 1

#import "APIPageConfiguration.h"
#import "APIUIClient.h"
#import "Logging.h"
#import "UIKitSPI.h"
#import "WKSpatialGestureRecognizer.h"
#import "WKXRTrackingManager.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import <WebCore/IntRect.h>
#import <WebCore/IntSize.h>
#import <WebCore/PlatformXRPose.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/TransformationMatrix.h>
#import <pal/spi/cocoa/MetalSPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SystemTracing.h>

#if HAVE(SPATIAL_CONTROLLERS)
#import "WKXRControllerManager.h"
#endif

#import <pal/cocoa/ARKitSoftLink.h>
#import <pal/cocoa/CompositorServicesSoftLink.h>

namespace WebKit {
WTF_MAKE_TZONE_ALLOCATED_IMPL(CompositorCoordinator);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

constexpr int defaultWidth = 256;
constexpr int defaultHeight = 256;
constexpr size_t reusableTextureMaxCount = 3;

static PlatformXR::FrameData::ExternalTexture makeMachSendRight(id<MTLTexture> texture)
{
#if !PLATFORM(IOS_SIMULATOR) || __VISION_OS_VERSION_MIN_REQUIRED >= 270000
    RetainPtr<MTLSharedTextureHandle> sharedTextureHandle = adoptNS([texture newSharedTextureHandle]);
    if (sharedTextureHandle)
        return { MachSendRight::adopt([sharedTextureHandle.get() createMachPort]), true };
#endif
    auto surface = WebCore::IOSurface::createFromSurface(texture.iosurface, WebCore::DestinationColorSpace::SRGB());
    if (!surface)
        return { MachSendRight(), false };
    return { surface->createSendRight(), false };
}

static std::optional<WebCore::IntSize> sizeFromLayerProperties(cp_layer_renderer_properties_t layerProperties)
{
    size_t textureCount = cp_layer_renderer_properties_get_texture_topology_count(layerProperties);
    if (!textureCount) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: No textures found from layer properties");
        return std::nullopt;
    }

    // We expect textureCount to always be 1 for the shared layout.
    ASSERT(textureCount == 1);

    return WebCore::IntSize { defaultWidth, defaultHeight };
}

namespace WebKit {

using namespace PAL;

bool CompositorCoordinator::isCompositorServicesAvailable()
{
    return isCompositorServicesFrameworkAvailable();
}

CompositorCoordinator* CompositorCoordinator::singleton()
{
    if (!isCompositorServicesAvailable())
        return nullptr;

    static LazyNeverDestroyed<CompositorCoordinator> singleton;
    static std::once_flag once;
    std::call_once(once, [] {
        singleton.construct();
    });
    return &singleton.get();
}

CompositorCoordinator::CompositorCoordinator()
    : m_sessionPage(0)
{
    ASSERT(isCompositorServicesAvailable());
    // FIXME: rdar://134998122
    m_forwardDepthAvailable = can_load_cp_drawable_set_write_forward_depth();
}

void CompositorCoordinator::getPrimaryDeviceInfo(WebPageProxy& page, DeviceInfoCallback&& callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_headsetIdentifier)
        m_headsetIdentifier = XRDeviceIdentifier::generate();

    CFErrorRef error = NULL;
    RetainPtr<cp_layer_renderer_configuration_t> defaultConfiguration = adoptNS(cp_layer_renderer_configuration_copy_system_default(&error));
    if (error) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Cannot get the system default layer configuration: %@", error);
        callback(std::nullopt);
        return;
    }

    m_foveationEnabled = cp_layer_renderer_configuration_get_foveation_enabled(defaultConfiguration.get());
    m_layeredModeEnabled = cp_layer_renderer_configuration_get_layout(defaultConfiguration.get());

    auto defaultDepthRange = cp_layer_renderer_configuration_get_default_depth_range(defaultConfiguration.get());
    m_depthRange.near = std::min(defaultDepthRange.x, defaultDepthRange.y);
    m_depthRange.far = std::max(defaultDepthRange.x, defaultDepthRange.y);

    cp_layer_renderer_configuration_set_use_shared_events(defaultConfiguration.get(), true);

    RetainPtr<cp_layer_renderer_properties_t> defaultProperties = adoptNS(cp_layer_renderer_properties_create_using_configuration(defaultConfiguration.get(), &error));
    if (error) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Cannot get system default layer properties: %@", error);
        callback(std::nullopt);
        return;
    }

    std::optional<WebCore::IntSize> recommendedResolution = sizeFromLayerProperties(defaultProperties.get());
    if (!recommendedResolution) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: No system default layer properties found");
        callback(std::nullopt);
        return;
    }

    RetainPtr<cp_layer_renderer_capabilities_t> defaultRenderCapabilities = adoptNS(cp_layer_renderer_capabilities_copy_system_default(&error));
    if (error) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Cannot get the system default layer render capabilities: %@", error);
        callback(std::nullopt);
        return;
    }
    double minimumNearClipPlane = cp_layer_renderer_capabilities_supported_minimum_near_plane_distance(defaultRenderCapabilities.get());

    ASSERT(m_headsetIdentifier);
    CompositorCoordinator::getSupportedFeatures(page);
    if (m_supportedVRFeatures.isEmpty() && m_supportedARFeatures.isEmpty()) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: No supported XR features");
        callback(std::nullopt);
        return;
    }

    XRDeviceInfo deviceInfo {
        .identifier = m_headsetIdentifier.value(),
        .supportsOrientationTracking = true,
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        .supportsStereoRendering = false,
#else
        .supportsStereoRendering = true,
#endif
        .vrFeatures = m_supportedVRFeatures,
        .arFeatures = m_supportedARFeatures,
        .recommendedResolution = *recommendedResolution,
        .minimumNearClipPlane = minimumNearClipPlane
    };

    callback(WTF::move(deviceInfo));
}

PlatformXR::Device::FeatureList CompositorCoordinator::filterOutUnsupportedFeatures(const PlatformXR::Device::FeatureList& features) const
{
    PlatformXR::Device::FeatureList result;

    if (m_supportedVRFeatures.isEmpty() && m_supportedARFeatures.isEmpty())
        return result;

    for (auto feature : features) {
        if (m_supportedVRFeatures.contains(feature) || m_supportedARFeatures.contains(feature))
            result.append(feature);
    }
    return result;
}

void CompositorCoordinator::requestPermissionOnSessionFeatures(WebPageProxy& page, const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, FeatureListCallback&& callback)
{
    // Make sure primary device info has been called.
    ASSERT(m_headsetIdentifier);

    // We only ask for explicit consent from user when starting immersive sessions.
    if (mode == PlatformXR::SessionMode::Inline) {
        callback(granted);
        return;
    }

    page.uiClient().requestPermissionOnXRSessionFeatures(page, securityOriginData, mode, filterOutUnsupportedFeatures(granted), filterOutUnsupportedFeatures(consentRequired), filterOutUnsupportedFeatures(consentOptional), filterOutUnsupportedFeatures(requiredFeaturesRequested), filterOutUnsupportedFeatures(optionalFeaturesRequested), [this, securityOriginData, callback = WTF::move(callback)](std::optional<Vector<PlatformXR::SessionFeature>> userGranted) mutable {
        if (userGranted)
            userGranted = filterOutUnsupportedFeatures(*userGranted);

#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
        if (userGranted && userGranted->contains(PlatformXR::SessionFeature::HandTracking))
            m_lastSecurityOriginGrantedHandTracking = securityOriginData;
        else
            m_lastSecurityOriginGrantedHandTracking = std::nullopt;
#else
        UNUSED_PARAM(this);
#endif

        callback(WTF::move(userGranted));
    });
}

void CompositorCoordinator::startSession(WebPageProxy& page, WeakPtr<PlatformXRCoordinatorSessionEventClient>&& sessionEventClient, const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& requestedFeatures, std::optional<WebCore::XRCanvasConfiguration>&& init)
{
    ASSERT(RunLoop::isMain());
    ASSERT(mode == PlatformXR::SessionMode::ImmersiveVr);

    if (m_cpLayer) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Trying to start another immersive session while there's already an active one");
        sessionRequestIsCancelled();
        if (sessionEventClient)
            sessionEventClient->sessionDidEnd(m_headsetIdentifier.value());
        return;
    }

    if (m_sessionStartPendingPageIdentifier) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: There is already an immersive session being started");
        sessionRequestIsCancelled();
        if (sessionEventClient)
            sessionEventClient->sessionDidEnd(m_headsetIdentifier.value());
        return;
    }

    auto pageIdentifier = page.webPageIDInMainFrameProcess();
    m_sessionStartPendingPageIdentifier = pageIdentifier;

    ASSERT(!m_sessionEventClient);
    ASSERT(!m_updateThread);
    ASSERT(!m_sessionPageIdentifier);

    m_sessionEventClient = WTF::move(sessionEventClient);

    page.uiClient().startXRSession(page, requestedFeatures, WTF::move(init), [this, weakPage = WeakPtr { page }, pageIdentifier, securityOriginData, requestedFeatures](RetainPtr<id> cpLayer, UIViewController *viewController) mutable {
        ASSERT(RunLoop::isMain());
        ASSERT(!cpLayer || [cpLayer.get() isKindOfClass:getCP_OBJECT_cp_layer_rendererClassSingleton()]);
        RefPtr strongPage = weakPage;
        if (!strongPage)
            return;

        m_cpLayer = cpLayer.get();
        didCompleteSessionSetup(*strongPage, pageIdentifier, securityOriginData, requestedFeatures, viewController);
    });
}

void CompositorCoordinator::endSessionIfExists(WebPageProxy& page)
{
    ASSERT(RunLoop::isMain());

    if (m_sessionStartPendingPageIdentifier && m_sessionStartPendingPageIdentifier == page.webPageIDInMainFrameProcess()) {
        RELEASE_LOG_ERROR(XR, "Page is ending a immersive session that's in the middle of starting");
        m_sessionStartPendingPageIdentifier = std::nullopt;
        return;
    }

    if (m_terminationPending)
        return;

    if (m_sessionPageIdentifier != page.webPageIDInMainFrameProcess()) {
        RELEASE_LOG_ERROR(XR, "Page tried to end immersive session initiated by a different page");
        return;
    }

    ASSERT(m_sessionPage.get() == &page);
    terminateSession(page, PlatformXRSessionEndReason::NoError);

    m_terminationPending = true;
}

void CompositorCoordinator::scheduleAnimationFrame(WebPageProxy& page, std::optional<PlatformXR::RequestData>&& requestData, PlatformXR::Device::RequestFrameCallback&& onFrameUpdateCallback)
{
    if (!m_sessionPageIdentifier && !m_sessionStartPendingPageIdentifier) {
        RELEASE_LOG(XR, "Page tried to schedule frame update for a session that it never initiated");
        onFrameUpdateCallback({ });
        return;
    }

    if (m_sessionPageIdentifier && m_sessionPageIdentifier != page.webPageIDInMainFrameProcess()) {
        RELEASE_LOG(XR, "Page tried to schedule frame update for a session initiated by a different page");
        onFrameUpdateCallback({ });
        return;
    }

    if (m_sessionStartPendingPageIdentifier && m_sessionStartPendingPageIdentifier != page.webPageIDInMainFrameProcess()) {
        RELEASE_LOG(XR, "Page tried to schedule frame update but another page has already requested a session");
        onFrameUpdateCallback({ });
        return;
    }

    if (m_terminationPending) {
        RELEASE_LOG_ERROR(XR, "Page tried to schedule frame update after requesting session termination");
        onFrameUpdateCallback({ });
        return;
    }

    if (m_onFrameUpdate) {
        // Clients should not schedule another frame while one is already in flight
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Cannot schedule another frame callback while one is in flight");
        onFrameUpdateCallback({ });
        return;
    }

    m_onFrameUpdate = WTF::move(onFrameUpdateCallback);
    if (requestData.has_value())
        m_depthRange = requestData->depthRange;
}

void CompositorCoordinator::submitFrame(WebPageProxy& page)
{
    ASSERT(RunLoop::isMain());
    if (!m_sessionPageIdentifier || m_sessionPageIdentifier != page.webPageIDInMainFrameProcess()) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: SubmitFrame should not be called from a page that's not running an XR session");
        return;
    }

    ASSERT(m_terminationPending || m_renderSemaphore);
    if (m_renderSemaphore)
        m_renderSemaphore->signal();
}

void CompositorCoordinator::setPaused(bool paused)
{
    ensureOnMainRunLoop([this, paused] {
        if (m_paused == paused)
            return;

        m_paused = paused;

        RELEASE_LOG(XR, "CompositorCoordinator: new pause state: %d", m_paused);

        updateVisibilityStateIfNeeded();
    });
}

void CompositorCoordinator::setBackgrounded(bool backgrounded)
{
    ASSERT(RunLoop::isMain());

    if (m_backgrounded == backgrounded)
        return;

    m_backgrounded = backgrounded;

    RELEASE_LOG_INFO(XR, "CompositorCoordinator: backgrounded: %d", m_backgrounded);

    updateVisibilityStateIfNeeded();
}

void CompositorCoordinator::updateVisibilityStateIfNeeded()
{
    ASSERT(RunLoop::isMain());

    if (!m_sessionEventClient)
        return;

    PlatformXR::VisibilityState visibilityState;
    if (m_paused)
        visibilityState = PlatformXR::VisibilityState::Hidden;
    else if (m_backgrounded)
        visibilityState = PlatformXR::VisibilityState::VisibleBlurred;
    else
        visibilityState = PlatformXR::VisibilityState::Visible;

    if (visibilityState == m_visibilityState)
        return;

    m_visibilityState = visibilityState;
    m_sessionEventClient->sessionDidUpdateVisibilityState(m_headsetIdentifier.value(), m_visibilityState);
}

bool CompositorCoordinator::processEvent()
{
    auto state = cp_layer_renderer_get_state(m_cpLayer.get());

    switch (state) {
    case cp_layer_renderer_state_paused:
        RELEASE_LOG(XR, "CompositorCoordinator: renderer is paused");
        setPaused(true);
        cp_layer_renderer_wait_until_running(m_cpLayer.get());
        return processEvent();

    case cp_layer_renderer_state_invalidated:
        RELEASE_LOG(XR, "CompositorCoordinator: renderer is invalidated");
        return false;

    case cp_layer_renderer_state_running:
        setPaused(false);
        return true;
    }

    return false;
}

void CompositorCoordinator::update()
{
    constexpr size_t minSkippedFrameCountForStalledRendering = 450;
    size_t skippedFrameCount = 0;
    bool isRenderingStalled = false;

    while (processEvent()) {
        // If the session has been terminated manually, don't start another frame
        // and just exit the frame render loop.
        if (m_terminationPending)
            break;

        cp_frame_t frame;
        if (!(frame = cp_layer_renderer_query_next_frame(m_cpLayer.get()))) {
            RELEASE_LOG_INFO(XR, "CompositorCoordinator: next frame is unavailable");
            continue;
        }

        tracePoint(WebXRCPFrameWaitStart);
        cp_frame_timing_t timing = cp_frame_predict_timing(frame);
        if (!timing) {
            RELEASE_LOG_INFO(XR, "CompositorCoordinator: frame timing is unavailable");
            continue;
        }

        cp_time_wait_until(cp_frame_timing_get_optimal_input_time(timing));
        tracePoint(WebXRCPFrameWaitEnd);

        cp_drawable_t drawable;
        if (!(drawable = cp_frame_query_drawable(frame))) {
            RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Cannot query drawable from frame");
            continue;
        }

        bool isFrameScheduled = !!m_onFrameUpdate;
        bool isTrackingValid = [m_xrTrackingManager isValid];
        if (!isFrameScheduled || !isTrackingValid) {
            cp_frame_skip_frame(frame);

            if (skippedFrameCount >= minSkippedFrameCountForStalledRendering) {
                RELEASE_LOG_ERROR(XR, "CompositorCoordinator: stall detected in render loop; tracking valid: %d; frame scheduled: %d, terminating session...", isTrackingValid, isFrameScheduled);
                isRenderingStalled = true;
                break;
            }

            skippedFrameCount++;
            continue;
        }

        skippedFrameCount = 0;
        NSTimeInterval presentationTime = cp_time_to_cf_time_interval(cp_frame_timing_get_presentation_time(timing));
        render(frame, drawable, presentationTime);
    }

    // FIXME: rdar://175742010
    callOnMainRunLoop([this, isRenderingStalled]() {
        if (!m_terminationPending && isRenderingStalled) {
            if (RefPtr<WebPageProxy> page = m_sessionPage.get())
                terminateSession(*page, PlatformXRSessionEndReason::NoFrameUpdateScheduled);
        }

        currentSessionHasEnded();
    });
}

static WebCore::IntRect convertToIntRect(const auto& viewport)
{
    return WebCore::IntRect(static_cast<int>(viewport.originX), static_cast<int>(viewport.originY), static_cast<int>(viewport.width), static_cast<int>(viewport.height));
}

static std::array<uint16_t, 2> convertToLayerSetupSize(const auto& size)
{
    return { static_cast<uint16_t>(size.width), static_cast<uint16_t>(size.height) };
}

void CompositorCoordinator::render(cp_frame_t frame, cp_drawable_t drawable, NSTimeInterval presentationTime)
{
    ASSERT(m_cpLayer);
    ASSERT(m_onFrameUpdate);

    @autoreleasepool {
        auto arDeviceAnchor = [m_xrTrackingManager deviceAnchorAtTime:presentationTime];
        std::optional<PlatformXRPose> platformDevicePose;
        if (arDeviceAnchor)
            platformDevicePose = PlatformXRPose(ar_anchor_get_origin_from_anchor_transform(arDeviceAnchor.get()));
        else
            platformDevicePose = PlatformXRPose(matrix_identity_float4x4);

        PlatformXR::FrameData frameData = { };
        frameData.predictedDisplayTime = presentationTime;
        frameData.isTrackingValid = !!platformDevicePose;
        frameData.isPositionValid = !!platformDevicePose;
        if (platformDevicePose) {
            frameData.origin = platformDevicePose->pose();
            auto floorPose = [m_xrTrackingManager latestFloorPose];
            if (floorPose)
                frameData.floorTransform = floorPose->verticalTransformPose().pose();
        }

        frameData.inputSources = [m_xrTrackingManager collectInputSources];

        // FIXME: rdar://134998122
        if (m_forwardDepthAvailable) {
            cp_drawable_set_write_forward_depth(drawable, m_depthRange.near < m_depthRange.far);
            cp_drawable_set_depth_range(drawable, simd_make_float2(std::max(m_depthRange.near, m_depthRange.far), std::min(m_depthRange.near, m_depthRange.far)));
        }

        size_t viewCount = cp_drawable_get_view_count(drawable);
        if (!viewCount) {
            RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Drawable should have non-zero view count");
            return;
        }

        for (size_t i = 0; i < viewCount; ++i) {
            auto view = cp_drawable_get_view(drawable, i);
            PlatformXR::FrameData::View frameDataView;
            auto projection = cp_drawable_compute_projection(drawable, cp_axis_direction_convention_right_up_forward, i);
            frameDataView.projection = WebCore::TransformationMatrix(projection).toColumnMajorFloatArray();

            PlatformXRPose pose(cp_view_get_transform(view));
            frameDataView.offset = pose.pose();
            frameData.views.append(WTF::move(frameDataView));
        }

        auto viewTextureMapLeft = cp_view_get_view_texture_map(cp_drawable_get_view(drawable, 0));
        auto viewTextureMapRight = cp_view_get_view_texture_map(cp_drawable_get_view(drawable, 1));
        MTLViewport viewportLeft = cp_view_texture_map_get_viewport(viewTextureMapLeft);
        MTLViewport viewportRight = cp_view_texture_map_get_viewport(viewTextureMapRight);
        size_t textureIndexLeft = cp_view_texture_map_get_texture_index(viewTextureMapLeft);
        size_t textureIndexRight = cp_view_texture_map_get_texture_index(viewTextureMapRight);

        id<MTLTexture> colorTextureLeft = cp_drawable_get_color_texture(drawable, textureIndexLeft);
        id<MTLTexture> colorTextureRight = cp_drawable_get_color_texture(drawable, textureIndexRight);
        RELEASE_ASSERT(colorTextureLeft);
        id<MTLTexture> depthTexture = cp_drawable_get_depth_texture(drawable, textureIndexLeft);
        RELEASE_ASSERT(depthTexture);
        PlatformXR::FrameData::ExternalTexture colorTextureSendRight;
        PlatformXR::FrameData::ExternalTexture depthStencilBufferSendRight;
        size_t reusableTextureIndex = 0;
        auto found = indexOfReusableTextures(colorTextureLeft, depthTexture);
        if (found) {
            reusableTextureIndex = *found;
            colorTextureSendRight = { MachSendRight(), false };
        } else {
            reusableTextureIndex = m_compositorTextures.size();
            if (reusableTextureIndex >= reusableTextureMaxCount) {
                m_compositorTextures.clear();
                reusableTextureIndex = 0;
            }
            RELEASE_LOG_INFO(XR, "CompositorCoordinator: found new color texture %p and depth texture %p at index %d", colorTextureLeft, depthTexture, (int)reusableTextureIndex);
            m_compositorTextures.append(std::tuple { colorTextureLeft, depthTexture });
            colorTextureSendRight = makeMachSendRight(colorTextureLeft);
            depthStencilBufferSendRight = makeMachSendRight(depthTexture);
        }

        auto renderingFrameIndex = cp_frame_get_frame_index(frame);

        // FIXME: rdar://77858090
        auto layerData = PlatformXR::FrameData::LayerData {
            .layerSetup = std::nullopt,
            .renderingFrameIndex = renderingFrameIndex,
            .textureData = PlatformXR::FrameData::ExternalTextureData {
                .reusableTextureIndex = reusableTextureIndex,
                .colorTexture = colorTextureSendRight,
                .depthStencilBuffer = depthStencilBufferSendRight,
            },
            // FIXME: rdar://134998122
            .requestDepth = m_forwardDepthAvailable,
        };

        MTLRasterizationRateMapDescriptor* desc = cp_drawable_get_rasterization_rate_map_descriptor(drawable, textureIndexLeft);
        id<MTLRasterizationRateMap> rateMap = cp_drawable_get_rasterization_rate_map(drawable, 0);
        auto leftIndex = cp_view_texture_map_get_slice_index(viewTextureMapLeft);
        auto rightIndex = cp_view_texture_map_get_slice_index(viewTextureMapRight);

        MTLSize physicalSizeLeft = [rateMap physicalSizeForLayer:leftIndex];
        MTLSize physicalSizeRight = [rateMap physicalSizeForLayer:rightIndex];
        id<MTLEvent> completionEvent = cp_layer_renderer_get_completion_event(m_cpLayer.get());
        ASSERT([completionEvent conformsToProtocol:@protocol(MTLSharedEvent)]);
        RetainPtr<MTLSharedEventHandle> completionHandle = adoptNS([(id<MTLSharedEvent>)completionEvent newSharedEventHandle]);
        auto completionPort = MachSendRight::create([completionHandle.get() eventPort]);

        constexpr auto layerSizeMax = PlatformXR::FrameData::LayerSetupSizeMax;
        RELEASE_ASSERT(colorTextureLeft.width <= layerSizeMax && colorTextureLeft.height <= layerSizeMax && physicalSizeLeft.width <= layerSizeMax && physicalSizeLeft.height <= layerSizeMax && physicalSizeRight.width <= layerSizeMax && physicalSizeRight.height <= layerSizeMax);

        PlatformXR::RateMapDescription foveationRateMapDesc;
        if (m_foveationEnabled) {
            foveationRateMapDesc.screenSize = WebCore::IntSize(rateMap.screenSize.width, rateMap.screenSize.height);
            foveationRateMapDesc.horizontalSamplesLeft = std::span { desc.layers[leftIndex].horizontalSampleStorage, desc.layers[leftIndex].sampleCount.width };
            foveationRateMapDesc.horizontalSamplesRight = std::span { desc.layers[rightIndex].horizontalSampleStorage, desc.layers[rightIndex].sampleCount.width };
            foveationRateMapDesc.verticalSamples = std::span { desc.layers[leftIndex].verticalSampleStorage, desc.layers[leftIndex].sampleCount.height };
        }

        // Adjust right viewport to "sit next to" the left viewport
        viewportRight.originX += viewportLeft.width;

        auto leftPhysicalSize = rateMap ? convertToLayerSetupSize(physicalSizeLeft) : convertToLayerSetupSize(colorTextureLeft);
        auto rightPhysicalSize = m_layeredModeEnabled ? convertToLayerSetupSize(physicalSizeRight) : convertToLayerSetupSize(MTLSizeMake(0, 0, 0));

        auto leftActualSize = convertToLayerSetupSize(colorTextureLeft);
        auto rightActualSize = convertToLayerSetupSize(colorTextureRight);
        layerData.layerSetup = PlatformXR::FrameData::LayerSetupData {
            .physicalSize = { leftPhysicalSize, rightPhysicalSize },
            .actualSize = { leftActualSize, rightActualSize },
            .viewports = { convertToIntRect(viewportLeft), convertToIntRect(viewportRight) },
            .foveationRateMapDesc = foveationRateMapDesc,
            .completionSyncEvent = WTF::move(completionPort),
        };

        auto layerDataRef = makeUniqueRef<PlatformXR::FrameData::LayerData>(layerData);
        frameData.layers.set(defaultLayerHandle(), WTF::move(layerDataRef));
        frameData.shouldRender = true;

        tracePoint(WebXRCPFrameStartSubmissionStart);
        cp_frame_start_submission(frame);
        tracePoint(WebXRCPFrameStartSubmissionEnd);

        // FIXME: rdar://175742010
        callOnMainRunLoop([callback = WTF::move(m_onFrameUpdate), frameData = WTF::move(frameData)]() mutable {
            callback(WTF::move(frameData));
        });

        // We should make sure the CPU work for the frame render is done before calling
        // cp_drawable_present() which is when the shared completion metal event is placed.
        // Otherwise, any stalls or hangs from handling rAF callback could result in GPU timeout errors.
        // While waiting for CPU work for the frame render to be done in the web process, periodically
        // check the renderer layer's state to see if the session has ended either by the system or
        // the web content.
        bool skipFramePresentation = false;
        while (!m_renderSemaphore->waitFor(300_ms)) {
            if (!processEvent()) {
                if (m_terminationPending)
                    RELEASE_LOG(XR, "CompositorCoordinator: Web content has ended the session during frame update: %llu", renderingFrameIndex);
                else
                    RELEASE_LOG(XR, "CompositorCoordinator: Layer invalidated during frame update: %llu", renderingFrameIndex);

                skipFramePresentation = true;
                break;
            }

            RELEASE_LOG_DEBUG(XR, "CompositorCoordinator: Continue to wait for frame %llu to finish update", renderingFrameIndex);
        }
        if (skipFramePresentation)
            return;

        if (arDeviceAnchor) {
            // Must use cp_drawable_set_device_anchor (not the simd version) for re-centering to work.
            cp_drawable_set_device_anchor(drawable, arDeviceAnchor.get());
        } else if ([m_xrTrackingManager isWorldTrackingSupported])
            RELEASE_LOG_ERROR(XR, "Failed to set device anchor on current frame");

        cp_drawable_present(drawable);

        tracePoint(WebXRCPFrameEndSubmissionStart);
        cp_frame_end_submission(frame);
        tracePoint(WebXRCPFrameEndSubmissionEnd);
    }
}

void CompositorCoordinator::terminateSession(WebPageProxy& page, PlatformXRSessionEndReason endReason)
{
    ASSERT(RunLoop::isMain());
    if (!m_cpLayer)
        return;

    page.uiClient().endXRSession(page, endReason);
}

void CompositorCoordinator::currentSessionHasEnded()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_headsetIdentifier);

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    cleanUpVisibilityPropagationViewsIfNeeded();
#endif

    m_cpLayer = nil;
    m_sessionStartPendingPageIdentifier = std::nullopt;
    m_sessionPageIdentifier = std::nullopt;
    m_sessionPage = nullptr;
    if (m_applicationDidEnterBackgroundObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:m_applicationDidEnterBackgroundObserver.get()];
        m_applicationDidEnterBackgroundObserver = nil;
    }
    if (m_applicationDidBecomeActiveObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:m_applicationDidBecomeActiveObserver.get()];
        m_applicationDidBecomeActiveObserver = nil;
    }
    [[m_spatialGestureRecognizer view] removeGestureRecognizer:m_spatialGestureRecognizer.get()];
    m_spatialGestureRecognizer = nil;
    m_xrTrackingManager = nil;
#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    m_lastSecurityOriginGrantedHandTracking = std::nullopt;
#endif
    m_compositorTextures.clear();

#if HAVE(SPATIAL_CONTROLLERS)
    m_controllerManager = nil;
#endif

    // m_onFrameUpdate can be non-null here if the presentation session is paused and
    // the session is terminated before the frame update can happen.
    if (m_onFrameUpdate) {
        m_onFrameUpdate({ });
        m_onFrameUpdate = nullptr;
    }

    if (m_sessionEventClient)
        m_sessionEventClient->sessionDidEnd(m_headsetIdentifier.value());

    m_sessionEventClient = nullptr;
    m_renderSemaphore = nullptr;
    m_updateThread = nullptr;
    m_terminationPending = false;

    RELEASE_LOG(XR, "CompositorCoordinator: Ended immersive session");
}

void CompositorCoordinator::didCompleteSessionSetup(WebPageProxy& page, WebCore::PageIdentifier pageIdentifier, const WebCore::SecurityOriginData& securityOriginData, const PlatformXR::Device::FeatureList& requestedFeatures, UIViewController *viewController)
{
    ASSERT(RunLoop::isMain());
    ASSERT(page.webPageIDInMainFrameProcess() == pageIdentifier);

    if (!m_cpLayer) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: failed to create immersive session");
        currentSessionHasEnded();
        return;
    }

    if (!m_sessionStartPendingPageIdentifier) {
        RELEASE_LOG_ERROR(XR, "CompositorCoordinator: Page has ended immersive session before the system has finished initializing it");
        terminateSession(page, PlatformXRSessionEndReason::NoError);
        currentSessionHasEnded();
        return;
    }

    ASSERT(m_sessionStartPendingPageIdentifier == pageIdentifier);
    RELEASE_LOG(XR, "CompositorCoordinator: started immersive session");

    m_sessionStartPendingPageIdentifier = std::nullopt;
    m_sessionPageIdentifier = pageIdentifier;
    m_sessionPage = page;
    m_paused = true;
    m_backgrounded = false;
    m_visibilityState = PlatformXR::VisibilityState::Visible;
    m_applicationDidEnterBackgroundObserver = [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationWillResignActiveNotification object:[UIApplication sharedApplication] queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *) {
        if (!m_terminationPending)
            setBackgrounded(true);
    }];
    m_applicationDidBecomeActiveObserver = [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication] queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *) {
        if (!m_terminationPending)
            setBackgrounded(false);
    }];

    m_compositorTextures.clear();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (viewController.view)
        setUpVisibilityPropagationViews(page, viewController.view);
#endif

    setUpDepthTextures();

    if (m_sessionEventClient) {
        uint32_t initialWidth = 0;
        uint32_t initialHeight = 0;
        uint32_t initialArrayLength = 0;
        if (id swapchainObject = (__bridge id)cp_layer_renderer_get_swapchain(m_cpLayer.get()); [swapchainObject isKindOfClass:getCP_OBJECT_cp_swapchainClassSingleton()]) {
            CP_OBJECT_cp_swapchain *swapchain = (CP_OBJECT_cp_swapchain *)swapchainObject;
            for (cp_swapchain_link_t swapchainLink in swapchain.swapchainLinks) {
                id<MTLTexture> referenceTexture = swapchainLink.depthTextures.firstObject;
                if (!referenceTexture)
                    continue;
                initialWidth = static_cast<uint32_t>(referenceTexture.width);
                initialHeight = static_cast<uint32_t>(referenceTexture.height);
                initialArrayLength = static_cast<uint32_t>(referenceTexture.arrayLength);
                break;
            }
        }
        if (initialWidth && initialHeight && initialArrayLength)
            m_sessionEventClient->sessionDidInitializeRendering(m_headsetIdentifier.value(), initialWidth, initialHeight, initialArrayLength);
    }

    BOOL handTrackingEnabled = NO;
#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    if (requestedFeatures.contains(PlatformXR::SessionFeature::HandTracking) && m_lastSecurityOriginGrantedHandTracking && securityOriginData == m_lastSecurityOriginGrantedHandTracking.value())
        handTrackingEnabled = YES;
#endif

#if HAVE(SPATIAL_CONTROLLERS)
    // FIXME: rdar://135355805
    RefPtr sessionPage = m_sessionPage.get();
    if (sessionPage && sessionPage->preferences().touchInputCompatibilityEnabled())
        m_controllerManager = adoptNS([[WKXRControllerManager alloc] init]);
#endif
    m_xrTrackingManager = adoptNS([[WKXRTrackingManager alloc] initWithHandTrackingEnabled:handTrackingEnabled layerRenderer:m_cpLayer.get()
#if HAVE(SPATIAL_CONTROLLERS)
    controllerManager:m_controllerManager
#endif
    ]);
    m_spatialGestureRecognizer = adoptNS([[WKSpatialGestureRecognizer alloc] init]);
    [m_spatialGestureRecognizer setSpatialGestureRecognizerDelegate:m_xrTrackingManager.get()];
    [viewController.view addGestureRecognizer:m_spatialGestureRecognizer.get()];
    ASSERT(!m_renderSemaphore);
    m_renderSemaphore = makeUnique<BinarySemaphore>();
    m_updateThread = Thread::create("Compositor session renderer"_s, [this] {
        update();
    });
}

void CompositorCoordinator::setUpDepthTextures()
{
    ASSERT(m_cpLayer);

    id swapchainObject = (__bridge id)cp_layer_renderer_get_swapchain(m_cpLayer.get());
    if (![swapchainObject isKindOfClass:getCP_OBJECT_cp_swapchainClassSingleton()])
        return;

    CP_OBJECT_cp_swapchain *swapchain = (CP_OBJECT_cp_swapchain *)swapchainObject;
    ASSERT(swapchain);

    auto mtlDevice = cp_layer_renderer_get_device(m_cpLayer.get());
    ASSERT(mtlDevice);

    RetainPtr mtlCommandQueue = adoptNS([mtlDevice newCommandQueue]);
    ASSERT(mtlCommandQueue);
    id<MTLCommandBuffer> commandBuffer = [mtlCommandQueue commandBuffer];
    ASSERT(commandBuffer);

    for (cp_swapchain_link_t swapchainLink in swapchain.swapchainLinks) {
        for (id<MTLTexture> depthTexture in swapchainLink.depthTextures) {
            for (NSUInteger slice = 0; slice < depthTexture.arrayLength; slice++) {
                MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                passDescriptor.depthAttachment.texture = depthTexture;
                passDescriptor.depthAttachment.slice = slice;
                passDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
                passDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
                passDescriptor.depthAttachment.clearDepth = FLT_MIN;

                id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
                [commandEncoder endEncoding];
            }
        }
    }
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
}

void CompositorCoordinator::sessionRequestIsCancelled()
{
#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    m_lastSecurityOriginGrantedHandTracking = std::nullopt;
#endif
}

void CompositorCoordinator::getSupportedFeatures(WebPageProxy& page)
{
    if (!m_supportedVRFeatures.isEmpty() || !m_supportedARFeatures.isEmpty())
        return;

    page.uiClient().supportedXRSessionFeatures(m_supportedVRFeatures, m_supportedARFeatures);
}

std::optional<size_t> CompositorCoordinator::indexOfReusableTextures(id<MTLTexture> colorTexture, id<MTLTexture> depthTexture)
{
    for (size_t i = 0; i < m_compositorTextures.size(); ++i) {
        auto [cachedColorTexture, cachedDepthTexture] = m_compositorTextures[i];
        if (cachedColorTexture && cachedColorTexture.get() == colorTexture && cachedDepthTexture && cachedDepthTexture.get() == depthTexture)
            return i;
    }

    return std::nullopt;
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void CompositorCoordinator::setUpVisibilityPropagationViews(WebPageProxy& page, UIView *view)
{
    if (!m_visibilityPropagationViewForWebProcess) {
        auto processID = page.legacyMainFrameProcessID();
        if (processID) {
            auto environmentIdentifier = page.legacyMainFrameProcess().environmentIdentifier();
            m_visibilityPropagationViewForWebProcess = adoptNS([[_UINonHostingVisibilityPropagationView alloc] initWithFrame:CGRectZero pid:processID environmentIdentifier:environmentIdentifier.createNSString().get()]);
            RELEASE_LOG_INFO(XR, "Created visibility propagation view %p from immersive scene to WebContent process with PID=%d", m_visibilityPropagationViewForWebProcess.get(), processID);
            [view addSubview:m_visibilityPropagationViewForWebProcess.get()];
        }
    }

#if ENABLE(GPU_PROCESS)
    if (m_visibilityPropagationViewForGPUProcess)
        return;

    RefPtr gpuProcess = page.configuration().processPool().gpuProcess();
    if (!gpuProcess)
        return;

    auto processID = gpuProcess->processID();
    auto contextID = page.contextIDForVisibilityPropagationInGPUProcess();
    if (!processID || !contextID)
        return;

    // Propagate the view's visibility state to the GPU process so that it is marked as "Foreground Running" when necessary.
    m_visibilityPropagationViewForGPUProcess = adoptNS([[_UILayerHostView alloc] initWithFrame:CGRectZero pid:processID contextID:contextID]);
    RELEASE_LOG_INFO(XR, "Created visibility propagation view %p (contextID=%u) for GPU process with PID=%d", m_visibilityPropagationViewForGPUProcess.get(), contextID, processID);
    [view addSubview:m_visibilityPropagationViewForGPUProcess.get()];
#endif // ENABLE(GPU_PROCESS)
}

void CompositorCoordinator::cleanUpVisibilityPropagationViewsIfNeeded()
{
    if (m_visibilityPropagationViewForWebProcess) {
        RELEASE_LOG_INFO(XR, "Removing visibility propagation view for web process %p from immersive scene", m_visibilityPropagationViewForWebProcess.get());
        [m_visibilityPropagationViewForWebProcess removeFromSuperview];
        m_visibilityPropagationViewForWebProcess = nullptr;
    }

#if ENABLE(GPU_PROCESS)
    if (m_visibilityPropagationViewForGPUProcess) {
        RELEASE_LOG_INFO(XR, "Removing visibility propagation view %p for GPU process from immersive scene", m_visibilityPropagationViewForGPUProcess.get());
        [m_visibilityPropagationViewForGPUProcess removeFromSuperview];
        m_visibilityPropagationViewForGPUProcess = nullptr;
    }
#endif // ENABLE(GPU_PROCESS)
}
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

} // namespace WebKit

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBXR) && USE(COMPOSITORXR)
