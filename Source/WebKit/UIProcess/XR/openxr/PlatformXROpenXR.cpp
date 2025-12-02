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
#include "OpenXRHitTestManager.h"
#include "OpenXRInput.h"
#include "OpenXRLayer.h"
#include "OpenXRUtils.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#if USE(LIBEPOXY)
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif
#include <WebCore/GLContext.h>
#include <WebCore/GLDisplay.h>
#include <WebCore/GLFence.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>

#if USE(GBM)
#include "DRMMainDevice.h"
#include <WebCore/DRMDevice.h>
#include <WebCore/GBMDevice.h>
#endif

#if OS(ANDROID)
#include <dlfcn.h>
using WPEAndroidRuntimeGetJavaVMFunction = JavaVM* (*)();
using WPEAndroidRuntimeGetActivityFunction = jobject (*)();
#undef jobject
#endif

namespace WebKit {

struct OpenXRCoordinator::RenderState {
    std::atomic<bool> terminateRequested;
    bool pendingFrame { false };
    PlatformXR::Device::RequestFrameCallback onFrameUpdate;
    XrFrameState frameState;
    bool passthroughFullyObscured { false };
#if ENABLE(WEBXR_HIT_TEST)
    PlatformXR::HitTestSource nextHitTestSource { 1 };
    PlatformXR::TransientInputHitTestSource nextTransientInputHitTestSource { 1 };
    HashMap<PlatformXR::HitTestSource, UniqueRef<PlatformXR::HitTestOptions>> hitTestSources;
    HashMap<PlatformXR::TransientInputHitTestSource, UniqueRef<PlatformXR::TransientInputHitTestOptions>> transientInputHitTestSources;
    std::unique_ptr<OpenXRHitTestManager> hitTestManager;
#endif
};

OpenXRCoordinator::OpenXRCoordinator()
{
    ASSERT(RunLoop::isMain());
}

OpenXRCoordinator::~OpenXRCoordinator()
{
    cleanupInstanceAndAssociatedResources();
}

void OpenXRCoordinator::getPrimaryDeviceInfo(WebPageProxy& page, DeviceInfoCallback&& callback)
{
    ASSERT(RunLoop::isMain());

    initializeDevice(page.protectedPreferences()->openXRDMABufRelaxedForTesting());
    if (m_instance == XR_NULL_HANDLE || m_systemId == XR_NULL_SYSTEM_ID) {
        LOG(XR, "Failed to initialize OpenXR system");
        callback(std::nullopt);
        return;
    }

    auto runtimeProperties = systemProperties(m_instance, m_systemId);

    auto recommendedResolution = [&views = m_viewConfigurationViews]() -> WebCore::IntSize {
        // OpenXR is very flexible wrt views resolution, but the current WebKit architecture expects a single resolution for all views.
        return { static_cast<int>(views.size() * views.first().recommendedImageRectWidth), static_cast<int>(views.first().recommendedImageRectHeight) };
    };

    XRDeviceInfo deviceInfo { .identifier = m_deviceIdentifier, .vrFeatures = { }, .arFeatures = { } };
    deviceInfo.supportsOrientationTracking = runtimeProperties.supportsOrientationTracking;
    deviceInfo.supportsStereoRendering = m_currentViewConfiguration == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    deviceInfo.recommendedResolution = recommendedResolution();
    LOG(XR, "OpenXR device info:\n\tOrientation tracking: %s\n\tStereo rendering: %s\n\tRecommended resolution: %dx%d", deviceInfo.supportsOrientationTracking ? "yes" : "no", deviceInfo.supportsStereoRendering ? "yes" : "no", deviceInfo.recommendedResolution.width(), deviceInfo.recommendedResolution.height());

    // OpenXR runtimes MUST support VIEW and LOCAL reference spaces.
    deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeViewer);
    deviceInfo.arFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeViewer);
    deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocal);
    deviceInfo.arFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocal);

    if (OpenXRExtensions::singleton().isExtensionSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME ""_span)) {
        deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded);
        deviceInfo.arFeatures.append(PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded);
    }

#if ENABLE(WEBXR_HANDS) && defined(XR_EXT_hand_tracking)
    if (runtimeProperties.supportsHandTracking && OpenXRExtensions::singleton().isExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME ""_span)) {
        deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::HandTracking);
        deviceInfo.arFeatures.append(PlatformXR::SessionFeature::HandTracking);
    }
