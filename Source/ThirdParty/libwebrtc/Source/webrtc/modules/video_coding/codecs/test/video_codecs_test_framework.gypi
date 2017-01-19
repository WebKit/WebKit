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
          'target_name': 'video_codecs_test_framework',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/test/test.gyp:test_support',
          ],
          'sources': [
            'mock/mock_packet_manipulator.h',
            'packet_manipulator.h',
            'packet_manipulator.cc',
            'predictive_packet_manipulator.h',
            'predictive_packet_manipulator.cc',
            'stats.h',
            'stats.cc',
            'videoprocessor.h',
            'videoprocessor.cc',
          ],
        },
      ], # targets
    }], # include_tests
  ], # conditions
}
