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
#pragma once

#include "DestinationColorSpace.h"
#include "FloatPoint3D.h"
#include "GraphicsTypesGL.h"
#include "IntRect.h"
#include "IntSize.h"
#include <memory>
#include <variant>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include "IOSurface.h"
#include <wtf/MachSendRight.h>
#endif

namespace WebCore {
struct SecurityOriginData;
}

namespace PlatformXR {

enum class SessionMode : uint8_t {
    Inline,
    ImmersiveVr,
    ImmersiveAr,
};

enum class ReferenceSpaceType {
    Viewer,
    Local,
    LocalFloor,
    BoundedFloor,
    Unbounded
};

enum class Eye {
    None,
    Left,
    Right,
};

enum class VisibilityState {
    Visible,
    VisibleBlurred,
    Hidden
};

using LayerHandle = int;

#if ENABLE(WEBXR)
using InputSourceHandle = int;

// https://immersive-web.github.io/webxr/#enumdef-xrhandedness
enum class XRHandedness {
    None,
    Left,
    Right,
};

// https://immersive-web.github.io/webxr/#enumdef-xrtargetraymode
enum class XRTargetRayMode {
    Gaze,
    TrackedPointer,
    Screen,
};

// https://immersive-web.github.io/webxr/#feature-descriptor
enum class SessionFeature : uint8_t {
    ReferenceSpaceTypeViewer,
    ReferenceSpaceTypeLocal,
    ReferenceSpaceTypeLocalFloor,
    ReferenceSpaceTypeBoundedFloor,
    ReferenceSpaceTypeUnbounded,
#if ENABLE(WEBXR_HANDS)
    HandTracking,
#endif
};

inline SessionFeature sessionFeatureFromReferenceSpaceType(ReferenceSpaceType referenceSpaceType)
{
    switch (referenceSpaceType) {
    case ReferenceSpaceType::Viewer:
        return SessionFeature::ReferenceSpaceTypeViewer;
    case ReferenceSpaceType::Local:
        return SessionFeature::ReferenceSpaceTypeLocal;
    case ReferenceSpaceType::LocalFloor:
        return SessionFeature::ReferenceSpaceTypeLocalFloor;
    case ReferenceSpaceType::BoundedFloor:
        return SessionFeature::ReferenceSpaceTypeBoundedFloor;
    case ReferenceSpaceType::Unbounded:
        return SessionFeature::ReferenceSpaceTypeUnbounded;
    }

    ASSERT_NOT_REACHED();
    return SessionFeature::ReferenceSpaceTypeViewer;
}

inline std::optional<SessionFeature> parseSessionFeatureDescriptor(StringView string)
{
    auto feature = string.stripWhiteSpace().convertToASCIILowercase();

    if (feature == "viewer"_s)
        return SessionFeature::ReferenceSpaceTypeViewer;
    if (feature == "local"_s)
        return SessionFeature::ReferenceSpaceTypeLocal;
    if (feature == "local-floor"_s)
        return SessionFeature::ReferenceSpaceTypeLocalFloor;
    if (feature == "bounded-floor"_s)
        return SessionFeature::ReferenceSpaceTypeBoundedFloor;
    if (feature == "unbounded"_s)
        return SessionFeature::ReferenceSpaceTypeUnbounded;
#if ENABLE(WEBXR_HANDS)
    if (feature == "hand-tracking"_s)
        return SessionFeature::HandTracking;
#endif

    return std::nullopt;
}

inline String sessionFeatureDescriptor(SessionFeature sessionFeature)
{
    switch (sessionFeature) {
    case SessionFeature::ReferenceSpaceTypeViewer:
        return "viewer"_s;
    case SessionFeature::ReferenceSpaceTypeLocal:
        return "local"_s;
    case SessionFeature::ReferenceSpaceTypeLocalFloor:
        return "local-floor"_s;
    case SessionFeature::ReferenceSpaceTypeBoundedFloor:
        return "bounded-floor"_s;
    case SessionFeature::ReferenceSpaceTypeUnbounded:
        return "unbounded"_s;
#if ENABLE(WEBXR_HANDS)
    case SessionFeature::HandTracking:
        return "hand-tracking"_s;
#endif
    default:
        ASSERT_NOT_REACHED();
        return ""_s;
    }
}

#if ENABLE(WEBXR_HANDS)

enum class HandJoint : unsigned {
    Wrist = 0,
    ThumbMetacarpal,
    ThumbPhalanxProximal,
    ThumbPhalanxDistal,
    ThumbTip,
    IndexFingerMetacarpal,
    IndexFingerPhalanxProximal,
    IndexFingerPhalanxIntermediate,
    IndexFingerPhalanxDistal,
    IndexFingerTip,
    MiddleFingerMetacarpal,
    MiddleFingerPhalanxProximal,
    MiddleFingerPhalanxIntermediate,
    MiddleFingerPhalanxDistal,
    MiddleFingerTip,
    RingFingerMetacarpal,
    RingFingerPhalanxProximal,
    RingFingerPhalanxIntermediate,
    RingFingerPhalanxDistal,
    RingFingerTip,
    PinkyFingerMetacarpal,
    PinkyFingerPhalanxProximal,
    PinkyFingerPhalanxIntermediate,
    PinkyFingerPhalanxDistal,
    PinkyFingerTip,
    Count
};

#endif

class TrackingAndRenderingClient;

class Device : public ThreadSafeRefCounted<Device>, public CanMakeWeakPtr<Device> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Device);
public:
    virtual ~Device() = default;

    using FeatureList = Vector<SessionFeature>;
    bool supports(SessionMode mode) const { return m_supportedFeaturesMap.contains(mode); }
    void setSupportedFeatures(SessionMode mode, const FeatureList& features) { m_supportedFeaturesMap.set(mode, features); }
    FeatureList supportedFeatures(SessionMode mode) const { return m_supportedFeaturesMap.get(mode); }
    void setEnabledFeatures(SessionMode mode, const FeatureList& features) { m_enabledFeaturesMap.set(mode, features); }
    FeatureList enabledFeatures(SessionMode mode) const { return m_enabledFeaturesMap.get(mode); }

    virtual WebCore::IntSize recommendedResolution(SessionMode) { return { 1, 1 }; }

    bool supportsOrientationTracking() const { return m_supportsOrientationTracking; }
    bool supportsViewportScaling() const { return m_supportsViewportScaling; }

    // Returns the value that the device's recommended resolution must be multiplied by
    // to yield the device's native framebuffer resolution.
    virtual double nativeFramebufferScalingFactor() const { return 1.0; }
    // Returns the value that the device's recommended resolution must be multiplied by
    // to yield the device's max framebuffer resolution. This resolution can be larger than
    // the native resolution if the device supports supersampling.
    virtual double maxFramebufferScalingFactor() const { return nativeFramebufferScalingFactor(); }


    virtual void initializeTrackingAndRendering(const WebCore::SecurityOriginData&, SessionMode, const FeatureList&) = 0;
    virtual void shutDownTrackingAndRendering() = 0;
    TrackingAndRenderingClient* trackingAndRenderingClient() const { return m_trackingAndRenderingClient.get(); }
    void setTrackingAndRenderingClient(WeakPtr<TrackingAndRenderingClient>&& client) { m_trackingAndRenderingClient = WTFMove(client); }

    // If this method returns true, that means the device will notify TrackingAndRenderingClient
    // when the platform has completed all steps to shut down the XR session.
    virtual bool supportsSessionShutdownNotification() const { return false; }
    virtual void initializeReferenceSpace(ReferenceSpaceType) = 0;
    virtual std::optional<LayerHandle> createLayerProjection(uint32_t width, uint32_t height, bool alpha) = 0;
    virtual void deleteLayer(LayerHandle) = 0;

    struct FrameData {
        struct FloatQuaternion {
            float x { 0.0f };
            float y { 0.0f };
            float z { 0.0f };
            float w { 1.0f };

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<FloatQuaternion> decode(Decoder&);
        };

        struct Pose {
            WebCore::FloatPoint3D position;
            FloatQuaternion orientation;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<Pose> decode(Decoder&);
        };

        struct Fov {
            // In radians
            float up { 0.0f };
            float down { 0.0f };
            float left { 0.0f };
            float right { 0.0f };

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<Fov> decode(Decoder&);
        };

        static constexpr size_t projectionMatrixSize = 16;
        typedef std::array<float, projectionMatrixSize> ProjectionMatrix;

        using Projection = std::variant<Fov, ProjectionMatrix, std::nullptr_t>;

        struct View {
            Pose offset;
            Projection projection = { nullptr };

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<View> decode(Decoder&);
        };

        struct StageParameters {
            int id { 0 };
            Vector<WebCore::FloatPoint> bounds;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<StageParameters> decode(Decoder&);
        };

        struct LayerData {
#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
            std::unique_ptr<WebCore::IOSurface> surface;
            bool isShared { false };
#else
            PlatformGLObject opaqueTexture { 0 };
#endif
#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
            MachSendRight completionPort { };
            uint64_t renderingFrameIndex { 0 };
#endif

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<LayerData> decode(Decoder&);
        };

        struct InputSourceButton {
            bool touched { false };
            bool pressed { false };
            float pressedValue { 0 };

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<InputSourceButton> decode(Decoder&);
        };

        struct InputSourcePose {
            Pose pose;
            bool isPositionEmulated { false };

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<InputSourcePose> decode(Decoder&);
        };

#if ENABLE(WEBXR_HANDS)
        struct InputSourceHandJoint {
            InputSourcePose pose;
            float radius { 0 };

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<InputSourceHandJoint> decode(Decoder&);
        };

        using HandJointsVector = Vector<std::optional<InputSourceHandJoint>>;
#endif

        struct InputSource {
            InputSourceHandle handle { 0 };
            XRHandedness handeness { XRHandedness::None };
            XRTargetRayMode targetRayMode { XRTargetRayMode::Gaze };
            Vector<String> profiles;
            InputSourcePose pointerOrigin;
            std::optional<InputSourcePose> gripOrigin;
            Vector<InputSourceButton> buttons;
            Vector<float> axes;
#if ENABLE(WEBXR_HANDS)
            std::optional<HandJointsVector> handJoints;
#endif

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<InputSource> decode(Decoder&);
        };

        bool isTrackingValid { false };
        bool isPositionValid { false };
        bool isPositionEmulated { false };
        bool shouldRender { false };
        long predictedDisplayTime { 0 };
        Pose origin;
        std::optional<Pose> floorTransform;
        StageParameters stageParameters;
        Vector<View> views;
        HashMap<LayerHandle, LayerData> layers;
        Vector<InputSource> inputSources;

        FrameData copy() const;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<FrameData> decode(Decoder&);
    };

    struct LayerView {
        Eye eye { Eye::None };
        WebCore::IntRect viewport;
    };

    struct Layer {
        LayerHandle handle { 0 };
        bool visible { true };
        Vector<LayerView> views;
    };

    struct ViewData {
        bool active { false };
        Eye eye { Eye::None };
    };

    virtual Vector<ViewData> views(SessionMode) const = 0;

    using RequestFrameCallback = Function<void(FrameData&&)>;
    virtual void requestFrame(RequestFrameCallback&&) = 0;
    virtual void submitFrame(Vector<Layer>&&) { };
