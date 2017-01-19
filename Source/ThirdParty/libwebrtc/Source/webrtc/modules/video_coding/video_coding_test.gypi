# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'rtp_player',
      'type': 'executable',
      'dependencies': [
         'rtp_rtcp',
         'webrtc_video_coding',
         '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
         '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
         '<(webrtc_root)/test/test.gyp:test_common',
      ],
      'sources': [
        # headers
        'test/receiver_tests.h',
        'test/rtp_player.h',
        'test/vcm_payload_sink_factory.h',

        # sources
        'test/rtp_player.cc',
        'test/test_util.cc',
        'test/tester_main.cc',
        'test/vcm_payload_sink_factory.cc',
        'test/video_rtp_play.cc',
      ], # sources
    },
  ],
}
