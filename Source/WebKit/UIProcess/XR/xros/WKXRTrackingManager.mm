/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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

#import "config.h"

#if ENABLE(WEBXR) && USE(COMPOSITORXR)
#import "WKXRTrackingManager.h"

#import "Logging.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/OSObjectPtr.h>

#import <pal/cocoa/ARKitSoftLink.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

typedef NS_OPTIONS(NSUInteger, WKXRTrackingProviderType) {
    WKXRTrackingProviderTypeWorldTracking = 1 << 0,
    WKXRTrackingProviderTypeHandTracking = 1 << 1,
#if HAVE(SPATIAL_CONTROLLERS)
    WKXRTrackingProviderTypeAccessoryTracking = 1 << 2,
#endif
};

@interface _WKTransientAction: NSObject
@property (nonatomic, readonly) simd_float4x4 targetRay;
@property (nonatomic, readonly) simd_float4x4 inversedStartingPose;
@property (nonatomic) simd_float4x4 pose;

- (instancetype)initWithTargetRay:(simd_float4x4)targetRay pose:(simd_float4x4)pose;
@end

@implementation _WKTransientAction
- (instancetype)initWithTargetRay:(simd_float4x4)targetRay pose:(simd_float4x4)pose
{
    self = [super init];
    if (!self)
        return nil;

    _targetRay = targetRay;
    _inversedStartingPose = simd_inverse(pose);
    _pose = pose;
    return self;
}
@end

@interface WKXRTrackingManager ()
@property WKXRTrackingProviderType runningProviderTypes;
@end

@implementation WKXRTrackingManager {
    dispatch_queue_t _accessQueue;
    __weak cp_layer_renderer_t _layerRenderer;
    RetainPtr<NSMutableDictionary<NSNumber *, _WKTransientAction *>> _transientActions;

    OSObjectPtr<ar_session_t> _arSession;
    OSObjectPtr<ar_world_tracking_provider_t> _worldTrackingProvider;

    BOOL _handTrackingEnabled;
    OSObjectPtr<ar_hand_tracking_provider_t> _handTrackingProvider;
#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    OSObjectPtr<ar_hand_anchor_t> _leftHandAnchor;
    OSObjectPtr<ar_hand_anchor_t> _rightHandAnchor;
#endif
#if HAVE(SPATIAL_CONTROLLERS)
    OSObjectPtr<ar_accessory_tracking_provider_t> _accessoryTrackingProvider;
    RetainPtr<WKXRControllerManager> _controllerManager;
#endif
}

- (instancetype)initWithHandTrackingEnabled:(BOOL)handTrackingEnabled layerRenderer:(cp_layer_renderer_t)layerRenderer
#if HAVE(SPATIAL_CONTROLLERS)
    controllerManager:(RetainPtr<WKXRControllerManager> &)controllerManager
#endif
{
    if (!PAL::isARKitFrameworkAvailable())
        return nil;

    if (!(self = [super init]))
        return nil;

    _accessQueue = dispatch_queue_create("com.apple.WebContent.WKXRTrackingManager.AccessQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    _handTrackingEnabled = handTrackingEnabled;
#else
    _handTrackingEnabled = NO;
#endif
    _layerRenderer = layerRenderer;
    _worldTrackingSupported = ar_world_tracking_provider_is_supported();

#if HAVE(SPATIAL_CONTROLLERS)
    _controllerManager = controllerManager;
#endif

    dispatch_async(_accessQueue, ^{
        [self _startTracking];
    });

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKXRTrackingManager.class, self))
        return;

    dispatch_sync(_accessQueue, ^{
        if (_arSession)
            [self _endTracking];

        ASSERT(!_arSession);
    });
    dispatch_release(_accessQueue);

    [super dealloc];
}

- (BOOL)isValid
{
    return (self.runningProviderTypes & WKXRTrackingProviderTypeWorldTracking) || !_worldTrackingSupported;
}

