# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    { 'target_name': 'audio_network_adaptor',
      'type': 'static_library',
      'sources': [
        'audio_network_adaptor.cc',
        'audio_network_adaptor_impl.cc',
        'audio_network_adaptor_impl.h',
        'bitrate_controller.h',
        'bitrate_controller.cc',
        'channel_controller.cc',
        'channel_controller.h',
        'controller.h',
        'controller.cc',
        'controller_manager.cc',
        'controller_manager.h',
        'debug_dump_writer.cc',
        'debug_dump_writer.h',
        'dtx_controller.h',
        'dtx_controller.cc',
        'fec_controller.h',
        'fec_controller.cc',
        'frame_length_controller.cc',
        'frame_length_controller.h',
        'include/audio_network_adaptor.h',
        'smoothing_filter.h',
        'smoothing_filter.cc',
      ], # sources
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'conditions': [
        ['enable_protobuf==1', {
          'dependencies': [
            'ana_config_proto',
            'ana_debug_dump_proto',
          ],
          'defines': ['WEBRTC_AUDIO_NETWORK_ADAPTOR_DEBUG_DUMP'],
        }],
      ], # conditions
    },
  ], # targets

  'conditions': [
    ['enable_protobuf==1', {
      'targets': [
        { 'target_name': 'ana_debug_dump_proto',
          'type': 'static_library',
          'sources': ['debug_dump.proto',],
          'variables': {
            'proto_in_dir': '.',
            # Workaround to protect against gyp's pathname relativization when
            # this file is included by modules.gyp.
            'proto_out_protected': 'webrtc/modules/audio_coding/audio_network_adaptor',
            'proto_out_dir': '<(proto_out_protected)',
          },
          'includes': ['../../../build/protoc.gypi',],
        },
        { 'target_name': 'ana_config_proto',
          'type': 'static_library',
          'sources': ['config.proto',],
          'variables': {
            'proto_in_dir': '.',
            # Workaround to protect against gyp's pathname relativization when
            # this file is included by modules.gyp.
            'proto_out_protected': 'webrtc/modules/audio_coding/audio_network_adaptor',
            'proto_out_dir': '<(proto_out_protected)',
          },
          'includes': ['../../../build/protoc.gypi',],
        },
      ], # targets
    }],
  ], # conditions
}
