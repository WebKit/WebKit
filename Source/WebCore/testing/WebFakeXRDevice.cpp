/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "WebFakeXRDevice.h"

#if ENABLE(WEBXR)

#include "DOMPointReadOnly.h"
#include "GraphicsContextGL.h"
#include "JSDOMPromiseDeferred.h"
#include "WebFakeXRInputController.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>

#if ENABLE(WEBXR_HIT_TEST)
#include <WebCore/XRHitTestTrackableType.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SimulatedXRDevice);

static constexpr Seconds FakeXRFrameTime = 15_ms;

void FakeXRView::setProjection(const Vector<float>& projection)
{
    std::ranges::copy(projection, std::begin(m_projection));
}

void FakeXRView::setFieldOfView(const FakeXRViewInit::FieldOfViewInit& fov)
{
    m_fov = PlatformXR::FrameData::Fov { deg2rad(fov.upDegrees), deg2rad(fov.downDegrees), deg2rad(fov.leftDegrees), deg2rad(fov.rightDegrees) };
}

SimulatedXRDevice::SimulatedXRDevice()
    : m_frameTimer(*this, &SimulatedXRDevice::frameTimerFired)
{
    m_supportsOrientationTracking = true;
}

SimulatedXRDevice::~SimulatedXRDevice()
{
    stopTimer();
}

void SimulatedXRDevice::setViews(Vector<PlatformXR::FrameData::View>&& views)
{
    m_frameData.views = WTF::move(views);
}

void SimulatedXRDevice::setNativeBoundsGeometry(const Vector<FakeXRBoundsPoint>& geometry)
{
    m_frameData.stageParameters.id++;
    m_frameData.stageParameters.bounds.clear();
    for (auto& point : geometry)
        m_frameData.stageParameters.bounds.append({ static_cast<float>(point.x), static_cast<float>(point.z) });
}

void SimulatedXRDevice::setViewerOrigin(const std::optional<PlatformXR::FrameData::Pose>& origin)
{
    if (origin) {
        m_frameData.origin = *origin;
        m_frameData.isPositionValid = true;
        m_frameData.isTrackingValid = true;
        return;
    }

    m_frameData.origin = PlatformXR::FrameData::Pose();
    m_frameData.isPositionValid = false;
    m_frameData.isTrackingValid = false;
}

void SimulatedXRDevice::setVisibilityState(XRVisibilityState visibilityState)
{
    if (m_trackingAndRenderingClient)
        m_trackingAndRenderingClient->updateSessionVisibilityState(visibilityState);
}

void SimulatedXRDevice::simulateShutdownCompleted()
{
    if (m_trackingAndRenderingClient)
        m_trackingAndRenderingClient->sessionDidEnd();
}

WebCore::IntSize SimulatedXRDevice::recommendedResolution(PlatformXR::SessionMode)
{
    // Return at least a valid size for a framebuffer.
    return IntSize(32, 32);
}

void SimulatedXRDevice::initializeTrackingAndRendering(const WebCore::SecurityOriginData&, PlatformXR::SessionMode sessionMode, const PlatformXR::Device::FeatureList&, std::optional<WebCore::XRCanvasConfiguration>&&)
{
    if (m_trackingAndRenderingClient) {
        // WebXR FakeDevice waits for simulateInputConnection calls to add input sources-
        // There is no way to know how many simulateInputConnection calls will the device receive,
        // so notify the input sources have been initialized with an empty list. This is not a problem because
        // WPT tests rely on requestAnimationFrame updates to test the input sources.
        callOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }]() {
            auto protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (m_trackingAndRenderingClient)
                m_trackingAndRenderingClient->sessionDidInitializeInputSources({ });
        });
    }
    m_frameData.environmentBlendMode = (sessionMode == PlatformXR::SessionMode::ImmersiveAr) ? PlatformXR::XREnvironmentBlendMode::AlphaBlend : PlatformXR::XREnvironmentBlendMode::Opaque;
}