#endif

#if ENABLE(WEBXR_HIT_TEST)
    deviceInfo.arFeatures.append(PlatformXR::SessionFeature::HitTest);
#endif

#if ENABLE(WEBXR_LAYERS)
    deviceInfo.vrFeatures.append(PlatformXR::SessionFeature::Layers);
    deviceInfo.arFeatures.append(PlatformXR::SessionFeature::Layers);
#endif

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
    ASSERT(!RunLoop::isMain());
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

std::unique_ptr<OpenXRSwapchain> OpenXRCoordinator::createSwapchain(uint32_t width, uint32_t height, bool alpha) const
{
    // Even if alpha is false we always ask for the RGBA8 format, as the DRM_FORMAT_RGB8 is not supported by ANGLE.
    // In this case we ignore the alpha channel by using DRM_FORMAT_XRGB8888 when exporting the texture.
    auto preferredFormat = GL_RGBA8;
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

    return OpenXRSwapchain::create(m_session, info, alpha ? OpenXRSwapchain::HasAlpha::Yes : OpenXRSwapchain::HasAlpha::No);
}

void OpenXRCoordinator::createLayerProjection(uint32_t width, uint32_t height, bool alpha, CompletionHandler<void(std::optional<PlatformXR::LayerHandle>)>&& reply)
{
    ASSERT(RunLoop::isMain());
    WTF::switchOn(m_state,
        [&](Idle&) { reply(std::nullopt); },
        [&](Active& active) {
            active.renderQueue->dispatch([this, width, height, alpha, completionHandler = WTFMove(reply)] mutable {
                if (!collectSwapchainFormatsIfNeeded()) {
                    RELEASE_LOG(XR, "OpenXRCoordinator: no supported swapchain formats");
                    completionHandler(std::nullopt);
                    return;
                }

                auto swapchain = createSwapchain(width, height, alpha);
                if (!swapchain) {
                    RELEASE_LOG(XR, "OpenXRCoordinator: failed to create swapchain");
                    completionHandler(std::nullopt);
                    return;
                }

                if (auto layer = OpenXRLayerProjection::create(WTFMove(swapchain))) {
#if USE(GBM)
                    if (m_gbmDevice)
                        layer->setGBMDevice(m_gbmDevice);
#endif
                    auto layerHandle = m_nextLayerHandle++;
                    m_layers.add(layerHandle, WTFMove(layer));
                    completionHandler(layerHandle);
                }
            });
        });
}

void OpenXRCoordinator::startSession(WebPageProxy& page, WeakPtr<PlatformXRCoordinatorSessionEventClient>&& sessionEventClient, const WebCore::SecurityOriginData&, PlatformXR::SessionMode sessionMode, const PlatformXR::Device::FeatureList&, std::optional<WebCore::XRCanvasConfiguration>&&)
{
    ASSERT(RunLoop::isMain());
    LOG(XR, "OpenXRCoordinator::startSession");

    initializeDevice(page.protectedPreferences()->openXRDMABufRelaxedForTesting());

    WTF::switchOn(m_state,
        [&](Idle&) {
            m_sessionMode = sessionMode;

            auto renderState = Box<RenderState>::create();
            renderState->terminateRequested = false;
            renderState->pendingFrame = false;

            auto renderQueue = WorkQueue::create("OpenXR render queue"_s);
            m_state = Active {
                .sessionEventClient = WTFMove(sessionEventClient),
                .pageIdentifier = page.webPageIDInMainFrameProcess(),
                .renderState = renderState,
                .renderQueue = renderQueue.get()
            };
            renderQueue->dispatch([this, renderState] {
                createSessionIfNeeded();
                if (m_session == XR_NULL_HANDLE) {
                    LOG(XR, "OpenXRCoordinator: failed to create the session");
                    return;
                }
                m_input = OpenXRInput::create(m_instance, m_session, systemProperties(m_instance, m_systemId));
                renderLoop(renderState);
            });
        },
        [&](Active&) {
            RELEASE_LOG_ERROR(XR, "OpenXRCoordinator: an existing immersive session is active");
            if (RefPtr protectedSessionEventClient = sessionEventClient.get())
                protectedSessionEventClient->sessionDidEnd(m_deviceIdentifier);
        });
}

