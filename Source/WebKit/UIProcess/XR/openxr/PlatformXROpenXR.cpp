/*
 * Copyright (C) 2025 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "PlatformXROpenXR.h"

#if ENABLE(WEBXR) && USE(OPENXR)

#include "APIUIClient.h"
#include "OpenXRExtensions.h"
#include "OpenXRLayer.h"
#include "OpenXRUtils.h"
#include "WebPageProxy.h"
#if USE(LIBEPOXY)
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif
#include <WebCore/GLContext.h>
#include <WebCore/PlatformDisplaySurfaceless.h>
#include <openxr/openxr_platform.h>
#include <wtf/RunLoop.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {

struct OpenXRCoordinator::RenderState {
    std::atomic<bool> terminateRequested;
    PlatformXR::Device::RequestFrameCallback onFrameUpdate;
    BinarySemaphore presentFrame;
    XrFrameState frameState;
};

OpenXRCoordinator::OpenXRCoordinator()
{
    ASSERT(RunLoop::isMain());
}

OpenXRCoordinator::~OpenXRCoordinator()
{
    cleanupSessionAndAssociatedResources();

    m_views.clear();
    m_viewConfigurationViews.clear();

    if (m_instance != XR_NULL_HANDLE)
        xrDestroyInstance(m_instance);
}

void OpenXRCoordinator::getPrimaryDeviceInfo(WebPageProxy&, DeviceInfoCallback&& callback)
{
    ASSERT(RunLoop::isMain());

    initializeDevice();
    if (m_instance == XR_NULL_HANDLE || m_systemId == XR_NULL_SYSTEM_ID) {
        LOG(XR, "Failed to initialize OpenXR system");
        callback(std::nullopt);
        return;
    }

    auto supportsOrientationTracking = [instance = m_instance, system = m_systemId]() -> bool {
        XrSystemProperties systemProperties = createOpenXRStruct<XrSystemProperties, XR_TYPE_SYSTEM_PROPERTIES>();
        CHECK_XRCMD(xrGetSystemProperties(instance, system, &systemProperties));
        return systemProperties.trackingProperties.orientationTracking == XR_TRUE;
    };

    auto recommendedResolution = [&views = m_viewConfigurationViews]() -> WebCore::IntSize {
        // OpenXR is very flexible wrt views resolution, but the current WebKit architecture expects a single resolution for all views.
        return { static_cast<int>(views.size() * views.first().recommendedImageRectWidth), static_cast<int>(views.first().recommendedImageRectHeight) };
    };

    XRDeviceInfo deviceInfo { .identifier = m_deviceIdentifier, .vrFeatures = { }, .arFeatures = { } };
    deviceInfo.supportsOrientationTracking = supportsOrientationTracking();
    deviceInfo.supportsStereoRendering = m_currentViewConfiguration == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    deviceInfo.recommendedResolution = recommendedResolution();
    LOG(XR, "OpenXR device info:\n\tOrientation tracking: %s\n\tStereo rendering: %s\n\tRecommended resolution: %dx%d", deviceInfo.supportsOrientationTracking ? "yes" : "no", deviceInfo.supportsStereoRendering ? "yes" : "no", deviceInfo.recommendedResolution.width(), deviceInfo.recommendedResolution.height());

    // OpenXR runtimes MUST support VIEW and LOCAL reference spaces.
    deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeViewer);
    deviceInfo.arFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeViewer);
    deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocal);
    deviceInfo.arFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocal);

    if (m_extensions->isExtensionSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME ""_span)) {
        deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded);
        deviceInfo.arFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded);
    }

    // In order to get the supported reference space types, we need the session to be created. However at this point we shouldn't do it.
    // Instead, we report ReferenceSpaceTypeLocalFloor as available, because we can supoport it via either the STAGE reference space, the
    // LOCAL_FLOOR reference space or even via an educated guess from the LOCAL reference space as a backup.
    deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor);
    deviceInfo.arFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor);

    callback(WTFMove(deviceInfo));
}

void OpenXRCoordinator::requestPermissionOnSessionFeatures(WebPageProxy& page, const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, FeatureListCallback&& callback)
{
    LOG(XR, "OpenXRCoordinator::requestPermissionOnSessionFeatures");
    if (mode == PlatformXR::SessionMode::Inline) {
        callback(granted);
        return;
    }

    page.uiClient().requestPermissionOnXRSessionFeatures(page, securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, [callback = WTFMove(callback)](std::optional<Vector<PlatformXR::SessionFeature>> userGranted) mutable {
        callback(WTFMove(userGranted));
    });
}

bool OpenXRCoordinator::collectSwapchainFormatsIfNeeded()
{
    ASSERT(RunLoop::isMain());
    if (!m_supportedSwapchainFormats.isEmpty())
        return true;

    uint32_t formatCount;
    CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr));
    if (!formatCount) {
        LOG(XR, "xrEnumerateSwapchainFormats(): no formats available");
        return false;
    }

    m_supportedSwapchainFormats.resize(formatCount);
    CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, m_supportedSwapchainFormats.mutableSpan().data()));
#if !LOG_DISABLED
    LOG(XR, "OpenXR: %d supported swapchain format%c", formatCount, formatCount > 1 ? 's' : ' ');
    for (auto& format : m_supportedSwapchainFormats)
        LOG(XR, "\t%ld", format);
#endif
    return true;
}

std::unique_ptr<OpenXRSwapchain> OpenXRCoordinator::createSwapchain(uint32_t width, uint32_t height, bool alpha)
{
    auto preferredFormat = alpha ? GL_RGBA8 : GL_RGB8;
    auto format = m_supportedSwapchainFormats.contains(preferredFormat) ? preferredFormat : m_supportedSwapchainFormats.first();
    auto sampleCount = m_viewConfigurationViews.isEmpty() ? 1 : m_viewConfigurationViews.first().recommendedSwapchainSampleCount;

    auto info = createOpenXRStruct<XrSwapchainCreateInfo, XR_TYPE_SWAPCHAIN_CREATE_INFO>();
    info.arraySize = 1;
    info.format = format;
    info.width = width;
    info.height = height;
    info.mipCount = 1;
    info.faceCount = 1;
    info.sampleCount = sampleCount;
    info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    return OpenXRSwapchain::create(m_instance, m_session, info);
}

void OpenXRCoordinator::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
    ASSERT(RunLoop::isMain());
    if (!collectSwapchainFormatsIfNeeded()) {
        RELEASE_LOG(XR, "OpenXRCoordinator: no supported swapchain formats");
        return;
    }

    auto swapchain = createSwapchain(width, height, alpha);
    if (!swapchain) {
        RELEASE_LOG(XR, "OpenXRCoordinator: failed to create swapchain");
        return;
    }

    if (auto layer = OpenXRLayerProjection::create(WTFMove(swapchain)))
        m_layers.add(defaultLayerHandle(), WTFMove(layer));
}

void OpenXRCoordinator::startSession(WebPageProxy& page, WeakPtr<PlatformXRCoordinatorSessionEventClient>&& sessionEventClient, const WebCore::SecurityOriginData&, PlatformXR::SessionMode sessionMode, const PlatformXR::Device::FeatureList&)
{
    ASSERT(RunLoop::isMain());
    LOG(XR, "OpenXRCoordinator::startSession");

    WTF::switchOn(m_state,
        [&](Idle&) {
            m_sessionMode = sessionMode;
            createSessionIfNeeded();
            if (m_session == XR_NULL_HANDLE) {
                LOG(XR, "OpenXRCoordinator: failed to create the session");
                return;
            }

            auto renderState = Box<RenderState>::create();
            renderState->terminateRequested = false;

            m_state = Active {
                .sessionEventClient = WTFMove(sessionEventClient),
                .pageIdentifier = page.webPageIDInMainFrameProcess(),
                .renderState = renderState,
                .renderThread = Thread::create("OpenXR render thread"_s, [this, renderState] { renderLoop(renderState); }),
            };
        },
        [&](Active&) {
            RELEASE_LOG_ERROR(XR, "OpenXRCoordinator: an existing immersive session is active");
            if (RefPtr protectedSessionEventClient = sessionEventClient.get())
                protectedSessionEventClient->sessionDidEnd(m_deviceIdentifier);
        });
}

void OpenXRCoordinator::endSessionIfExists(WebPageProxy& page)
{
    LOG(XR, "OpenXRCoordinator: endSessionIfExists");
    endSessionIfExists(page.webPageIDInMainFrameProcess());
}

void OpenXRCoordinator::endSessionIfExists(std::optional<WebCore::PageIdentifier> pageIdentifier)
{
    ASSERT(RunLoop::isMain());

    WTF::switchOn(m_state,
        [&](Idle&) { },
        [&](Active& active) {
            if (pageIdentifier && active.pageIdentifier != *pageIdentifier) {
                LOG(XR, "OpenXRCoordinator: trying to end an immersive session owned by another page");
                return;
            }
            if (active.renderState->terminateRequested)
                return;

            // OpenXR will transition the session to STOPPING state and then we will call xrEndSession().
            CHECK_XRCMD(xrRequestExitSession(m_session));

            active.renderState->presentFrame.signal();
            active.renderThread->waitForCompletion();

            if (active.renderState->onFrameUpdate)
                active.renderState->onFrameUpdate({ });

            auto& sessionEventClient = active.sessionEventClient;
            if (sessionEventClient) {
                LOG(XR, "... immersive session end sent");
                sessionEventClient->sessionDidEnd(m_deviceIdentifier);
            }

            m_state = Idle { };
        });
}

void OpenXRCoordinator::scheduleAnimationFrame(WebPageProxy& page, std::optional<PlatformXR::RequestData>&&, PlatformXR::Device::RequestFrameCallback&& onFrameUpdateCallback)
{
    WTF::switchOn(m_state,
        [&](Idle&) {
            RELEASE_LOG(XR, "OpenXRCoordinator: trying to schedule frame update for an inactive session");
            onFrameUpdateCallback({ });
        },
        [&](Active& active) {
            if (active.pageIdentifier != page.webPageIDInMainFrameProcess()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to schedule frame update for session owned by another page");
                return;
            }

            if (active.renderState->terminateRequested) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to schedule frame for terminating session");
                onFrameUpdateCallback({ });
            }

            active.renderState->onFrameUpdate = WTFMove(onFrameUpdateCallback);
        });
}

void OpenXRCoordinator::submitFrameInternal(Box<RenderState> renderState, Vector<XRDeviceLayer>&& layers)
{
    WebCore::GLContext::ScopedGLContextCurrent scopedContext(*m_glContext);
    Vector<const XrCompositionLayerBaseHeader*, 1> frameEndLayers;
    for (auto& layer : layers) {
        auto it = m_layers.find(layer.handle);
        if (it == m_layers.end()) {
            LOG(XR, "Didn't find a OpenXRLayer with %d handle", layer.handle);
            continue;
        }
        auto header = it->value->endFrame(layer, m_localSpace, m_views);
        if (!header) {
            LOG(XR, "endFrame() call failed in OpenXRLayer with %d handle", layer.handle);
            continue;
        }

        frameEndLayers.append(header);
    }

    XrFrameEndInfo frameEndInfo = createOpenXRStruct<XrFrameEndInfo, XR_TYPE_FRAME_END_INFO>();
    frameEndInfo.displayTime = renderState->frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = m_sessionMode == PlatformXR::SessionMode::ImmersiveAr ? m_arBlendMode : m_vrBlendMode;
    frameEndInfo.layerCount = static_cast<uint32_t>(frameEndLayers.size());
    frameEndInfo.layers = frameEndLayers.mutableSpan().data();
    CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
}

void OpenXRCoordinator::submitFrame(WebPageProxy& page, Vector<XRDeviceLayer>&& layers)
{
    ASSERT(RunLoop::isMain());
    WTF::switchOn(m_state,
        [&](Idle&) {
            RELEASE_LOG(XR, "OpenXRCoordinator: trying to submit frame update for an inactive session");
        },
        [&](Active& active) {
            if (active.pageIdentifier != page.webPageIDInMainFrameProcess()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to submit frame update for session owned by another page");
                return;
            }

            if (active.renderState->terminateRequested.load()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to submit frame update for a terminating session");
                return;
            }

            submitFrameInternal(active.renderState, WTFMove(layers));
            active.renderState->presentFrame.signal();
        });
}

void OpenXRCoordinator::createInstance()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_instance == XR_NULL_HANDLE);

    Vector<char *, 2> extensions;
#if defined(XR_USE_PLATFORM_EGL)
    if (m_extensions->isExtensionSupported(XR_MNDX_EGL_ENABLE_EXTENSION_NAME ""_span))
        extensions.append(const_cast<char*>(XR_MNDX_EGL_ENABLE_EXTENSION_NAME));
#endif
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    extensions.append(const_cast<char*>(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME));
#endif

    XrInstanceCreateInfo createInfo = createOpenXRStruct<XrInstanceCreateInfo, XR_TYPE_INSTANCE_CREATE_INFO >();
    createInfo.applicationInfo = { "WebKit", 1, "WebKit", 1, XR_CURRENT_API_VERSION };
    createInfo.enabledApiLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.mutableSpan().data();

    CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));
}

void OpenXRCoordinator::collectViewConfigurations()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_instance != XR_NULL_HANDLE);

    uint32_t viewConfigurationCount;
    CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigurationCount, nullptr));

    if (!viewConfigurationCount)
        return;

    Vector<XrViewConfigurationType> viewConfigurations(viewConfigurationCount);
    CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations.mutableSpan().data()));

    const XrViewConfigurationType preferredViewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    m_currentViewConfiguration = viewConfigurations.find(preferredViewConfiguration) != notFound ? preferredViewConfiguration : viewConfigurations.first();
    LOG(XR, "OpenXR selected view configurations: %s", toString(m_currentViewConfiguration));

    uint32_t viewCount;
    CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_currentViewConfiguration, 0, &viewCount, nullptr));
    if (!viewCount) {
        LOG(XR, "No views available for configuration type %s", toString(m_currentViewConfiguration));
        return;
    }

    m_views.resize(viewCount);

    m_viewConfigurationViews.fill(createOpenXRStruct<XrViewConfigurationView, XR_TYPE_VIEW_CONFIGURATION_VIEW>(), viewCount);
    CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_currentViewConfiguration, viewCount, &viewCount, m_viewConfigurationViews.mutableSpan().data()));
}

void OpenXRCoordinator::initializeSystem()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_instance != XR_NULL_HANDLE);

    XrSystemGetInfo systemInfo = createOpenXRStruct<XrSystemGetInfo, XR_TYPE_SYSTEM_GET_INFO>();
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));
}

void OpenXRCoordinator::initializeDevice()
{
    ASSERT(RunLoop::isMain());

    if (m_instance != XR_NULL_HANDLE)
        return;

    m_extensions = OpenXRExtensions::create();
    if (!m_extensions) {
        LOG(XR, "Failed to create OpenXRExtensions.");
        return;
    }

    createInstance();
    if (m_instance == XR_NULL_HANDLE) {
        LOG(XR, "Failed to create OpenXR instance.");
        return;
    }

    if (!m_extensions->loadMethods(m_instance)) {
        LOG(XR, "Failed to load extension methods.");
        return;
    }

    initializeSystem();
    if (m_systemId == XR_NULL_SYSTEM_ID) {
        LOG(XR, "Failed to get OpenXR system ID.");
        return;
    }

    collectViewConfigurations();
    initializeBlendModes();
}

void OpenXRCoordinator::initializeBlendModes()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_instance != XR_NULL_HANDLE);
    ASSERT(m_viewConfigurationViews.size());

    uint32_t count;
    CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, m_currentViewConfiguration, 0, &count, nullptr));
    ASSERT(count);

    Vector<XrEnvironmentBlendMode> blendModes(count);
    CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, m_currentViewConfiguration, count, &count, blendModes.mutableSpan().data()));

#if !LOG_DISABLED
    LOG(XR, "OpenXR: %d supported blend mode%c", count, count > 1 ? 's' : ' ');
    for (const auto& blendMode : blendModes)
        LOG(XR, "\t%s", toString(blendMode));
#endif

    const bool supportsOpaqueBlendMode = blendModes.contains(XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
    const bool supportsAdditiveBlendMode = blendModes.contains(XR_ENVIRONMENT_BLEND_MODE_ADDITIVE);
    const bool supportsAlphaBlendMode = blendModes.contains(XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND);
    ASSERT(supportsOpaqueBlendMode || supportsAdditiveBlendMode || supportsAlphaBlendMode);

    m_arBlendMode = supportsAdditiveBlendMode ? XR_ENVIRONMENT_BLEND_MODE_ADDITIVE : (supportsAlphaBlendMode ? XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND : XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
    m_vrBlendMode = supportsOpaqueBlendMode ? XR_ENVIRONMENT_BLEND_MODE_OPAQUE : m_arBlendMode;
}

void OpenXRCoordinator::tryInitializeGraphicsBinding()
{
    if (!m_extensions->isExtensionSupported(XR_MNDX_EGL_ENABLE_EXTENSION_NAME ""_span)) {
        LOG(XR, "OpenXR MNDX_EGL_ENABLE extension is not supported.");
        return;
    }

    if (!m_platformDisplay) {
        m_platformDisplay = WebCore::PlatformDisplaySurfaceless::create();
        if (!m_platformDisplay) {
            LOG(XR, "Failed to create a platform display for OpenXR.");
            return;
        }
    }
    if (!m_glContext) {
        m_glContext = WebCore::GLContext::createOffscreen(*m_platformDisplay);
        if (!m_glContext) {
            LOG(XR, "Failed to create the GL context for OpenXR.");
            return;
        }
    }

    m_graphicsBinding = createOpenXRStruct<XrGraphicsBindingEGLMNDX, XR_TYPE_GRAPHICS_BINDING_EGL_MNDX>();
    m_graphicsBinding.display = m_platformDisplay->eglDisplay();
    m_graphicsBinding.context = m_glContext->platformContext();
    m_graphicsBinding.config = m_glContext->config();
    m_graphicsBinding.getProcAddress = m_extensions->methods().getProcAddressFunc;
}

void OpenXRCoordinator::createSessionIfNeeded()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_instance != XR_NULL_HANDLE);

    if (m_session != XR_NULL_HANDLE)
        return;

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    auto requirements = createOpenXRStruct<XrGraphicsRequirementsOpenGLESKHR, XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR>();
    CHECK_XRCMD(m_extensions->methods().xrGetOpenGLESGraphicsRequirementsKHR(m_instance, m_systemId, &requirements));
#endif

    tryInitializeGraphicsBinding();

    // Create the session.
    auto sessionCreateInfo = createOpenXRStruct<XrSessionCreateInfo, XR_TYPE_SESSION_CREATE_INFO>();
    sessionCreateInfo.systemId = m_systemId;
    sessionCreateInfo.next = &m_graphicsBinding;
    CHECK_XRCMD(xrCreateSession(m_instance, &sessionCreateInfo, &m_session));
}

void OpenXRCoordinator::cleanupSessionAndAssociatedResources()
{
    if (m_localSpace != XR_NULL_HANDLE) {
        CHECK_XRCMD(xrDestroySpace(m_localSpace));
        m_localSpace = XR_NULL_HANDLE;
    }

    if (m_floorSpace != XR_NULL_HANDLE) {
        CHECK_XRCMD(xrDestroySpace(m_floorSpace));
        m_floorSpace = XR_NULL_HANDLE;
    }

    m_layers.clear();

    if (m_session != XR_NULL_HANDLE) {
        CHECK_XRCMD(xrDestroySession(m_session));
        m_session = XR_NULL_HANDLE;
    }

    m_glContext.reset();
    m_platformDisplay.reset();
}


void OpenXRCoordinator::handleSessionStateChange(Box<RenderState> active)
{
    ASSERT(!RunLoop::isMain());

    switch (m_sessionState) {
    case XR_SESSION_STATE_READY: {
        auto sessionBeginInfo = createOpenXRStruct<XrSessionBeginInfo, XR_TYPE_SESSION_BEGIN_INFO>();
        sessionBeginInfo.primaryViewConfigurationType = m_currentViewConfiguration;
        CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
        m_isSessionRunning = true;
        break;
    }
    case XR_SESSION_STATE_STOPPING:
        // Once xrEndSession() is called we cannot longer call xrWaitFrame()->xrBeginFrame()->xrEndFrame() from any thread.
        // However we cannot terminate the thread just now as we need to call xrPollEvent() to handle the session state change.
        active->terminateRequested = true;
        CHECK_XRCMD(xrEndSession(m_session));
        m_isSessionRunning = false;
        break;
    case XR_SESSION_STATE_LOSS_PENDING:
    case XR_SESSION_STATE_EXITING:
        cleanupSessionAndAssociatedResources();
        break;
    default:
        LOG(XR, "OpenXR session state changed to %s", toString(m_sessionState));
        break;
    }
}

enum class OpenXRCoordinator::PollResult : bool { Stop, Continue };

OpenXRCoordinator::PollResult OpenXRCoordinator::pollEvents(Box<RenderState> active)
{
    ASSERT(!RunLoop::isMain());
    auto runtimeEvent = createOpenXRStruct<XrEventDataBuffer, XR_TYPE_EVENT_DATA_BUFFER>();
    while (xrPollEvent(m_instance, &runtimeEvent) == XR_SUCCESS) {
        switch (runtimeEvent.type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            LOG(XR, "OpenXR instance loss");
            return PollResult::Stop;
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            auto* event = reinterpret_cast<XrEventDataSessionStateChanged*>(&runtimeEvent);
            LOG(XR, "OpenXR session state changed: %s", toString(event->state));
            m_sessionState = event->state;
            handleSessionStateChange(active);
            return m_session == XR_NULL_HANDLE ? PollResult::Stop : PollResult::Continue;
        }
        default:
            LOG(XR, "Unhandled OpenXR event type %d\n", runtimeEvent.type);
        }
    }
    return PollResult::Continue;
}

PlatformXR::FrameData OpenXRCoordinator::populateFrameData(Box<RenderState> active)
{
    ASSERT(!RunLoop::isMain());
    WebCore::GLContext::ScopedGLContextCurrent scopedContext(*m_glContext);

    PlatformXR::FrameData frameData;
    frameData.predictedDisplayTime = active->frameState.predictedDisplayTime;
    frameData.shouldRender = active->frameState.shouldRender;
    if (!frameData.shouldRender)
        return frameData;

    XrViewLocateInfo viewLocateInfo = createOpenXRStruct<XrViewLocateInfo, XR_TYPE_VIEW_LOCATE_INFO>();
    viewLocateInfo.viewConfigurationType = m_currentViewConfiguration;
    viewLocateInfo.displayTime = active->frameState.predictedDisplayTime;
    viewLocateInfo.space = m_localSpace;

    uint32_t viewCapacityInput = static_cast<uint32_t>(m_views.size());
    m_views.fill(createOpenXRStruct<XrView, XR_TYPE_VIEW>(), viewCapacityInput);

    XrViewState viewState = createOpenXRStruct<XrViewState, XR_TYPE_VIEW_STATE>();
    uint32_t viewCountOutput;
    CHECK_XRCMD(xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.mutableSpan().data()));
    ASSERT(viewCountOutput == viewCapacityInput);

    for (auto& view : m_views)
        frameData.views.append(XrViewToView(view));

    frameData.isTrackingValid = viewState.viewStateFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    frameData.isPositionValid = viewState.viewStateFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT;
    frameData.isPositionEmulated = !(viewState.viewStateFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT);

    frameData.origin = XrIdentityPose();

    if (m_floorSpace != XR_NULL_HANDLE) {
        XrSpaceLocation floorLocation = createOpenXRStruct<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
        CHECK_XRCMD(xrLocateSpace(m_floorSpace, m_localSpace, active->frameState.predictedDisplayTime, &floorLocation));
        frameData.floorTransform = XrPosefToPose(floorLocation.pose);
    } else
        frameData.floorTransform = XrIdentityPose();

    for (auto& layer : m_layers) {
        auto layerData = layer.value->startFrame();
        if (layerData) {
            auto layerDataRef = makeUniqueRef<PlatformXR::FrameData::LayerData>(WTFMove(*layerData));
            frameData.layers.add(layer.key, WTFMove(layerDataRef));
        }
    }

    return frameData;
}

void OpenXRCoordinator::createReferenceSpacesIfNeeded(Box<RenderState> active)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_session != XR_NULL_HANDLE);
    if (m_localSpace != XR_NULL_HANDLE)
        return;

    uint32_t spaceCount;
    CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
    Vector<XrReferenceSpaceType> supportedSpaces(spaceCount);
    CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount, supportedSpaces.mutableSpan().data()));

    if (supportedSpaces.isEmpty()) {
        LOG(XR, "No reference spaces available for the current OpenXR session.");
        return;
    }

#if !LOG_DISABLED
    LOG(XR, "OpenXR reference spaces available:");
    for (const auto& spaceType : supportedSpaces)
        LOG(XR, "\t%s", toString(spaceType));
#endif

    auto createReferenceSpace = [&](XrReferenceSpaceType type) -> XrSpace {
        XrSpace referenceSpace = XR_NULL_HANDLE;
        XrReferenceSpaceCreateInfo createInfo = createOpenXRStruct<XrReferenceSpaceCreateInfo, XR_TYPE_REFERENCE_SPACE_CREATE_INFO>();
        createInfo.referenceSpaceType = type;
        createInfo.poseInReferenceSpace = { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        CHECK_XRCMD(xrCreateReferenceSpace(m_session, &createInfo, &referenceSpace));
        return referenceSpace;
    };

    m_localSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);

#if defined(XR_EXT_local_floor)
    if (supportedSpaces.contains(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT) && m_extensions->isExtensionSupported(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME ""_span)) {
        m_floorSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT);
        LOG(XR, "OpenXRCoordinator: created LOCAL_FLOOR reference space");
    }
#endif
    if (m_floorSpace != XR_NULL_HANDLE)
        return;

    // If neither LOCAL_FLOOR nor STAGE are available then we won't return any floorTransform information. WebXR will make an educated guess
    // in that case (see WebXRReferenceSpace::floorOriginTransform()).
    if (!supportedSpaces.contains(XR_REFERENCE_SPACE_TYPE_STAGE))
        return;

    // Build a LOCAL_FLOOR like reference space from local and stage spaces
    XrSpace stageSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_STAGE);

    XrSpaceLocation stageLocation = createOpenXRStruct<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
    CHECK_XRCMD(xrLocateSpace(stageSpace, m_localSpace, active->frameState.predictedDisplayTime , &stageLocation));
    CHECK_XRCMD(xrDestroySpace(stageSpace));

    float floorOffset = stageLocation.pose.position.y;

    XrReferenceSpaceCreateInfo localFloorCreateInfo = createOpenXRStruct<XrReferenceSpaceCreateInfo, XR_TYPE_REFERENCE_SPACE_CREATE_INFO>();
    localFloorCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    localFloorCreateInfo.poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, floorOffset, 0.f } };
    CHECK_XRCMD(xrCreateReferenceSpace(m_session, &localFloorCreateInfo, &m_floorSpace));
}

void OpenXRCoordinator::renderLoop(Box<RenderState> active)
{
    for (;;) {
        if (pollEvents(active) == PollResult::Stop)
            break;

        auto throttleThreadIfNeeded = [sessionState = m_sessionState]() {
            // Throttle the loop if the session is not running as xrWaitFrame() won't be called.
            if (sessionState < XR_SESSION_STATE_READY || sessionState >= XR_SESSION_STATE_STOPPING)
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
        };

        if (!active->onFrameUpdate || active->terminateRequested || !m_isSessionRunning) {
            throttleThreadIfNeeded();
            continue;
        }

        XrFrameWaitInfo frameWaitInfo = createOpenXRStruct<XrFrameWaitInfo, XR_TYPE_FRAME_WAIT_INFO>();
        XrFrameState frameState = createOpenXRStruct<XrFrameState, XR_TYPE_FRAME_STATE>();
        CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

        XrFrameBeginInfo frameBeginInfo = createOpenXRStruct<XrFrameBeginInfo, XR_TYPE_FRAME_BEGIN_INFO>();
        CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

        // We should not directly use active->frameState in the xrWaitFrame() in order not to override the previous (ongoing) value.
        active->frameState = frameState;

        createReferenceSpacesIfNeeded(active);
        PlatformXR::FrameData frameData = populateFrameData(active);

        callOnMainRunLoop([callback = WTFMove(active->onFrameUpdate), frameData = WTFMove(frameData)]() mutable {
            callback(WTFMove(frameData));
        });

        if (!active->frameState.shouldRender) {
            // We must always call xrEndFrame() if we had previously called xrBeginFrame(), even if we don't render anything. Don't wait
            // for submitFrame() as in the normal flow because it won't ever be called (see WebXRSession::onFrame()).
            submitFrameInternal(active, { });
            continue;
        }

        active->presentFrame.wait();

        throttleThreadIfNeeded();
    }

    LOG(XR, "OpenXRCoordinator::renderLoop exiting...");
}


} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
