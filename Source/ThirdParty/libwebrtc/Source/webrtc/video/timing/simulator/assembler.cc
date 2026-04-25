/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/assembler.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/rtp_parameters.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_codec_type.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc::video_timing_simulator {

namespace {

VideoReceiveStreamInterface::Config CreateVideoReceiveStreamConfig(
    uint32_t ssrc) {
  VideoReceiveStreamInterface::Config config(/*rtcp_send_transport=*/nullptr);
  config.rtp.remote_ssrc = ssrc;
  // From `kNackHistoryMs` in webrtc_video_engine.cc. This enables creating the
  // `NackRequester`.
  config.rtp.nack.rtp_history_ms = 1000;
  // The value of `local_ssrc` is not really used, but we need to set it to
  // _something_ due to an RTC_DCHECK in rtp_video_stream_receiver2.cc.
  config.rtp.local_ssrc = ssrc + 1;
  return config;
}

}  // namespace

Assembler::Assembler(const Environment& env,
                     uint32_t ssrc,
                     AssemblerEvents* absl_nonnull observer,
                     AssembledFrameCallback* absl_nonnull assembled_frame_cb)
    : env_(env),
      video_receive_stream_config_(CreateVideoReceiveStreamConfig(ssrc)),
      rtp_receive_statistics_(
          ReceiveStatistics::CreateThreadCompatible(&env_.clock())),
      // Provide the minimal needed dependencies to the
      // `RtpVideoStreamReceiver2`.
      rtp_video_stream_receiver2_(env_,
                                  TaskQueueBase::Current(),
                                  /*transport=*/this,
                                  /*rtt_stats=*/nullptr,
                                  /*packet_router=*/nullptr,
                                  &video_receive_stream_config_,
                                  rtp_receive_statistics_.get(),
                                  /*rtcp_packet_type_counter_observer=*/nullptr,
                                  /*rtcp_cname_callback=*/nullptr,
                                  &nack_periodic_processor_,
                                  /*complete_frame_callback=*/this,
                                  /*frame_decryptor=*/nullptr,
                                  /*frame_transformer=*/nullptr),
      observer_(*observer),
      assembled_frame_cb_(*assembled_frame_cb) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  rtp_video_stream_receiver2_.StartReceive();
}

Assembler::~Assembler() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  rtp_video_stream_receiver2_.StopReceive();
}

void Assembler::OnReceivedRtpPacket(const RtpPacketReceived& rtp_packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Register all payload types as generic codec with raw packetization.
  uint8_t payload_type = rtp_packet.PayloadType();
  if (registered_payload_types_.find(payload_type) ==
      registered_payload_types_.end()) {
    CodecParameterMap unused_map;
    rtp_video_stream_receiver2_.AddReceiveCodec(
        payload_type, VideoCodecType::kVideoCodecGeneric, unused_map,
        /*raw_payload=*/true);
    registered_payload_types_.insert(payload_type);
  }
  rtp_video_stream_receiver2_.OnRtpPacket(rtp_packet);
}

void Assembler::OnDecodedFrameId(int64_t frame_id) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Clear the internal `PacketBuffer` when the frames have been "decoded".
  rtp_video_stream_receiver2_.FrameDecoded(frame_id);
}

void Assembler::OnCompleteFrame(std::unique_ptr<EncodedFrame> encoded_frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // We rename this callback from `OnCompleteFrame` to `OnAssembledFrame`, since
  // the latter is a more descriptive.
  observer_.OnAssembledFrame(*encoded_frame);
  assembled_frame_cb_.OnAssembledFrame(std::move(encoded_frame));
}

}  // namespace webrtc::video_timing_simulator
