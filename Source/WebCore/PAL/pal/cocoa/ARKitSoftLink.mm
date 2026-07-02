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

#import "config.h"

#if HAVE(ARKIT)

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#import <ARKit/ARKit.h>
ALLOW_DEPRECATED_DECLARATIONS_END
#import <pal/spi/cocoa/ARKitSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, PAL_EXPORT);

SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ARQuickLookPreviewItem, PAL_EXPORT);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ARSession, PAL_EXPORT);
ALLOW_DEPRECATED_DECLARATIONS_END
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ARWorldTrackingConfiguration, PAL_EXPORT);

#if PLATFORM(VISION)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ARMatrix4x4DoubleToFloat, simd_float4x4, (simd_double4x4 matrix), (matrix), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_anchor_get_origin_from_anchor_transform, simd_float4x4, (ar_anchor_t anchor), (anchor), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_trackable_anchor_is_tracked, bool, (ar_trackable_anchor_t anchor), (anchor), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_data_provider_get_state, ar_data_provider_state_t, (ar_data_provider_t data_provider), (data_provider), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_data_providers_create, ar_data_providers_t, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_data_providers_add_data_provider, void, (ar_data_providers_t data_providers, ar_data_provider_t data_provider_to_add), (data_providers, data_provider_to_add), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_error_get_error_code, ar_error_code_t, (ar_error_t error), (error), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_plane_detection_configuration_create, ar_plane_detection_configuration_t, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_plane_detection_configuration_set_alignment, void, (ar_plane_detection_configuration_t plane_detection_configuration, ar_plane_alignment_t alignment), (plane_detection_configuration, alignment), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_plane_detection_provider_create, ar_plane_detection_provider_t, (ar_plane_detection_configuration_t plane_detection_configuration), (plane_detection_configuration), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_plane_detection_provider_get_floor_plane_anchor_for_device_pose, void, (ar_plane_detection_provider_t plane_detection_provider, ar_plane_detection_floor_plane_completion_handler_t floor_plane_completion_handler), (plane_detection_provider, floor_plane_completion_handler), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_plane_detection_provider_is_supported, bool, (void), (), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_device_anchor_create, ar_device_anchor_t, (void), (), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_session_create, ar_session_t, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_session_run, void, (ar_session_t session, ar_data_providers_t data_providers), (session, data_providers), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_session_set_data_provider_state_change_handler, void, (ar_session_t session, dispatch_queue_t queue, ar_session_data_provider_state_change_handler_t data_provider_state_change_handler), (session, queue, data_provider_state_change_handler), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_session_stop, void, (ar_session_t session), (session), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_world_tracking_configuration_create, ar_world_tracking_configuration_t, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_world_tracking_provider_is_supported, bool, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_world_tracking_provider_create, ar_world_tracking_provider_t, (ar_world_tracking_configuration_t world_tracking_configuration), (world_tracking_configuration), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_world_tracking_provider_query_device_anchor_at_timestamp, ar_device_anchor_query_status_t, (ar_world_tracking_provider_t world_tracking_provider, CFTimeInterval timestamp, ar_device_anchor_t device_anchor), (world_tracking_provider, timestamp, device_anchor), PAL_EXPORT)

#if ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_anchor_create, ar_hand_anchor_t, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_anchor_get_chirality, ar_hand_chirality_t, (ar_hand_anchor_t hand_anchor), (hand_anchor), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_anchor_get_hand_skeleton, ar_hand_skeleton_t, (ar_hand_anchor_t hand_anchor), (hand_anchor), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_skeleton_get_joint_named, ar_skeleton_joint_t, (ar_hand_skeleton_t hand_skeleton, ar_hand_skeleton_joint_name_t joint_name), (hand_skeleton, joint_name), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_tracking_configuration_create, ar_hand_tracking_configuration_t, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_tracking_provider_create, ar_hand_tracking_provider_t, (ar_hand_tracking_configuration_t hand_tracking_configuration), (hand_tracking_configuration), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_tracking_provider_get_latest_anchors, bool, (ar_hand_tracking_provider_t hand_tracking_provider, ar_hand_anchor_t hand_anchor_left, ar_hand_anchor_t hand_anchor_right), (hand_tracking_provider, hand_anchor_left, hand_anchor_right), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_hand_tracking_provider_is_supported, bool, (void), (), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_skeleton_joint_get_anchor_from_joint_transform, simd_float4x4, (ar_skeleton_joint_t joint), (joint), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_skeleton_joint_is_tracked, bool, (ar_skeleton_joint_t joint), (joint), PAL_EXPORT)

#endif // ENABLE(WEBXR_HANDS) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#if HAVE(SPATIAL_CONTROLLERS)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_accessory_anchors_enumerate_anchors, void, (ar_accessory_anchors_t accessory_anchors, ar_accessory_anchors_enumerator_t accessory_anchors_enumerator), (accessory_anchors, accessory_anchors_enumerator), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_accessory_anchor_get_inherent_chirality, ar_accessory_chirality_t, (ar_accessory_anchor_t accessory_anchor), (accessory_anchor), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_accessory_anchor_is_tracked, bool, (ar_accessory_anchor_t accessory_anchor), (accessory_anchor), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_accessory_tracking_configuration_create, ar_accessory_tracking_configuration_t, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_accessory_tracking_provider_create, ar_accessory_tracking_provider_t, (ar_accessory_tracking_configuration_t accessory_tracking_configuration), (accessory_tracking_configuration), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_accessory_tracking_provider_get_latest_anchors, ar_accessory_anchors_t, (ar_accessory_tracking_provider_t accessory_tracking_provider), (accessory_tracking_provider), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, ARKit, ar_accessory_tracking_provider_is_supported, bool, (void), (), PAL_EXPORT)

#endif // HAVE(SPATIAL_CONTROLLERS)
#endif // ENABLE(WEBXR) && HAVE(WEBXR_INTERNALS)
#endif // HAVE(ARKIT)
