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
      'target_name': 'paced_sender',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/modules/modules.gyp:bitrate_controller',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
      ],
      'sources': [
        'alr_detector.cc',
        'alr_detector.h',
        'bitrate_prober.cc',
        'bitrate_prober.h',
        'paced_sender.cc',
        'paced_sender.h',
        'packet_router.cc',
        'packet_router.h',
      ],
    },
  ], # targets
}
