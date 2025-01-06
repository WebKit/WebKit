/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_receiver_interface.h"

#include <string>
#include <vector>

#include "api/crypto/frame_decryptor_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/transport/rtp/rtp_source.h"

namespace webrtc {

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
    rtc::scoped_refptr<FrameDecryptorInterface> /* frame_decryptor */) {}

rtc::scoped_refptr<FrameDecryptorInterface>
RtpReceiverInterface::GetFrameDecryptor() const {
  return nullptr;
}

rtc::scoped_refptr<DtlsTransportInterface>
RtpReceiverInterface::dtls_transport() const {
  return nullptr;
}

void RtpReceiverInterface::SetFrameTransformer(
    rtc::scoped_refptr<FrameTransformerInterface> /* frame_transformer */) {}

}  // namespace webrtc