void SimulatedXRDevice::shutDownTrackingAndRendering()
{
    if (m_supportsShutdownNotification)
        simulateShutdownCompleted();
    stopTimer();
    m_layers.clear();
}

void SimulatedXRDevice::stopTimer()
{
    if (m_frameTimer.isActive())
        m_frameTimer.stop();
}

void SimulatedXRDevice::frameTimerFired()
{
    PlatformXR::FrameData data = m_frameData.copy();
    data.shouldRender = true;

    for (auto& layer : m_layers) {
        PlatformXR::FrameData::LayerSetupData layerSetupData;
        auto width = layer.value.width();
        auto height = layer.value.height();
        layerSetupData.physicalSize[0] = { static_cast<uint16_t>(width), static_cast<uint16_t>(height) };
        layerSetupData.viewports[0] = { 0, 0, width, height };
        layerSetupData.physicalSize[1] = { 0, 0 };
        layerSetupData.viewports[1] = { 0, 0, 0, 0 };
        auto layerData = makeUniqueRef<PlatformXR::FrameData::LayerData>(PlatformXR::FrameData::LayerData {
            .layerSetup = layerSetupData,
            .renderingFrameIndex = 0,
            .textureData = std::nullopt,
            .requestDepth = false,
            .isForTesting = true
        });
        data.layers.add(layer.key, WTF::move(layerData));
    }

    for (auto& input : m_inputConnections) {
        if (input->isConnected())
            data.inputSources.append(input->getFrameData());
    }

#if ENABLE(WEBXR_HIT_TEST)
    auto transformFromPose = [](const PlatformXR::FrameData::Pose& pose) {
        TransformationMatrix translation;
        translation.translate3d(pose.position.x(), pose.position.y(), pose.position.z());
        auto rotation = TransformationMatrix::fromQuaternion({ pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w });
        return translation * rotation;
    };
    auto mapPoint = [](const TransformationMatrix& m, FloatPoint3D p, double w) {
        float x = m.m11() * p.x() + m.m21() * p.y() + m.m31() * p.z() + m.m41() * w;
        float y = m.m12() * p.x() + m.m22() * p.y() + m.m32() * p.z() + m.m42() * w;
        float z = m.m13() * p.x() + m.m23() * p.y() + m.m33() * p.z() + m.m43() * w;
        return FloatPoint3D { x, y, z };
    };
    auto transformRay = [&](const PlatformXR::FrameData::Pose& origin, const PlatformXR::Ray& ray) {
        auto transform = transformFromPose(origin);
        return PlatformXR::Ray {
            .origin = mapPoint(transform, ray.origin, 1),
            .direction = mapPoint(transform, ray.direction, 0)
        };
    };
    // Non-transient hit test
    for (const auto& pair : m_hitTestSources) {
        std::optional<PlatformXR::FrameData::Pose> origin;
        WTF::switchOn(pair.value->nativeOrigin, [&](const PlatformXR::ReferenceSpaceType& referenceSpaceType) {
            switch (referenceSpaceType) {
            case PlatformXR::ReferenceSpaceType::Viewer:
                origin = data.origin;
                break;
            case PlatformXR::ReferenceSpaceType::Local:
                origin = PlatformXR::FrameData::Pose();
                break;
            case PlatformXR::ReferenceSpaceType::LocalFloor:
                origin = data.floorTransform;
                break;
            default:
                break;
            }
        }, [&](const PlatformXR::InputSourceSpaceInfo& inputSource) {
            auto i = data.inputSources.findIf([&](auto& item) { return item.handle == inputSource.handle; });
            if (i == notFound)
                return;
            if (inputSource.type == PlatformXR::InputSourceSpaceType::TargetRay)
                origin = data.inputSources[i].pointerOrigin.pose;
            else
                origin = data.inputSources[i].gripOrigin.value_or(PlatformXR::FrameData::InputSourcePose { }).pose;
        });
        if (!origin)
            continue;
        PlatformXR::Ray ray = transformRay(*origin, pair.value->offsetRay);
        data.hitTestResults.add(pair.key, hitTestWorld(ray, pair.value->entityTypes));
    }
    // Transient hit test
    for (const auto& pair : m_transientInputHitTestSources) {
        Vector<PlatformXR::FrameData::TransientInputHitTestResult> results;
        for (const auto& source : data.inputSources) {
            if (source.profiles.contains(pair.value->profile)) {
                PlatformXR::Ray ray = transformRay(source.pointerOrigin.pose, pair.value->offsetRay);
                results.append({ source.handle, hitTestWorld(ray, pair.value->entityTypes) });
            }
        }
        data.transientInputHitTestResults.add(pair.key, WTF::move(results));
    }
#endif

    if (m_FrameCallback)
        m_FrameCallback(WTF::move(data));
}

