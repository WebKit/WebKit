# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'congestion_controller',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:bitrate_controller',
        '<(webrtc_root)/modules/modules.gyp:paced_sender',
      ],
      'sources': [
        'congestion_controller.cc',
        'delay_based_bwe.cc',
        'delay_based_bwe.h',
        'include/congestion_controller.h',
        'probe_bitrate_estimator.cc',
        'probe_bitrate_estimator.h',
        'probe_controller.cc',
        'probe_controller.h',
        'transport_feedback_adapter.cc',
        'transport_feedback_adapter.h',
      ],
      # TODO(jschuh): Bug 1348: fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ], # targets
}
