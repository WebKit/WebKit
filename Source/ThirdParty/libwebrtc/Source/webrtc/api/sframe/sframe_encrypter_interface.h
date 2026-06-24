/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_SFRAME_SFRAME_ENCRYPTER_INTERFACE_H_
#define API_SFRAME_SFRAME_ENCRYPTER_INTERFACE_H_

#include <cstdint>
#include <span>

#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "api/sframe/sframe_types.h"

namespace webrtc {

struct SframeEncrypterInit {
  SframeMode mode;
  SframeCipherSuite cipher_suite;
};

// Key management handle for Sframe sender encryption.
class SframeEncrypterInterface : public RefCountInterface {
 public:
  virtual RTCError SetEncryptionKey(uint64_t key_id,
                                    std::span<const uint8_t> key_material) = 0;

 protected:
  ~SframeEncrypterInterface() override = default;
};

}  // namespace webrtc

#endif  // API_SFRAME_SFRAME_ENCRYPTER_INTERFACE_H_