protected:
    Device() = default;

    // https://immersive-web.github.io/webxr/#xr-device-concept
    // Each XR device has a list of enabled features for each XRSessionMode in its list of supported modes,
    // which is a list of feature descriptors which MUST be initially an empty list.
    using FeaturesPerModeMap = HashMap<SessionMode, FeatureList, IntHash<SessionMode>, WTF::StrongEnumHashTraits<SessionMode>>;
    FeaturesPerModeMap m_enabledFeaturesMap;
    FeaturesPerModeMap m_supportedFeaturesMap;

    bool m_supportsOrientationTracking { false };
    bool m_supportsViewportScaling { false };
    WeakPtr<TrackingAndRenderingClient> m_trackingAndRenderingClient;
};

class TrackingAndRenderingClient : public CanMakeWeakPtr<TrackingAndRenderingClient> {
public:
    virtual ~TrackingAndRenderingClient() = default;

    // This event is used to ensure that initial inputsourceschange events occur after the initial session is resolved.
    // WebxR apps can wait for the input source events before calling requestAnimationFrame.
    // Per-frame input source updates are handled via session.requestAnimationFrame which calls Device::requestFrame.
    virtual void sessionDidInitializeInputSources(Vector<Device::FrameData::InputSource>&&) = 0;
    virtual void sessionDidEnd() = 0;
    virtual void updateSessionVisibilityState(VisibilityState) = 0;
    // FIXME: handle frame update
};

