/*
 * Copyright (C) 2020 Igalia, S.L.
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#include "GraphicsContextGL.h"
#include "OpenXRExtensions.h"
#include "OpenXRInput.h"
#include "OpenXRInputSource.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/threads/BinarySemaphore.h>

using namespace WebCore;

namespace PlatformXR {

static bool isSessionReady(XrSessionState state)
{
    return state >= XR_SESSION_STATE_READY  && state < XR_SESSION_STATE_STOPPING;
}

Ref<OpenXRDevice> OpenXRDevice::create(XrInstance instance, XrSystemId system, Ref<WorkQueue>&& queue, const OpenXRExtensions& extensions, CompletionHandler<void()>&& callback)
{
    auto device = adoptRef(*new OpenXRDevice(instance, system, WTFMove(queue), extensions));
    device->initialize(WTFMove(callback));
    return device;
}

OpenXRDevice::OpenXRDevice(XrInstance instance, XrSystemId system, Ref<WorkQueue>&& queue, const OpenXRExtensions& extensions)
    : m_instance(instance)
    , m_systemId(system)
    , m_queue(WTFMove(queue))
    , m_extensions(extensions)
{
}

void OpenXRDevice::initialize(CompletionHandler<void()>&& callback)
{
    ASSERT(isMainThread());
    m_queue.dispatch([this, protectedThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        auto systemProperties = createStructure<XrSystemProperties, XR_TYPE_SYSTEM_PROPERTIES>();
        auto result = xrGetSystemProperties(m_instance, m_systemId, &systemProperties);
        if (XR_SUCCEEDED(result))
            m_supportsOrientationTracking = systemProperties.trackingProperties.orientationTracking == XR_TRUE;
#if !LOG_DISABLED
        else
            LOG(XR, "xrGetSystemProperties(): error %s\n", resultToString(result, m_instance).utf8().data());
        LOG(XR, "Found XRSystem %lu: \"%s\", vendor ID %d\n", systemProperties.systemId, systemProperties.systemName, systemProperties.vendorId);
#endif

        collectSupportedSessionModes();
        collectConfigurationViews();

        callOnMainThread(WTFMove(callback));
    });
}

WebCore::IntSize OpenXRDevice::recommendedResolution(SessionMode mode)
{
    auto configType = toXrViewConfigurationType(mode);
    auto viewsIterator = m_configurationViews.find(configType);
    if (viewsIterator != m_configurationViews.end())
        return { static_cast<int>(2 * viewsIterator->value[0].recommendedImageRectWidth), static_cast<int>(viewsIterator->value[0].recommendedImageRectHeight) };
    return Device::recommendedResolution(mode);
}

void OpenXRDevice::initializeTrackingAndRendering(const WebCore::SecurityOriginData&, SessionMode mode, const Device::FeatureList&)
{
    m_queue.dispatch([this, protectedThis = Ref { *this }, mode]() {
        ASSERT(m_instance != XR_NULL_HANDLE);
        ASSERT(m_session == XR_NULL_HANDLE);
        ASSERT(m_extensions.methods().xrGetOpenGLGraphicsRequirementsKHR);

        m_currentViewConfigurationType = toXrViewConfigurationType(mode);
        ASSERT(m_configurationViews.contains(m_currentViewConfigurationType));

        // https://www.khronos.org/registry/OpenXR/specs/1.0/man/html/xrGetOpenGLGraphicsRequirementsKHR.html
        // OpenXR requires to call xrGetOpenGLGraphicsRequirementsKHR before creating a session.
        auto requirements = createStructure<XrGraphicsRequirementsOpenGLKHR, XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR>();
        auto result = m_extensions.methods().xrGetOpenGLGraphicsRequirementsKHR(m_instance, m_systemId, &requirements);
        RETURN_IF_FAILED(result, "xrGetOpenGLGraphicsRequirementsKHR", m_instance);

        m_graphicsBinding = createStructure<XrGraphicsBindingEGLMNDX, XR_TYPE_GRAPHICS_BINDING_EGL_MNDX>();
        m_egl = GLContext::createSharing(PlatformDisplay::sharedDisplay());
        if (!m_egl) {
            LOG(XR, "Failed to create EGL context");
            return;
        }

        m_egl->makeContextCurrent();

        GraphicsContextGLAttributes attributes;
        attributes.depth = false;
        attributes.stencil = false;
        attributes.antialias = false;

        m_gl = createWebProcessGraphicsContextGL(attributes);
        if (!m_gl) {
            LOG(XR, "Failed to create a valid GraphicsContextGL");
            return;
        }

        m_graphicsBinding.display = PlatformDisplay::sharedDisplay().eglDisplay();
        m_graphicsBinding.context = m_egl->platformContext();
        m_graphicsBinding.config = m_egl->config();
        m_graphicsBinding.getProcAddress = m_extensions.methods().getProcAddressFunc;

        // Create the session.
        auto sessionCreateInfo = createStructure<XrSessionCreateInfo, XR_TYPE_SESSION_CREATE_INFO>();
        sessionCreateInfo.systemId = m_systemId;
        sessionCreateInfo.next = &m_graphicsBinding;

        result = xrCreateSession(m_instance, &sessionCreateInfo, &m_session);
        RETURN_IF_FAILED(result, "xrCreateSession", m_instance);

        // Create the default reference spaces
        m_localSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);
        m_viewSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        // Initialize input tracking
        m_input = OpenXRInput::create(m_instance, m_session, m_localSpace);
    });
}

void OpenXRDevice::shutDownTrackingAndRendering()
{
    m_queue.dispatch([this, protectedThis = Ref { *this }]() {
        if (m_session == XR_NULL_HANDLE)
            return;

        // xrRequestExitSession() will transition the session to STOPPED state.
        // If the session was not running we have to reset the session ourselves.
        if (XR_FAILED(xrRequestExitSession(m_session))) {
            resetSession();
            return;
        }

        // OpenXR needs to wait for the XR_SESSION_STATE_STOPPING state to properly end the session.
        waitUntilStopping();
    });
}

void OpenXRDevice::initializeReferenceSpace(PlatformXR::ReferenceSpaceType spaceType)
{
    if ((spaceType == ReferenceSpaceType::LocalFloor || spaceType == ReferenceSpaceType::BoundedFloor) && m_stageSpace == XR_NULL_HANDLE)
        m_stageSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_STAGE);
    if (spaceType == ReferenceSpaceType::BoundedFloor)
        updateStageParameters();
}

void OpenXRDevice::requestFrame(RequestFrameCallback&& callback)
{
    m_queue.dispatch([this, protectedThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        pollEvents();
        if (!isSessionReady(m_sessionState)) {
            callOnMainThread([callback = WTFMove(callback)]() mutable {
                // Device not ready or stopping. Report frameData with invalid tracking.
                callback({ });
            });
            return;
        }

        m_frameState = createStructure<XrFrameState, XR_TYPE_FRAME_STATE>();
        auto frameWaitInfo = createStructure<XrFrameWaitInfo, XR_TYPE_FRAME_WAIT_INFO>();
        auto result = xrWaitFrame(m_session, &frameWaitInfo, &m_frameState);
        RETURN_IF_FAILED(result, "xrWaitFrame", m_instance);

        auto frameBeginInfo = createStructure<XrFrameBeginInfo, XR_TYPE_FRAME_BEGIN_INFO>();
        result = xrBeginFrame(m_session, &frameBeginInfo);
        RETURN_IF_FAILED(result, "xrBeginFrame", m_instance);

        FrameData frameData;
        frameData.predictedDisplayTime = m_frameState.predictedDisplayTime;
        frameData.shouldRender = m_frameState.shouldRender;
        frameData.stageParameters = m_stageParameters;

        ASSERT(m_configurationViews.contains(m_currentViewConfigurationType));
        const auto& configurationView = m_configurationViews.get(m_currentViewConfigurationType);

        uint32_t viewCount = configurationView.size();
        m_frameViews.fill(createStructure<XrView, XR_TYPE_VIEW>(), viewCount);

        if (m_frameState.shouldRender) {
            // Query head location
            auto location = createStructure<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
            xrLocateSpace(m_viewSpace, m_localSpace, m_frameState.predictedDisplayTime, &location);
            frameData.isTrackingValid = location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            frameData.isPositionValid = location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT;
            frameData.isPositionEmulated = location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

            if (frameData.isTrackingValid)
                frameData.origin = XrPosefToPose(location.pose);

            auto viewLocateInfo = createStructure<XrViewLocateInfo, XR_TYPE_VIEW_LOCATE_INFO>();
            viewLocateInfo.displayTime = m_frameState.predictedDisplayTime;
            viewLocateInfo.space = m_viewSpace;

            auto viewState = createStructure<XrViewState, XR_TYPE_VIEW_STATE>();
            uint32_t viewCountOutput;
            result = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCount, &viewCountOutput, m_frameViews.data());
            if (!XR_FAILED(result)) {
                for (auto& view : m_frameViews)
                    frameData.views.append(xrViewToPose(view));
            }

            // Query floor transform
            if (m_stageSpace != XR_NULL_HANDLE) {
                auto floorLocation = createStructure<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
                xrLocateSpace(m_stageSpace, m_localSpace, m_frameState.predictedDisplayTime, &floorLocation);
                frameData.floorTransform = { XrPosefToPose(floorLocation.pose) };
            }

            for (auto& layer : m_layers) {
                auto layerData = layer.value->startFrame();
                if (layerData)
                    frameData.layers.add(layer.key, *layerData);
            }

            if (m_input)
                frameData.inputSources = m_input->collectInputSources(m_frameState);

        } else {
            // https://www.khronos.org/registry/OpenXR/specs/1.0/man/html/XrFrameState.html
            // When shouldRender is false the application should avoid heavy GPU work where possible,
            // for example by skipping layer rendering and then omitting those layers when calling xrEndFrame.
            // In this case we don't need to wait for OpenXRDevice::submitFrame to finish the frame.
            auto frameEndInfo = createStructure<XrFrameEndInfo, XR_TYPE_FRAME_END_INFO>();
            frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
            frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            frameEndInfo.layerCount = 0;
            frameEndInfo.layers = nullptr;
            LOG_IF_FAILED(xrEndFrame(m_session, &frameEndInfo), "xrEndFrame", m_instance);
        }

        callOnMainThread([frameData = WTFMove(frameData), callback = WTFMove(callback)]() mutable {
            callback(WTFMove(frameData));
        });
    });
}

void OpenXRDevice::submitFrame(Vector<Device::Layer>&& layers)
{   
    m_queue.dispatch([this, protectedThis = Ref { *this }, layers = WTFMove(layers)]() mutable {
        ASSERT(m_frameState.shouldRender);
        Vector<const XrCompositionLayerBaseHeader*> frameEndLayers;
        if (m_frameState.shouldRender) {
            for (auto& layer : layers) {
                auto it = m_layers.find(layer.handle);
                if (it == m_layers.end()) {
                    LOG(XR, "Didn't find a OpenXRLayer with %d handle", layer.handle);
                    continue;
                }
                auto header = it->value->endFrame(layer, m_viewSpace, m_frameViews);
                if (!header) {
                    LOG(XR, "endFrame() call failed in OpenXRLayer with %d handle", layer.handle);
                    continue;
                }

                frameEndLayers.append(header);
            }
        }

        auto frameEndInfo = createStructure<XrFrameEndInfo, XR_TYPE_FRAME_END_INFO>();
        frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        frameEndInfo.layerCount = frameEndLayers.size();
        frameEndInfo.layers = frameEndLayers.data();
        auto result = xrEndFrame(m_session, &frameEndInfo);
        RETURN_IF_FAILED(result, "xrEndFrame", m_instance);
    });
}

Vector<Device::ViewData> OpenXRDevice::views(SessionMode mode) const
{
    Vector<Device::ViewData> views;
    auto configurationType = toXrViewConfigurationType(mode);

    if (configurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO)
        views.append({ .active = true, .eye = Eye::None });
    else {
        ASSERT(configurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);
        views.append({ .active = true, .eye = Eye::Left });
        views.append({ .active = true, .eye = Eye::Right });
    }
    return views;
}

std::optional<LayerHandle> OpenXRDevice::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
    std::optional<LayerHandle> handle;

    BinarySemaphore semaphore;
    m_queue.dispatch([this, width, height, alpha, &handle, &semaphore]() mutable {
        auto format = alpha ? GL_RGBA8 : GL_RGB8;
        int64_t sampleCount = 1;

        auto it = m_configurationViews.find(m_currentViewConfigurationType);
        if (it != m_configurationViews.end())
            sampleCount = it->value[0].recommendedSwapchainSampleCount;

        auto layer = OpenXRLayerProjection::create(m_instance, m_session, width, height, format, sampleCount);
        if (layer) {
            handle = generateLayerHandle();
            m_layers.add(*handle, WTFMove(layer));
        }

        semaphore.signal();
    });
    semaphore.wait();

    return handle;
}

void OpenXRDevice::deleteLayer(LayerHandle handle)
{
    m_layers.remove(handle);
}

Device::FeatureList OpenXRDevice::collectSupportedFeatures() const
{
    Device::FeatureList features;

    // https://www.khronos.org/registry/OpenXR/specs/1.0/man/html/XrReferenceSpaceType.html
    // OpenXR runtimes must support Viewer and Local spaces.
    features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeViewer);
    features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocal);

    // Mark LocalFloor as supported regardless if XR_REFERENCE_SPACE_TYPE_STAGE is available.
    // The spec uses a estimated height if we don't provide a floor transform in frameData.
    features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor);

    // Mark BoundedFloor as supported regardless if XR_REFERENCE_SPACE_TYPE_STAGE is available.
    // The spec allows reporting an empty array if xrGetReferenceSpaceBoundsRect fails.
    features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeBoundedFloor);

    if (m_extensions.isExtensionSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME))
        features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded);

    return features;
}

void OpenXRDevice::collectSupportedSessionModes()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    uint32_t viewConfigurationCount;
    auto result = xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigurationCount, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    Vector<XrViewConfigurationType> viewConfigurations(viewConfigurationCount);
    result = xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations.data());
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    FeatureList features = collectSupportedFeatures();
    for (auto& viewConfiguration : viewConfigurations) {
        auto viewConfigurationProperties = createStructure<XrViewConfigurationProperties, XR_TYPE_VIEW_CONFIGURATION_PROPERTIES>();
        result = xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfiguration, &viewConfigurationProperties);
        if (result != XR_SUCCESS) {
            LOG(XR, "xrGetViewConfigurationProperties(): error %s\n", resultToString(result, m_instance).utf8().data());
            continue;
        }
        auto configType = viewConfigurationProperties.viewConfigurationType;
        switch (configType) {
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
            setSupportedFeatures(SessionMode::Inline, features);
            break;
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
            setSupportedFeatures(SessionMode::ImmersiveVr, features);
            break;
        default:
            continue;
        };
        m_viewConfigurationProperties.add(configType, WTFMove(viewConfigurationProperties));
    }
}

void OpenXRDevice::collectConfigurationViews()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    for (auto& config : m_viewConfigurationProperties.values()) {
        uint32_t viewCount;
        auto configType = config.viewConfigurationType;
        auto result = xrEnumerateViewConfigurationViews(m_instance, m_systemId, configType, 0, &viewCount, nullptr);
        if (result != XR_SUCCESS) {
            LOG(XR, "%s %s: %s\n", __func__, "xrEnumerateViewConfigurationViews", resultToString(result, m_instance).utf8().data());
            continue;
        }

        Vector<XrViewConfigurationView> configViews(viewCount,
            [] {
                XrViewConfigurationView object;
                std::memset(&object, 0, sizeof(XrViewConfigurationView));
                object.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                return object;
            }());
        result = xrEnumerateViewConfigurationViews(m_instance, m_systemId, configType, viewCount, &viewCount, configViews.data());
        if (result != XR_SUCCESS)
            continue;
        m_configurationViews.add(configType, WTFMove(configViews));
    }
}

XrSpace OpenXRDevice::createReferenceSpace(XrReferenceSpaceType type)
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    ASSERT(m_session != XR_NULL_HANDLE);
    ASSERT(m_instance != XR_NULL_HANDLE);

    XrPosef identityPose {
        .orientation = { .x = 0, .y = 0, .z = 0, .w = 1.0 },
        .position = { .x = 0, .y = 0, .z = 0 }
    };

    auto spaceCreateInfo = createStructure<XrReferenceSpaceCreateInfo, XR_TYPE_REFERENCE_SPACE_CREATE_INFO>();
    spaceCreateInfo.referenceSpaceType = type;
    spaceCreateInfo.poseInReferenceSpace = identityPose;

    XrSpace space;
    auto result = xrCreateReferenceSpace(m_session, &spaceCreateInfo, &space);
    RETURN_IF_FAILED(result, "xrCreateReferenceSpace", m_instance, XR_NULL_HANDLE);

    return space;
}

void OpenXRDevice::pollEvents()
{
    ASSERT(!isMainThread());
    auto runtimeEvent = createStructure<XrEventDataBuffer, XR_TYPE_EVENT_DATA_BUFFER>();
    while (xrPollEvent(m_instance, &runtimeEvent) == XR_SUCCESS) {
        switch (runtimeEvent.type) {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            auto* event = reinterpret_cast<XrEventDataSessionStateChanged*>(&runtimeEvent);
            m_sessionState = event->state;
            handleSessionStateChange();
            break;
        }
        case XR_TYPE_EVENT_DATA_EVENTS_LOST:
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            auto* event = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&runtimeEvent);
            if (event->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE)
                updateStageParameters();
            break;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            updateInteractionProfile();
            break;
        case XR_TYPE_EVENT_DATA_MAIN_SESSION_VISIBILITY_CHANGED_EXTX:
        case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
        case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT:
            break;
        default:
            ASSERT_NOT_REACHED("Unhandled event type %d\n", runtimeEvent.type);
        }
    }
}

XrResult OpenXRDevice::beginSession()
{
    ASSERT(!isMainThread());
    ASSERT(m_sessionState == XR_SESSION_STATE_READY);

    auto sessionBeginInfo = createStructure<XrSessionBeginInfo, XR_TYPE_SESSION_BEGIN_INFO>();
    sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    auto result = xrBeginSession(m_session, &sessionBeginInfo);
#if !LOG_DISABLED
    if (XR_FAILED(result))
        LOG(XR, "%s %s: %s\n", __func__, "xrBeginSession", resultToString(result, m_instance).utf8().data());
#endif
    return result;
}

void OpenXRDevice::endSession()
{
    ASSERT(m_session != XR_NULL_HANDLE);
    xrEndSession(m_session);
    resetSession();
    if (!m_trackingAndRenderingClient)
        return;

    // Notify did end event
    callOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }]() {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (m_trackingAndRenderingClient)
            m_trackingAndRenderingClient->sessionDidEnd();
    });
}

void OpenXRDevice::resetSession()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    m_layers.clear();
    m_input.reset();
    if (m_session != XR_NULL_HANDLE) {
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
    }
    m_sessionState = XR_SESSION_STATE_UNKNOWN;
    m_localSpace = XR_NULL_HANDLE;
    m_viewSpace  = XR_NULL_HANDLE;
    m_stageSpace = XR_NULL_HANDLE;
    m_stageParameters = { };

    // deallocate graphic resources
    m_gl = nullptr;
    m_egl.reset();
}

void OpenXRDevice::handleSessionStateChange()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    if (m_sessionState == XR_SESSION_STATE_STOPPING) {
        // The application should exit the render loop and call xrEndSession
        endSession();
    } else if (m_sessionState == XR_SESSION_STATE_READY) {
        // The application is ready to call xrBeginSession.
        beginSession();
    }
}

void OpenXRDevice::waitUntilStopping()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    pollEvents();
    if (m_sessionState >= XR_SESSION_STATE_STOPPING)
        return;
    m_queue.dispatch([this, protectedThis = Ref { *this }]() {
        waitUntilStopping();
    });
}

void OpenXRDevice::updateStageParameters()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    if (m_stageSpace == XR_NULL_HANDLE)
        return; // Stage space not requested.

    m_stageParameters.id++;

    XrExtent2Df bounds;
    if (XR_SUCCEEDED(xrGetReferenceSpaceBoundsRect(m_session, XR_REFERENCE_SPACE_TYPE_STAGE, &bounds))) {
        // https://immersive-web.github.io/webxr/#xrboundedreferencespace-native-bounds-geometry
        // Points MUST be given in a clockwise order as viewed from above, looking towards the negative end of the Y axis.
        m_stageParameters.bounds = Vector<WebCore::FloatPoint> {
            { bounds.width * 0.5f, -bounds.height * 0.5f },
            { bounds.width * 0.5f, bounds.height * 0.5f },
            { -bounds.width * 0.5f, bounds.height * 0.5f },
            { -bounds.width * 0.5f, -bounds.height * 0.5f }
        };
    } else {
        // https://immersive-web.github.io/webxr/#dom-xrboundedreferencespace-boundsgeometry
        // If the native bounds geometry is temporarily unavailable, which may occur for several reasons such as during XR device initialization,
        // extended periods of tracking loss, or movement between pre-configured spaces, the boundsGeometry MUST report an empty array.
        m_stageParameters.bounds.clear();
    }
}

void OpenXRDevice::updateInteractionProfile()
{
    if (!m_input)
        return;

    m_input->updateInteractionProfile();

    if (didNotifyInputInitialization)
        return;

    didNotifyInputInitialization = true;
    auto inputSources = m_input->collectInputSources(m_frameState);
    callOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }, inputSources = WTFMove(inputSources)]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (m_trackingAndRenderingClient)
            m_trackingAndRenderingClient->sessionDidInitializeInputSources(WTFMove(inputSources));
    });
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
