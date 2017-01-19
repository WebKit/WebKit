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
      'target_name': 'webrtc_utility',
      'type': 'static_library',
      'dependencies': [
        'audio_coding_module',
        'media_file',
        '<(webrtc_root)/base/base.gyp:rtc_task_queue',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'include/audio_frame_operations.h',
        'include/file_player.h',
        'include/file_recorder.h',
        'include/helpers_android.h',
        'include/helpers_ios.h',
        'include/jvm_android.h',
        'include/process_thread.h',
        'source/audio_frame_operations.cc',
        'source/coder.cc',
        'source/coder.h',
        'source/file_player.cc',
        'source/file_recorder.cc',
        'source/helpers_android.cc',
        'source/helpers_ios.mm',
        'source/jvm_android.cc',
        'source/process_thread_impl.cc',
        'source/process_thread_impl.h',
      ],
    },
  ], # targets
}
