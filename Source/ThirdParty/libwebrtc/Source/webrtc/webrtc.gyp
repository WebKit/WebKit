# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'includes': [
    'build/common.gypi',
    'audio/webrtc_audio.gypi',
    'call/webrtc_call.gypi',
    'video/webrtc_video.gypi',
  ],
  'targets': [
    {
      'target_name': 'webrtc',
      'type': 'static_library',
      'sources': [
        'call.h',
        'config.h',
        'transport.h',
        'video_receive_stream.h',
        'video_send_stream.h',

        '<@(webrtc_audio_sources)',
        '<@(webrtc_call_sources)',
        '<@(webrtc_video_sources)',
      ],
      'dependencies': [
        'common.gyp:*',
        '<@(webrtc_audio_dependencies)',
        '<@(webrtc_call_dependencies)',
        '<@(webrtc_video_dependencies)',
        'rtc_event_log_impl',
      ],
      'conditions': [
        # TODO(andresp): Chromium should link directly with this and no if
        # conditions should be needed on webrtc build files.
        ['build_with_chromium==1', {
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_capture',
          ],
        }],
      ],
    },
    {
      'target_name': 'rtc_event_log_api',
      'type': 'static_library',
      'sources': [
        'logging/rtc_event_log/rtc_event_log.h',
      ],
    },
    {
      'target_name': 'rtc_event_log_impl',
      'type': 'static_library',
      'sources': [
        'logging/rtc_event_log/ringbuffer.h',
        'logging/rtc_event_log/rtc_event_log.cc',
        'logging/rtc_event_log/rtc_event_log_helper_thread.cc',
        'logging/rtc_event_log/rtc_event_log_helper_thread.h',
      ],
      'conditions': [
        # If enable_protobuf is defined, we want to compile the protobuf
        # and add rtc_event_log.pb.h and rtc_event_log.pb.cc to the sources.
        ['enable_protobuf==1', {
          'dependencies': [
            'rtc_event_log_api',
            'rtc_event_log_proto',
            '<(webrtc_root)/api/api.gyp:call_api',
          ],
          'defines': [
            'ENABLE_RTC_EVENT_LOG',
          ],
        }],
      ],
    },
  ],  # targets
  'conditions': [
    ['include_tests==1', {
      'includes': [
        'webrtc_tests.gypi',
      ],
    }],
    ['enable_protobuf==1', {
      'targets': [
        {
          # This target should only be built if enable_protobuf is defined
          'target_name': 'rtc_event_log_proto',
          'type': 'static_library',
          'sources': ['logging/rtc_event_log/rtc_event_log.proto',],
          'variables': {
            'proto_in_dir': 'logging/rtc_event_log',
            'proto_out_dir': 'webrtc/logging/rtc_event_log',
          },
          'includes': ['build/protoc.gypi'],
        },
        {
          'target_name': 'rtc_event_log_parser',
          'type': 'static_library',
          'sources': [
            'logging/rtc_event_log/rtc_event_log_parser.cc',
            'logging/rtc_event_log/rtc_event_log_parser.h',
          ],
          'dependencies': [
            'rtc_event_log_proto',
          ],
          'export_dependent_settings': [
            'rtc_event_log_proto',
          ],
        },
      ],
    }],
    ['include_tests==1 and enable_protobuf==1', {
      'targets': [
        {
          'target_name': 'rtc_event_log2rtp_dump',
          'type': 'executable',
          'sources': ['logging/rtc_event_log2rtp_dump.cc',],
          'dependencies': [
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            'rtc_event_log_parser',
            'rtc_event_log_proto',
            'test/test.gyp:rtp_test_utils'
          ],
        },
      ],
    }],
  ],  # conditions
}