- (std::optional<PlatformXRPose>)latestFloorPose
{
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    return std::nullopt;
#else
    return PlatformXRPose(matrix_identity_float4x4);
#endif
}

- (OSObjectPtr<ar_device_anchor_t>)deviceAnchorAtTime:(NSTimeInterval)time
{
    __block OSObjectPtr<ar_device_anchor_t> deviceAnchor;

    dispatch_sync(_accessQueue, ^{
        if (!_worldTrackingProvider || !(self.runningProviderTypes & WKXRTrackingProviderTypeWorldTracking))
            return;

        deviceAnchor = adoptOSObject(ar_device_anchor_create());
        ar_device_anchor_query_status_t status = ar_world_tracking_provider_query_device_anchor_at_timestamp(_worldTrackingProvider.get(), time, deviceAnchor.get());

        if (status != ar_device_anchor_query_status_success) {
            RELEASE_LOG_ERROR(XR, "Cannot get device anchor from world tracking provider");
            return;
        }
    });

    return deviceAnchor;
}

- (void)_startTracking
{
    dispatch_assert_queue(_accessQueue);
    ASSERT(!_arSession);

    _arSession = adoptOSObject(ar_session_create());
    if (!_arSession) {
        RELEASE_LOG_ERROR(XR, "Cannot create AR session");
        return;
    }

    OSObjectPtr<ar_data_providers_t> dataProviders = adoptOSObject(ar_data_providers_create());

    if (_worldTrackingSupported) {
        OSObjectPtr<ar_world_tracking_configuration_t> worldTrackingConfiguration = adoptOSObject(ar_world_tracking_configuration_create());
        if (worldTrackingConfiguration)
            _worldTrackingProvider = adoptOSObject(ar_world_tracking_provider_create(worldTrackingConfiguration.get()));
    } else
        RELEASE_LOG_ERROR(XR, "World tracking is not supported");

    if (_worldTrackingProvider) {
        ar_data_providers_add_data_provider(dataProviders.get(), _worldTrackingProvider.get());
        RELEASE_LOG_INFO(XR, "World tracking is configured");
    } else
        RELEASE_LOG_ERROR(XR, "Cannot create world tracking provider");

#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    if (_handTrackingEnabled && ar_hand_tracking_provider_is_supported()) {
        OSObjectPtr<ar_hand_tracking_configuration_t> handTrackingConfiguration = adoptOSObject(ar_hand_tracking_configuration_create());
        if (handTrackingConfiguration)
            _handTrackingProvider = adoptOSObject(ar_hand_tracking_provider_create(handTrackingConfiguration.get()));

        if (_handTrackingProvider) {
            ar_data_providers_add_data_provider(dataProviders.get(), _handTrackingProvider.get());
            RELEASE_LOG_INFO(XR, "Hand tracking is configured");
        } else
            RELEASE_LOG_ERROR(XR, "Cannot create hand tracking provider");
    }
#endif

#if HAVE(SPATIAL_CONTROLLERS)
    if (ar_accessory_tracking_provider_is_supported()) {
        OSObjectPtr<ar_accessory_tracking_configuration_t> accessoryTrackingConfiguration = adoptOSObject(ar_accessory_tracking_configuration_create());
        if (accessoryTrackingConfiguration)
            _accessoryTrackingProvider = adoptOSObject(ar_accessory_tracking_provider_create(accessoryTrackingConfiguration.get()));
        else
            RELEASE_LOG_ERROR(XR, "Unable to get accessoryTrackingConfiguration");
        if (_accessoryTrackingProvider) {
            ar_data_providers_add_data_provider(dataProviders.get(), _accessoryTrackingProvider.get());
            RELEASE_LOG_INFO(XR, "Accessory tracking is configured");
        } else
            RELEASE_LOG_ERROR(XR, "Cannot create accessory tracking provider");
    } else
        RELEASE_LOG_ERROR(XR, "Accessory tracking provider is not supported");
