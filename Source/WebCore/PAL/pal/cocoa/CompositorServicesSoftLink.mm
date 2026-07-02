/*
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
 */

#import "config.h"

#if ENABLE(WEBXR)

#import <pal/spi/cocoa/CompositorServicesSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, PAL_EXPORT)

SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, CP_OBJECT_cp_layer_renderer, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, CP_OBJECT_cp_swapchain, PAL_EXPORT)


SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_compute_projection, simd_float4x4, (cp_drawable_t drawable, cp_axis_direction_convention convention, size_t index), (drawable, convention, index), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_get_frame_timing, cp_frame_timing_t, (cp_drawable_t drawable), (drawable), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_get_color_texture, id<MTLTexture>, (cp_drawable_t drawable, size_t index), (drawable, index), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_get_depth_texture, id<MTLTexture>, (cp_drawable_t drawable, size_t index), (drawable, index), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_get_view, cp_view_t, (cp_drawable_t drawable, size_t index), (drawable, index), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_get_view_count, size_t, (cp_drawable_t drawable), (drawable), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_present, void, (cp_drawable_t drawable), (drawable), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_set_depth_range, void, (cp_drawable_t drawable, simd_float2 depth_range), (drawable, depth_range), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_set_device_anchor, void, (cp_drawable_t drawable, ar_device_anchor_t device_anchor), (drawable, device_anchor), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_get_rasterization_rate_map_descriptor, MTLRasterizationRateMapDescriptor*, (cp_drawable_t drawable, size_t index), (drawable, index), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_get_rasterization_rate_map, id<MTLRasterizationRateMap>, (cp_drawable_t drawable, size_t index), (drawable, index), PAL_EXPORT)

// FIXME: <rdar://134998122> Remove may fail when CC update lands.
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_drawable_set_write_forward_depth, void, (cp_drawable_t drawable, bool is_forward_z), (drawable, is_forward_z), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_end_submission, void, (cp_frame_t frame), (frame), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_get_frame_index, cp_layer_frame_index_t, (cp_frame_t frame), (frame), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_query_drawable, cp_drawable_t, (cp_frame_t frame), (frame), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_predict_timing, cp_frame_timing_t, (cp_frame_t frame), (frame), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_skip_frame, void, (cp_frame_t frame), (frame), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_start_submission, void, (cp_frame_t frame), (frame), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_set_minimum_frame_repeat_count, void, (cp_layer_renderer_t layer_renderer, int frame_repeat_count), (layer_renderer, frame_repeat_count), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_get_completion_event, id<MTLEvent>, (cp_layer_renderer_t layer), (layer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_get_device, id<MTLDevice>, (cp_layer_renderer_t layer), (layer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_get_state, cp_layer_renderer_state, (cp_layer_renderer_t layer), (layer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_get_swapchain, cp_swapchain_t, (cp_layer_renderer_t layer), (layer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_query_next_frame, cp_frame_t, (cp_layer_renderer_t layer), (layer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_wait_until_running, void, (cp_layer_renderer_t layer), (layer), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_capabilities_copy_system_default, cp_layer_renderer_capabilities_t, (CFErrorRef *error), (error), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_capabilities_supported_minimum_near_plane_distance, float, (cp_layer_renderer_capabilities_t layer_capabilities), (layer_capabilities), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_copy_system_default, cp_layer_renderer_configuration_t, (CFErrorRef *error), (error), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_get_default_depth_range, simd_float2, (cp_layer_renderer_configuration_t configuration), (configuration), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_get_contents_inverted_vertically, bool, (cp_layer_renderer_configuration_t configuration), (configuration), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_set_contents_inverted_vertically, void, (cp_layer_renderer_configuration_t configuration, bool inverted_vertically), (configuration, inverted_vertically), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_set_foveation_enabled, void, (cp_layer_renderer_configuration_t configuration, bool foveation_enabled), (configuration, foveation_enabled), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_get_foveation_enabled, bool, (cp_layer_renderer_configuration_t configuration), (configuration), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_set_layout, void, (cp_layer_renderer_configuration_t configuration, cp_layer_renderer_layout layout), (configuration, layout), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_get_layout, cp_layer_renderer_layout, (cp_layer_renderer_configuration_t configuration), (configuration), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_set_drawable_render_context_raster_sample_count, void, (cp_layer_renderer_configuration_t configuration, int raster_sample_count), (configuration, raster_sample_count), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_set_color_format, void, (cp_layer_renderer_configuration_t configuration, MTLPixelFormat color_format), (configuration, color_format), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_set_depth_format, void, (cp_layer_renderer_configuration_t configuration, MTLPixelFormat depth_format), (configuration, depth_format), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_configuration_set_use_shared_events, void, (cp_layer_renderer_configuration_t configuration, bool use_shared_events), (configuration, use_shared_events), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_properties_create_using_configuration, cp_layer_renderer_properties_t, (cp_layer_renderer_configuration_t configuration, CFErrorRef *error), (configuration, error), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_layer_renderer_properties_get_texture_topology_count, size_t, (cp_layer_renderer_properties_t layer_properties), (layer_properties), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_time_to_cf_time_interval, CFTimeInterval, (cp_time_t time), (time), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_time_wait_until, void, (cp_time_t time), (time), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_timing_get_presentation_time, cp_time_t, (cp_frame_timing_t frame_timing), (frame_timing), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_frame_timing_get_optimal_input_time, cp_time_t, (cp_frame_timing_t frame_timing), (frame_timing), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_view_get_transform, simd_float4x4, (cp_view_t view), (view), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_view_get_view_texture_map, cp_view_texture_map_t, (cp_view_t view), (view), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_view_texture_map_get_texture_index, size_t, (cp_view_texture_map_t view_texture_map), (view_texture_map), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_view_texture_map_get_viewport, MTLViewport, (cp_view_texture_map_t view_texture_map), (view_texture_map), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CompositorServices, cp_view_texture_map_get_slice_index, size_t, (cp_view_texture_map_t view_texture_map), (view_texture_map), PAL_EXPORT)

#endif // ENABLE(WEBXR)
