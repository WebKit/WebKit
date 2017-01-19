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
      'target_name': 'audio_conference_mixer',
      'type': 'static_library',
      'dependencies': [
        'audio_processing',
        'webrtc_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'include/audio_conference_mixer.h',
        'include/audio_conference_mixer_defines.h',
        'source/audio_frame_manipulator.cc',
        'source/audio_frame_manipulator.h',
        'source/memory_pool.h',
        'source/memory_pool_posix.h',
        'source/memory_pool_win.h',
        'source/audio_conference_mixer_impl.cc',
        'source/audio_conference_mixer_impl.h',
        'source/time_scheduler.cc',
        'source/time_scheduler.h',
      ],
    },
  ], # targets
}