void OpenXRCoordinator::endSessionIfExists(WebPageProxy& page)
{
    ASSERT(RunLoop::isMain());
    LOG(XR, "OpenXRCoordinator: endSessionIfExists");

    WTF::switchOn(m_state,
        [&](Idle&) { },
        [&](Active& active) {
            if (active.pageIdentifier != page.webPageIDInMainFrameProcess()) {
                LOG(XR, "OpenXRCoordinator: trying to end an immersive session owned by another page");
                return;
            }

            if (active.renderState->terminateRequested)
                return;

            active.renderState->terminateRequested = true;
            active.renderQueue->dispatchSync([this, renderState = active.renderState] {
                if (!m_isSessionRunning) {
                    cleanupAllResources();
                    return;
                }
                // OpenXR will transition the session to STOPPING state and then we will call xrEndSession().
                CHECK_XRCMD(xrRequestExitSession(m_session));
                while (m_session != XR_NULL_HANDLE)
                    renderLoop(renderState);
            });
            active.renderQueue = nullptr;

            if (active.renderState->onFrameUpdate)
                active.renderState->onFrameUpdate({ });

            auto& sessionEventClient = active.sessionEventClient;
            if (sessionEventClient) {
                LOG(XR, "... immersive session end sent");
                sessionEventClient->sessionDidEnd(m_deviceIdentifier);
            }

            if (active.didStart)
                page.uiClient().didEndXRSession(page);
            m_state = Idle { };
        });
}

void OpenXRCoordinator::scheduleAnimationFrame(WebPageProxy& page, std::optional<PlatformXR::RequestData>&& requestData, PlatformXR::Device::RequestFrameCallback&& onFrameUpdateCallback)
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

            active.renderQueue->dispatch([this, renderState = active.renderState, requestData = WTFMove(requestData), onFrameUpdateCallback = WTFMove(onFrameUpdateCallback)]() mutable {
                renderState->passthroughFullyObscured = requestData ? requestData->isPassthroughFullyObscured : false;
                renderState->onFrameUpdate = WTFMove(onFrameUpdateCallback);
                renderLoop(renderState);
            });
        });
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

            active.renderQueue->dispatch([this, renderState = active.renderState, layers = WTFMove(layers)]() mutable {
                endFrame(renderState, WTFMove(layers));
                renderLoop(renderState);
            });
        });
}

#if ENABLE(WEBXR_HIT_TEST)
void OpenXRCoordinator::requestHitTestSource(WebPageProxy& page, const PlatformXR::HitTestOptions& options, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::HitTestSource>)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    WTF::switchOn(m_state,
        [&](Idle&) {
            RELEASE_LOG(XR, "OpenXRCoordinator: trying to request a hit test source for an inactive session");
        },
        [&](Active& active) {
            if (active.pageIdentifier != page.webPageIDInMainFrameProcess()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to request a hit test source for session owned by another page");
                return;
            }

            if (active.renderState->terminateRequested.load()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to request a hit test source for a terminating session");
                return;
            }

            auto copiedOptions = makeUniqueRef<PlatformXR::HitTestOptions>(options);
            active.renderQueue->dispatch([this, renderState = active.renderState, options = WTFMove(copiedOptions), completionHandler = WTFMove(completionHandler)]() mutable {
                if (!renderState->hitTestManager)
                    renderState->hitTestManager = makeUnique<OpenXRHitTestManager>(m_session);
                auto addResult = renderState->hitTestSources.add(renderState->nextHitTestSource, WTFMove(options));
                ASSERT_UNUSED(addResult.isNewEntry, addResult);
                callOnMainRunLoop([source = renderState->nextHitTestSource, completionHandler = WTFMove(completionHandler)] mutable {
                    completionHandler(source);
                });
                renderState->nextHitTestSource++;
            });
        });
}

void OpenXRCoordinator::deleteHitTestSource(WebPageProxy& page, PlatformXR::HitTestSource source)
{
    ASSERT(RunLoop::isMain());
    WTF::switchOn(m_state,
        [&](Idle&) {
            RELEASE_LOG(XR, "OpenXRCoordinator: trying to delete a hit test source for an inactive session");
        },
        [&](Active& active) {
            if (active.pageIdentifier != page.webPageIDInMainFrameProcess()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to delete a hit test source for session owned by another page");
                return;
            }

            if (active.renderState->terminateRequested.load()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to delete a hit test source for a terminating session");
                return;
            }

            active.renderQueue->dispatch([renderState = active.renderState, source]() mutable {
                bool removed = renderState->hitTestSources.remove(source);
                ASSERT_UNUSED(removed, removed);
            });
        });
}