class Instance {
public:
    WEBCORE_EXPORT static Instance& singleton();

    using DeviceList = Vector<Ref<Device>>;
    WEBCORE_EXPORT void enumerateImmersiveXRDevices(CompletionHandler<void(const DeviceList&)>&&);

private:
    friend LazyNeverDestroyed<Instance>;
    Instance();
    ~Instance() = default;

    struct Impl;
    UniqueRef<Impl> m_impl;

    DeviceList m_immersiveXRDevices;
};

template<class Encoder>
void Device::FrameData::FloatQuaternion::encode(Encoder& encoder) const
{
    encoder << x << y << z << w;
}

template<class Decoder>
std::optional<Device::FrameData::FloatQuaternion> Device::FrameData::FloatQuaternion::decode(Decoder& decoder)
{
    Device::FrameData::FloatQuaternion floatQuaternion;
    if (!decoder.decode(floatQuaternion.x))
        return std::nullopt;
    if (!decoder.decode(floatQuaternion.y))
        return std::nullopt;
    if (!decoder.decode(floatQuaternion.z))
        return std::nullopt;
    if (!decoder.decode(floatQuaternion.w))
        return std::nullopt;
    return floatQuaternion;
}

template<class Encoder>
void Device::FrameData::Pose::encode(Encoder& encoder) const
{
    encoder << position << orientation;
}

