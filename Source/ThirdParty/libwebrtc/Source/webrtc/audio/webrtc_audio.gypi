# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'variables': {
    'webrtc_audio_dependencies': [
      '<(webrtc_root)/api/api.gyp:audio_mixer_api',
      '<(webrtc_root)/api/api.gyp:call_api',
      '<(webrtc_root)/common.gyp:webrtc_common',
      '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
      '<(webrtc_root)/webrtc.gyp:rtc_event_log_api',
    ],
    'webrtc_audio_sources': [
      'audio/audio_receive_stream.cc',
      'audio/audio_receive_stream.h',
      'audio/audio_send_stream.cc',
      'audio/audio_send_stream.h',
      'audio/audio_state.cc',
      'audio/audio_state.h',
      'audio/conversion.h',
      'audio/scoped_voe_interface.h',
    ],
  },
}
