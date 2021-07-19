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
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include "IOSurface.h"
#include <wtf/MachSendRight.h>
#endif

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

class TrackingAndRenderingClient;

class Device : public ThreadSafeRefCounted<Device>, public CanMakeWeakPtr<Device> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Device);
public:
    virtual ~Device() = default;

    using FeatureList = Vector<ReferenceSpaceType>;
    bool supports(SessionMode mode) const { return m_enabledFeaturesMap.contains(mode); }
    void setSupportedFeatures(SessionMode mode, const FeatureList& features) { m_enabledFeaturesMap.set(mode, features); }
    FeatureList supportedFeatures(SessionMode mode) const { return m_enabledFeaturesMap.get(mode); }
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


    virtual void initializeTrackingAndRendering(SessionMode) = 0;
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

        using Projection = Variant<Fov, ProjectionMatrix, std::nullptr_t>;

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

        struct InputSource {
            InputSourceHandle handle { 0 };
            XRHandedness handeness { XRHandedness::None };
            XRTargetRayMode targetRayMode { XRTargetRayMode::Gaze };
            Vector<String> profiles;
            InputSourcePose pointerOrigin;
            std::optional<InputSourcePose> gripOrigin;
            Vector<InputSourceButton> buttons;
            Vector<float> axes;

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
    using FeaturesPerModeMap = WTF::HashMap<SessionMode, FeatureList, WTF::IntHash<SessionMode>, WTF::StrongEnumHashTraits<SessionMode>>;
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
    // FIXME: handle frame update
    // FIXME: handle visibility changes
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

    bool hasFov = WTF::holds_alternative<PlatformXR::Device::FrameData::Fov>(projection);
    encoder << hasFov;
    if (hasFov) {
        encoder << WTF::get<PlatformXR::Device::FrameData::Fov>(projection);
        return;
    }

    bool hasProjectionMatrix = WTF::holds_alternative<PlatformXR::Device::FrameData::ProjectionMatrix>(projection);
    encoder << hasProjectionMatrix;
    if (hasProjectionMatrix) {
        for (float f : WTF::get<PlatformXR::Device::FrameData::ProjectionMatrix>(projection))
            encoder << f;
        return;
    }

    ASSERT(WTF::holds_alternative<std::nullptr_t>(projection));
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
    WTF::MachSendRight surfaceSendRight = surface ? surface->createSendRight() : WTF::MachSendRight();
    encoder << surfaceSendRight;
    encoder << isShared;
#else
    encoder << opaqueTexture;
#endif
}

template<class Decoder>
std::optional<Device::FrameData::LayerData> Device::FrameData::LayerData::decode(Decoder& decoder)
{
    PlatformXR::Device::FrameData::LayerData layerData;
#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
    WTF::MachSendRight surfaceSendRight;
    if (!decoder.decode(surfaceSendRight))
        return std::nullopt;
    layerData.surface = WebCore::IOSurface::createFromSendRight(WTFMove(surfaceSendRight), WebCore::DestinationColorSpace::SRGB());
    if (!decoder.decode(layerData.isShared))
        return std::nullopt;
#else
    if (!decoder.decode(layerData.opaqueTexture))
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

}

#endif
