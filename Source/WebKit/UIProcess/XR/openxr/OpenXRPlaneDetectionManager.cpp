/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "OpenXRPlaneDetectionManager.h"

#if ENABLE(WEBXR_PLANE_DETECTION) && USE(OPENXR)

#include "OpenXRExtensions.h"
#include "OpenXRUtils.h"
#include <WebCore/FloatPoint3D.h>
#include <WebCore/Quaternion.h>
#include <numbers>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRPlaneDetectionManager);

#if defined(XR_EXT_spatial_plane_tracking)

// Planes change slowly so 1/4s should be more than enough.
static constexpr Seconds discoveryInterval = 250_ms;

static Vector<XrSpatialComponentTypeEXT> supportedPlaneComponents(XrInstance instance, XrSystemId systemId)
{
    auto& methods = OpenXRExtensions::singleton().methods();
    if (!methods.xrEnumerateSpatialCapabilitiesEXT || !methods.xrEnumerateSpatialCapabilityComponentTypesEXT)
        return { };

    uint32_t capabilityCount = 0;
    if (XR_FAILED(methods.xrEnumerateSpatialCapabilitiesEXT(instance, systemId, 0, &capabilityCount, nullptr)) || !capabilityCount)
        return { };
    Vector<XrSpatialCapabilityEXT> capabilities(capabilityCount);
    if (XR_FAILED(methods.xrEnumerateSpatialCapabilitiesEXT(instance, systemId, capabilityCount, &capabilityCount, capabilities.mutableSpan().data())))
        return { };
    if (!capabilities.contains(XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT))
        return { };

    auto componentTypes = createOpenXRStruct<XrSpatialCapabilityComponentTypesEXT, XR_TYPE_SPATIAL_CAPABILITY_COMPONENT_TYPES_EXT>();
    if (XR_FAILED(methods.xrEnumerateSpatialCapabilityComponentTypesEXT(instance, systemId, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT, &componentTypes)) || !componentTypes.componentTypeCountOutput)
        return { };
    Vector<XrSpatialComponentTypeEXT> components(componentTypes.componentTypeCountOutput);
    componentTypes.componentTypeCapacityInput = components.size();
    componentTypes.componentTypes = components.mutableSpan().data();
    if (XR_FAILED(methods.xrEnumerateSpatialCapabilityComponentTypesEXT(instance, systemId, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT, &componentTypes)))
        return { };
    return components;
}

static constexpr std::array requiredComponents { XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT, XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT };
static constexpr std::array optionalComponents { XR_SPATIAL_COMPONENT_TYPE_POLYGON_2D_EXT, XR_SPATIAL_COMPONENT_TYPE_PLANE_SEMANTIC_LABEL_EXT };

static bool hasRequiredComponents(const Vector<XrSpatialComponentTypeEXT>& components)
{
    return std::ranges::all_of(requiredComponents, [&](auto component) {
        return components.contains(component);
    });
}

static bool hasRequiredMethods()
{
    auto& methods = OpenXRExtensions::singleton().methods();
    return methods.xrPollFutureEXT && methods.xrCancelFutureEXT
        && methods.xrCreateSpatialContextAsyncEXT && methods.xrCreateSpatialContextCompleteEXT && methods.xrDestroySpatialContextEXT
        && methods.xrCreateSpatialDiscoverySnapshotAsyncEXT && methods.xrCreateSpatialDiscoverySnapshotCompleteEXT
        && methods.xrQuerySpatialComponentDataEXT && methods.xrDestroySpatialSnapshotEXT && methods.xrGetSpatialBufferVector2fEXT;
}

static WebCore::Quaternion toQuaternion(const XrQuaternionf& q)
{
    return { q.x, q.y, q.z, q.w };
}

// https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#ext_spatial_entity_bounded2D_component
// OpenXR planes lie on the X-Y plane with +Z as the normal, while WebXR planes lie on the X-Z plane with +Y as the normal.
// The plane space is rotated by +90 degrees around X for the latter, which maps the OpenXR vertex (x, y) to the WebXR vertex (x, -y).
static PlatformXR::FrameData::Pose toWebXRPlanePose(const XrPosef& center)
{
    constexpr double halfSqrt2 = std::numbers::sqrt2 / 2;
    auto orientation = toQuaternion(center.orientation).accumulate({ halfSqrt2, 0, 0, halfSqrt2 });
    return { { center.position.x, center.position.y, center.position.z }, { static_cast<float>(orientation.x), static_cast<float>(orientation.y), static_cast<float>(orientation.z), static_cast<float>(orientation.w) } };
}

