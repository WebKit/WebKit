# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'webrtc_vp8',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/modules/video_coding/utility/video_coding_utility.gyp:video_coding_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'conditions': [
        ['build_libvpx==1', {
          'dependencies': [
            '<(libvpx_dir)/libvpx.gyp:libvpx',
          ],
        }],
      ],
      'sources': [
        'default_temporal_layers.cc',
        'default_temporal_layers.h',
        'include/vp8.h',
        'include/vp8_common_types.h',
        'realtime_temporal_layers.cc',
        'reference_picture_selection.cc',
        'reference_picture_selection.h',
        'screenshare_layers.cc',
        'screenshare_layers.h',
        'simulcast_encoder_adapter.cc',
        'simulcast_encoder_adapter.h',
        'temporal_layers.h',
        'vp8_impl.cc',
        'vp8_impl.h',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'vp8_coder',
          'type': 'executable',
          'dependencies': [
            'webrtc_vp8',
            '<(webrtc_root)/common_video/common_video.gyp:common_video',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
          ],
          'sources': [
            'vp8_sequence_coder.cc',
          ],
        },
      ], # targets
    }], # include_tests
  ],
}
