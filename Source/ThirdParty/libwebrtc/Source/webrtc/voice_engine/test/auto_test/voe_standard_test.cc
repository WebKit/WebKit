/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/voice_engine/include/voe_neteq_stats.h"
#include "webrtc/voice_engine/test/auto_test/automated_mode.h"
#include "webrtc/voice_engine/test/auto_test/voe_cpu_test.h"
#include "webrtc/voice_engine/test/auto_test/voe_stress_test.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_defines.h"
#include "webrtc/voice_engine/voice_engine_defines.h"
#include "webrtc/voice_engine_configurations.h"

DEFINE_bool(include_timing_dependent_tests, true,
            "If true, we will include tests / parts of tests that are known "
            "to break in slow execution environments (such as valgrind).");
DEFINE_bool(automated, false,
            "If true, we'll run the automated tests we have in noninteractive "
            "mode.");

using namespace webrtc;

namespace voetest {

int dummy = 0;  // Dummy used in different functions to avoid warnings

void SubAPIManager::DisplayStatus() const {
  TEST_LOG("Supported sub APIs:\n\n");
  if (_base)
    TEST_LOG("  Base\n");
  if (_codec)
    TEST_LOG("  Codec\n");
  if (_externalMedia)
    TEST_LOG("  ExternalMedia\n");
  if (_file)
    TEST_LOG("  File\n");
  if (_hardware)
    TEST_LOG("  Hardware\n");
  if (_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (_network)
    TEST_LOG("  Network\n");
  if (_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (_videoSync)
    TEST_LOG("  VideoSync\n");
  if (_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
  TEST_LOG("Excluded sub APIs:\n\n");
  if (!_base)
    TEST_LOG("  Base\n");
  if (!_codec)
    TEST_LOG("  Codec\n");
  if (!_externalMedia)
    TEST_LOG("  ExternamMedia\n");
  if (!_file)
    TEST_LOG("  File\n");
  if (!_hardware)
    TEST_LOG("  Hardware\n");
  if (!_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (!_network)
    TEST_LOG("  Network\n");
  if (!_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (!_videoSync)
    TEST_LOG("  VideoSync\n");
  if (!_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (!_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
}

VoETestManager::VoETestManager()
    : initialized_(false),
      voice_engine_(NULL),
      voe_base_(0),
      voe_codec_(0),
      voe_xmedia_(0),
      voe_file_(0),
      voe_hardware_(0),
      voe_network_(0),
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
      voe_neteq_stats_(NULL),
#endif
      voe_rtp_rtcp_(0),
      voe_vsync_(0),
      voe_volume_control_(0),
      voe_apm_(0) {
}

VoETestManager::~VoETestManager() {
}

bool VoETestManager::Init() {
  if (initialized_)
    return true;

  voice_engine_ = VoiceEngine::Create();
  if (!voice_engine_) {
    TEST_LOG("Failed to create VoiceEngine\n");
    return false;
  }

  return true;
}

void VoETestManager::GetInterfaces() {
  if (voice_engine_) {
    voe_base_ = VoEBase::GetInterface(voice_engine_);
    voe_codec_ = VoECodec::GetInterface(voice_engine_);
    voe_volume_control_ = VoEVolumeControl::GetInterface(voice_engine_);
    voe_rtp_rtcp_ = VoERTP_RTCP::GetInterface(voice_engine_);
    voe_apm_ = VoEAudioProcessing::GetInterface(voice_engine_);
    voe_network_ = VoENetwork::GetInterface(voice_engine_);
    voe_file_ = VoEFile::GetInterface(voice_engine_);
#ifdef _TEST_VIDEO_SYNC_
    voe_vsync_ = VoEVideoSync::GetInterface(voice_engine_);
#endif
    voe_hardware_ = VoEHardware::GetInterface(voice_engine_);
    // Set the audio layer to use in all tests
    if (voe_hardware_) {
      int res = voe_hardware_->SetAudioDeviceLayer(TESTED_AUDIO_LAYER);
      if (res < 0) {
        printf("\nERROR: failed to set audio layer to use in "
          "testing\n");
      } else {
        printf("\nAudio layer %d will be used in testing\n",
               TESTED_AUDIO_LAYER);
      }
    }
#ifdef _TEST_XMEDIA_
    voe_xmedia_ = VoEExternalMedia::GetInterface(voice_engine_);
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
    voe_neteq_stats_ = VoENetEqStats::GetInterface(voice_engine_);
#endif
  }
}

int VoETestManager::ReleaseInterfaces() {
  bool releaseOK(true);

  if (voe_base_) {
    voe_base_->Release();
    voe_base_ = NULL;
  }
  if (voe_codec_) {
    voe_codec_->Release();
    voe_codec_ = NULL;
  }
  if (voe_volume_control_) {
    voe_volume_control_->Release();
    voe_volume_control_ = NULL;
  }
  if (voe_rtp_rtcp_) {
    voe_rtp_rtcp_->Release();
    voe_rtp_rtcp_ = NULL;
  }
  if (voe_apm_) {
    voe_apm_->Release();
    voe_apm_ = NULL;
  }
  if (voe_network_) {
    voe_network_->Release();
    voe_network_ = NULL;
  }
  if (voe_file_) {
    voe_file_->Release();
    voe_file_ = NULL;
  }
#ifdef _TEST_VIDEO_SYNC_
  if (voe_vsync_) {
    voe_vsync_->Release();
    voe_vsync_ = NULL;
  }
#endif
  if (voe_hardware_) {
    voe_hardware_->Release();
    voe_hardware_ = NULL;
  }
#ifdef _TEST_XMEDIA_
  if (voe_xmedia_) {
    voe_xmedia_->Release();
    voe_xmedia_ = NULL;
  }
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
  if (voe_neteq_stats_) {
    voe_neteq_stats_->Release();
    voe_neteq_stats_ = NULL;
  }
#endif
  if (false == VoiceEngine::Delete(voice_engine_)) {
    TEST_LOG("\n\nVoiceEngine::Delete() failed. \n");
    releaseOK = false;
  }

  return (releaseOK == true) ? 0 : -1;
}

int run_auto_test(TestType test_type) {
  assert(test_type != Standard);

  SubAPIManager api_manager;
  api_manager.DisplayStatus();

  ////////////////////////////////////
  // Create VoiceEngine and sub API:s

  voetest::VoETestManager test_manager;
  if (!test_manager.Init()) {
    return -1;
  }
  test_manager.GetInterfaces();

  int result = -1;
  if (test_type == Stress) {
    VoEStressTest stressTest(test_manager);
    result = stressTest.DoTest();
  } else if (test_type == CPU) {
    VoECpuTest cpuTest(test_manager);
    result = cpuTest.DoTest();
  } else {
    // Should never end up here
    assert(false);
  }

  //////////////////
  // Release/Delete

  int release_ok = test_manager.ReleaseInterfaces();

  if ((0 == result) && (release_ok != -1)) {
    TEST_LOG("\n\n*** All tests passed *** \n\n");
  } else {
    TEST_LOG("\n\n*** Test failed! *** \n");
  }

  return 0;
}
}  // namespace voetest

int RunInManualMode() {
  using namespace voetest;

  SubAPIManager api_manager;
  api_manager.DisplayStatus();

  printf("----------------------------\n");
  printf("Select type of test\n\n");
  printf(" (0)  Quit\n");
  printf(" (1)  Standard test\n");
  printf(" (2)  [OBSOLETE: Extended test(s)...]\n");
  printf(" (3)  Stress test(s)...\n");
  printf(" (4)  [OBSOLETE: Unit test(s)...]\n");
  printf(" (5)  CPU & memory reference test [Windows]...\n");
  printf("\n: ");

  int selection(0);
  dummy = scanf("%d", &selection);

  TestType test_type = Invalid;
  switch (selection) {
    case 0:
      return 0;
    case 1:
      test_type = Standard;
      break;
    case 2:
      break;
    case 3:
      test_type = Stress;
      break;
    case 4:
      break;
    case 5:
      test_type = CPU;
      break;
    default:
      TEST_LOG("Invalid selection!\n");
      return 0;
  }

  if (test_type == Standard) {
    TEST_LOG("\n\n+++ Running standard tests +++\n\n");

    // Currently, all googletest-rewritten tests are in the "automated" suite.
    return RunInAutomatedMode();
  }

  // Function that can be called from other entry functions.
  return run_auto_test(test_type);
}

// ----------------------------------------------------------------------------
//                                       main
// ----------------------------------------------------------------------------

#if !defined(WEBRTC_IOS)
int main(int argc, char** argv) {
  // This function and RunInAutomatedMode is defined in automated_mode.cc
  // to avoid macro clashes with googletest (for instance ASSERT_TRUE).
  InitializeGoogleTest(&argc, argv);
  // AllowCommandLineParsing allows us to ignore flags passed on to us by
  // Chromium build bots without having to explicitly disable them.
  google::AllowCommandLineReparsing();
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_automated) {
    return RunInAutomatedMode();
  }

  return RunInManualMode();
}
#endif //#if !defined(WEBRTC_IOS)
