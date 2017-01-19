# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'variables': {
    'webrtc_video_dependencies': [
      '<(webrtc_root)/base/base.gyp:rtc_base_approved',
      '<(webrtc_root)/common.gyp:webrtc_common',
      '<(webrtc_root)/common_video/common_video.gyp:common_video',
      '<(webrtc_root)/modules/modules.gyp:bitrate_controller',
      '<(webrtc_root)/modules/modules.gyp:congestion_controller',
      '<(webrtc_root)/modules/modules.gyp:paced_sender',
      '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
      '<(webrtc_root)/modules/modules.gyp:video_processing',
      '<(webrtc_root)/modules/modules.gyp:webrtc_utility',
      '<(webrtc_root)/modules/modules.gyp:webrtc_video_coding',
      '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
      '<(webrtc_root)/webrtc.gyp:rtc_event_log_api',
    ],
    'webrtc_video_sources': [
      'video/call_stats.cc',
      'video/call_stats.h',
      'video/encoder_rtcp_feedback.cc',
      'video/encoder_rtcp_feedback.h',
      'video/overuse_frame_detector.cc',
      'video/overuse_frame_detector.h',
      'video/payload_router.cc',
      'video/payload_router.h',
      'video/receive_statistics_proxy.cc',
      'video/receive_statistics_proxy.h',
      'video/report_block_stats.cc',
      'video/report_block_stats.h',
      'video/rtp_stream_receiver.cc',
      'video/rtp_stream_receiver.h',
      'video/rtp_streams_synchronizer.cc',
      'video/rtp_streams_synchronizer.h',
      'video/send_delay_stats.cc',
      'video/send_delay_stats.h',
      'video/send_statistics_proxy.cc',
      'video/send_statistics_proxy.h',
      'video/stats_counter.cc',
      'video/stats_counter.h',
      'video/stream_synchronization.cc',
      'video/stream_synchronization.h',
      'video/video_decoder.cc',
      'video/video_encoder.cc',
      'video/video_receive_stream.cc',
      'video/video_receive_stream.h',
      'video/video_send_stream.cc',
      'video/video_send_stream.h',
      'video/video_stream_decoder.cc',
      'video/video_stream_decoder.h',
      'video/vie_encoder.cc',
      'video/vie_encoder.h',
      'video/vie_remb.cc',
      'video/vie_remb.h',
    ],
  },
}
