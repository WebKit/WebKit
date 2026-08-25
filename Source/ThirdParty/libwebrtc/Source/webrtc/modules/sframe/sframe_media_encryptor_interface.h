/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_SFRAME_SFRAME_MEDIA_ENCRYPTOR_INTERFACE_H_
#define MODULES_SFRAME_SFRAME_MEDIA_ENCRYPTOR_INTERFACE_H_

#include <cstddef>
#include <cstdint>
#include <span>

#include "api/rtc_error.h"
#include "api/sframe/sframe_encryptor_interface.h"
#include "api/sframe/sframe_types.h"

namespace webrtc {

// Internal media pipeline interface that extends the public key management
// interface with the actual encrypt operation.
class SframeMediaEncryptorInterface : public SframeEncryptorInterface {
 public:
  virtual SframeMode mode() const = 0;

  virtual RTCErrorOr<size_t> Encrypt(std::span<const uint8_t> frame,
                                     std::span<const uint8_t> additional_data,
                                     std::span<uint8_t> encrypted_frame) = 0;

  virtual size_t GetMaxCiphertextByteSize(size_t frame_size) = 0;

 protected:
  ~SframeMediaEncryptorInterface() override = default;
};

}  // namespace webrtc

#endif  // MODULES_SFRAME_SFRAME_MEDIA_ENCRYPTOR_INTERFACE_H_
