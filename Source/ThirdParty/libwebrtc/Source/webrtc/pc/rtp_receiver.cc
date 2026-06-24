/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtp_receiver.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/sframe/sframe_decrypter_interface.h"
#include "api/sframe/sframe_types.h"
#include "pc/media_stream.h"
#include "pc/media_stream_proxy.h"
#include "rtc_base/thread.h"

namespace webrtc {

// This function is only expected to be called on the signalling thread.
// On the other hand, some test or even production setups may use
// several signaling threads.
int RtpReceiverInternal::GenerateUniqueId() {
  static std::atomic<int> g_unique_id{0};

  return ++g_unique_id;
}

std::vector<scoped_refptr<MediaStreamInterface>>
RtpReceiverInternal::CreateStreamsFromIds(std::vector<std::string> stream_ids) {
  std::vector<scoped_refptr<MediaStreamInterface>> streams(stream_ids.size());
  for (size_t i = 0; i < stream_ids.size(); ++i) {
    streams[i] = MediaStreamProxy::Create(
        Thread::Current(), MediaStream::Create(std::move(stream_ids[i])));
  }
  return streams;
}

RtpReceiverBase::RtpReceiverBase(
    Thread* worker_thread,
    absl::AnyInvocable<RTCError()> enable_sframe_at_owner)
    : worker_thread_(worker_thread),
      enable_sframe_at_owner_(std::move(enable_sframe_at_owner)) {}

std::optional<uint32_t> RtpReceiverBase::ssrc() const {
  RTC_DCHECK_RUN_ON(worker_thread_);
  if (!signaled_ssrc_.has_value() && media_channel()) {
    return media_channel()->GetUnsignaledSsrc();
  }
  return signaled_ssrc_;
}

void RtpReceiverBase::SetFrameDecryptor(
    scoped_refptr<FrameDecryptorInterface> frame_decryptor) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  frame_decryptor_ = std::move(frame_decryptor);
  // Special Case: Set the frame decryptor to any value on any existing channel.
  if (media_channel() && signaled_ssrc_) {
    media_channel()->SetFrameDecryptor(*signaled_ssrc_, frame_decryptor_);
  }
}

scoped_refptr<FrameDecryptorInterface> RtpReceiverBase::GetFrameDecryptor()
    const {
  RTC_DCHECK_RUN_ON(worker_thread_);
  return frame_decryptor_;
}

void RtpReceiverBase::SetFrameTransformer(
    scoped_refptr<FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  frame_transformer_ = std::move(frame_transformer);
  if (media_channel()) {
    media_channel()->SetDepacketizerToDecoderFrameTransformer(
        signaled_ssrc_.value_or(0), frame_transformer_);
  }
}

RTCErrorOr<scoped_refptr<SframeDecrypterInterface>>
RtpReceiverBase::CreateSframeDecrypterOrError(SframeCipherSuite cipher_suite) {
  RTC_DCHECK_RUN_ON(&signaling_thread_checker_);

  if (!enable_sframe_at_owner_) {
    return RTCError(RTCErrorType::INTERNAL_ERROR,
                    "Receiver is not associated with a transceiver");
  }

  RTCError error = enable_sframe_at_owner_();
  if (!error.ok()) {
    return error;
  }

  // TODO(bugs.webrtc.org/479862368): Create the internal Sframe decryption
  // pipeline and return a key management handle.
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                  "Sframe decrypter not yet implemented");
}

}  // namespace webrtc
