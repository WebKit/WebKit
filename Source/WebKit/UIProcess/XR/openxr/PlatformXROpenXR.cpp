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

namespace WebKit {

struct OpenXRCoordinator::RenderState {
    std::atomic<bool> terminateRequested;
    PlatformXR::Device::RequestFrameCallback onFrameUpdate;
    XrFrameState frameState;
};

OpenXRCoordinator::OpenXRCoordinator()
{
    ASSERT(RunLoop::isMain());
}

OpenXRCoordinator::~OpenXRCoordinator()
{
    if (m_session != XR_NULL_HANDLE)
        xrDestroySession(m_session);

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
    RELEASE_LOG(XR, "OpenXRCoordinator::scheduleAnimationFrame");
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

void OpenXRCoordinator::submitFrameInternal(Box<RenderState> renderState)
{
    Vector<XrCompositionLayerBaseHeader*> layers;

    XrFrameEndInfo frameEndInfo = createOpenXRStruct<XrFrameEndInfo, XR_TYPE_FRAME_END_INFO>();
    frameEndInfo.displayTime = renderState->frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = m_sessionMode == PlatformXR::SessionMode::ImmersiveAr ? m_arBlendMode : m_vrBlendMode;
    frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
    frameEndInfo.layers = layers.mutableSpan().data();
    CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
}

void OpenXRCoordinator::submitFrame(WebPageProxy& page)
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

            submitFrameInternal(active.renderState);
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

WebCore::IntSize OpenXRCoordinator::recommendedResolution() const
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_viewConfigurations.size());

    uint32_t viewCount;
    CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_currentViewConfiguration, 0, &viewCount, nullptr));
    if (!viewCount) {
        LOG(XR, "No views available for configuration type %s", toString(m_currentViewConfiguration));
        return { 0, 0 };
    }

    Vector<XrViewConfigurationView> views(viewCount, createOpenXRStruct<XrViewConfigurationView, XR_TYPE_VIEW_CONFIGURATION_VIEW>());
    CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_currentViewConfiguration, viewCount, &viewCount, views.mutableSpan().data()));

    // OpenXR is very flexible wrt views resolution, but the current WebKit architecture expects a single resolution for all views.
    return { static_cast<int>(viewCount * views.first().recommendedImageRectWidth), static_cast<int>(views.first().recommendedImageRectHeight) };
}

void OpenXRCoordinator::collectViewConfigurations()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_instance != XR_NULL_HANDLE);

    uint32_t viewConfigurationCount;
    CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigurationCount, nullptr));

    if (!viewConfigurationCount)
        return;

    m_viewConfigurations.resize(viewConfigurationCount);
    CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigurationCount, &viewConfigurationCount, m_viewConfigurations.mutableSpan().data()));

    const XrViewConfigurationType preferredViewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    m_currentViewConfiguration = m_viewConfigurations.find(preferredViewConfiguration) != notFound ? preferredViewConfiguration : m_viewConfigurations.first();
    LOG(XR, "OpenXR selected view configurations: %s", toString(m_currentViewConfiguration));
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
    ASSERT(m_viewConfigurations.size());

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
    case XR_SESSION_STATE_EXITING: {
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
        break;
    }
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

        PlatformXR::FrameData frameData;
        frameData.predictedDisplayTime = active->frameState.predictedDisplayTime;
        frameData.shouldRender = active->frameState.shouldRender;

        callOnMainRunLoop([callback = WTFMove(active->onFrameUpdate), frameData = WTFMove(frameData)]() mutable {
            callback(WTFMove(frameData));
        });

        if (!active->frameState.shouldRender) {
            // We must always call xrEndFrame() if we had previously called xrBeginFrame(), even if we don't render anything. Don't wait
            // for submitFrame() as in the normal flow because it won't ever be called (see WebXRSession::onFrame()).
            submitFrameInternal(active);
            continue;
        }

        throttleThreadIfNeeded();
    }

    LOG(XR, "OpenXRCoordinator::renderLoop exiting...");
}


} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
