/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_decoder2.h"

#include <cstdint>
#include <optional>

#include "api/units/time_delta.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/video_receiver2.h"
#include "rtc_base/checks.h"
#include "video/receive_statistics_proxy.h"

namespace webrtc {
namespace internal {

VideoStreamDecoder::VideoStreamDecoder(
    VideoReceiver2* video_receiver,
    ReceiveStatisticsProxy* receive_statistics_proxy,
    rtc::VideoSinkInterface<VideoFrame>* incoming_video_stream)
    : video_receiver_(video_receiver),
      receive_stats_callback_(receive_statistics_proxy),
      incoming_video_stream_(incoming_video_stream) {
  RTC_DCHECK(video_receiver_);

  video_receiver_->RegisterReceiveCallback(this);
}

VideoStreamDecoder::~VideoStreamDecoder() {
  // Note: There's an assumption at this point that the decoder thread is
  // *not* running. If it was, then there could be a race for each of these
  // callbacks.

  // Unset all the callback pointers that we set in the ctor.
  video_receiver_->RegisterReceiveCallback(nullptr);
}

// Do not acquire the lock of `video_receiver_` in this function. Decode
// callback won't necessarily be called from the decoding thread. The decoding
// thread may have held the lock when calling VideoDecoder::Decode, Reset, or
// Release. Acquiring the same lock in the path of decode callback can deadlock.
int32_t VideoStreamDecoder::FrameToRender(VideoFrame& video_frame,
                                          std::optional<uint8_t> qp,
                                          TimeDelta decode_time,
                                          VideoContentType content_type,
                                          VideoFrameType frame_type) {
  return OnFrameToRender({.video_frame = video_frame,
                          .qp = qp,
                          .decode_time = decode_time,
                          .content_type = content_type,
                          .frame_type = frame_type});
}
int32_t VideoStreamDecoder::OnFrameToRender(
    const struct FrameToRender& arguments) {
  receive_stats_callback_->OnDecodedFrame(
      arguments.video_frame, arguments.qp, arguments.decode_time,
      arguments.content_type, arguments.frame_type);
  if (arguments.corruption_score.has_value()) {
    receive_stats_callback_->OnCorruptionScore(*arguments.corruption_score,
                                               arguments.content_type);
  }
  incoming_video_stream_->OnFrame(arguments.video_frame);
  return 0;
}

void VideoStreamDecoder::OnDroppedFrames(uint32_t frames_dropped) {
  receive_stats_callback_->OnDroppedFrames(frames_dropped);
}

void VideoStreamDecoder::OnIncomingPayloadType(int payload_type) {
  receive_stats_callback_->OnIncomingPayloadType(payload_type);
}

void VideoStreamDecoder::OnDecoderInfoChanged(
    const VideoDecoder::DecoderInfo& decoder_info) {
  receive_stats_callback_->OnDecoderInfo(decoder_info);
}

}  // namespace internal
}  // namespace webrtc