bool OpenXRPlaneDetectionManager::isSupported(XrInstance instance, XrSystemId systemId)
{
    return hasRequiredMethods() && hasRequiredComponents(supportedPlaneComponents(instance, systemId));
}

std::unique_ptr<OpenXRPlaneDetectionManager> OpenXRPlaneDetectionManager::create(XrInstance instance, XrSystemId systemId, XrSession session)
{
    if (!hasRequiredMethods())
        return nullptr;
    auto supported = supportedPlaneComponents(instance, systemId);
    if (!hasRequiredComponents(supported))
        return nullptr;

    Vector<XrSpatialComponentTypeEXT> components(requiredComponents);
    for (auto optionalComponent : optionalComponents) {
        if (supported.contains(optionalComponent))
            components.append(optionalComponent);
    }
    return makeUnique<OpenXRPlaneDetectionManager>(instance, session, WTF::move(components));
}

OpenXRPlaneDetectionManager::OpenXRPlaneDetectionManager(XrInstance instance, XrSession session, Vector<XrSpatialComponentTypeEXT>&& components)
    : m_instance(instance)
    , m_session(session)
    , m_components(WTF::move(components))
{
}

OpenXRPlaneDetectionManager::~OpenXRPlaneDetectionManager()
{
    auto& methods = OpenXRExtensions::singleton().methods();
    for (auto future : { m_contextFuture, m_discoveryFuture }) {
        if (future == XR_NULL_FUTURE_EXT)
            continue;
        auto cancelInfo = createOpenXRStruct<XrFutureCancelInfoEXT, XR_TYPE_FUTURE_CANCEL_INFO_EXT>();
        cancelInfo.future = future;
        CHECK_XRCMD(methods.xrCancelFutureEXT(m_instance, &cancelInfo));
    }
    if (m_context != XR_NULL_HANDLE)
        CHECK_XRCMD(methods.xrDestroySpatialContextEXT(m_context));
}

bool OpenXRPlaneDetectionManager::hasComponent(XrSpatialComponentTypeEXT component) const
{
    return m_components.contains(component);
}

bool OpenXRPlaneDetectionManager::isFutureReady(XrFutureEXT future) const
{
    auto pollInfo = createOpenXRStruct<XrFuturePollInfoEXT, XR_TYPE_FUTURE_POLL_INFO_EXT>();
    pollInfo.future = future;
    auto pollResult = createOpenXRStruct<XrFuturePollResultEXT, XR_TYPE_FUTURE_POLL_RESULT_EXT>();
    auto result = OpenXRExtensions::singleton().methods().xrPollFutureEXT(m_instance, &pollInfo, &pollResult);
    if (XR_FAILED(result)) {
        LOG(XR, "xrPollFutureEXT failed: %s", toString(result));
        return false;
    }
    return pollResult.state == XR_FUTURE_STATE_READY_EXT;
}

void OpenXRPlaneDetectionManager::createSpatialContextAsync()
{
    auto& methods = OpenXRExtensions::singleton().methods();

    if (m_contextFuture == XR_NULL_FUTURE_EXT) {
        auto planeConfig = createOpenXRStruct<XrSpatialCapabilityConfigurationPlaneTrackingEXT, XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_PLANE_TRACKING_EXT>();
        planeConfig.capability = XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT;
        planeConfig.enabledComponentCount = m_components.size();
        planeConfig.enabledComponents = m_components.span().data();

        const XrSpatialCapabilityConfigurationBaseHeaderEXT* configs[] = { reinterpret_cast<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>(&planeConfig) };
        auto createInfo = createOpenXRStruct<XrSpatialContextCreateInfoEXT, XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT>();
        createInfo.capabilityConfigCount = 1;
        createInfo.capabilityConfigs = configs;

        auto result = methods.xrCreateSpatialContextAsyncEXT(m_session, &createInfo, &m_contextFuture);
        if (XR_FAILED(result)) {
            LOG(XR, "xrCreateSpatialContextAsyncEXT failed: %s", toString(result));
            m_contextFuture = XR_NULL_FUTURE_EXT;
            m_initializationFailed = true;
        }
        return;
    }

    if (!isFutureReady(m_contextFuture))
        return;

    auto completion = createOpenXRStruct<XrCreateSpatialContextCompletionEXT, XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT>();
    auto result = methods.xrCreateSpatialContextCompleteEXT(m_session, m_contextFuture, &completion);
    m_contextFuture = XR_NULL_FUTURE_EXT;
    if (XR_FAILED(result) || XR_FAILED(completion.futureResult)) {
        LOG(XR, "Failed to create the spatial context: %s %s", toString(result), toString(completion.futureResult));
        m_initializationFailed = true;
        return;
    }
    m_context = completion.spatialContext;
}