void SimulatedXRDevice::requestFrame(std::optional<PlatformXR::RequestData>&&, RequestFrameCallback&& callback)
{
    m_FrameCallback = WTF::move(callback);
    if (!m_frameTimer.isActive())
        m_frameTimer.startOneShot(FakeXRFrameTime);
}

std::optional<PlatformXR::LayerHandle> SimulatedXRDevice::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
    // TODO: Might need to pass the format type to WebXROpaqueFramebuffer to ensure alpha is handled correctly in tests.
    UNUSED_PARAM(alpha);
    PlatformXR::LayerHandle handle = ++m_layerIndex;
    m_layers.add(handle, IntSize { static_cast<int>(width), static_cast<int>(height) });
    return handle;
}

void SimulatedXRDevice::deleteLayer(PlatformXR::LayerHandle handle)
{
    auto it = m_layers.find(handle);
    if (it != m_layers.end()) {
        m_layers.remove(it);
    }
}

#if ENABLE(WEBXR_HIT_TEST)
void SimulatedXRDevice::requestHitTestSource(const PlatformXR::HitTestOptions& options, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::HitTestSource>)>&& completionHandler)
{
    auto addResult = m_hitTestSources.add(m_nextHitTestSource, makeUniqueRef<PlatformXR::HitTestOptions>(options));
    ASSERT_UNUSED(addResult.isNewEntry, addResult);
    completionHandler(m_nextHitTestSource);
    m_nextHitTestSource++;
}

void SimulatedXRDevice::deleteHitTestSource(PlatformXR::HitTestSource source)
{
    bool removed = m_hitTestSources.remove(source);
    ASSERT_UNUSED(removed, removed);
}

void SimulatedXRDevice::requestTransientInputHitTestSource(const PlatformXR::TransientInputHitTestOptions& options, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource>)>&& completionHandler)
{
    auto addResult = m_transientInputHitTestSources.add(m_nextTransientInputHitTestSource, makeUniqueRef<PlatformXR::TransientInputHitTestOptions>(options));
    ASSERT_UNUSED(addResult.isNewEntry, addResult);
    completionHandler(m_nextTransientInputHitTestSource);
    m_nextTransientInputHitTestSource++;
}

void SimulatedXRDevice::deleteTransientInputHitTestSource(PlatformXR::TransientInputHitTestSource source)
{
    bool removed = m_transientInputHitTestSources.remove(source);
    ASSERT_UNUSED(removed, removed);
}

