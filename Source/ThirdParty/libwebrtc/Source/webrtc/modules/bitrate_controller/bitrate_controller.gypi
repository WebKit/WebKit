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
      'target_name': 'bitrate_controller',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'bitrate_controller_impl.cc',
        'bitrate_controller_impl.h',
        'include/bitrate_controller.h',
        'send_side_bandwidth_estimation.cc',
        'send_side_bandwidth_estimation.h',
      ],
      'conditions': [
        ['enable_bwe_test_logging==1', {
          'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=1' ],
        }, {
          'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=0' ],
        }],
      ],
      # TODO(jschuh): Bug 1348: fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ], # targets
}
