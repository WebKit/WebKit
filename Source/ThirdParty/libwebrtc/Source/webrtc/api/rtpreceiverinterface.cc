/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtpreceiverinterface.h"

namespace webrtc {

RtpSource::RtpSource(int64_t timestamp_ms,
                     uint32_t source_id,
                     RtpSourceType source_type)
    : timestamp_ms_(timestamp_ms),
      source_id_(source_id),
      source_type_(source_type) {}

RtpSource::RtpSource(int64_t timestamp_ms,
                     uint32_t source_id,
                     RtpSourceType source_type,
                     uint8_t audio_level)
    : timestamp_ms_(timestamp_ms),
      source_id_(source_id),
      source_type_(source_type),
      audio_level_(audio_level) {}

RtpSource::RtpSource(const RtpSource&) = default;
RtpSource& RtpSource::operator=(const RtpSource&) = default;
RtpSource::~RtpSource() = default;

std::vector<std::string> RtpReceiverInterface::stream_ids() const {
  return {};
}

std::vector<rtc::scoped_refptr<MediaStreamInterface>>
RtpReceiverInterface::streams() const {
  return {};
}

std::vector<RtpSource> RtpReceiverInterface::GetSources() const {
  return {};
}

void RtpReceiverInterface::SetFrameDecryptor(
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) {}

rtc::scoped_refptr<FrameDecryptorInterface>
RtpReceiverInterface::GetFrameDecryptor() const {
  return nullptr;
}

}  // namespace webrtc
