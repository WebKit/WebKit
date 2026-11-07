/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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

#pragma once

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

#if HAVE(ARKIT)

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#import <ARKit/ARKit.h>
ALLOW_DEPRECATED_DECLARATIONS_END
#import <pal/spi/cocoa/ARKitSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, ARKit)

SOFT_LINK_CLASS_FOR_HEADER(PAL, ARQuickLookPreviewItem);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
SOFT_LINK_CLASS_FOR_HEADER(PAL, ARSession);
ALLOW_DEPRECATED_DECLARATIONS_END
SOFT_LINK_CLASS_FOR_HEADER(PAL, ARWorldTrackingConfiguration)

#if PLATFORM(VISION)

#import <simd/simd.h>

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ARMatrix4x4DoubleToFloat, simd_float4x4, (simd_double4x4 matrix), (matrix))
#define ARMatrix4x4DoubleToFloat PAL::softLink_ARKit_ARMatrix4x4DoubleToFloat

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_anchor_get_origin_from_anchor_transform, simd_float4x4, (ar_anchor_t anchor), (anchor))
#define ar_anchor_get_origin_from_anchor_transform PAL::softLink_ARKit_ar_anchor_get_origin_from_anchor_transform
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_trackable_anchor_is_tracked, bool, (ar_trackable_anchor_t anchor), (anchor))
#define ar_trackable_anchor_is_tracked PAL::softLink_ARKit_ar_trackable_anchor_is_tracked

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_data_provider_get_state, ar_data_provider_state_t, (ar_data_provider_t data_provider), (data_provider))
#define ar_data_provider_get_state PAL::softLink_ARKit_ar_data_provider_get_state

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_data_providers_create, ar_data_providers_t, (void), ())
#define ar_data_providers_create PAL::softLink_ARKit_ar_data_providers_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_data_providers_add_data_provider, void, (ar_data_providers_t data_providers, ar_data_provider_t data_provider_to_add), (data_providers, data_provider_to_add))
#define ar_data_providers_add_data_provider PAL::softLink_ARKit_ar_data_providers_add_data_provider

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_error_get_error_code, ar_error_code_t, (ar_error_t error), (error))
#define ar_error_get_error_code PAL::softLink_ARKit_ar_error_get_error_code

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_plane_detection_configuration_create, ar_plane_detection_configuration_t, (void), ())
#define ar_plane_detection_configuration_create PAL::softLink_ARKit_ar_plane_detection_configuration_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_plane_detection_configuration_set_alignment, void, (ar_plane_detection_configuration_t plane_detection_configuration, ar_plane_alignment_t alignment), (plane_detection_configuration, alignment))
#define ar_plane_detection_configuration_set_alignment PAL::softLink_ARKit_ar_plane_detection_configuration_set_alignment
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_plane_detection_provider_create, ar_plane_detection_provider_t, (ar_plane_detection_configuration_t plane_detection_configuration), (plane_detection_configuration))
#define ar_plane_detection_provider_create PAL::softLink_ARKit_ar_plane_detection_provider_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_plane_detection_provider_get_floor_plane_anchor_for_device_pose, void, (ar_plane_detection_provider_t plane_detection_provider, ar_plane_detection_floor_plane_completion_handler_t floor_plane_completion_handler), (plane_detection_provider, floor_plane_completion_handler))
#define ar_plane_detection_provider_get_floor_plane_anchor_for_device_pose PAL::softLink_ARKit_ar_plane_detection_provider_get_floor_plane_anchor_for_device_pose
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_plane_detection_provider_is_supported, bool, (void), ())
#define ar_plane_detection_provider_is_supported PAL::softLink_ARKit_ar_plane_detection_provider_is_supported

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_device_anchor_create, ar_device_anchor_t, (void), ())
#define ar_device_anchor_create PAL::softLink_ARKit_ar_device_anchor_create

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_session_create, ar_session_t, (void), ())
#define ar_session_create PAL::softLink_ARKit_ar_session_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_session_run, void, (ar_session_t session, ar_data_providers_t data_providers), (session, data_providers))
#define ar_session_run PAL::softLink_ARKit_ar_session_run
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_session_set_data_provider_state_change_handler, void, (ar_session_t session, dispatch_queue_t queue, ar_session_data_provider_state_change_handler_t data_provider_state_change_handler), (session, queue, data_provider_state_change_handler))
#define ar_session_set_data_provider_state_change_handler PAL::softLink_ARKit_ar_session_set_data_provider_state_change_handler
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_session_stop, void, (ar_session_t session), (session))
#define ar_session_stop PAL::softLink_ARKit_ar_session_stop

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_world_tracking_configuration_create, ar_world_tracking_configuration_t, (void), ())
#define ar_world_tracking_configuration_create PAL::softLink_ARKit_ar_world_tracking_configuration_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_world_tracking_provider_is_supported, bool, (void), ())
#define ar_world_tracking_provider_is_supported PAL::softLink_ARKit_ar_world_tracking_provider_is_supported
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_world_tracking_provider_create, ar_world_tracking_provider_t, (ar_world_tracking_configuration_t world_tracking_configuration), (world_tracking_configuration))
#define ar_world_tracking_provider_create PAL::softLink_ARKit_ar_world_tracking_provider_create

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_world_tracking_provider_query_device_anchor_at_timestamp, ar_device_anchor_query_status_t, (ar_world_tracking_provider_t world_tracking_provider, CFTimeInterval timestamp, ar_device_anchor_t device_anchor), (world_tracking_provider, timestamp, device_anchor))
#define ar_world_tracking_provider_query_device_anchor_at_timestamp PAL::softLink_ARKit_ar_world_tracking_provider_query_device_anchor_at_timestamp

