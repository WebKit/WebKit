/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/voe_cpu_test.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <conio.h>
#endif

#include <memory>

#include "webrtc/voice_engine/test/channel_transport/channel_transport.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_defines.h"

using namespace webrtc;
using namespace test;

namespace voetest {

#define CHECK(expr)                                             \
    if (expr)                                                   \
    {                                                           \
        printf("Error at line: %i, %s \n", __LINE__, #expr);    \
        printf("Error code: %i \n", base->LastError());  \
        PAUSE												    \
        return -1;                                              \
    }

VoECpuTest::VoECpuTest(VoETestManager& mgr)
    : _mgr(mgr) {

}

int VoECpuTest::DoTest() {
  printf("------------------------------------------------\n");
  printf(" CPU Reference Test\n");
  printf("------------------------------------------------\n");

  VoEBase* base = _mgr.BasePtr();
  VoEFile* file = _mgr.FilePtr();
  VoECodec* codec = _mgr.CodecPtr();
  VoEAudioProcessing* apm = _mgr.APMPtr();
  VoENetwork* voe_network = _mgr.NetworkPtr();

  int channel(-1);
  CodecInst isac;

  isac.pltype = 104;
  strcpy(isac.plname, "ISAC");
  isac.pacsize = 960;
  isac.plfreq = 32000;
  isac.channels = 1;
  isac.rate = -1;

  CHECK(base->Init());
  channel = base->CreateChannel();

  std::unique_ptr<VoiceChannelTransport> voice_socket_transport(
      new VoiceChannelTransport(voe_network, channel));

  CHECK(voice_socket_transport->SetSendDestination("127.0.0.1", 5566));
  CHECK(voice_socket_transport->SetLocalReceiver(5566));

  CHECK(codec->SetRecPayloadType(channel, isac));
  CHECK(codec->SetSendCodec(channel, isac));

  CHECK(base->StartPlayout(channel));
  CHECK(base->StartSend(channel));
  CHECK(file->StartPlayingFileAsMicrophone(channel, _mgr.AudioFilename(),
          true, true));

  CHECK(codec->SetVADStatus(channel, true));
  CHECK(apm->SetAgcStatus(true, kAgcAdaptiveAnalog));
  CHECK(apm->SetNsStatus(true, kNsModerateSuppression));
  CHECK(apm->SetEcStatus(true, kEcAec));

  TEST_LOG("\nMeasure CPU and memory while running a full-duplex"
    " iSAC-swb call.\n\n");

  PAUSE

  CHECK(base->StopSend(channel));
  CHECK(base->StopPlayout(channel));

  base->DeleteChannel(channel);
  CHECK(base->Terminate());
  return 0;
}

}  // namespace voetest
