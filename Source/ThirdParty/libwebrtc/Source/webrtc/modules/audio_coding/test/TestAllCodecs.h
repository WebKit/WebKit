/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_TEST_TESTALLCODECS_H_
#define MODULES_AUDIO_CODING_TEST_TESTALLCODECS_H_

#include <memory>

#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/test/PCMFile.h"

namespace webrtc {

class TestPack : public AudioPacketizationCallback {
 public:
  TestPack();
  ~TestPack();

  void RegisterReceiverACM(AudioCodingModule* acm);

  int32_t SendData(AudioFrameType frame_type,
                   uint8_t payload_type,
                   uint32_t timestamp,
                   const uint8_t* payload_data,
                   size_t payload_size,
                   int64_t absolute_capture_timestamp_ms) override;

  size_t payload_size();
  uint32_t timestamp_diff();
  void reset_payload_size();

 private:
  AudioCodingModule* receiver_acm_;
  uint16_t sequence_number_;
  uint8_t payload_data_[60 * 32 * 2 * 2];
  uint32_t timestamp_diff_;
  uint32_t last_in_timestamp_;
  uint64_t total_bytes_;
  size_t payload_size_;
};

class TestAllCodecs {
 public:
  TestAllCodecs();
  ~TestAllCodecs();

  void Perform();

 private:
  // The default value of '-1' indicates that the registration is based only on
  // codec name, and a sampling frequency matching is not required.
  // This is useful for codecs which support several sampling frequency.
  // Note! Only mono mode is tested in this test.
  void RegisterSendCodec(char side,
                         char* codec_name,
                         int32_t sampling_freq_hz,
                         int rate,
                         int packet_size,
                         size_t extra_byte);

  void Run(TestPack* channel);
  void OpenOutFile(int test_number);

  std::unique_ptr<AudioCodingModule> acm_a_;
  std::unique_ptr<AudioCodingModule> acm_b_;
  TestPack* channel_a_to_b_;
  PCMFile infile_a_;
  PCMFile outfile_b_;
  int test_count_;
  int packet_size_samples_;
  size_t packet_size_bytes_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_TEST_TESTALLCODECS_H_
