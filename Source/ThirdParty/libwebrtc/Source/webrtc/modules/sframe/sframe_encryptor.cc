/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/sframe/sframe_encryptor.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/sframe/sframe_types.h"
#include "third_party/sframe/src/include/sframe/result.h"
#include "third_party/sframe/src/include/sframe/sframe.h"

namespace webrtc {

namespace {
sframe::CipherSuite ToSframeCipherSuite(SframeCipherSuite suite) {
  switch (suite) {
    case SframeCipherSuite::kAes128CtrHmacSha256_80:
      return sframe::CipherSuite::AES_128_CTR_HMAC_SHA256_80;
    case SframeCipherSuite::kAes128CtrHmacSha256_64:
      return sframe::CipherSuite::AES_128_CTR_HMAC_SHA256_64;
    case SframeCipherSuite::kAes128CtrHmacSha256_32:
      return sframe::CipherSuite::AES_128_CTR_HMAC_SHA256_32;
    case SframeCipherSuite::kAes128GcmSha256_128:
      return sframe::CipherSuite::AES_GCM_128_SHA256;
    case SframeCipherSuite::kAes256GcmSha512_128:
      return sframe::CipherSuite::AES_GCM_256_SHA512;
  }
}
}  // namespace

scoped_refptr<SframeEncryptor> SframeEncryptor::Create(
    SframeMode mode,
    SframeCipherSuite cipher_suite) {
  return make_ref_counted<SframeEncryptor>(mode, cipher_suite);
}

SframeEncryptor::SframeEncryptor(SframeMode mode,
                                 SframeCipherSuite cipher_suite)
    : sequence_checker_(SequenceChecker::kDetached),
      mode_(mode),
      context_(std::make_unique<sframe::Context>(
          ToSframeCipherSuite(cipher_suite))) {}

SframeEncryptor::~SframeEncryptor() = default;

RTCError SframeEncryptor::SetEncryptionKey(
    uint64_t key_id,
    std::span<const uint8_t> key_material) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  sframe::Result<void> result = context_->add_key(
      key_id, sframe::KeyUsage::protect,
      sframe::input_bytes(key_material.data(), key_material.size()));
  if (result.is_err()) {
    RTCError error = RTCError::InternalError("Failed to set encryption key");
    if (const char* message = result.error().message()) {
      error.string_builder() << ": " << message;
    }
    return error;
  }

  if (active_key_id_ && *active_key_id_ != key_id) {
    context_->remove_key(*active_key_id_);
  }

  active_key_id_ = key_id;

  return RTCError::OK();
}

RTCErrorOr<size_t> SframeEncryptor::Encrypt(
    std::span<const uint8_t> frame,
    std::span<const uint8_t> additional_data,
    std::span<uint8_t> encrypted_frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!active_key_id_) {
    return RTCError::InvalidState("Sframe encryption key not set");
  }

  auto result = context_->protect(
      *active_key_id_,
      sframe::output_bytes(encrypted_frame.data(), encrypted_frame.size()),
      sframe::input_bytes(frame.data(), frame.size()),
      sframe::input_bytes(additional_data.data(), additional_data.size()));
  if (result.is_err()) {
    RTCError error = RTCError::InternalError("Sframe encryption failed");
    if (const char* message = result.error().message()) {
      error.string_builder() << ": " << message;
    }
    return error;
  }
  return result.value().size();
}

size_t SframeEncryptor::GetMaxCiphertextByteSize(size_t frame_size) {
  return frame_size + sframe::Context::max_overhead;
}

}  // namespace webrtc