#endif

    __weak WKXRTrackingManager *weakSelf = self;
    ar_session_set_data_provider_state_change_handler(_arSession.get(), nil, ^(ar_data_providers_t, ar_data_provider_state_t state, ar_error_t error, ar_data_provider_t failedProvider) {
        if (state == ar_data_provider_state_running) {
            ASSERT(!error);
            RELEASE_LOG(XR, "AR session started successfully");
        } else if (error) {
            // ARKit will release the error so we don't have to release that ref ourselves.
            RELEASE_LOG_ERROR(XR, "AR session failed to start with error code %d", static_cast<int>(ar_error_get_error_code(error)));
        } else if (!!failedProvider) {
            // Probably unlikely that there's a failed provider without an error.
            RELEASE_LOG_ERROR(XR, "AR session failed to start with unknown reason");
        }
        [weakSelf _stateDidChangeForDataProviders];
    });

    ar_session_run(_arSession.get(), dataProviders.get());
}

- (void)_stateDidChangeForDataProviders
{
    dispatch_async(_accessQueue, ^{
        if (!_arSession)
            return;

        WKXRTrackingProviderType newRunningProviderTypes = 0;
        if (_worldTrackingProvider && ar_data_provider_get_state(_worldTrackingProvider.get()) == ar_data_provider_state_running)
            newRunningProviderTypes |= WKXRTrackingProviderTypeWorldTracking;
        if (_handTrackingProvider && ar_data_provider_get_state(_handTrackingProvider.get()) == ar_data_provider_state_running)
            newRunningProviderTypes |= WKXRTrackingProviderTypeHandTracking;
#if HAVE(SPATIAL_CONTROLLERS)
        if (_accessoryTrackingProvider && ar_data_provider_get_state(_accessoryTrackingProvider.get()) == ar_data_provider_state_running)
            newRunningProviderTypes |= WKXRTrackingProviderTypeAccessoryTracking;
#endif
        WKXRTrackingProviderType oldRunningProviderTypes = self.runningProviderTypes;
        if (newRunningProviderTypes == oldRunningProviderTypes)
            return;

        RELEASE_LOG(XR, "Running data provider types changed from %d to %d", (int)oldRunningProviderTypes, (int)newRunningProviderTypes);
        self.runningProviderTypes = newRunningProviderTypes;
    });
}

- (void)_endTracking
{
    dispatch_assert_queue(_accessQueue);
    ASSERT(!!_arSession);

    ar_session_stop(_arSession.get());

    _worldTrackingProvider = nil;
    _handTrackingProvider = nil;
#if HAVE(SPATIAL_CONTROLLERS)
    _accessoryTrackingProvider = nil;
#endif
    _arSession = nil;

    RELEASE_LOG(XR, "AR tracking ended");
}

// MARK: - WKSpatialGestureRecognizerDelegate

- (cp_layer_renderer_t)cpLayerForGestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer
{
    return _layerRenderer;
}

- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidStart:(NSNumber *)actionIdentifier targetRayTransform:(simd_float4x4)targetRayTransform poseTransform:(simd_float4x4)poseTransform
{
    dispatch_async(_accessQueue, ^{
        if (!_transientActions)
            _transientActions = adoptNS([[NSMutableDictionary alloc] init]);
        RetainPtr<_WKTransientAction> transientAction = adoptNS([[_WKTransientAction alloc] initWithTargetRay:targetRayTransform pose:poseTransform]);
        [_transientActions setObject:transientAction.get() forKey:actionIdentifier];
        RELEASE_LOG_DEBUG(XR, "Transient action started: %@", actionIdentifier);
    });
}

- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidUpdate:(NSNumber *)actionIdentifier poseTransform:(simd_float4x4)poseTransform
{
    dispatch_async(_accessQueue, ^{
        _WKTransientAction *transientAction = [_transientActions objectForKey:actionIdentifier];
        ASSERT(transientAction);
        if (!transientAction)
            RELEASE_LOG_ERROR(XR, "ERROR: Transient action updated without begin: %@", actionIdentifier);
        [transientAction setPose:poseTransform];
    });
}

- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidEnd:(NSNumber *)actionIdentifier
{
    [self _doneWithTransientActionWithIdentifier:actionIdentifier];
}

- (void)gestureRecognizer:(WKSpatialGestureRecognizer *)gestureRecognizer transientActionDidCancel:(NSNumber *)actionIdentifier
{
    [self _doneWithTransientActionWithIdentifier:actionIdentifier];
}

- (void)_doneWithTransientActionWithIdentifier:(NSNumber *)actionIdentifier
{
    dispatch_async(_accessQueue, ^{
        _WKTransientAction *transientAction = [_transientActions objectForKey:actionIdentifier];
        ASSERT(transientAction);
        if (!transientAction)
            RELEASE_LOG_ERROR(XR, "ERROR: Transient action ended without begin: %@", actionIdentifier);
        [_transientActions removeObjectForKey:actionIdentifier];
        RELEASE_LOG_DEBUG(XR, "Transient action ended: %@", actionIdentifier);
    });
}

// MARK: - Input sources collection

- (std::optional<PlatformXR::FrameData::InputSource>)_platformXRInputSourceFromTransientAction:(_WKTransientAction *)transientAction actionIdentifier:(int)actionIdentifier
{
    dispatch_assert_queue(_accessQueue);

    simd_float4x4 multiplier = simd_mul(transientAction.pose, transientAction.inversedStartingPose);
    PlatformXRPose targetRay(simd_mul(multiplier, transientAction.targetRay));
    PlatformXRPose pose(transientAction.pose);

    PlatformXR::FrameData::InputSource data;
    data.handedness = PlatformXR::XRHandedness::None;
    data.handle = actionIdentifier;

    if (_handTrackingEnabled)
        data.profiles = Vector<String> { "generic-button"_s };
    else
        data.profiles = Vector<String> { "generic-button-invisible"_s, "generic-button"_s };

    data.targetRayMode = PlatformXR::XRTargetRayMode::TransientPointer;

    data.pointerOrigin.pose = targetRay.pose();
    data.pointerOrigin.isPositionEmulated = false;

    PlatformXR::FrameData::InputSourcePose gripPose;
    gripPose.pose = pose.pose();
    gripPose.isPositionEmulated = false;
    data.gripOrigin = gripPose;

    PlatformXR::FrameData::InputSourceButton button;
    button.pressed = true;
    button.touched = true;
    button.pressedValue = 1.0;
    data.buttons.append(button);

    return data;
}

- (Vector<PlatformXR::FrameData::InputSource>)collectInputSources
{
    __block Vector<PlatformXR::FrameData::InputSource> result;

    dispatch_sync(_accessQueue, ^{
        if (!self.isValid)
            return;

        [_transientActions enumerateKeysAndObjectsUsingBlock:^(NSNumber *actionIdentifier, _WKTransientAction *transientAction, BOOL*) {
            if (auto transientInputData = [self _platformXRInputSourceFromTransientAction:transientAction actionIdentifier:actionIdentifier.intValue])
                result.append(transientInputData.value());
        }];

#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
        if (_handTrackingProvider && (self.runningProviderTypes & WKXRTrackingProviderTypeHandTracking)) {
            if (!_leftHandAnchor) {
                _leftHandAnchor = adoptOSObject(ar_hand_anchor_create());
                _rightHandAnchor = adoptOSObject(ar_hand_anchor_create());
            }

            ar_hand_tracking_provider_get_latest_anchors(_handTrackingProvider.get(), _leftHandAnchor.get(), _rightHandAnchor.get());
            for (ar_hand_anchor_t handAnchor in @[_leftHandAnchor.get(), _rightHandAnchor.get()]) {
                if (auto data = [self _platformXRInputSourceFromHandAnchor:handAnchor])
                    result.append(data.value());
            }
        }
#endif // ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#if HAVE(SPATIAL_CONTROLLERS)
        if (_controllerManager && _accessoryTrackingProvider && (self.runningProviderTypes & WKXRTrackingProviderTypeAccessoryTracking)) {
            ar_accessory_anchors_t accessory_anchors = ar_accessory_tracking_provider_get_latest_anchors(_accessoryTrackingProvider.get());
            ar_accessory_anchors_enumerate_anchors(accessory_anchors, ^bool(ar_accessory_anchor_t _Nonnull accessoryAnchor) {
                if (auto data = [self _platformXRInputSourceFromAccessoryAnchor:accessoryAnchor])
                    result.append(data.value());
                return true;
            });
        }
#endif // HAVE(SPATIAL_CONTROLLERS)
    });

    return result;
}

