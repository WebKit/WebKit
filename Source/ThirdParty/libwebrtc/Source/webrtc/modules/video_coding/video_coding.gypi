# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'webrtc_video_coding',
      'type': 'static_library',
      'dependencies': [
        'webrtc_h264',
        'webrtc_i420',
        '../base/base.gyp:rtc_task_queue',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/modules/video_coding/utility/video_coding_utility.gyp:video_coding_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_vp8_dir)/vp8.gyp:webrtc_vp8',
        '<(webrtc_vp9_dir)/vp9.gyp:webrtc_vp9',
      ],
      'sources': [
        # interfaces
        'include/video_coding.h',
        'include/video_coding_defines.h',

        # headers
        'codec_database.h',
        'codec_timer.h',
        'decoding_state.h',
        'encoded_frame.h',
        'fec_tables_xor.h',
        'frame_buffer.h',
        'frame_buffer2.h',
        'frame_object.h',
        'rtp_frame_reference_finder.h',
        'generic_decoder.h',
        'generic_encoder.h',
        'histogram.h',
        'inter_frame_delay.h',
        'internal_defines.h',
        'jitter_buffer.h',
        'jitter_buffer_common.h',
        'jitter_estimator.h',
        'media_opt_util.h',
        'media_optimization.h',
        'nack_fec_tables.h',
        'nack_module.h',
        'packet.h',
        'packet_buffer.h',
        'percentile_filter.h',
        'protection_bitrate_calculator.h',
        'receiver.h',
        'rtt_filter.h',
        'session_info.h',
        'timestamp_map.h',
        'timing.h',
        'video_coding_impl.h',

        # sources
        'codec_database.cc',
        'codec_timer.cc',
        'decoding_state.cc',
        'encoded_frame.cc',
        'frame_buffer.cc',
        'frame_buffer2.cc',
        'frame_object.cc',
        'rtp_frame_reference_finder.cc',
        'generic_decoder.cc',
        'generic_encoder.cc',
        'inter_frame_delay.cc',
        'histogram.cc',
        'jitter_buffer.cc',
        'jitter_estimator.cc',
        'media_opt_util.cc',
        'media_optimization.cc',
        'protection_bitrate_calculator.cc',
        'nack_module.cc',
        'packet.cc',
        'packet_buffer.cc',
        'percentile_filter.cc',
        'receiver.cc',
        'rtt_filter.cc',
        'session_info.cc',
        'timestamp_map.cc',
        'timing.cc',
        'video_coding_impl.cc',
        'video_sender.cc',
        'video_receiver.cc',
      ], # source
      # TODO(jschuh): Bug 1348: fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
