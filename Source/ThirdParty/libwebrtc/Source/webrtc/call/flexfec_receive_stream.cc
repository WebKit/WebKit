/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/flexfec_receive_stream.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

std::string FlexfecReceiveStream::Stats::ToString(int64_t time_ms) const {
  std::stringstream ss;
  ss << "FlexfecReceiveStream stats: " << time_ms
     << ", {flexfec_bitrate_bps: " << flexfec_bitrate_bps << "}";
  return ss.str();
}

namespace {

// TODO(brandtr): Update this function when we support multistream protection.
std::unique_ptr<FlexfecReceiver> MaybeCreateFlexfecReceiver(
    const FlexfecReceiveStream::Config& config,
    RecoveredPacketReceiver* recovered_packet_callback) {
  if (config.flexfec_payload_type < 0) {
    LOG(LS_WARNING) << "Invalid FlexFEC payload type given. "
                    << "This FlexfecReceiveStream will therefore be useless.";
    return nullptr;
  }
  RTC_DCHECK_GE(config.flexfec_payload_type, 0);
  RTC_DCHECK_LE(config.flexfec_payload_type, 127);
  if (config.flexfec_ssrc == 0) {
    LOG(LS_WARNING) << "Invalid FlexFEC SSRC given. "
                    << "This FlexfecReceiveStream will therefore be useless.";
    return nullptr;
  }
  if (config.protected_media_ssrcs.empty()) {
    LOG(LS_WARNING) << "No protected media SSRC supplied. "
                    << "This FlexfecReceiveStream will therefore be useless.";
    return nullptr;
  }

  if (config.protected_media_ssrcs.size() > 1) {
    LOG(LS_WARNING)
        << "The supplied FlexfecConfig contained multiple protected "
           "media streams, but our implementation currently only "
           "supports protecting a single media stream. "
           "To avoid confusion, disabling FlexFEC completely.";
    return nullptr;
  }
  RTC_DCHECK_EQ(1U, config.protected_media_ssrcs.size());
  return std::unique_ptr<FlexfecReceiver>(
      new FlexfecReceiver(config.flexfec_ssrc, config.protected_media_ssrcs[0],
                          recovered_packet_callback));
}

}  // namespace

namespace internal {

FlexfecReceiveStream::FlexfecReceiveStream(
    Config configuration,
    RecoveredPacketReceiver* recovered_packet_callback)
    : started_(false),
      config_(configuration),
      receiver_(
          MaybeCreateFlexfecReceiver(config_, recovered_packet_callback)) {
  LOG(LS_INFO) << "FlexfecReceiveStream: " << config_.ToString();
}

FlexfecReceiveStream::~FlexfecReceiveStream() {
  LOG(LS_INFO) << "~FlexfecReceiveStream: " << config_.ToString();
  Stop();
}

bool FlexfecReceiveStream::AddAndProcessReceivedPacket(const uint8_t* packet,
                                                       size_t packet_length) {
  {
    rtc::CritScope cs(&crit_);
    if (!started_)
      return false;
  }
  if (!receiver_)
    return false;
  return receiver_->AddAndProcessReceivedPacket(packet, packet_length);
}

void FlexfecReceiveStream::Start() {
  rtc::CritScope cs(&crit_);
  started_ = true;
}

void FlexfecReceiveStream::Stop() {
  rtc::CritScope cs(&crit_);
  started_ = false;
}

// TODO(brandtr): Implement this member function when we have designed the
// stats for FlexFEC.
FlexfecReceiveStream::Stats FlexfecReceiveStream::GetStats() const {
  return webrtc::FlexfecReceiveStream::Stats();
}

}  // namespace internal

}  // namespace webrtc