template<class Decoder>
std::optional<Device::FrameData::Pose> Device::FrameData::Pose::decode(Decoder& decoder)
{
    Device::FrameData::Pose pose;
    if (!decoder.decode(pose.position))
        return std::nullopt;
    if (!decoder.decode(pose.orientation))
        return std::nullopt;
    return pose;
}

template<class Encoder>
void Device::FrameData::Fov::encode(Encoder& encoder) const
{
    encoder << up << down << left << right;
}

template<class Decoder>
std::optional<Device::FrameData::Fov> Device::FrameData::Fov::decode(Decoder& decoder)
{
    Device::FrameData::Fov fov;
    if (!decoder.decode(fov.up))
        return std::nullopt;
    if (!decoder.decode(fov.down))
        return std::nullopt;
    if (!decoder.decode(fov.left))
        return std::nullopt;
    if (!decoder.decode(fov.right))
        return std::nullopt;
    return fov;
}

template<class Encoder>
void Device::FrameData::View::encode(Encoder& encoder) const
{
    encoder << offset;

    bool hasFov = std::holds_alternative<PlatformXR::Device::FrameData::Fov>(projection);
    encoder << hasFov;
    if (hasFov) {
        encoder << std::get<PlatformXR::Device::FrameData::Fov>(projection);
        return;
    }

    bool hasProjectionMatrix = std::holds_alternative<PlatformXR::Device::FrameData::ProjectionMatrix>(projection);
    encoder << hasProjectionMatrix;
    if (hasProjectionMatrix) {
        for (float f : std::get<PlatformXR::Device::FrameData::ProjectionMatrix>(projection))
            encoder << f;
        return;
    }

    ASSERT(std::holds_alternative<std::nullptr_t>(projection));
}

template<class Decoder>
std::optional<Device::FrameData::View> Device::FrameData::View::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData::View view;
    if (!decoder.decode(view.offset))
        return std::nullopt;

    bool hasFov;
    if (!decoder.decode(hasFov))
        return std::nullopt;

    if (hasFov) {
        PlatformXR::Device::FrameData::Fov fov;
        if (!decoder.decode(fov))
            return std::nullopt;
        view.projection = { WTFMove(fov) };
        return view;
    }

    bool hasProjectionMatrix;
    if (!decoder.decode(hasProjectionMatrix))
        return std::nullopt;

    if (hasProjectionMatrix) {
        PlatformXR::Device::FrameData::ProjectionMatrix projectionMatrix;
        for (size_t i = 0; i < PlatformXR::Device::FrameData::projectionMatrixSize; ++i) {
            float f;
            if (!decoder.decode(f))
                return std::nullopt;
            projectionMatrix[i] = f;
        }
        view.projection = { WTFMove(projectionMatrix) };
        return view;
    }

    view.projection = { nullptr };
    return view;
}

template<class Encoder>
void Device::FrameData::StageParameters::encode(Encoder& encoder) const
{
    encoder << id;
    encoder << bounds;
}

template<class Decoder>
std::optional<Device::FrameData::StageParameters> Device::FrameData::StageParameters::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData::StageParameters stageParameters;
    if (!decoder.decode(stageParameters.id))
        return std::nullopt;
    if (!decoder.decode(stageParameters.bounds))
        return std::nullopt;
    return stageParameters;
}