// https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/web_tests/external/wpt/resources/chromium/webxr-test.js
Vector<PlatformXR::FrameData::HitTestResult> SimulatedXRDevice::hitTestWorld(const PlatformXR::Ray& ray, const Vector<XRHitTestTrackableType>& entityTypes)
{
    struct HitTestResult {
        double distance;
        PlatformXR::FrameData::Pose pose;
    };
    Vector<HitTestResult> resultsForRegions;
    for (const auto& region : m_world.hitTestRegions) {
        std::optional<XRHitTestTrackableType> type;
        switch (region.type) {
        case FakeXRWorldInit::RegionType::Point:
            type = XRHitTestTrackableType::Point;
            break;
        case FakeXRWorldInit::RegionType::Plane:
            type = XRHitTestTrackableType::Plane;
            break;
        case FakeXRWorldInit::RegionType::Mesh:
            type = XRHitTestTrackableType::Mesh;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        if (!entityTypes.contains(*type))
            continue;

        Vector<HitTestResult> resultsForFaces;
        for (const auto& face : region.faces) {
            using Point = DOMPointInit;
            auto toPoint = [](FloatPoint3D point, double w) -> Point {
                return { point.x(), point.y(), point.z(), w };
            };
            auto neg = [](Point p) -> Point {
                return { -p.x, -p.y, -p.z, p.w };
            };
            auto sub = [](Point lhs, Point rhs) -> Point {
                // .w is treated here like an entity type, 1 signifies points, 0 signifies vectors.
                // point - point, point - vector, vector - vector are ok, vector - point is not.
                RELEASE_ASSERT(lhs.w == rhs.w || lhs.w);
                return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w };
            };
            auto add = [](Point lhs, Point rhs) -> Point {
                RELEASE_ASSERT(!lhs.w || !rhs.w); // point + point not allowed
                return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w };
            };
            auto cross = [](Point lhs, Point rhs) -> Point {
                RELEASE_ASSERT(!lhs.w);
                RELEASE_ASSERT(!rhs.w);
                return {
                    .x = lhs.y * rhs.z - lhs.z * rhs.y,
                    .y = lhs.z * rhs.x - lhs.x * rhs.z,
                    .z = lhs.x * rhs.y - lhs.y * rhs.x,
                    .w = 0
                };
            };
            auto dot = [](Point lhs, Point rhs) -> double {
                RELEASE_ASSERT(!lhs.w);
                RELEASE_ASSERT(!rhs.w);
                return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
            };
            auto mul = [](double scalar, Point vector) -> Point {
                RELEASE_ASSERT(!vector.w);
                return { vector.x * scalar, vector.y * scalar, vector.z * scalar, vector.w };
            };
            auto length = [&](Point vector) -> double {
                return std::sqrt(dot(vector, vector));
            };
            auto normalize = [&](Point vector) -> Point {
                return mul(1 / length(vector), vector);
            };
            // All |face|'s points and |point| must be co-planar.
            auto pointInFace = [&](Point point, const FakeXRWorldInit::TriangleInit& face) -> bool {
                std::optional<bool> onTheRight;
                Point previousPoint = face.vertices.last();

                // |point| is in |face| if it's on the same side of all the edges.
                for (unsigned i = 0; i < face.vertices.size(); ++i) {
                    Point currentPoint = face.vertices[i];

                    Point edgeDirection = normalize(sub(currentPoint, previousPoint));
                    Point turnDirection = normalize(sub(point, currentPoint));

                    double sinTurnAngle = length(cross(edgeDirection, turnDirection));

                    if (!onTheRight)
                        onTheRight = sinTurnAngle >= 0;
                    else {
                        if (*onTheRight && sinTurnAngle < 0)
                            return false;
                        if (!*onTheRight && sinTurnAngle > 0)
                            return false;
                    }

                    previousPoint = currentPoint;
                }
                return true;
            };
            auto rigidTransformToPose = [](TransformationMatrix matrix) -> PlatformXR::FrameData::Pose {
                TransformationMatrix::Decomposed4Type decomposed;
                bool succeeded = matrix.decompose4(decomposed);
                RELEASE_ASSERT(succeeded);
                FloatPoint3D position(decomposed.translateX, decomposed.translateY, decomposed.translateZ);
                PlatformXR::FrameData::FloatQuaternion orientation(decomposed.quaternion.x, decomposed.quaternion.y, decomposed.quaternion.z, decomposed.quaternion.w);
                return { position, orientation };
            };
            constexpr double epsilon = 0.001;

            // 1. Calculate plane normal in world coordinates.
            Point pointA = face.vertices[0];
            Point pointB = face.vertices[1];
            Point pointC = face.vertices[2];

            Point edgeAB = sub(pointB, pointA);
            Point edgeAC = sub(pointC, pointA);

            Point normal = normalize(cross(edgeAB, edgeAC));

            Point origin = toPoint(ray.origin, 1);
            double numerator = dot(sub(pointA, origin), normal);
            Point direction = toPoint(ray.direction, 0);
            double denominator = dot(direction, normal);
            if (std::abs(denominator) < epsilon)
                continue;
            double distance = numerator / denominator;
            if (distance < 0)
                continue;

            Point intersectionPoint = add(origin, mul(distance, direction));
            // Since we are treating the face as a solid, flip the normal so that its
            // half-space will contain the ray origin.
            Point yAxis = denominator > 0 ? neg(normal) : normal;

            Point zAxis;
            double cosDirectionAndYAxis = dot(direction, yAxis);
            if (std::abs(cosDirectionAndYAxis) > (1 - epsilon)) {
                // Ray and the hit test normal are co-linear - try using the 'up' or 'right' vector's projection on the face plane as the Z axis.
                // Note: this edge case is currently not covered by the spec.
                Point up { 0, 1, 0, 0 };
                Point right { 1, 0, 0, 0 };

                zAxis = std::abs(dot(up, yAxis)) > (1 - epsilon)
                    ? sub(up, mul(dot(right, yAxis), yAxis)) // `up is also co-linear with hit test normal, use `right`
                    : sub(up, mul(dot(up, yAxis), yAxis)); // `up` is not co-linear with hit test normal, use it
            } else {
                // Project the ray direction onto the plane, negate it and use as a Z axis.
                zAxis = neg(sub(direction, mul(cosDirectionAndYAxis, yAxis))); // Z should point towards the ray origin, not away.
            }

            zAxis = normalize(zAxis);

            Point xAxis = normalize(cross(yAxis, zAxis));

            // Filter out the points not in polygon.
            if (!pointInFace(intersectionPoint, face))
                continue;

            TransformationMatrix matrix;
            matrix.setM11(xAxis.x);
            matrix.setM12(xAxis.y);
            matrix.setM13(xAxis.z);
            matrix.setM14(0);

            matrix.setM21(yAxis.x);
            matrix.setM22(yAxis.y);
            matrix.setM23(yAxis.z);
            matrix.setM24(0);

            matrix.setM31(zAxis.x);
            matrix.setM32(zAxis.y);
            matrix.setM33(zAxis.z);
            matrix.setM34(0);

            matrix.setM41(intersectionPoint.x);
            matrix.setM42(intersectionPoint.y);
            matrix.setM43(intersectionPoint.z);
            matrix.setM44(1);

            resultsForFaces.append({ distance, rigidTransformToPose(matrix) });
        }
        // The results should be sorted by distance and there should be no 2 entries with
        // the same distance from ray origin - that would mean they are the same point.
        // This situation is possible when a ray intersects the region through an edge shared
        // by 2 faces.
        std::ranges::sort(resultsForFaces, { }, &HitTestResult::distance);
        for (auto it = resultsForFaces.begin(); it != resultsForFaces.end(); it++) {
            if (it == resultsForFaces.begin() || it->distance != (it-1)->distance)
                resultsForRegions.append(*it);
        }
    }
    std::ranges::sort(resultsForRegions, { }, &HitTestResult::distance);
    return resultsForRegions.map([](auto& x) { return PlatformXR::FrameData::HitTestResult { x.pose }; });
}

