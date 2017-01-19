# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'video_quality_analysis',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'export_dependent_settings': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'sources': [
        'frame_analyzer/video_quality_analysis.h',
        'frame_analyzer/video_quality_analysis.cc',
      ],
    }, # video_quality_analysis
    {
      'target_name': 'frame_analyzer',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'video_quality_analysis',
      ],
      'sources': [
        'frame_analyzer/frame_analyzer.cc',
      ],
    }, # frame_analyzer
    {
      'target_name': 'psnr_ssim_analyzer',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'video_quality_analysis',
      ],
      'sources': [
        'psnr_ssim_analyzer/psnr_ssim_analyzer.cc',
      ],
    }, # psnr_ssim_analyzer
    {
      'target_name': 'rgba_to_i420_converter',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
      ],
      'sources': [
        'converter/converter.h',
        'converter/converter.cc',
        'converter/rgba_to_i420_converter.cc',
      ],
    }, # rgba_to_i420_converter
    {
      'target_name': 'frame_editing_lib',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'sources': [
        'frame_editing/frame_editing_lib.cc',
        'frame_editing/frame_editing_lib.h',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    }, # frame_editing_lib
    {
      'target_name': 'frame_editor',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'frame_editing_lib',
      ],
      'sources': [
        'frame_editing/frame_editing.cc',
      ],
    }, # frame_editing
    {
      'target_name': 'force_mic_volume_max',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:audio_device',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
      ],
      'sources': [
        'force_mic_volume_max/force_mic_volume_max.cc',
      ],
    }, # force_mic_volume_max
  ],
  'conditions': [
    ['enable_protobuf==1', {
      'targets': [
        {
          'target_name': 'chart_proto',
          'type': 'static_library',
          'sources': [
            'event_log_visualizer/chart.proto',
          ],
          'variables': {
            'proto_in_dir': 'event_log_visualizer',
            'proto_out_dir': 'webrtc/tools/event_log_visualizer',
          },
          'includes': ['../build/protoc.gypi'],
        },
        {
          # RTC event log visualization library
          'target_name': 'event_log_visualizer_utils',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/webrtc.gyp:rtc_event_log_impl',
            '<(webrtc_root)/webrtc.gyp:rtc_event_log_parser',
            '<(webrtc_root)/modules/modules.gyp:congestion_controller',
            '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
            ':chart_proto',
          ],
          'sources': [
            'event_log_visualizer/analyzer.cc',
            'event_log_visualizer/analyzer.h',
            'event_log_visualizer/plot_base.cc',
            'event_log_visualizer/plot_base.h',
            'event_log_visualizer/plot_protobuf.cc',
            'event_log_visualizer/plot_protobuf.h',
            'event_log_visualizer/plot_python.cc',
            'event_log_visualizer/plot_python.h',
          ],
          'export_dependent_settings': [
            '<(webrtc_root)/webrtc.gyp:rtc_event_log_parser',
            ':chart_proto',
          ],
        },
      ],
    }],
  ], # conditions
}