void OpenXRCoordinator::requestTransientInputHitTestSource(WebPageProxy& page, const PlatformXR::TransientInputHitTestOptions& options, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource>)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    WTF::switchOn(m_state,
        [&](Idle&) {
            RELEASE_LOG(XR, "OpenXRCoordinator: trying to request a transient input hit test source for an inactive session");
        },
        [&](Active& active) {
            if (active.pageIdentifier != page.webPageIDInMainFrameProcess()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to request a transient input hit test source for session owned by another page");
                return;
            }

            if (active.renderState->terminateRequested.load()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to request a transient input hit test source for a terminating session");
                return;
            }

            auto copiedOptions = makeUniqueRef<PlatformXR::TransientInputHitTestOptions>(options);
            active.renderQueue->dispatch([this, renderState = active.renderState, options = WTFMove(copiedOptions), completionHandler = WTFMove(completionHandler)]() mutable {
                if (!renderState->hitTestManager)
                    renderState->hitTestManager = makeUnique<OpenXRHitTestManager>(m_session);
                auto addResult = renderState->transientInputHitTestSources.add(renderState->nextTransientInputHitTestSource, WTFMove(options));
                ASSERT_UNUSED(addResult.isNewEntry, addResult);
                callOnMainRunLoop([source = renderState->nextTransientInputHitTestSource, completionHandler = WTFMove(completionHandler)] mutable {
                    completionHandler(source);
                });
                renderState->nextTransientInputHitTestSource++;
            });
        });
}

void OpenXRCoordinator::deleteTransientInputHitTestSource(WebPageProxy& page, PlatformXR::TransientInputHitTestSource source)
{
    ASSERT(RunLoop::isMain());
    WTF::switchOn(m_state,
        [&](Idle&) {
            RELEASE_LOG(XR, "OpenXRCoordinator: trying to delete a transient input hit test source for an inactive session");
        },
        [&](Active& active) {
            if (active.pageIdentifier != page.webPageIDInMainFrameProcess()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to delete a transient input hit test source for session owned by another page");
                return;
            }

            if (active.renderState->terminateRequested.load()) {
                RELEASE_LOG(XR, "OpenXRCoordinator: trying to delete a transient input hit test source for a terminating session");
                return;
            }

            active.renderQueue->dispatch([renderState = active.renderState, source]() mutable {
                bool removed = renderState->transientInputHitTestSources.remove(source);
                ASSERT_UNUSED(removed, removed);
            });
        });
}
#endif

void OpenXRCoordinator::createInstance()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_instance == XR_NULL_HANDLE);

    Vector<char *> extensions;
#if defined(XR_USE_PLATFORM_EGL)
    if (OpenXRExtensions::singleton().isExtensionSupported(XR_MNDX_EGL_ENABLE_EXTENSION_NAME ""_span))
        extensions.append(const_cast<char*>(XR_MNDX_EGL_ENABLE_EXTENSION_NAME));
#endif
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    extensions.append(const_cast<char*>(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME));
#endif
#if defined(XR_EXT_hand_interaction)
    if (OpenXRExtensions::singleton().isExtensionSupported(XR_EXT_HAND_INTERACTION_EXTENSION_NAME ""_span))
        extensions.append(const_cast<char*>(XR_EXT_HAND_INTERACTION_EXTENSION_NAME));
#endif
#if ENABLE(WEBXR_HANDS) && defined(XR_EXT_hand_tracking)
    if (OpenXRExtensions::singleton().isExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME ""_span))
        extensions.append(const_cast<char*>(XR_EXT_HAND_TRACKING_EXTENSION_NAME));
#endif
#if OS(ANDROID)
    extensions.append(const_cast<char*>(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME));
#endif

    XrInstanceCreateInfo createInfo = createOpenXRStruct<XrInstanceCreateInfo, XR_TYPE_INSTANCE_CREATE_INFO >();
    createInfo.applicationInfo = { "WebKit", 1, "WebKit", 1, XR_CURRENT_API_VERSION };
    createInfo.enabledApiLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.mutableSpan().data();

