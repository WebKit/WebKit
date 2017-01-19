# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'video_coding_utility',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'frame_dropper.cc',
        'frame_dropper.h',
        'ivf_file_writer.cc',
        'ivf_file_writer.h',
        'moving_average.cc',
        'moving_average.h',
        'qp_parser.cc',
        'qp_parser.h',
        'quality_scaler.cc',
        'quality_scaler.h',
        'simulcast_rate_allocator.cc',
        'simulcast_rate_allocator.h',
        'vp8_header_parser.cc',
        'vp8_header_parser.h',
      ],
    },
  ], # targets
}
