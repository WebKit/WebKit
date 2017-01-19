# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'remote_bitrate_estimator',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'include/bwe_defines.h',
        'include/remote_bitrate_estimator.h',
        'include/send_time_history.h',
        'aimd_rate_control.cc',
        'aimd_rate_control.h',
        'bwe_defines.cc',
        'inter_arrival.cc',
        'inter_arrival.h',
        'overuse_detector.cc',
        'overuse_detector.h',
        'overuse_estimator.cc',
        'overuse_estimator.h',
        'remote_bitrate_estimator_abs_send_time.cc',
        'remote_bitrate_estimator_abs_send_time.h',
        'remote_bitrate_estimator_single_stream.cc',
        'remote_bitrate_estimator_single_stream.h',
        'remote_estimator_proxy.cc',
        'remote_estimator_proxy.h',
        'send_time_history.cc',
        'test/bwe_test_logging.h',
      ], # source
      'conditions': [
        ['enable_bwe_test_logging==1', {
          'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=1' ],
          'sources': [
            'test/bwe_test_logging.cc'
          ],
        }, {
          'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=0' ],
        }],
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'bwe_tools_util',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            'rtp_rtcp',
          ],
          'sources': [
            'tools/bwe_rtp.cc',
            'tools/bwe_rtp.h',
          ],
        },
        {
          'target_name': 'bwe_rtp_to_text',
          'type': 'executable',
          'includes': [
            '../rtp_rtcp/rtp_rtcp.gypi',
          ],
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(webrtc_root)/test/test.gyp:rtp_test_utils',
            'bwe_tools_util',
            'rtp_rtcp',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'include',
            ],
          },
          'sources': [
            'tools/rtp_to_text.cc',
          ], # source
        },
        {
          'target_name': 'bwe_rtp_play',
          'type': 'executable',
          'includes': [
            '../rtp_rtcp/rtp_rtcp.gypi',
          ],
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(webrtc_root)/test/test.gyp:rtp_test_utils',
            'bwe_tools_util',
            'rtp_rtcp',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'include',
            ],
          },
          'sources': [
            'tools/bwe_rtp_play.cc',
          ], # source
        },
      ],
    }],  # include_tests==1
  ],
}
