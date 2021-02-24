/*
 * Copyright (C) 2020 Igalia, S.L.
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

#include "OpenXRExtensions.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>

using namespace WebCore;

namespace PlatformXR {


static bool isSessionActive(XrSessionState state)
{
    return state == XR_SESSION_STATE_VISIBLE || state == XR_SESSION_STATE_FOCUSED;
}

static bool isSessionReady(XrSessionState state)
{
    return state >= XR_SESSION_STATE_READY  && state < XR_SESSION_STATE_STOPPING;
}

OpenXRDevice::OpenXRDevice(XrInstance instance, XrSystemId system, WorkQueue& queue, const OpenXRExtensions& extensions, CompletionHandler<void()>&& callback)
    : m_instance(instance)
    , m_systemId(system)
    , m_queue(queue)
    , m_extensions(extensions)
{
    ASSERT(isMainThread());
    m_queue.dispatch([this, callback = WTFMove(callback)]() mutable {
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
        return { static_cast<int>(viewsIterator->value[0].recommendedImageRectWidth), static_cast<int>(viewsIterator->value[0].recommendedImageRectHeight) };
    return Device::recommendedResolution(mode);
}

void OpenXRDevice::initializeTrackingAndRendering(SessionMode mode)
{
    m_queue.dispatch([this, mode]() {
        ASSERT(m_instance != XR_NULL_HANDLE);
        ASSERT(m_session == XR_NULL_HANDLE);

        m_currentViewConfigurationType = toXrViewConfigurationType(mode);
        ASSERT(m_configurationViews.contains(m_currentViewConfigurationType));

        // Create the session.
        auto sessionCreateInfo = createStructure<XrSessionCreateInfo, XR_TYPE_SESSION_CREATE_INFO>();
        sessionCreateInfo.systemId = m_systemId;

        auto result = xrCreateSession(m_instance, &sessionCreateInfo, &m_session);
        RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties", m_instance);

        // Create the default reference spaces
        m_localSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL);
        m_viewSpace = createReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);
    });
}

void OpenXRDevice::shutDownTrackingAndRendering()
{
    m_queue.dispatch([this]() {
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
}

void OpenXRDevice::requestFrame(RequestFrameCallback&& callback)
{
    m_queue.dispatch([this, callback = WTFMove(callback)]() mutable {
        pollEvents();
        if (!isSessionReady(m_sessionState)) {
            callOnMainThread([callback = WTFMove(callback)]() mutable {
                // Device not ready or stopping. Report frameData with invalid tracking.
                callback({ });
            });
            return;
        }

        auto frameState = createStructure<XrFrameState, XR_TYPE_FRAME_STATE>();
        auto frameWaitInfo = createStructure<XrFrameWaitInfo, XR_TYPE_FRAME_WAIT_INFO>();
        auto result = xrWaitFrame(m_session, &frameWaitInfo, &frameState);
        RETURN_IF_FAILED(result, "xrWaitFrame", m_instance);
        XrTime predictedTime = frameState.predictedDisplayTime;

        auto frameBeginInfo = createStructure<XrFrameBeginInfo, XR_TYPE_FRAME_BEGIN_INFO>();
        result = xrBeginFrame(m_session, &frameBeginInfo);
        RETURN_IF_FAILED(result, "xrBeginFrame", m_instance);

        Device::FrameData frameData;
        frameData.predictedDisplayTime = frameState.predictedDisplayTime;


        ASSERT(m_configurationViews.contains(m_currentViewConfigurationType));
        const auto& configurationView = m_configurationViews.get(m_currentViewConfigurationType);

        uint32_t viewCount = configurationView.size();
        Vector<XrView> views(viewCount, [] {
            return createStructure<XrView, XR_TYPE_VIEW>();
        }());


        if (isSessionActive(m_sessionState)) {
            // Query head location
            auto location = createStructure<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
            xrLocateSpace(m_viewSpace, m_localSpace, frameState.predictedDisplayTime, &location);
            frameData.isTrackingValid = location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            frameData.isPositionValid = location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT;
            frameData.isPositionEmulated = location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

            if (frameData.isTrackingValid)
                frameData.origin = XrPosefToPose(location.pose);
            

            auto viewLocateInfo = createStructure<XrViewLocateInfo, XR_TYPE_VIEW_LOCATE_INFO>();
            viewLocateInfo.displayTime = predictedTime;
            viewLocateInfo.space = m_localSpace;

            auto viewState = createStructure<XrViewState, XR_TYPE_VIEW_STATE>();
            uint32_t viewCountOutput;
            result = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCount, &viewCountOutput, views.data());
            if (!XR_FAILED(result)) {
                for (auto& view : views)
                    frameData.views.append(xrViewToPose(view));
            }

            // Query floor transform
            if (m_stageSpace != XR_NULL_HANDLE) {
                auto floorLocation = createStructure<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
                xrLocateSpace(m_stageSpace, m_localSpace, frameState.predictedDisplayTime, &floorLocation);
                frameData.floorTransform = { XrPosefToPose(floorLocation.pose) };
            }
        }

        callOnMainThread([frameData = WTFMove(frameData), callback = WTFMove(callback)]() mutable {
            callback(WTFMove(frameData));
        });


        Vector<const XrCompositionLayerBaseHeader*> layers;

        auto frameEndInfo = createStructure<XrFrameEndInfo, XR_TYPE_FRAME_END_INFO>();
        frameEndInfo.displayTime = predictedTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        frameEndInfo.layerCount = layers.size();
        result = xrEndFrame(m_session, &frameEndInfo);
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
        views.append({ .active = true, Eye::Left });
        views.append({ .active = true, Eye::Right });
    }
    return views;
}

Device::ListOfEnabledFeatures OpenXRDevice::enumerateReferenceSpaces(XrSession session) const
{
    uint32_t referenceSpacesCount;
    auto result = xrEnumerateReferenceSpaces(session, 0, &referenceSpacesCount, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateReferenceSpaces", m_instance, { });

    Vector<XrReferenceSpaceType> referenceSpaces(referenceSpacesCount);
    referenceSpaces.fill(XR_REFERENCE_SPACE_TYPE_VIEW, referenceSpacesCount);
    result = xrEnumerateReferenceSpaces(session, referenceSpacesCount, &referenceSpacesCount, referenceSpaces.data());
    RETURN_IF_FAILED(result, "xrEnumerateReferenceSpaces", m_instance, { });

    ListOfEnabledFeatures enabledFeatures;
    for (auto& referenceSpace : referenceSpaces) {
        switch (referenceSpace) {
        case XR_REFERENCE_SPACE_TYPE_VIEW:
            enabledFeatures.append(ReferenceSpaceType::Viewer);
            LOG(XR, "\tDevice supports VIEW reference space");
            break;
        case XR_REFERENCE_SPACE_TYPE_LOCAL:
            enabledFeatures.append(ReferenceSpaceType::Local);
            LOG(XR, "\tDevice supports LOCAL reference space");
            break;
        case XR_REFERENCE_SPACE_TYPE_STAGE:
            enabledFeatures.append(ReferenceSpaceType::LocalFloor);
            enabledFeatures.append(ReferenceSpaceType::BoundedFloor);
            LOG(XR, "\tDevice supports STAGE reference space");
            break;
        case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
            enabledFeatures.append(ReferenceSpaceType::Unbounded);
            LOG(XR, "\tDevice supports UNBOUNDED reference space");
            break;
        default:
            continue;
        }
    }

    return enabledFeatures;
}

void OpenXRDevice::collectSupportedSessionModes()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    uint32_t viewConfigurationCount;
    auto result = xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigurationCount, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    XrViewConfigurationType viewConfigurations[viewConfigurationCount];
    result = xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations);
    RETURN_IF_FAILED(result, "xrEnumerateViewConfigurations", m_instance);

    // Retrieving the supported reference spaces requires an initialized session. There is no need to initialize all the graphics
    // stuff so we'll use a headless session that will be discarded after getting the info we need.
    auto sessionCreateInfo = createStructure<XrSessionCreateInfo, XR_TYPE_SESSION_CREATE_INFO>();
    sessionCreateInfo.systemId = m_systemId;
    XrSession ephemeralSession;
    result = xrCreateSession(m_instance, &sessionCreateInfo, &ephemeralSession);
    RETURN_IF_FAILED(result, "xrCreateSession", m_instance);

    ListOfEnabledFeatures features = enumerateReferenceSpaces(ephemeralSession);
    for (uint32_t i = 0; i < viewConfigurationCount; ++i) {
        auto viewConfigurationProperties = createStructure<XrViewConfigurationProperties, XR_TYPE_VIEW_CONFIGURATION_PROPERTIES>();
        result = xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigurations[i], &viewConfigurationProperties);
        if (result != XR_SUCCESS) {
            LOG(XR, "xrGetViewConfigurationProperties(): error %s\n", resultToString(result, m_instance).utf8().data());
            continue;
        }
        auto configType = viewConfigurationProperties.viewConfigurationType;
        switch (configType) {
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
            setEnabledFeatures(SessionMode::Inline, features);
            break;
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
            setEnabledFeatures(SessionMode::ImmersiveVr, features);
            break;
        default:
            continue;
        };
        m_viewConfigurationProperties.add(configType, WTFMove(viewConfigurationProperties));
    }
    xrDestroySession(ephemeralSession);
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
            auto* event = (XrEventDataSessionStateChanged*)&runtimeEvent;
            m_sessionState = event->state;
            handleSessionStateChange();
            break;
        }
        case XR_TYPE_EVENT_DATA_EVENTS_LOST:
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
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
    callOnMainThread([this, weakThis = makeWeakPtr(*this)]() {
        if (!weakThis)
            return;
        if (m_trackingAndRenderingClient)
            m_trackingAndRenderingClient->sessionDidEnd();
    });
}

void OpenXRDevice::resetSession()
{
    ASSERT(&RunLoop::current() == &m_queue.runLoop());
    if (m_session != XR_NULL_HANDLE) {
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
    }
    m_sessionState = XR_SESSION_STATE_UNKNOWN;
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
    m_queue.dispatch([this]() {
        waitUntilStopping();
    });
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
