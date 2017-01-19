# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'video_quality_measurement',
          'type': 'executable',
          'dependencies': [
            'video_codecs_test_framework',
            'webrtc_video_coding',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            '<(webrtc_root)/common.gyp:webrtc_common',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_vp8_dir)/vp8.gyp:webrtc_vp8',
          ],
          'sources': [
            'video_quality_measurement.cc',
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
        },
      ], # targets
    }], # include_tests
  ], # conditions
}