void SimulatedXRDevice::setWorld(const FakeXRWorldInit& world)
{
    m_world = world;
}

void SimulatedXRDevice::clearWorld()
{
    m_world.hitTestRegions.clear();
}
#endif

Vector<PlatformXR::Device::ViewData> SimulatedXRDevice::views(PlatformXR::SessionMode mode) const
{
    if (mode == PlatformXR::SessionMode::ImmersiveVr)
        return { { .active = true, .eye = PlatformXR::Eye::Left }, { .active = true, .eye = PlatformXR::Eye::Right } };

    return { { .active = true, .eye = PlatformXR::Eye::None } };
}

WebFakeXRDevice::WebFakeXRDevice()
    : m_device(adoptRef(*new SimulatedXRDevice()))
{
}

void WebFakeXRDevice::setViews(const Vector<FakeXRViewInit>& views)
{
    Vector<PlatformXR::FrameData::View> deviceViews;

    for (auto& viewInit : views) {
        auto parsedView = parseView(viewInit);
        if (!parsedView.hasException()) {
            auto fakeView = parsedView.releaseReturnValue();
            PlatformXR::FrameData::View view;
            view.offset = fakeView->offset();
            if (fakeView->fieldOfView())
                view.projection = { *fakeView->fieldOfView() };
            else
                view.projection = { fakeView->projection() };

            deviceViews.append(view);
        }
    }

    m_device->setViews(WTF::move(deviceViews));
}