void OpenXRPlaneDetectionManager::createSpatialDiscoverySnapshotAsync(XrSpace baseSpace, XrTime time)
{
    auto& methods = OpenXRExtensions::singleton().methods();

    if (m_discoveryFuture == XR_NULL_FUTURE_EXT) {
        if (MonotonicTime::now() - m_lastDiscoveryTime < discoveryInterval)
            return;

        auto createInfo = createOpenXRStruct<XrSpatialDiscoverySnapshotCreateInfoEXT, XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT>();
        createInfo.componentTypeCount = m_components.size();
        createInfo.componentTypes = m_components.span().data();
        auto result = methods.xrCreateSpatialDiscoverySnapshotAsyncEXT(m_context, &createInfo, &m_discoveryFuture);
        if (XR_FAILED(result)) {
            LOG(XR, "xrCreateSpatialDiscoverySnapshotAsyncEXT failed: %s", toString(result));
            m_discoveryFuture = XR_NULL_FUTURE_EXT;
            m_lastDiscoveryTime = MonotonicTime::now();
        }
        return;
    }

    if (!isFutureReady(m_discoveryFuture))
        return;

    auto completionInfo = createOpenXRStruct<XrCreateSpatialDiscoverySnapshotCompletionInfoEXT, XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT>();
    completionInfo.baseSpace = baseSpace;
    completionInfo.time = time;
    completionInfo.future = m_discoveryFuture;
    auto completion = createOpenXRStruct<XrCreateSpatialDiscoverySnapshotCompletionEXT, XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT>();
    auto result = methods.xrCreateSpatialDiscoverySnapshotCompleteEXT(m_context, &completionInfo, &completion);
    m_discoveryFuture = XR_NULL_FUTURE_EXT;
    m_lastDiscoveryTime = MonotonicTime::now();
    if (XR_FAILED(result) || XR_FAILED(completion.futureResult)) {
        LOG(XR, "Failed to complete the spatial discovery snapshot: %s %s", toString(result), toString(completion.futureResult));
        return;
    }

    collectPlanes(completion.snapshot);
    CHECK_XRCMD(methods.xrDestroySpatialSnapshotEXT(completion.snapshot));
}

