/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_stream_decoder.h"

#include <algorithm>
#include <map>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/include/frame_callback.h"
#include "webrtc/modules/video_coding/video_coding_impl.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/payload_router.h"
#include "webrtc/video/receive_statistics_proxy.h"

namespace webrtc {

VideoStreamDecoder::VideoStreamDecoder(
    vcm::VideoReceiver* video_receiver,
    VCMFrameTypeCallback* vcm_frame_type_callback,
    VCMPacketRequestCallback* vcm_packet_request_callback,
    bool enable_nack,
    bool enable_fec,
    ReceiveStatisticsProxy* receive_statistics_proxy,
    rtc::VideoSinkInterface<VideoFrame>* incoming_video_stream)
    : video_receiver_(video_receiver),
      receive_stats_callback_(receive_statistics_proxy),
      incoming_video_stream_(incoming_video_stream),
      last_rtt_ms_(0) {
  RTC_DCHECK(video_receiver_);

  static const int kMaxPacketAgeToNack = 450;
  static const int kMaxNackListSize = 250;
  video_receiver_->SetNackSettings(kMaxNackListSize,
                                   kMaxPacketAgeToNack, 0);
  video_receiver_->RegisterReceiveCallback(this);
  video_receiver_->RegisterFrameTypeCallback(vcm_frame_type_callback);
  video_receiver_->RegisterReceiveStatisticsCallback(this);
  video_receiver_->RegisterDecoderTimingCallback(this);

  VCMVideoProtection video_protection =
      enable_nack ? (enable_fec ? kProtectionNackFEC : kProtectionNack)
                  : kProtectionNone;

  VCMDecodeErrorMode decode_error_mode = enable_nack ? kNoErrors : kWithErrors;
  video_receiver_->SetVideoProtection(video_protection, true);
  video_receiver_->SetDecodeErrorMode(decode_error_mode);
  VCMPacketRequestCallback* packet_request_callback =
      enable_nack ? vcm_packet_request_callback : nullptr;
  video_receiver_->RegisterPacketRequestCallback(packet_request_callback);
}

VideoStreamDecoder::~VideoStreamDecoder() {
  // Unset all the callback pointers that we set in the ctor.
  video_receiver_->RegisterPacketRequestCallback(nullptr);
  video_receiver_->RegisterDecoderTimingCallback(nullptr);
  video_receiver_->RegisterReceiveStatisticsCallback(nullptr);
  video_receiver_->RegisterFrameTypeCallback(nullptr);
  video_receiver_->RegisterReceiveCallback(nullptr);
}

// Do not acquire the lock of |video_receiver_| in this function. Decode
// callback won't necessarily be called from the decoding thread. The decoding
// thread may have held the lock when calling VideoDecoder::Decode, Reset, or
// Release. Acquiring the same lock in the path of decode callback can deadlock.
int32_t VideoStreamDecoder::FrameToRender(VideoFrame& video_frame,
                                          rtc::Optional<uint8_t> qp) {
  receive_stats_callback_->OnDecodedFrame(qp);
  incoming_video_stream_->OnFrame(video_frame);

  return 0;
}

int32_t VideoStreamDecoder::ReceivedDecodedReferenceFrame(
  const uint64_t picture_id) {
  RTC_NOTREACHED();
  return 0;
}

void VideoStreamDecoder::OnIncomingPayloadType(int payload_type) {
  receive_stats_callback_->OnIncomingPayloadType(payload_type);
}

void VideoStreamDecoder::OnDecoderImplementationName(
    const char* implementation_name) {
  receive_stats_callback_->OnDecoderImplementationName(implementation_name);
}

void VideoStreamDecoder::OnReceiveRatesUpdated(uint32_t bit_rate,
                                               uint32_t frame_rate) {
  receive_stats_callback_->OnIncomingRate(frame_rate, bit_rate);
}

void VideoStreamDecoder::OnDiscardedPacketsUpdated(int discarded_packets) {
  receive_stats_callback_->OnDiscardedPacketsUpdated(discarded_packets);
}

void VideoStreamDecoder::OnFrameCountsUpdated(const FrameCounts& frame_counts) {
  receive_stats_callback_->OnFrameCountsUpdated(frame_counts);
}

void VideoStreamDecoder::OnDecoderTiming(int decode_ms,
                                         int max_decode_ms,
                                         int current_delay_ms,
                                         int target_delay_ms,
                                         int jitter_buffer_ms,
                                         int min_playout_delay_ms,
                                         int render_delay_ms) {}

void VideoStreamDecoder::OnFrameBufferTimingsUpdated(int decode_ms,
                                                     int max_decode_ms,
                                                     int current_delay_ms,
                                                     int target_delay_ms,
                                                     int jitter_buffer_ms,
                                                     int min_playout_delay_ms,
                                                     int render_delay_ms) {}

void VideoStreamDecoder::OnCompleteFrame(bool is_keyframe, size_t size_bytes) {}

void VideoStreamDecoder::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  video_receiver_->SetReceiveChannelParameters(max_rtt_ms);

  rtc::CritScope lock(&crit_);
  last_rtt_ms_ = avg_rtt_ms;
}
}  // namespace webrtc
