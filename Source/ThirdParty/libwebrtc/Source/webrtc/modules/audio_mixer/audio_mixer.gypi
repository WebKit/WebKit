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
      'target_name': 'audio_mixer_impl',
      'type': 'static_library',
      'dependencies': [
        'audio_frame_manipulator',
        'audio_processing',
        'webrtc_utility',
        '<(webrtc_root)/api/api.gyp:audio_mixer_api',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'audio_mixer_impl.cc',
        'audio_mixer_impl.h',
      ],
    },
    {
      'target_name': 'audio_frame_manipulator',
      'type': 'static_library',
      'dependencies': [
          'webrtc_utility',
          '<(webrtc_root)/base/base.gyp:rtc_base_approved',
      ],
      'sources': [
        'audio_frame_manipulator.cc',
        'audio_frame_manipulator.h',
      ],
    },
  ], # targets
}