#if OS(ANDROID)
    static WPEAndroidRuntimeGetJavaVMFunction s_wpeAndroidRuntimeGetJavaVM =
        reinterpret_cast<WPEAndroidRuntimeGetJavaVMFunction>(dlsym(RTLD_DEFAULT, "wpe_android_runtime_get_current_java_vm"));
    if (!s_wpeAndroidRuntimeGetJavaVM) [[unlikely]] {
        RELEASE_LOG_ERROR(XR, "Cannot resolve wpe_android_runtime_get_current_java_vm(): %s.", dlerror());
        return;
    }

    static WPEAndroidRuntimeGetActivityFunction s_wpeAndroidRuntimeGetActivity =
        reinterpret_cast<WPEAndroidRuntimeGetActivityFunction>(dlsym(RTLD_DEFAULT, "wpe_android_runtime_get_current_activity"));
    if (!s_wpeAndroidRuntimeGetActivity) [[unlikely]] {
        RELEASE_LOG_ERROR(XR, "Cannot resolve wpe_android_runtime_get_current_activity(): %s.", dlerror());
        return;
    }

    auto java = createOpenXRStruct<XrInstanceCreateInfoAndroidKHR, XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR>();
    java.applicationVM = s_wpeAndroidRuntimeGetJavaVM();
    java.applicationActivity = s_wpeAndroidRuntimeGetActivity();
    createInfo.next = reinterpret_cast<XrBaseInStructure*>(&java);
#endif

    CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));
}

RefPtr<WebCore::GLDisplay> OpenXRCoordinator::createGLDisplay(bool isForTesting) const
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_glDisplay);

    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    auto tryCreateDisplay = [&](EGLenum platform, void *native) -> RefPtr<WebCore::GLDisplay> {
        if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base"))
            return WebCore::GLDisplay::create(eglGetPlatformDisplayEXT(platform, native, nullptr));
        if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base"))
            return WebCore::GLDisplay::create(eglGetPlatformDisplay(platform, native, nullptr));
        return nullptr;
    };

    RefPtr<WebCore::GLDisplay> glDisplay;

#if OS(ANDROID)
    if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_android")) {
        glDisplay = tryCreateDisplay(EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY);
        if (!glDisplay)
            glDisplay = WebCore::GLDisplay::create(eglGetDisplay(EGL_DEFAULT_DISPLAY));
        if (glDisplay && !(glDisplay->extensions().ANDROID_get_native_client_buffer && glDisplay->extensions().ANDROID_image_native_buffer))
            glDisplay = nullptr;
    }
#endif // OS(ANDROID)

    if (WebCore::GLContext::isExtensionSupported(extensions, "EGL_MESA_platform_surfaceless")) {
        glDisplay = tryCreateDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY);
        if (glDisplay && !isForTesting && !glDisplay->extensions().MESA_image_dma_buf_export)
            glDisplay = nullptr;
    }

#if USE(GBM)
    if (!glDisplay && WebCore::GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_gbm")) {
        const auto& mainDevice = drmMainDevice();
        if (!mainDevice.isNull()) {
            m_gbmDevice = WebCore::GBMDevice::create(!mainDevice.renderNode.isNull() ? mainDevice.renderNode : mainDevice.primaryNode);
            if (m_gbmDevice)
                glDisplay = tryCreateDisplay(EGL_PLATFORM_GBM_KHR, m_gbmDevice->device());
        }
    }
#endif

    return glDisplay;
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