void OpenXRPlaneDetectionManager::collectPlanes(XrSpatialSnapshotEXT snapshot)
{
    auto& methods = OpenXRExtensions::singleton().methods();

    auto trackingFilter = createOpenXRStruct<XrSpatialFilterTrackingStateEXT, XR_TYPE_SPATIAL_FILTER_TRACKING_STATE_EXT>();
    trackingFilter.trackingState = XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT;
    auto condition = createOpenXRStruct<XrSpatialComponentDataQueryConditionEXT, XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT>();
    condition.next = &trackingFilter;
    condition.componentTypeCount = m_components.size();
    condition.componentTypes = m_components.span().data();

    auto queryResult = createOpenXRStruct<XrSpatialComponentDataQueryResultEXT, XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT>();
    if (XR_FAILED(methods.xrQuerySpatialComponentDataEXT(snapshot, &condition, &queryResult)))
        return;

    uint32_t count = queryResult.entityIdCountOutput;
    if (!count) {
        m_pendingPlanes = Vector<PlatformXR::FrameData::Plane> { };
        return;
    }

    Vector<XrSpatialEntityIdEXT> entityIds(count);
    Vector<XrSpatialEntityTrackingStateEXT> entityStates(count);
    queryResult.entityIdCapacityInput = count;
    queryResult.entityIds = entityIds.mutableSpan().data();
    queryResult.entityStateCapacityInput = count;
    queryResult.entityStates = entityStates.mutableSpan().data();

    Vector<XrSpatialBounded2DDataEXT> bounds(count);
    auto boundsList = createOpenXRStruct<XrSpatialComponentBounded2DListEXT, XR_TYPE_SPATIAL_COMPONENT_BOUNDED_2D_LIST_EXT>();
    boundsList.boundCount = count;
    boundsList.bounds = bounds.mutableSpan().data();
    queryResult.next = &boundsList;

    Vector<XrSpatialPlaneAlignmentEXT> alignments(count);
    auto alignmentList = createOpenXRStruct<XrSpatialComponentPlaneAlignmentListEXT, XR_TYPE_SPATIAL_COMPONENT_PLANE_ALIGNMENT_LIST_EXT>();
    alignmentList.planeAlignmentCount = count;
    alignmentList.planeAlignments = alignments.mutableSpan().data();
    alignmentList.next = queryResult.next;
    queryResult.next = &alignmentList;

    bool hasPolygons = hasComponent(XR_SPATIAL_COMPONENT_TYPE_POLYGON_2D_EXT);
    Vector<XrSpatialPolygon2DDataEXT> polygons(hasPolygons ? count : 0);
    auto polygonList = createOpenXRStruct<XrSpatialComponentPolygon2DListEXT, XR_TYPE_SPATIAL_COMPONENT_POLYGON_2D_LIST_EXT>();
    if (hasPolygons) {
        polygonList.polygonCount = count;
        polygonList.polygons = polygons.mutableSpan().data();
        polygonList.next = queryResult.next;
        queryResult.next = &polygonList;
    }

    bool hasSemanticLabels = hasComponent(XR_SPATIAL_COMPONENT_TYPE_PLANE_SEMANTIC_LABEL_EXT);
    Vector<XrSpatialPlaneSemanticLabelEXT> semanticLabels(hasSemanticLabels ? count : 0);
    auto semanticLabelList = createOpenXRStruct<XrSpatialComponentPlaneSemanticLabelListEXT, XR_TYPE_SPATIAL_COMPONENT_PLANE_SEMANTIC_LABEL_LIST_EXT>();
    if (hasSemanticLabels) {
        semanticLabelList.semanticLabelCount = count;
        semanticLabelList.semanticLabels = semanticLabels.mutableSpan().data();
        semanticLabelList.next = queryResult.next;
        queryResult.next = &semanticLabelList;
    }

    if (XR_FAILED(methods.xrQuerySpatialComponentDataEXT(snapshot, &condition, &queryResult)))
        return;

    Vector<PlatformXR::FrameData::Plane> planes;
    for (uint32_t i = 0; i < queryResult.entityIdCountOutput; ++i) {
        if (entityStates[i] != XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT)
            continue;

        PlatformXR::FrameData::Plane plane;
        plane.id = entityIds[i];
        plane.pose = toWebXRPlanePose(bounds[i].center);

        switch (alignments[i]) {
        case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_UPWARD_EXT:
        case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_DOWNWARD_EXT:
            plane.orientation = PlatformXR::PlaneOrientation::Horizontal;
            break;
        case XR_SPATIAL_PLANE_ALIGNMENT_VERTICAL_EXT:
            plane.orientation = PlatformXR::PlaneOrientation::Vertical;
            break;
        default:
            break;
        }

        if (hasSemanticLabels) {
            switch (semanticLabels[i]) {
            case XR_SPATIAL_PLANE_SEMANTIC_LABEL_FLOOR_EXT:
                plane.semanticLabel = PlatformXR::PlaneSemanticLabel::Floor;
                break;
            case XR_SPATIAL_PLANE_SEMANTIC_LABEL_WALL_EXT:
                plane.semanticLabel = PlatformXR::PlaneSemanticLabel::Wall;
                break;
            case XR_SPATIAL_PLANE_SEMANTIC_LABEL_CEILING_EXT:
                plane.semanticLabel = PlatformXR::PlaneSemanticLabel::Ceiling;
                break;
            case XR_SPATIAL_PLANE_SEMANTIC_LABEL_TABLE_EXT:
                plane.semanticLabel = PlatformXR::PlaneSemanticLabel::Table;
                break;
            default:
                break;
            }
        }

        // The polygon is given in the plane's coordinate system (X-Y plane) relative to its own origin, and WebXR wants it on the X-Z plane.
        auto appendVertex = [&](const WebCore::FloatPoint3D& vertexInPlane) {
            plane.polygon.append(WebCore::FloatPoint(vertexInPlane.x(), -vertexInPlane.y()));
        };
        // The polygon origin is expressed in the snapshot's base space, like the bounds center. Vertices are needed relative to the center.
        auto appendPolygonVertices = [&](uint32_t index) {
            const auto& polygon = polygons[index];
            auto bufferInfo = createOpenXRStruct<XrSpatialBufferGetInfoEXT, XR_TYPE_SPATIAL_BUFFER_GET_INFO_EXT>();
            bufferInfo.bufferId = polygon.vertexBuffer.bufferId;
            uint32_t vertexCount = 0;
            if (XR_FAILED(methods.xrGetSpatialBufferVector2fEXT(snapshot, &bufferInfo, 0, &vertexCount, nullptr)))
                return false;
            Vector<XrVector2f> vertices(vertexCount);
            if (XR_FAILED(methods.xrGetSpatialBufferVector2fEXT(snapshot, &bufferInfo, vertexCount, &vertexCount, vertices.mutableSpan().data())))
                return false;
            auto inverseCenterOrientation = toQuaternion(bounds[index].center.orientation).conjugate();
            auto relativeOrientation = inverseCenterOrientation.accumulate(toQuaternion(polygon.origin.orientation));
            auto relativePosition = inverseCenterOrientation.rotate({ polygon.origin.position.x - bounds[index].center.position.x, polygon.origin.position.y - bounds[index].center.position.y, polygon.origin.position.z - bounds[index].center.position.z });
            for (auto& vertex : vertices) {
                auto rotated = relativeOrientation.rotate({ vertex.x, vertex.y, 0 });
                appendVertex(rotated + relativePosition);
            }
            return true;
        };
        // A plane whose polygon cannot be read falls back to its bounds rectangle so that it stays in the snapshot and keeps its identity.
        if (!hasPolygons || !appendPolygonVertices(i)) {
            float halfWidth = bounds[i].extents.width / 2;
            float halfHeight = bounds[i].extents.height / 2;
            appendVertex({ -halfWidth, -halfHeight, 0 });
            appendVertex({ halfWidth, -halfHeight, 0 });
            appendVertex({ halfWidth, halfHeight, 0 });
            appendVertex({ -halfWidth, halfHeight, 0 });
        }

        planes.append(WTF::move(plane));
    }
    m_pendingPlanes = WTF::move(planes);
}

