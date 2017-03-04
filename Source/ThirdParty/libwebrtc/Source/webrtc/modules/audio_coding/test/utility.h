/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_TEST_UTILITY_H_
#define WEBRTC_MODULES_AUDIO_CODING_TEST_UTILITY_H_

#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

//-----------------------------
#define CHECK_ERROR(f)                                                         \
  do {                                                                         \
    EXPECT_GE(f, 0) << "Error Calling API";                                    \
  } while(0)

//-----------------------------
#define CHECK_PROTECTED(f)                                                     \
  do {                                                                         \
    if (f >= 0) {                                                              \
      ADD_FAILURE() << "Error Calling API";                                    \
    } else {                                                                   \
      printf("An expected error is caught.\n");                                \
    }                                                                          \
  } while(0)

//----------------------------
#define CHECK_ERROR_MT(f)                                                      \
  do {                                                                         \
    if (f < 0) {                                                               \
      fprintf(stderr, "Error Calling API in file %s at line %d \n",            \
              __FILE__, __LINE__);                                             \
    }                                                                          \
  } while(0)

//----------------------------
#define CHECK_PROTECTED_MT(f)                                                  \
  do {                                                                         \
    if (f >= 0) {                                                              \
      fprintf(stderr, "Error Calling API in file %s at line %d \n",            \
              __FILE__, __LINE__);                                             \
    } else {                                                                   \
      printf("An expected error is caught.\n");                                \
    }                                                                          \
  } while(0)

#define DELETE_POINTER(p)                                                      \
  do {                                                                         \
    if (p != NULL) {                                                           \
      delete p;                                                                \
      p = NULL;                                                                \
    }                                                                          \
  } while(0)

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
  void CurrentTime(unsigned long& h, unsigned char& m, unsigned char& s,
                   unsigned short& ms);

 private:
  void Adjust();

  unsigned short _msec;
  unsigned char _sec;
  unsigned char _min;
  unsigned long _hour;
};

// To avoid clashes with CircularBuffer in APM.
namespace test {

class CircularBuffer {
 public:
  CircularBuffer(uint32_t len);
  ~CircularBuffer();

  void SetArithMean(bool enable);
  void SetVariance(bool enable);

  void Update(const double newVal);
  void IsBufferFull();

  int16_t Variance(double& var);
  int16_t ArithMean(double& mean);

 protected:
  double* _buff;
  uint32_t _idx;
  uint32_t _buffLen;

  bool _buffIsFull;
  bool _calcAvg;
  bool _calcVar;
  double _sum;
  double _sumSqr;
};

}  // namespace test

int16_t ChooseCodec(CodecInst& codecInst);

void PrintCodecs();

bool FixedPayloadTypeCodec(const char* payloadName);

class VADCallback : public ACMVADCallback {
 public:
  VADCallback();

  int32_t InFrameType(FrameType frame_type) override;

  void PrintFrameTypes();
  void Reset();

 private:
  uint32_t _numFrameTypes[5];
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_TEST_UTILITY_H_
