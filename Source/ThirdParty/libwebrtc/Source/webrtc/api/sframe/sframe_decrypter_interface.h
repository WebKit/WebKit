/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_SFRAME_SFRAME_DECRYPTER_INTERFACE_H_
#define API_SFRAME_SFRAME_DECRYPTER_INTERFACE_H_

#include <cstdint>
#include <span>

#include "api/ref_count.h"
#include "api/rtc_error.h"

namespace webrtc {

// Key management handle for Sframe receiver decryption.
class SframeDecrypterInterface : public RefCountInterface {
 public:
  virtual RTCError AddDecryptionKey(uint64_t key_id,
                                    std::span<const uint8_t> key_material) = 0;

  virtual RTCError RemoveDecryptionKey(uint64_t key_id) = 0;

 protected:
  ~SframeDecrypterInterface() override = default;
};

}  // namespace webrtc

#endif  // API_SFRAME_SFRAME_DECRYPTER_INTERFACE_H_
