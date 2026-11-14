/*
 * Copyright (C) 2026 Igalia, S.L.
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "OpenXRAnchorManager.h"

#if ENABLE(WEBXR_ANCHORS) && USE(OPENXR)

#include "OpenXRExtensions.h"
#include "OpenXRUtils.h"

namespace WebKit {

std::unique_ptr<OpenXRAnchorManager> OpenXRAnchorManager::create(XrInstance instance, XrSession session, XrSpace localSpace)
{
#if defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)
    return std::unique_ptr<OpenXRAnchorManager>(new OpenXRAnchorManager(instance, session, localSpace));
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(session);
    UNUSED_PARAM(localSpace);
    return nullptr;
#endif
}

OpenXRAnchorManager::OpenXRAnchorManager(XrInstance instance, XrSession session, XrSpace localSpace)
    : m_instance(instance)
    , m_session(session)
    , m_localSpace(localSpace)
{
}

OpenXRAnchorManager::~OpenXRAnchorManager()
{
#if defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)
    if (m_spatialContext)
        OpenXRExtensions::singleton().methods().xrDestroySpatialContextEXT(m_spatialContext);
    m_spatialContextFuture = XR_NULL_FUTURE_EXT;
#endif
}

#if defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)

bool OpenXRAnchorManager::ensureSpatialContext()
{
    if (m_spatialContext)
        return true;

    auto& methods = OpenXRExtensions::singleton().methods();
    // Creating a spatial context is an async operation, the first call starts the creation and gets a future, and later calls (once per frame) poll that future until the runtime reports it ready.
    if (m_spatialContextFuture == XR_NULL_FUTURE_EXT) {
        // Request a context that only enables the anchor capability, with the anchor component as the sole component to be queried.
        static const XrSpatialComponentTypeEXT anchorComponent = XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT;
        XrSpatialCapabilityConfigurationAnchorEXT anchorConfig = { XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ANCHOR_EXT, nullptr, XR_SPATIAL_CAPABILITY_ANCHOR_EXT, 1, const_cast<XrSpatialComponentTypeEXT*>(&anchorComponent) };
        XrSpatialCapabilityConfigurationBaseHeaderEXT* capabilityConfigs[] = { reinterpret_cast<XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&anchorConfig) };

        XrSpatialContextCreateInfoEXT createInfo = { XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT, nullptr, 1, capabilityConfigs };
        XrFutureEXT future = XR_NULL_FUTURE_EXT;
        XrResult res = methods.xrCreateSpatialContextAsyncEXT(m_session, &createInfo, &future);
        if (XR_FAILED(res))
            return false;
        m_spatialContextFuture = future;
    }

    // Poll the future created by the call above. A failed poll invalidates it, so the creation is restarted on the next call.
    auto pollInfo = createOpenXRStruct<XrFuturePollInfoEXT, XR_TYPE_FUTURE_POLL_INFO_EXT>();
    pollInfo.future = m_spatialContextFuture;
    auto pollResult = createOpenXRStruct<XrFuturePollResultEXT, XR_TYPE_FUTURE_POLL_RESULT_EXT>();
    XrResult res = methods.xrPollFutureEXT(m_instance, &pollInfo, &pollResult);
    if (XR_FAILED(res)) {
        LOG(XR, "OpenXRAnchorManager: xrPollFutureEXT failed for spatial context");
        m_spatialContextFuture = XR_NULL_FUTURE_EXT;
        return false;
    }
    if (pollResult.state != XR_FUTURE_STATE_READY_EXT)
        return false;

    // The future is ready so call complete to get the spatial context. The future is consumed whether the call succeeds or not.
    auto completion = createOpenXRStruct<XrCreateSpatialContextCompletionEXT, XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT>();
    res = methods.xrCreateSpatialContextCompleteEXT(m_session, m_spatialContextFuture, &completion);
    m_spatialContextFuture = XR_NULL_FUTURE_EXT;
    if (XR_FAILED(res) || completion.futureResult != XR_SUCCESS) {
        LOG(XR, "OpenXRAnchorManager: xrCreateSpatialContextCompleteEXT failed");
        return false;
    }

    m_spatialContext = completion.spatialContext;
    return true;
}

void OpenXRAnchorManager::processPendingCreates(XrTime time)
{
    if (m_pendingCreates.isEmpty())
        return;
    auto& methods = OpenXRExtensions::singleton().methods();
    if (!ensureSpatialContext()) {
        LOG(XR, "OpenXRAnchorManager: spatial context not ready; keeping pending anchor creates for a later frame");
        return;
    }

    if (!m_spatialContext || m_localSpace == XR_NULL_HANDLE || m_session == XR_NULL_HANDLE) {
        LOG(XR, "OpenXRAnchorManager: invalid session, local space, or spatial context; failing pending anchor creates");
        for (auto& [pose, callback] : m_pendingCreates)
            callback(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError, "OpenXR anchor prerequisites not met"_s });
        m_pendingCreates.clear();
        return;
    }

    for (auto& [pose, callback] : m_pendingCreates) {
        XrPosef xrPoseItem = PoseToXrPosef(pose);
        XrSpatialAnchorCreateInfoEXT createInfo = { XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_EXT, nullptr, m_localSpace, time, xrPoseItem };
        XrSpatialEntityIdEXT entityId = 0;
        XrSpatialEntityEXT entity { };
        XrResult res = methods.xrCreateSpatialAnchorEXT(m_spatialContext, &createInfo, &entityId, &entity);
        CreateAnchorCallback completionHandler = WTF::move(callback);
        if (XR_FAILED(res)) {
            completionHandler(WebCore::Exception { WebCore::ExceptionCode::NotSupportedError, makeString("xrCreateSpatialAnchorEXT failed: "_s, res) });
            continue;
        }
        auto identifier = PlatformXR::AnchorIdentifier::generate();
        m_anchors.set(identifier, std::make_pair(entityId, entity));
        m_entityToIdentifier.set(entityId, identifier);
        completionHandler(WebCore::ExceptionOr<PlatformXR::AnchorIdentifier>(WTF::move(identifier)));
    }
    m_pendingCreates.clear();
}

#endif // defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)

void OpenXRAnchorManager::createAnchor(const PlatformXR::FrameData::Pose& pose, CreateAnchorCallback&& callback)
{
#if defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)
    m_pendingCreates.append({ pose, WTF::move(callback) });
#else
    UNUSED_PARAM(pose);
    callback(WebCore::Exception { WebCore::ExceptionCode::NotSupportedError, "OpenXR spatial anchor extensions not available in SDK"_s });
#endif
}

void OpenXRAnchorManager::deleteAnchor(PlatformXR::AnchorIdentifier identifier)
{
#if defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)
    auto it = m_anchors.find(identifier);
    if (it != m_anchors.end()) {
        OpenXRExtensions::singleton().methods().xrDestroySpatialEntityEXT(it->value.second);
        m_entityToIdentifier.remove(it->value.first);
        m_anchors.remove(it);
    }
#else
    UNUSED_PARAM(identifier);
#endif
}

// Anchors the runtime did not report at all are left out. A paused anchor is still tracked but has no usable pose, and a stopped one will not be tracked anymore.
HashMap<PlatformXR::AnchorIdentifier, PlatformXR::AnchorState> OpenXRAnchorManager::computeTrackedAnchors(XrTime predictedDisplayTime)
{
#if defined(XR_EXT_spatial_entity) && defined(XR_EXT_future) && defined(XR_EXT_spatial_anchor)
    processPendingCreates(predictedDisplayTime);

    if (m_anchors.isEmpty())
        return { };

    if (!ensureSpatialContext()) {
        LOG(XR, "OpenXRAnchorManager: spatial context not ready; skipping anchor pose update for this frame");
        return { };
    }

    auto& methods = OpenXRExtensions::singleton().methods();

    // Locating the anchors takes a snapshot of the current state of all the known anchor entities relative to the local space at the predicted display time, then querying the anchor component of that snapshot in three passes: entity count, entity ids with their tracking states, and finally the poses.
    Vector<XrSpatialEntityEXT> entities;
    entities.reserveCapacity(m_anchors.size());
    for (auto& [id, pair] : m_anchors)
        entities.append(pair.second);

    static const XrSpatialComponentTypeEXT snapshotComponentTypes[] = { XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT };
    XrSpatialUpdateSnapshotCreateInfoEXT snapshotInfo = {
        XR_TYPE_SPATIAL_UPDATE_SNAPSHOT_CREATE_INFO_EXT,
        nullptr,
        static_cast<uint32_t>(entities.size()),
        entities.span().data(),
        1u,
        snapshotComponentTypes,
        m_localSpace,
        predictedDisplayTime
    };
    XrSpatialSnapshotEXT snapshot { };
    XrResult res = methods.xrCreateSpatialUpdateSnapshotEXT(m_spatialContext, &snapshotInfo, &snapshot);
    if (XR_FAILED(res) || !snapshot)
        return { };

    static const XrSpatialComponentTypeEXT componentTypes[] = { XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT };
    XrSpatialComponentDataQueryConditionEXT queryCond = { XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT, nullptr, 1, componentTypes };
    XrSpatialComponentDataQueryResultEXT queryResult = { XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT, nullptr, 0, 0, nullptr, 0, 0, nullptr };

    // First figure out how many entities the snapshot holds.
    res = methods.xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult);
    uint32_t count = queryResult.entityIdCountOutput;
    if (XR_FAILED(res) || !count) {
        methods.xrDestroySpatialSnapshotEXT(snapshot);
        return { };
    }

    // Fetch the entity ids and their tracking states.
    Vector<XrSpatialEntityIdEXT> entityIds(count);
    Vector<XrSpatialEntityTrackingStateEXT> entityStates(count);
    queryResult.entityIdCapacityInput = count;
    queryResult.entityIds = entityIds.mutableSpan().data();
    queryResult.entityStateCapacityInput = count;
    queryResult.entityStates = entityStates.mutableSpan().data();
    queryResult.next = nullptr;

    res = methods.xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult);
    if (XR_FAILED(res)) {
        methods.xrDestroySpatialSnapshotEXT(snapshot);
        return { };
    }

    // Get the poses by passing an anchor list. They are returned in the same order as the entity ids.
    Vector<XrPosef> locations(count);
    XrSpatialComponentAnchorListEXT locationList = { XR_TYPE_SPATIAL_COMPONENT_ANCHOR_LIST_EXT, nullptr, count, locations.mutableSpan().data() };
    queryResult.next = &locationList;
    res = methods.xrQuerySpatialComponentDataEXT(snapshot, &queryCond, &queryResult);
    methods.xrDestroySpatialSnapshotEXT(snapshot);
    if (XR_FAILED(res))
        return { };

    // Map the runtime entity ids back to our anchor identifiers.
    HashMap<PlatformXR::AnchorIdentifier, PlatformXR::AnchorState> result;
    for (uint32_t i = 0; i < count; ++i) {
        auto it = m_entityToIdentifier.find(entityIds[i]);
        if (it == m_entityToIdentifier.end())
            continue;
        switch (entityStates[i]) {
        case XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT:
            result.set(it->value, PlatformXR::AnchorState { XrPosefToPose(locations[i]), PlatformXR::AnchorTrackingState::Tracking });
            break;
        case XR_SPATIAL_ENTITY_TRACKING_STATE_PAUSED_EXT:
            result.set(it->value, PlatformXR::AnchorState { XrIdentityPose(), PlatformXR::AnchorTrackingState::Paused });
            break;
        case XR_SPATIAL_ENTITY_TRACKING_STATE_STOPPED_EXT:
            result.set(it->value, PlatformXR::AnchorState { XrIdentityPose(), PlatformXR::AnchorTrackingState::Stopped });
            break;
        default:
            break;
        }
    }
    return result;
#else
    UNUSED_PARAM(predictedDisplayTime);
    return { };
#endif
}

} // namespace WebKit

#endif // ENABLE(WEBXR_ANCHORS) && USE(OPENXR)
