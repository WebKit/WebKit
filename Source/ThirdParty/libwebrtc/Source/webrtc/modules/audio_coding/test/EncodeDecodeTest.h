/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_TEST_ENCODEDECODETEST_H_
#define MODULES_AUDIO_CODING_TEST_ENCODEDECODETEST_H_

#include <stdio.h>
#include <string.h>

#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/test/PCMFile.h"
#include "modules/audio_coding/test/RTPFile.h"
#include "modules/include/module_common_types.h"

namespace webrtc {

#define MAX_INCOMING_PAYLOAD 8096

// TestPacketization callback which writes the encoded payloads to file
class TestPacketization : public AudioPacketizationCallback {
 public:
  TestPacketization(RTPStream* rtpStream, uint16_t frequency);
  ~TestPacketization();
  int32_t SendData(const AudioFrameType frameType,
                   const uint8_t payloadType,
                   const uint32_t timeStamp,
                   const uint8_t* payloadData,
                   const size_t payloadSize,
                   int64_t absolute_capture_timestamp_ms) override;

 private:
  static void MakeRTPheader(uint8_t* rtpHeader,
                            uint8_t payloadType,
                            int16_t seqNo,
                            uint32_t timeStamp,
                            uint32_t ssrc);
  RTPStream* _rtpStream;
  int32_t _frequency;
  int16_t _seqNo;
};

class Sender {
 public:
  Sender();
  void Setup(AudioCodingModule* acm,
             RTPStream* rtpStream,
             std::string in_file_name,
             int in_sample_rate,
             int payload_type,
             SdpAudioFormat format);
  void Teardown();
  void Run();
  bool Add10MsData();

 protected:
  AudioCodingModule* _acm;

 private:
  PCMFile _pcmFile;
  AudioFrame _audioFrame;
  TestPacketization* _packetization;
};

class Receiver {
 public:
  Receiver();
  virtual ~Receiver() {}
  void Setup(AudioCodingModule* acm,
             RTPStream* rtpStream,
             std::string out_file_name,
             size_t channels,
             int file_num);
  void Teardown();
  void Run();
  virtual bool IncomingPacket();
  bool PlayoutData();

 private:
  PCMFile _pcmFile;
  int16_t* _playoutBuffer;
  uint16_t _playoutLengthSmpls;
  int32_t _frequency;
  bool _firstTime;

 protected:
  AudioCodingModule* _acm;
  uint8_t _incomingPayload[MAX_INCOMING_PAYLOAD];
  RTPStream* _rtpStream;
  RTPHeader _rtpHeader;
  size_t _realPayloadSizeBytes;
  size_t _payloadSizeBytes;
  uint32_t _nextTime;
};

class EncodeDecodeTest {
 public:
  EncodeDecodeTest();
  void Perform();
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_TEST_ENCODEDECODETEST_H_
