# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'webrtc_h264',
      'type': 'static_library',
      'conditions': [
        ['OS=="ios"', {
          'dependencies': [
            'webrtc_h264_video_toolbox',
          ],
          'sources': [
            'h264_objc.mm',
          ],
        }],
        # TODO(hbos): Consider renaming this flag and the below macro to
        # something which helps distinguish OpenH264/FFmpeg from other H264
        # implementations.
        ['rtc_use_h264==1', {
          'defines': [
            'WEBRTC_USE_H264',
          ],
          'conditions': [
            ['rtc_initialize_ffmpeg==1', {
              'defines': [
                'WEBRTC_INITIALIZE_FFMPEG',
              ],
            }],
          ],
          'dependencies': [
            '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
            '<(DEPTH)/third_party/openh264/openh264.gyp:openh264_encoder',
            '<(webrtc_root)/common_video/common_video.gyp:common_video',
          ],
          'sources': [
            'h264_decoder_impl.cc',
            'h264_decoder_impl.h',
            'h264_encoder_impl.cc',
            'h264_encoder_impl.h',
          ],
        }],
      ],
      'sources': [
        'h264.cc',
        'include/h264.h',
      ],
    }, # webrtc_h264
  ],
  'conditions': [
    ['OS=="ios"', {
      'targets': [
        {
          'target_name': 'webrtc_h264_video_toolbox',
          'type': 'static_library',
          'includes': [ '../../../../build/objc_common.gypi' ],
          'dependencies': [
            '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_common_objc',
          ],
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework CoreFoundation',
                '-framework CoreMedia',
                '-framework CoreVideo',
                '-framework VideoToolbox',
              ],
            },
          },
          'sources': [
            'h264_video_toolbox_decoder.cc',
            'h264_video_toolbox_decoder.h',
            'h264_video_toolbox_encoder.h',
            'h264_video_toolbox_encoder.mm',
            'h264_video_toolbox_nalu.cc',
            'h264_video_toolbox_nalu.h',
          ],
          'conditions': [
            ['build_libyuv==1', {
              'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv'],
            }],
          ],
        }, # webrtc_h264_video_toolbox
      ], # targets
    }], # OS=="ios"
  ], # conditions
}
