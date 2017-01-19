# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'targets': [
    {
      'target_name': 'video_quality_test',
      'type': 'static_library',
      'sources': [
        'video/video_quality_test.cc',
        'video/video_quality_test.h',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        'webrtc',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies!': [
            '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
          ],
        }],
      ],
    },
    {
      'target_name': 'screenshare_loopback',
      'type': 'executable',
      'sources': [
        'test/mac/run_test.mm',
        'test/run_test.cc',
        'test/run_test.h',
        'video/screenshare_loopback.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'test/run_test.cc',
          ],
        }],
      ],
      'dependencies': [
        'video_quality_test',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'test/test.gyp:test_common',
        'test/test.gyp:test_main',
        'test/test.gyp:test_renderer',
        'webrtc',
      ],
    },
    {
      'target_name': 'video_replay',
      'type': 'executable',
      'sources': [
        'test/mac/run_test.mm',
        'test/run_test.cc',
        'test/run_test.h',
        'video/replay.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'test/run_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'test/test.gyp:test_common',
        'test/test.gyp:test_renderer',
        '<(webrtc_root)/modules/modules.gyp:video_capture',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
        'webrtc',
      ],
    },
  ],
}
