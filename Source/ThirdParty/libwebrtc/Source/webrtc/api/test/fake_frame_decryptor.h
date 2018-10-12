/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_FAKE_FRAME_DECRYPTOR_H_
#define API_TEST_FAKE_FRAME_DECRYPTOR_H_

#include <vector>

#include "api/crypto/framedecryptorinterface.h"
#include "rtc_base/refcountedobject.h"

namespace webrtc {

// The FakeFrameDecryptor is a TEST ONLY fake implementation of the
// FrameDecryptorInterface. It is constructed with a simple single digit key and
// a fixed postfix byte. This is just to validate that the core code works
// as expected.
class FakeFrameDecryptor
    : public rtc::RefCountedObject<FrameDecryptorInterface> {
 public:
  // Provide a key (0,255) and some postfix byte (0,255) this should match the
  // byte you expect from the FakeFrameEncryptor.
  explicit FakeFrameDecryptor(uint8_t fake_key = 1,
                              uint8_t expected_postfix_byte = 255);

  // FrameDecryptorInterface implementation
  int Decrypt(cricket::MediaType media_type,
              const std::vector<uint32_t>& csrcs,
              rtc::ArrayView<const uint8_t> additional_data,
              rtc::ArrayView<const uint8_t> encrypted_frame,
              rtc::ArrayView<uint8_t> frame,
              size_t* bytes_written) override;

  size_t GetMaxPlaintextByteSize(cricket::MediaType media_type,
                                 size_t encrypted_frame_size) override;

  void SetFakeKey(uint8_t fake_key);

  uint8_t GetFakeKey() const;

  void SetExpectedPostfixByte(uint8_t expected_postfix_byte);

  uint8_t GetExpectedPostfixByte() const;

  void SetFailDecryption(bool fail_decryption);

 private:
  uint8_t fake_key_ = 0;
  uint8_t expected_postfix_byte_ = 0;
  bool fail_decryption_ = false;
};

}  // namespace webrtc

#endif  // API_TEST_FAKE_FRAME_DECRYPTOR_H_