void OpenXRCoordinator::initializeDevice(bool isForTesting)
{
    ASSERT(RunLoop::isMain());

    if (m_instance != XR_NULL_HANDLE)
        return;

    auto display = createGLDisplay(isForTesting);
    if (!display) {
        LOG(XR, "Failed to create a display for OpenXR.");
        return;
    }

    createInstance();
    if (m_instance == XR_NULL_HANDLE) {
        LOG(XR, "Failed to create OpenXR instance.");
        return;
    }

    if (!OpenXRExtensions::singleton().loadMethods(m_instance)) {
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

    m_glDisplay = WTFMove(display);
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
#if !OS(ANDROID)
    if (!OpenXRExtensions::singleton().isExtensionSupported(XR_MNDX_EGL_ENABLE_EXTENSION_NAME ""_span)) {
        LOG(XR, "OpenXR MNDX_EGL_ENABLE extension is not supported.");
        return;
    }
#endif

    if (!m_glContext) {
#if USE(GBM)
        const WebCore::GLContext::Target target = m_gbmDevice ? WebCore::GLContext::Target::Default : WebCore::GLContext::Target::Surfaceless;
#else
        static const WebCore::GLContext::Target target = WebCore::GLContext::Target::Surfaceless;
#endif
        m_glContext = WebCore::GLContext::create(*m_glDisplay, target);
        if (!m_glContext) {
            LOG(XR, "Failed to create the GL context for OpenXR.");
            return;
        }
    }

#if OS(ANDROID)
    m_graphicsBinding = createOpenXRStruct<XrGraphicsBindingOpenGLESAndroidKHR, XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR>();
#else
    m_graphicsBinding = createOpenXRStruct<XrGraphicsBindingEGLMNDX, XR_TYPE_GRAPHICS_BINDING_EGL_MNDX>();
    m_graphicsBinding.getProcAddress = OpenXRExtensions::singleton().methods().getProcAddressFunc;
#endif
    m_graphicsBinding.display = m_glDisplay->eglDisplay();
    m_graphicsBinding.context = m_glContext->platformContext();
    m_graphicsBinding.config = m_glContext->config();
}

void OpenXRCoordinator::createSessionIfNeeded()
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_instance != XR_NULL_HANDLE);

    if (m_session != XR_NULL_HANDLE)
        return;

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    auto requirements = createOpenXRStruct<XrGraphicsRequirementsOpenGLESKHR, XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR>();
    CHECK_XRCMD(OpenXRExtensions::singleton().methods().xrGetOpenGLESGraphicsRequirementsKHR(m_instance, m_systemId, &requirements));
#endif

    tryInitializeGraphicsBinding();

    m_views.resize(m_viewConfigurationViews.size());

    // Create the session.
    auto sessionCreateInfo = createOpenXRStruct<XrSessionCreateInfo, XR_TYPE_SESSION_CREATE_INFO>();
    sessionCreateInfo.systemId = m_systemId;
    sessionCreateInfo.next = &m_graphicsBinding;
    CHECK_XRCMD(xrCreateSession(m_instance, &sessionCreateInfo, &m_session));
}

void OpenXRCoordinator::cleanupSessionAndAssociatedResources()
{
    ASSERT(!RunLoop::isMain());

    if (m_localSpace != XR_NULL_HANDLE) {
        CHECK_XRCMD(xrDestroySpace(m_localSpace));
        m_localSpace = XR_NULL_HANDLE;
    }

    if (m_floorSpace != XR_NULL_HANDLE) {
        CHECK_XRCMD(xrDestroySpace(m_floorSpace));
        m_floorSpace = XR_NULL_HANDLE;
    }

    m_layers.clear();
    m_views.clear();
    m_input.reset();

    if (m_session != XR_NULL_HANDLE) {
        CHECK_XRCMD(xrDestroySession(m_session));
        m_session = XR_NULL_HANDLE;
    }

    m_glContext.reset();
}

void OpenXRCoordinator::cleanupInstanceAndAssociatedResources()
{
    m_viewConfigurationViews.clear();
    m_systemId = XR_NULL_SYSTEM_ID;

    ASSERT(!m_glContext);
    m_glDisplay = nullptr;

    if (m_instance == XR_NULL_HANDLE)
        return;

    xrDestroyInstance(m_instance);
    m_instance = XR_NULL_HANDLE;
}

void OpenXRCoordinator::cleanupAllResources()
{
    cleanupSessionAndAssociatedResources();
    cleanupInstanceAndAssociatedResources();
}