void WebFakeXRDevice::disconnect(DOMPromiseDeferred<void>&& promise)
{
    promise.resolve();
}

void WebFakeXRDevice::setViewerOrigin(FakeXRRigidTransformInit origin, bool emulatedPosition)
{
    auto pose = parseRigidTransform(origin);
    if (pose.hasException())
        return;

    m_device->setViewerOrigin(pose.releaseReturnValue());
    m_device->setEmulatedPosition(emulatedPosition);
}

void WebFakeXRDevice::simulateVisibilityChange(XRVisibilityState visibilityState)
{
    m_device->setVisibilityState(visibilityState);
}

void WebFakeXRDevice::setFloorOrigin(FakeXRRigidTransformInit origin)
{
    auto pose = parseRigidTransform(origin);
    if (pose.hasException())
        return;

    m_device->setFloorOrigin(pose.releaseReturnValue());
}

void WebFakeXRDevice::simulateResetPose()
{
}

Ref<WebFakeXRInputController> WebFakeXRDevice::simulateInputSourceConnection(const FakeXRInputSourceInit& init)
{
    auto handle = ++mInputSourceHandleIndex;
    auto input = WebFakeXRInputController::create(handle, init);
    m_device->addInputConnection(input.copyRef());
    return input;
}

ExceptionOr<PlatformXR::FrameData::Pose> WebFakeXRDevice::parseRigidTransform(const FakeXRRigidTransformInit& init)
{
    if (init.position.size() != 3 || init.orientation.size() != 4)
        return Exception { ExceptionCode::TypeError };

    PlatformXR::FrameData::Pose pose;
    pose.position = { init.position[0], init.position[1], init.position[2] };
    pose.orientation = { init.orientation[0], init.orientation[1], init.orientation[2], init.orientation[3] };

    return pose;
}

ExceptionOr<Ref<FakeXRView>> WebFakeXRDevice::parseView(const FakeXRViewInit& init)
{
    // https://immersive-web.github.io/webxr-test-api/#parse-a-view
    auto fakeView = FakeXRView::create(init.eye);

    if (init.projectionMatrix.size() != 16)
        return Exception { ExceptionCode::TypeError };
    fakeView->setProjection(init.projectionMatrix);

    auto viewOffset = parseRigidTransform(init.viewOffset);
    if (viewOffset.hasException())
        return viewOffset.releaseException();
    fakeView->setOffset(viewOffset.releaseReturnValue());

    fakeView->setResolution(init.resolution);

    if (init.fieldOfView) {
        fakeView->setFieldOfView(init.fieldOfView.value());
    }

    return fakeView;
}

void WebFakeXRDevice::setSupportsShutdownNotification()
{
    m_device->setSupportsShutdownNotification(true);
}

void WebFakeXRDevice::simulateShutdown()
{
    m_device->simulateShutdownCompleted();
}

#if ENABLE(WEBXR_HIT_TEST)
void WebFakeXRDevice::setWorld(const FakeXRWorldInit& world)
{
    m_device->setWorld(world);
}

void WebFakeXRDevice::clearWorld()
{
    m_device->clearWorld();
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBXR)