template<class Encoder>
void Device::FrameData::LayerData::encode(Encoder& encoder) const
{
#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
    MachSendRight surfaceSendRight = surface ? surface->createSendRight() : MachSendRight();
    encoder << surfaceSendRight;
    encoder << isShared;
#else
    encoder << opaqueTexture;
#endif
#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    encoder << completionPort;
    encoder << renderingFrameIndex;
#endif
}

template<class Decoder>
std::optional<Device::FrameData::LayerData> Device::FrameData::LayerData::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData::LayerData layerData;
#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
    MachSendRight surfaceSendRight;
    if (!decoder.decode(surfaceSendRight))
        return std::nullopt;
    layerData.surface = WebCore::IOSurface::createFromSendRight(WTFMove(surfaceSendRight));
    if (!decoder.decode(layerData.isShared))
        return std::nullopt;
#else
    if (!decoder.decode(layerData.opaqueTexture))
        return std::nullopt;
#endif
#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    if (!decoder.decode(layerData.completionPort))
        return std::nullopt;
    if (!decoder.decode(layerData.renderingFrameIndex))
        return std::nullopt;
#endif
    return layerData;
}

template<class Encoder>
void Device::FrameData::InputSourceButton::encode(Encoder& encoder) const
{
    encoder << touched;
    encoder << pressed;
    encoder << pressedValue;
}

template<class Decoder>
std::optional<Device::FrameData::InputSourceButton> Device::FrameData::InputSourceButton::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData::InputSourceButton button;
    if (!decoder.decode(button.touched))
        return std::nullopt;
    if (!decoder.decode(button.pressed))
        return std::nullopt;
    if (!decoder.decode(button.pressedValue))
        return std::nullopt;
    return button;
}

template<class Encoder>
void Device::FrameData::InputSourcePose::encode(Encoder& encoder) const
{
    encoder << pose;
    encoder << isPositionEmulated;
}

template<class Decoder>
std::optional<Device::FrameData::InputSourcePose> Device::FrameData::InputSourcePose::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData::InputSourcePose inputSourcePose;
    if (!decoder.decode(inputSourcePose.pose))
        return std::nullopt;
    if (!decoder.decode(inputSourcePose.isPositionEmulated))
        return std::nullopt;
    return inputSourcePose;
}

#if ENABLE(WEBXR_HANDS)
template<class Encoder>
void Device::FrameData::InputSourceHandJoint::encode(Encoder& encoder) const
{
    encoder << pose;
    encoder << radius;
}

template<class Decoder>
std::optional<Device::FrameData::InputSourceHandJoint> Device::FrameData::InputSourceHandJoint::decode(Decoder& decoder)
{
    std::optional<InputSourcePose> pose;
    decoder >> pose;
    if (!pose)
        return std::nullopt;
    std::optional<float> radius;
    decoder >> radius;
    if (!radius)
        return std::nullopt;
    return { { WTFMove(*pose), *radius } };
}
#endif

template<class Encoder>
void Device::FrameData::InputSource::encode(Encoder& encoder) const
{
    encoder << handle;
    encoder << handeness;
    encoder << targetRayMode;
    encoder << profiles;
    encoder << pointerOrigin;
    encoder << gripOrigin;
    encoder << buttons;
    encoder << axes;
#if ENABLE(WEBXR_HANDS)
    encoder << handJoints;
#endif
}

template<class Decoder>
std::optional<Device::FrameData::InputSource> Device::FrameData::InputSource::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData::InputSource source;
    if (!decoder.decode(source.handle))
        return std::nullopt;
    if (!decoder.decode(source.handeness))
        return std::nullopt;
    if (!decoder.decode(source.targetRayMode))
        return std::nullopt;
    if (!decoder.decode(source.profiles))
        return std::nullopt;
    if (!decoder.decode(source.pointerOrigin))
        return std::nullopt;
    if (!decoder.decode(source.gripOrigin))
        return std::nullopt;
    if (!decoder.decode(source.buttons))
        return std::nullopt;
    if (!decoder.decode(source.axes))
        return std::nullopt;
#if ENABLE(WEBXR_HANDS)
    if (!decoder.decode(source.handJoints))
        return std::nullopt;
#endif
    return source;
}


