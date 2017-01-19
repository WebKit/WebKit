# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# TODO(andrew): consider moving test_support to src/base/test.
{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'video_test_common',
      'type': 'static_library',
      'sources': [
        'fake_texture_frame.cc',
        'fake_texture_frame.h',
        'frame_generator.cc',
        'frame_generator.h',
        'frame_utils.cc',
        'frame_utils.h',
      ],
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
    },
    {
      'target_name': 'rtp_test_utils',
      'type': 'static_library',
      'sources': [
        'rtcp_packet_parser.cc',
        'rtcp_packet_parser.h',
        'rtp_file_reader.cc',
        'rtp_file_reader.h',
        'rtp_file_writer.cc',
        'rtp_file_writer.h',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
      ],
    },
    {
      'target_name': 'field_trial',
      'type': 'static_library',
      'sources': [
        'field_trial.cc',
        'field_trial.h',
      ],
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
    },
    {
      'target_name': 'test_main',
      'type': 'static_library',
      'sources': [
        'test_main.cc',
      ],
      'dependencies': [
        'field_trial',
        'test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
      ],
    },
    {
      'target_name': 'test_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/base/base.gyp:gtest_prod',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        'video_test_common',
      ],
      'sources': [
        'gmock.h',
        'gtest.h',
        'testsupport/fileutils.cc',
        'testsupport/fileutils.h',
        'testsupport/frame_reader.cc',
        'testsupport/frame_reader.h',
        'testsupport/frame_writer.cc',
        'testsupport/frame_writer.h',
        'testsupport/iosfileutils.mm',
        'testsupport/metrics/video_metrics.h',
        'testsupport/metrics/video_metrics.cc',
        'testsupport/mock/mock_frame_reader.h',
        'testsupport/mock/mock_frame_writer.h',
        'testsupport/packet_reader.cc',
        'testsupport/packet_reader.h',
        'testsupport/perf_test.cc',
        'testsupport/perf_test.h',
        'testsupport/trace_to_stderr.cc',
        'testsupport/trace_to_stderr.h',
      ],
      'conditions': [
        ['OS=="ios"', {
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
          },
        }],
        ['use_x11==1', {
          'dependencies': [
            '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],
    },
    {
      # Depend on this target when you want to have test_support but also the
      # main method needed for gtest to execute!
      'target_name': 'test_support_main',
      'type': 'static_library',
      'dependencies': [
        'field_trial',
        'test_support',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
      ],
      'sources': [
        'run_all_unittests.cc',
        'test_suite.cc',
        'test_suite.h',
      ],
    },
    {
      # Depend on this target when you want to have test_support and a special
      # main for mac which will run your test on a worker thread and consume
      # events on the main thread. Useful if you want to access a webcam.
      # This main will provide all the scaffolding and objective-c black magic
      # for you. All you need to do is to implement a function in the
      # run_threaded_main_mac.h file (ImplementThisToRunYourTest).
      'target_name': 'test_support_main_threaded_mac',
      'type': 'static_library',
      'dependencies': [
        'test_support',
      ],
      'sources': [
        'testsupport/mac/run_threaded_main_mac.h',
        'testsupport/mac/run_threaded_main_mac.mm',
      ],
    },
    {
     'target_name': 'test_common',
     'type': 'static_library',
     'sources': [
       'call_test.cc',
       'call_test.h',
       'configurable_frame_size_encoder.cc',
       'configurable_frame_size_encoder.h',
       'constants.cc',
       'constants.h',
       'direct_transport.cc',
       'direct_transport.h',
       'drifting_clock.cc',
       'drifting_clock.h',
       'encoder_settings.cc',
       'encoder_settings.h',
       'fake_audio_device.cc',
       'fake_audio_device.h',
       'fake_decoder.cc',
       'fake_decoder.h',
       'fake_encoder.cc',
       'fake_encoder.h',
       'fake_network_pipe.cc',
       'fake_network_pipe.h',
       'fake_videorenderer.h',
       'frame_generator_capturer.cc',
       'frame_generator_capturer.h',
       'layer_filtering_transport.cc',
       'layer_filtering_transport.h',
       'mock_transport.h',
       'mock_voe_channel_proxy.h',
       'mock_voice_engine.h',
       'null_transport.cc',
       'null_transport.h',
       'rtp_rtcp_observer.h',
       'statistics.cc',
       'statistics.h',
       'vcm_capturer.cc',
       'vcm_capturer.h',
       'video_capturer.h',
       'win/run_loop_win.cc',
     ],
     'conditions': [
       ['OS!="win"', {
         'sources': [
            'run_loop.h',
            'run_loop.cc',
         ],
       }],
     ],
     'dependencies': [
       '<(DEPTH)/testing/gmock.gyp:gmock',
       '<(DEPTH)/testing/gtest.gyp:gtest',
       '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
       '<(webrtc_root)/base/base.gyp:rtc_base_approved',
       '<(webrtc_root)/common.gyp:webrtc_common',
       '<(webrtc_root)/modules/modules.gyp:media_file',
       '<(webrtc_root)/webrtc.gyp:webrtc',
       'rtp_test_utils',
       'test_support',
     ],
    },
    {
     'target_name': 'test_renderer',
     'type': 'static_library',
     'sources': [
       'linux/glx_renderer.cc',
       'linux/glx_renderer.h',
       'linux/video_renderer_linux.cc',
       'mac/video_renderer_mac.h',
       'mac/video_renderer_mac.mm',
       'video_renderer.cc',
       'video_renderer.h',
       'win/d3d_renderer.cc',
       'win/d3d_renderer.h',
     ],
     'conditions': [
       ['OS!="linux" and OS!="mac" and OS!="win"', {
         'sources': [
           'null_platform_renderer.cc',
         ],
       }],
       ['OS=="linux" or OS=="mac"', {
         'sources' : [
           'gl/gl_renderer.cc',
           'gl/gl_renderer.h',
         ],
       }],
       ['OS=="win"', {
         'include_dirs': [
           '<(directx_sdk_path)/Include',
         ],
       }],
       ['OS=="win" and clang==1', {
         'msvs_settings': {
           'VCCLCompilerTool': {
             'AdditionalOptions': [
               # Disable warnings failing when compiling with Clang on Windows.
               # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
               '-Wno-bool-conversion',
               '-Wno-comment',
               '-Wno-delete-non-virtual-dtor',
             ],
           },
         },
       }],
     ],
     'dependencies': [
       '<(DEPTH)/testing/gtest.gyp:gtest',
       '<(webrtc_root)/modules/modules.gyp:media_file',
       'test_support',
       'video_test_common',
     ],
     'direct_dependent_settings': {
       'conditions': [
         ['OS=="linux"', {
           'libraries': [
             '-lXext',
             '-lX11',
             '-lGL',
           ],
         }],
         ['OS=="android"', {
           'libraries' : [
             '-lGLESv2', '-llog',
           ],
         }],
         ['OS=="mac"', {
           'xcode_settings' : {
             'OTHER_LDFLAGS' : [
               '-framework Cocoa',
               '-framework OpenGL',
               '-framework CoreVideo',
             ],
           },
         }],
       ],
     },
    },
  ],
}
