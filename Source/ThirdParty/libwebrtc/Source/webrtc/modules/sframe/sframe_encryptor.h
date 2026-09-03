/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_SFRAME_SFRAME_ENCRYPTOR_H_
#define MODULES_SFRAME_SFRAME_ENCRYPTOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "absl/base/nullability.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/sframe/sframe_types.h"
#include "modules/sframe/sframe_media_encryptor_interface.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace sframe {
class Context;
}  // namespace sframe

namespace webrtc {

class SframeEncryptor : public SframeMediaEncryptorInterface {
 public:
  // Creates a new SframeEncryptor. This factory method never fails.
  static absl_nonnull scoped_refptr<SframeEncryptor> Create(
      SframeMode mode,
      SframeCipherSuite cipher_suite);

  SframeEncryptor(SframeMode mode, SframeCipherSuite cipher_suite);
  ~SframeEncryptor() override;

  // SframeEncryptorInterface implementation.
  RTCError SetEncryptionKey(uint64_t key_id,
                            std::span<const uint8_t> key_material) override;

  // SframeMediaEncryptorInterface implementation.
  RTCErrorOr<size_t> Encrypt(std::span<const uint8_t> frame,
                             std::span<const uint8_t> additional_data,
                             std::span<uint8_t> encrypted_frame) override;

  size_t GetMaxCiphertextByteSize(size_t frame_size) override;

  SframeMode mode() const override { return mode_; }

 private:
  // Callers must use this object from a single sequence. Today that sequence
  // is the media-pipeline (worker) thread reached via the signaling thread.
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;

  const SframeMode mode_;
  std::unique_ptr<sframe::Context> context_ RTC_GUARDED_BY(sequence_checker_);
  std::optional<uint64_t> active_key_id_ RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace webrtc

#endif  // MODULES_SFRAME_SFRAME_ENCRYPTOR_H_