void OpenXRCoordinator::handleSessionStateChange()
{
    ASSERT(!RunLoop::isMain());

    switch (m_sessionState) {
    case XR_SESSION_STATE_READY: {
        auto sessionBeginInfo = createOpenXRStruct<XrSessionBeginInfo, XR_TYPE_SESSION_BEGIN_INFO>();
        sessionBeginInfo.primaryViewConfigurationType = m_currentViewConfiguration;
        CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
        m_isSessionRunning = true;
        callOnMainRunLoop([this] {
            WTF::switchOn(m_state,
                [&](Idle&) { },
                [&](Active& active) {
                    if (RefPtr page = WebProcessProxy::webPage(active.pageIdentifier)) {
                        active.didStart = true;
                        page->uiClient().didStartXRSession(*page);
                    }
                });
        });
        break;
    }
    case XR_SESSION_STATE_STOPPING:
        // Once xrEndSession() is called we cannot longer call xrWaitFrame()->xrBeginFrame()->xrEndFrame() from any thread.
        // However we cannot terminate the thread just now as we need to call xrPollEvent() to handle the session state change.
        CHECK_XRCMD(xrEndSession(m_session));
        m_isSessionRunning = false;
        break;
    case XR_SESSION_STATE_LOSS_PENDING:
    case XR_SESSION_STATE_EXITING:
        cleanupAllResources();
        break;
    default:
        break;
    }
}

enum class OpenXRCoordinator::PollResult : bool { Stop, Continue };

OpenXRCoordinator::PollResult OpenXRCoordinator::pollEvents()
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
            handleSessionStateChange();
            return m_session == XR_NULL_HANDLE ? PollResult::Stop : PollResult::Continue;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
            auto* event = reinterpret_cast<XrEventDataInteractionProfileChanged*>(&runtimeEvent);
            LOG(XR, "OpenXR interaction profile changed for session %p", static_cast<void*>(event->session));
            if (m_input && event->session == m_session)
                m_input->updateInteractionProfile();
            break;
        }
        default:
            LOG(XR, "Unhandled OpenXR event type %d\n", runtimeEvent.type);
        }
    }
    return PollResult::Continue;
}

XrEnvironmentBlendMode OpenXRCoordinator::blendModeForSessionMode(Box<RenderState> renderState) const
{
    return (m_sessionMode == PlatformXR::SessionMode::ImmersiveAr && !renderState->passthroughFullyObscured) ? m_arBlendMode : m_vrBlendMode;
}

PlatformXR::FrameData OpenXRCoordinator::populateFrameData(Box<RenderState> renderState)
{
    ASSERT(!RunLoop::isMain());

    PlatformXR::FrameData frameData;
    frameData.predictedDisplayTime = renderState->frameState.predictedDisplayTime;
    frameData.shouldRender = renderState->frameState.shouldRender;
    if (!frameData.shouldRender)
        return frameData;

    XrViewLocateInfo viewLocateInfo = createOpenXRStruct<XrViewLocateInfo, XR_TYPE_VIEW_LOCATE_INFO>();
    viewLocateInfo.viewConfigurationType = m_currentViewConfiguration;
    viewLocateInfo.displayTime = renderState->frameState.predictedDisplayTime;
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

    if (m_input)
        frameData.inputSources = m_input->collectInputSources(renderState->frameState, m_localSpace);

    frameData.origin = XrIdentityPose();

    if (m_floorSpace != XR_NULL_HANDLE) {
        XrSpaceLocation floorLocation = createOpenXRStruct<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
        CHECK_XRCMD(xrLocateSpace(m_floorSpace, m_localSpace, renderState->frameState.predictedDisplayTime, &floorLocation));
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

#if ENABLE(WEBXR_HIT_TEST)
    for (auto& pair : renderState->hitTestSources)
        frameData.hitTestResults.add(pair.key, renderState->hitTestManager->requestHitTest(pair.value->offsetRay, m_localSpace, renderState->frameState.predictedDisplayTime));
    for (auto& pair : renderState->transientInputHitTestSources) {
        Vector<PlatformXR::FrameData::TransientInputHitTestResult> results;
        for (const auto& inputSource : frameData.inputSources) {
            if (inputSource.profiles.contains(pair.value->profile)) {
                PlatformXR::FrameData::TransientInputHitTestResult result = {
                    inputSource.handle,
                    renderState->hitTestManager->requestHitTest(pair.value->offsetRay, m_localSpace, renderState->frameState.predictedDisplayTime)
                };
                results.append(WTFMove(result));
            }
        }
        frameData.transientInputHitTestResults.add(pair.key, WTFMove(results));
    }
#endif

    auto toXREnvironmentBlendMode = [](XrEnvironmentBlendMode mode) {
        switch (mode) {
        case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
            return PlatformXR::XREnvironmentBlendMode::Opaque;
        case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
            return PlatformXR::XREnvironmentBlendMode::Additive;
        case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
            return PlatformXR::XREnvironmentBlendMode::AlphaBlend;
        default:
            ASSERT_NOT_REACHED();
            return PlatformXR::XREnvironmentBlendMode::Opaque;
        }
    };
    frameData.environmentBlendMode = toXREnvironmentBlendMode(blendModeForSessionMode(renderState));

    return frameData;
}

void OpenXRCoordinator::createReferenceSpacesIfNeeded(Box<RenderState> renderState)
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
    if (supportedSpaces.contains(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT) && OpenXRExtensions::singleton().isExtensionSupported(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME ""_span)) {
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
    CHECK_XRCMD(xrLocateSpace(stageSpace, m_localSpace, renderState->frameState.predictedDisplayTime , &stageLocation));
    CHECK_XRCMD(xrDestroySpace(stageSpace));

    float floorOffset = stageLocation.pose.position.y;

    XrReferenceSpaceCreateInfo localFloorCreateInfo = createOpenXRStruct<XrReferenceSpaceCreateInfo, XR_TYPE_REFERENCE_SPACE_CREATE_INFO>();
    localFloorCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    localFloorCreateInfo.poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, floorOffset, 0.f } };
    CHECK_XRCMD(xrCreateReferenceSpace(m_session, &localFloorCreateInfo, &m_floorSpace));
}

