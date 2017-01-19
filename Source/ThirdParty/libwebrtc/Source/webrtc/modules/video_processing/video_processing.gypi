# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'video_processing',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'include/video_processing.h',
        'include/video_processing_defines.h',
        'frame_preprocessor.cc',
        'frame_preprocessor.h',
        'spatial_resampler.cc',
        'spatial_resampler.h',
        'video_decimator.cc',
        'video_decimator.h',
        'video_processing_impl.cc',
        'video_processing_impl.h',
        'video_denoiser.cc',
        'video_denoiser.h',
        'util/denoiser_filter.cc',
        'util/denoiser_filter.h',
        'util/denoiser_filter_c.cc',
        'util/denoiser_filter_c.h',
        'util/noise_estimation.cc',
        'util/noise_estimation.h',
        'util/skin_detection.cc',
        'util/skin_detection.h',
      ],
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [ 'video_processing_sse2', ],
        }],
        ['target_arch=="arm" or target_arch == "arm64"', {
          'dependencies': [ 'video_processing_neon', ],
        }],
      ],
    },
  ],
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'video_processing_sse2',
          'type': 'static_library',
          'sources': [
            'util/denoiser_filter_sse2.cc',
            'util/denoiser_filter_sse2.h',
          ],
          'conditions': [
            ['os_posix==1 and OS!="mac"', {
              'cflags': [ '-msse2', ],
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_CFLAGS': [ '-msse2', ],
              },
            }],
          ],
        },
      ],
    }],
    ['target_arch=="arm" or target_arch == "arm64"', {
      'targets': [
        {
          'target_name': 'video_processing_neon',
          'type': 'static_library',
          'includes': [ '../../build/arm_neon.gypi', ],
          'sources': [
            'util/denoiser_filter_neon.cc',
            'util/denoiser_filter_neon.h',
          ],
        },
      ],
    }],
  ],
}