// MARK: - Hand tracking

#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)
static const size_t xrJointCount = static_cast<size_t>(PlatformXR::HandJoint::Count);

- (std::optional<PlatformXR::FrameData::InputSource>)_platformXRInputSourceFromHandAnchor:(ar_hand_anchor_t)handAnchor
{
    if (!ar_trackable_anchor_is_tracked(handAnchor))
        return std::nullopt;

    PlatformXR::FrameData::InputSource data;
    BOOL isLeftHand = ar_hand_anchor_get_chirality(handAnchor) == ar_hand_chirality_left;
    data.handedness = isLeftHand ? PlatformXR::XRHandedness::Left : PlatformXR::XRHandedness::Right;
    data.handle = (int)data.handedness;

    data.profiles = Vector<String> { "generic-hand"_s };

    simd_quatf rotation = simd_quaternion(M_PI / 2, simd_make_float3(0.0, !isLeftHand ? 1.0 : -1.0, 0.0));
    simd_float4x4 rotationTransform = simd_matrix4x4(rotation);
    const simd_float4x4 originFromWrist = ar_anchor_get_origin_from_anchor_transform(handAnchor);
    PlatformXRPose targetingPose(simd_mul(originFromWrist, rotationTransform));
    data.targetRayMode = PlatformXR::XRTargetRayMode::TrackedPointer;
    data.pointerOrigin.pose = targetingPose.pose();
    data.pointerOrigin.isPositionEmulated = false;

    PlatformXR::FrameData::HandJointsVector handJoints;
    static const ar_hand_skeleton_joint_name_t jointNames[] = {
        ar_hand_skeleton_joint_name_wrist,
        ar_hand_skeleton_joint_name_thumb_knuckle,
        ar_hand_skeleton_joint_name_thumb_intermediate_base,
        ar_hand_skeleton_joint_name_thumb_intermediate_tip,
        ar_hand_skeleton_joint_name_thumb_tip,
        ar_hand_skeleton_joint_name_index_finger_metacarpal,
        ar_hand_skeleton_joint_name_index_finger_knuckle,
        ar_hand_skeleton_joint_name_index_finger_intermediate_base,
        ar_hand_skeleton_joint_name_index_finger_intermediate_tip,
        ar_hand_skeleton_joint_name_index_finger_tip,
        ar_hand_skeleton_joint_name_middle_finger_metacarpal,
        ar_hand_skeleton_joint_name_middle_finger_knuckle,
        ar_hand_skeleton_joint_name_middle_finger_intermediate_base,
        ar_hand_skeleton_joint_name_middle_finger_intermediate_tip,
        ar_hand_skeleton_joint_name_middle_finger_tip,
        ar_hand_skeleton_joint_name_ring_finger_metacarpal,
        ar_hand_skeleton_joint_name_ring_finger_knuckle,
        ar_hand_skeleton_joint_name_ring_finger_intermediate_base,
        ar_hand_skeleton_joint_name_ring_finger_intermediate_tip,
        ar_hand_skeleton_joint_name_ring_finger_tip,
        ar_hand_skeleton_joint_name_little_finger_metacarpal,
        ar_hand_skeleton_joint_name_little_finger_knuckle,
        ar_hand_skeleton_joint_name_little_finger_intermediate_base,
        ar_hand_skeleton_joint_name_little_finger_intermediate_tip,
        ar_hand_skeleton_joint_name_little_finger_tip
    };
    ASSERT(ARRAY_SIZE(jointNames) == xrJointCount);

    // Until ARKit has a way to expose WebXR compliant joint orientation (rdar://107336123),
    // we'll correct the orientation here.
    simd_float4x4 jointPoseTransform;
    if (isLeftHand) {
        simd_quatf yawQuat = simd_quaternion(M_PI / 2, simd_make_float3(0, -1, 0));
        simd_quatf pitchQuat = simd_quaternion(M_PI, simd_make_float3(1, 0, 0));
        jointPoseTransform = simd_matrix4x4(simd_mul(pitchQuat, yawQuat));
    } else
        jointPoseTransform = simd_matrix4x4(simd_quaternion(M_PI / 2, simd_make_float3(0, 1, 0)));

    ar_hand_skeleton_t handSkeleton = ar_hand_anchor_get_hand_skeleton(handAnchor);
    for (size_t i = 0; i < xrJointCount; ++i) {
        ar_hand_skeleton_joint_name_t jointName = jointNames[i];
        ar_skeleton_joint_t skeletonJoint = ar_hand_skeleton_get_joint_named(handSkeleton, jointName);

        PlatformXR::FrameData::InputSourceHandJoint joint;
        simd_float4x4 skeletonTransform = ar_skeleton_joint_get_anchor_from_joint_transform(skeletonJoint);
        skeletonTransform = simd_mul(skeletonTransform, jointPoseTransform);
        joint.pose.pose = PlatformXRPose(skeletonTransform, originFromWrist).pose();
        joint.pose.isPositionEmulated = !ar_skeleton_joint_is_tracked(skeletonJoint);
        joint.radius = 0.01f;
        handJoints.append(joint);
    }
    data.handJoints = handJoints;

    return data;
}
#endif // ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#if HAVE(SPATIAL_CONTROLLERS)
- (std::optional<PlatformXR::FrameData::InputSource>)_platformXRInputSourceFromAccessoryAnchor:(ar_accessory_anchor_t)accessoryAnchor
{
    if (!ar_accessory_anchor_is_tracked(accessoryAnchor))
        return std::nullopt;

    PlatformXR::FrameData::InputSource data;
    BOOL isLeftHand = ar_accessory_anchor_get_inherent_chirality(accessoryAnchor) == ar_accessory_chirality_left;
    data.handedness = isLeftHand ? PlatformXR::XRHandedness::Left : PlatformXR::XRHandedness::Right;
    // FIXME: rdar://135359974
    data.handle = (int)PlatformXR::XRHandedness::Right + (isLeftHand ? 1 : 2);

    data.profiles = Vector<String> { "generic-trigger-squeeze-thumbstick"_s };

    simd_quatf rotation = simd_quaternion(0.0, simd_make_float3(0.0, !isLeftHand ? 1.0 : -1.0, 0.0));
    simd_float4x4 rotationTransform = simd_matrix4x4(rotation);
    const simd_float4x4 originFromWrist = ar_anchor_get_origin_from_anchor_transform(accessoryAnchor);
    PlatformXRPose targetingPose(simd_mul(originFromWrist, rotationTransform));
    data.targetRayMode = PlatformXR::XRTargetRayMode::TrackedPointer;
    data.pointerOrigin.pose = targetingPose.pose();
    data.pointerOrigin.isPositionEmulated = false;

    PlatformXR::FrameData::InputSourcePose gripPose;
    gripPose.pose = targetingPose.pose();
    gripPose.isPositionEmulated = false;
    data.gripOrigin = gripPose;

    if (_controllerManager) {
        data.buttons = [_controllerManager getButtonsState:isLeftHand];
        data.axes = [_controllerManager getAxesState:isLeftHand];
    }

    return data;
}
#endif // HAVE(SPATIAL_CONTROLLERS)

@end

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBXR) && USE(COMPOSITORXR)