void OpenXRCoordinator::beginFrame(Box<RenderState> renderState)
{
    ASSERT(!RunLoop::isMain());
    if (!m_glContext->makeContextCurrent())
        return;

    XrFrameWaitInfo frameWaitInfo = createOpenXRStruct<XrFrameWaitInfo, XR_TYPE_FRAME_WAIT_INFO>();
    XrFrameState frameState = createOpenXRStruct<XrFrameState, XR_TYPE_FRAME_STATE>();
    CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

    XrFrameBeginInfo frameBeginInfo = createOpenXRStruct<XrFrameBeginInfo, XR_TYPE_FRAME_BEGIN_INFO>();
    CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

    // We should not directly use renderState->frameState in the xrWaitFrame() in order not to override the previous (ongoing) value.
    renderState->frameState = frameState;

    createReferenceSpacesIfNeeded(renderState);
    PlatformXR::FrameData frameData = populateFrameData(renderState);

    callOnMainRunLoop([callback = WTFMove(renderState->onFrameUpdate), frameData = WTFMove(frameData)]() mutable {
        callback(WTFMove(frameData));
    });

    renderState->pendingFrame = true;

    if (!renderState->frameState.shouldRender) {
        // We must always call xrEndFrame() if we had previously called xrBeginFrame(), even if we don't render anything. Don't wait
        // for submitFrame() as in the normal flow because it won't ever be called (see WebXRSession::onFrame()).
        endFrame(renderState, { });
    }
}

void OpenXRCoordinator::endFrame(Box<RenderState> renderState, Vector<XRDeviceLayer>&& layers)
{
    ASSERT(!RunLoop::isMain());

    if (!m_glContext->makeContextCurrent())
        return;

    Vector<const XrCompositionLayerBaseHeader*, 1> frameEndLayers;
    for (auto& layer : layers) {
        auto it = m_layers.find(layer.handle);
        if (it == m_layers.end()) {
            LOG(XR, "Didn't find a OpenXRLayer with %d handle", layer.handle);
            continue;
        }

        if (layer.fenceFD) {
            if (auto fence = WebCore::GLFence::importFD(*m_glDisplay, WTFMove(layer.fenceFD)))
                fence->serverWait();
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
    frameEndInfo.environmentBlendMode = blendModeForSessionMode(renderState);
    frameEndInfo.layerCount = static_cast<uint32_t>(frameEndLayers.size());
    frameEndInfo.layers = frameEndLayers.mutableSpan().data();
    CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));

    renderState->pendingFrame = false;
}

void OpenXRCoordinator::renderLoop(Box<RenderState> renderState)
{
    while (pollEvents() != PollResult::Stop) {
        if (!m_isSessionRunning && m_sessionState < XR_SESSION_STATE_READY) {
            RunLoop::currentSingleton().dispatchAfter(250_ms, [this, renderState] {
                renderLoop(renderState);
            });
            return;
        }

        if (!renderState->onFrameUpdate || renderState->pendingFrame)
            return;

        beginFrame(renderState);
    }
}


} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
