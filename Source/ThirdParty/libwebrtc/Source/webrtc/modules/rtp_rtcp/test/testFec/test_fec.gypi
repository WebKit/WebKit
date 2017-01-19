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
      # The test below takes long to run, no need to add it to any bot.
      'target_name': 'test_packet_masks_metrics',
      'type': 'executable',
      'dependencies': [
        'rtp_rtcp',
        '<(webrtc_root)/test/test.gyp:test_support_main',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test_packet_masks_metrics.cc',
        'average_residual_loss_xor_codes.h',
      ],
    },
  ],
}