#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_anchor_create, ar_hand_anchor_t, (void), ())
#define ar_hand_anchor_create PAL::softLink_ARKit_ar_hand_anchor_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_anchor_get_chirality, ar_hand_chirality_t, (ar_hand_anchor_t hand_anchor), (hand_anchor))
#define ar_hand_anchor_get_chirality PAL::softLink_ARKit_ar_hand_anchor_get_chirality
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_anchor_get_hand_skeleton, ar_hand_skeleton_t, (ar_hand_anchor_t hand_anchor), (hand_anchor))
#define ar_hand_anchor_get_hand_skeleton PAL::softLink_ARKit_ar_hand_anchor_get_hand_skeleton

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_skeleton_get_joint_named, ar_skeleton_joint_t, (ar_hand_skeleton_t hand_skeleton, ar_hand_skeleton_joint_name_t joint_name), (hand_skeleton, joint_name))
#define ar_hand_skeleton_get_joint_named PAL::softLink_ARKit_ar_hand_skeleton_get_joint_named

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_tracking_configuration_create, ar_hand_tracking_configuration_t, (void), ())
#define ar_hand_tracking_configuration_create PAL::softLink_ARKit_ar_hand_tracking_configuration_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_tracking_provider_create, ar_hand_tracking_provider_t, (ar_hand_tracking_configuration_t hand_tracking_configuration), (hand_tracking_configuration))
#define ar_hand_tracking_provider_create PAL::softLink_ARKit_ar_hand_tracking_provider_create
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_tracking_provider_get_latest_anchors, bool, (ar_hand_tracking_provider_t hand_tracking_provider, ar_hand_anchor_t hand_anchor_left, ar_hand_anchor_t hand_anchor_right), (hand_tracking_provider, hand_anchor_left, hand_anchor_right))
#define ar_hand_tracking_provider_get_latest_anchors PAL::softLink_ARKit_ar_hand_tracking_provider_get_latest_anchors
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_hand_tracking_provider_is_supported, bool, (void), ())
#define ar_hand_tracking_provider_is_supported PAL::softLink_ARKit_ar_hand_tracking_provider_is_supported

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_skeleton_joint_get_anchor_from_joint_transform, simd_float4x4, (ar_skeleton_joint_t joint), (joint))
#define ar_skeleton_joint_get_anchor_from_joint_transform PAL::softLink_ARKit_ar_skeleton_joint_get_anchor_from_joint_transform
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_skeleton_joint_is_tracked, bool, (ar_skeleton_joint_t joint), (joint))
#define ar_skeleton_joint_is_tracked PAL::softLink_ARKit_ar_skeleton_joint_is_tracked

#endif // ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#if HAVE(SPATIAL_CONTROLLERS)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_accessory_anchors_enumerate_anchors, void, (ar_accessory_anchors_t accessory_anchors, ar_accessory_anchors_enumerator_t accessory_anchors_enumerator), (accessory_anchors, accessory_anchors_enumerator))
#define ar_accessory_anchors_enumerate_anchors PAL::softLink_ARKit_ar_accessory_anchors_enumerate_anchors

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_accessory_anchor_get_inherent_chirality, ar_accessory_chirality_t, (ar_accessory_anchor_t accessory_anchor), (accessory_anchor))
#define ar_accessory_anchor_get_inherent_chirality PAL::softLink_ARKit_ar_accessory_anchor_get_inherent_chirality

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_accessory_anchor_is_tracked, bool, (ar_accessory_anchor_t accessory_anchor), (accessory_anchor))
#define ar_accessory_anchor_is_tracked PAL::softLink_ARKit_ar_accessory_anchor_is_tracked

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_accessory_tracking_configuration_create, ar_accessory_tracking_configuration_t, (void), ())
#define ar_accessory_tracking_configuration_create PAL::softLink_ARKit_ar_accessory_tracking_configuration_create

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_accessory_tracking_provider_create, ar_accessory_tracking_provider_t, (ar_accessory_tracking_configuration_t accessory_tracking_configuration), (accessory_tracking_configuration))
#define ar_accessory_tracking_provider_create PAL::softLink_ARKit_ar_accessory_tracking_provider_create

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_accessory_tracking_provider_get_latest_anchors, ar_accessory_anchors_t, (ar_accessory_tracking_provider_t accessory_tracking_provider), (accessory_tracking_provider))
#define ar_accessory_tracking_provider_get_latest_anchors PAL::softLink_ARKit_ar_accessory_tracking_provider_get_latest_anchors

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, ARKit, ar_accessory_tracking_provider_is_supported, bool, (void), ())
#define ar_accessory_tracking_provider_is_supported PAL::softLink_ARKit_ar_accessory_tracking_provider_is_supported

#endif // HAVE(SPATIAL_CONTROLLERS)
#endif // PLATFORM(VISION)
#endif // HAVE(ARKIT)

#endif // !__has_feature(modules)