std::optional<Vector<PlatformXR::FrameData::Plane>> OpenXRPlaneDetectionManager::update(XrSpace baseSpace, XrTime time)
{
    if (m_initializationFailed || baseSpace == XR_NULL_HANDLE)
        return std::nullopt;

    if (m_context == XR_NULL_HANDLE)
        createSpatialContextAsync();
    else
        createSpatialDiscoverySnapshotAsync(baseSpace, time);

    return std::exchange(m_pendingPlanes, std::nullopt);
}

#else // !defined(XR_EXT_spatial_plane_tracking)

bool OpenXRPlaneDetectionManager::isSupported(XrInstance, XrSystemId)
{
    return false;
}

std::unique_ptr<OpenXRPlaneDetectionManager> OpenXRPlaneDetectionManager::create(XrInstance, XrSystemId, XrSession)
{
    return nullptr;
}

OpenXRPlaneDetectionManager::~OpenXRPlaneDetectionManager() = default;

std::optional<Vector<PlatformXR::FrameData::Plane>> OpenXRPlaneDetectionManager::update(XrSpace, XrTime)
{
    return std::nullopt;
}

#endif // defined(XR_EXT_spatial_plane_tracking)

} // namespace WebKit

#endif // ENABLE(WEBXR_PLANE_DETECTION) && USE(OPENXR)