template<class Encoder>
void Device::FrameData::encode(Encoder& encoder) const
{
    encoder << isTrackingValid;
    encoder << isPositionValid;
    encoder << isPositionEmulated;
    encoder << shouldRender;
    encoder << predictedDisplayTime;
    encoder << origin;
    encoder << floorTransform;
    encoder << stageParameters;
    encoder << views;
    encoder << layers;
    encoder << inputSources;
}

template<class Decoder>
std::optional<Device::FrameData> Device::FrameData::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData frameData;
    if (!decoder.decode(frameData.isTrackingValid))
        return std::nullopt;
    if (!decoder.decode(frameData.isPositionValid))
        return std::nullopt;
    if (!decoder.decode(frameData.isPositionEmulated))
        return std::nullopt;
    if (!decoder.decode(frameData.shouldRender))
        return std::nullopt;
    if (!decoder.decode(frameData.predictedDisplayTime))
        return std::nullopt;
    if (!decoder.decode(frameData.origin))
        return std::nullopt;
    if (!decoder.decode(frameData.floorTransform))
        return std::nullopt;
    if (!decoder.decode(frameData.stageParameters))
        return std::nullopt;
    if (!decoder.decode(frameData.views))
        return std::nullopt;
    if (!decoder.decode(frameData.layers))
        return std::nullopt;
    if (!decoder.decode(frameData.inputSources))
        return std::nullopt;


    return frameData;
}

inline Device::FrameData Device::FrameData::copy() const
{
    PlatformXR::Device::FrameData frameData;
    frameData.isTrackingValid = isTrackingValid;
    frameData.isPositionValid = isPositionValid;
    frameData.isPositionEmulated = isPositionEmulated;
    frameData.shouldRender = shouldRender;
    frameData.predictedDisplayTime = predictedDisplayTime;
    frameData.origin = origin;
    frameData.floorTransform = floorTransform;
    frameData.stageParameters = stageParameters;
    frameData.views = views;
    frameData.inputSources = inputSources;
    return frameData;
}

#endif // ENABLE(WEBXR)

} // namespace PlatformXR

#if ENABLE(WEBXR)

namespace WTF {

template<> struct EnumTraits<PlatformXR::SessionMode> {
    using values = EnumValues<
        PlatformXR::SessionMode,
        PlatformXR::SessionMode::Inline,
        PlatformXR::SessionMode::ImmersiveVr,
        PlatformXR::SessionMode::ImmersiveAr
    >;
};

template<> struct EnumTraits<PlatformXR::ReferenceSpaceType> {
    using values = EnumValues<
        PlatformXR::ReferenceSpaceType,
        PlatformXR::ReferenceSpaceType::Viewer,
        PlatformXR::ReferenceSpaceType::Local,
        PlatformXR::ReferenceSpaceType::LocalFloor,
        PlatformXR::ReferenceSpaceType::BoundedFloor,
        PlatformXR::ReferenceSpaceType::Unbounded
    >;
};

template<> struct EnumTraits<PlatformXR::VisibilityState> {
    using values = EnumValues<
        PlatformXR::VisibilityState,
        PlatformXR::VisibilityState::Visible,
        PlatformXR::VisibilityState::VisibleBlurred,
        PlatformXR::VisibilityState::Hidden
    >;
};

template<> struct EnumTraits<PlatformXR::XRHandedness> {
    using values = EnumValues<
        PlatformXR::XRHandedness,
        PlatformXR::XRHandedness::None,
        PlatformXR::XRHandedness::Left,
        PlatformXR::XRHandedness::Right
    >;
};

template<> struct EnumTraits<PlatformXR::XRTargetRayMode> {
    using values = EnumValues<
        PlatformXR::XRTargetRayMode,
        PlatformXR::XRTargetRayMode::Gaze,
        PlatformXR::XRTargetRayMode::TrackedPointer,
        PlatformXR::XRTargetRayMode::Screen
    >;
};

template<> struct EnumTraits<PlatformXR::SessionFeature> {
    using values = EnumValues<
        PlatformXR::SessionFeature,
        PlatformXR::SessionFeature::ReferenceSpaceTypeViewer,
        PlatformXR::SessionFeature::ReferenceSpaceTypeLocal,
        PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor,
        PlatformXR::SessionFeature::ReferenceSpaceTypeBoundedFloor,
        PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded
#if ENABLE(WEBXR_HANDS)
        , PlatformXR::SessionFeature::HandTracking
#endif
    >;
};

}

#endif
