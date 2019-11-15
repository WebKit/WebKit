/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_TEST_ISACTEST_H_
#define MODULES_AUDIO_CODING_TEST_ISACTEST_H_

#include <string.h>

#include <memory>

#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/test/Channel.h"
#include "modules/audio_coding/test/PCMFile.h"

namespace webrtc {

struct ACMTestISACConfig {
  int32_t currentRateBitPerSec;
  int16_t currentFrameSizeMsec;
  int16_t encodingMode;
  uint32_t initRateBitPerSec;
  int16_t initFrameSizeInMsec;
  bool enforceFrameSize;
};

class ISACTest {
 public:
  ISACTest();
  ~ISACTest();

  void Perform();

 private:
  class ACMTestTimer {
   public:
    ACMTestTimer();
    ~ACMTestTimer();

    void Reset();
    void Tick10ms();
    void Tick1ms();
    void Tick100ms();
    void Tick1sec();
    void CurrentTimeHMS(char* currTime);
    void CurrentTime(unsigned long& h,
                     unsigned char& m,
                     unsigned char& s,
                     unsigned short& ms);

   private:
    void Adjust();

    unsigned short _msec;
    unsigned char _sec;
    unsigned char _min;
    unsigned long _hour;
  };

  void Setup();

  void Run10ms();

  void EncodeDecode(int testNr,
                    ACMTestISACConfig& wbISACConfig,
                    ACMTestISACConfig& swbISACConfig);

  void SwitchingSamplingRate(int testNr, int maxSampRateChange);

  std::unique_ptr<AudioCodingModule> _acmA;
  std::unique_ptr<AudioCodingModule> _acmB;

  std::unique_ptr<Channel> _channel_A2B;
  std::unique_ptr<Channel> _channel_B2A;

  PCMFile _inFileA;
  PCMFile _inFileB;

  PCMFile _outFileA;
  PCMFile _outFileB;

  std::string file_name_swb_;

  ACMTestTimer _myTimer;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_TEST_ISACTEST_H_
