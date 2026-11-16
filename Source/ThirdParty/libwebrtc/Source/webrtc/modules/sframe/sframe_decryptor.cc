/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/sframe/sframe_decryptor.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/sframe/sframe_types.h"
#include "modules/sframe/sframe_context_factory.h"
#include "modules/sframe/sframe_media_decryptor_interface.h"
#include "third_party/sframe/src/include/sframe/result.h"
#include "third_party/sframe/src/include/sframe/sframe.h"

namespace webrtc {

namespace {

// Maps a third_party/sframe failure onto SframeDecryptFailure, whose three
// categories mirror SFrameTransformErrorEventType from the WebRTC Encoded
// Transform spec:
// https://www.w3.org/TR/webrtc-encoded-transform/#enumdef-sframetransformerroreventtype
SframeDecryptFailure ToSframeDecryptFailure(const sframe::SFrameError& error) {
  switch (error.type()) {
    case sframe::SFrameErrorType::authentication_error:
    case sframe::SFrameErrorType::crypto_error:
      return SframeDecryptFailure::Authentication();
    case sframe::SFrameErrorType::unknown_key_id_error:
      return SframeDecryptFailure::KeyId(error.key_id());
    default:
      return SframeDecryptFailure::Syntax();
  }
}

}  // namespace

scoped_refptr<SframeDecryptor> SframeDecryptor::Create(
    SframeCipherSuite cipher_suite) {
  return make_ref_counted<SframeDecryptor>(cipher_suite);
}

SframeDecryptor::SframeDecryptor(SframeCipherSuite cipher_suite)
    : sequence_checker_(SequenceChecker::kDetached),
      context_(CreateSframeContext(cipher_suite)) {}

SframeDecryptor::~SframeDecryptor() = default;

RTCError SframeDecryptor::AddDecryptionKey(
    uint64_t key_id,
    std::span<const uint8_t> key_material) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  sframe::Result<void> result = context_->add_key(
      key_id, sframe::KeyUsage::unprotect,
      sframe::input_bytes(key_material.data(), key_material.size()));
  if (result.is_err()) {
    RTCError error = RTCError::InternalError("Failed to add decryption key");
    if (const char* message = result.error().message()) {
      error.string_builder() << ": " << message;
    }
    return error;
  }
  return RTCError::OK();
}

RTCError SframeDecryptor::RemoveDecryptionKey(uint64_t key_id) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  context_->remove_key(key_id);
  return RTCError::OK();
}

SframeDecryptResult SframeDecryptor::Decrypt(
    std::span<const uint8_t> encrypted_frame,
    std::span<const uint8_t> additional_data,
    std::span<uint8_t> frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // The key id is encoded in the Sframe header; `unprotect` selects the
  // matching key that was registered via AddDecryptionKey.
  sframe::Result<sframe::output_bytes> result = context_->unprotect(
      sframe::output_bytes(frame.data(), frame.size()),
      sframe::input_bytes(encrypted_frame.data(), encrypted_frame.size()),
      sframe::input_bytes(additional_data.data(), additional_data.size()));

  if (result.is_ok()) {
    return SframeDecryptSuccess{.bytes_written = result.value().size()};
  }

  return ToSframeDecryptFailure(result.error());
}

size_t SframeDecryptor::GetMaxPlaintextByteSize(size_t encrypted_frame_size) {
  // Decryption strips the Sframe header and authentication tag, so the
  // plaintext is never larger than the ciphertext.
  return encrypted_frame_size;
}

}  // namespace webrtc
