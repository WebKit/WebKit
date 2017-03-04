/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <memory>
#include <vector>

#include "gflags/gflags.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_neteq_stats.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"
#include "webrtc/voice_engine/test/channel_transport/channel_transport.h"

DEFINE_bool(use_log_file, false,
    "Output logs to a file; by default they will be printed to stderr.");

using namespace webrtc;
using namespace test;

#define VALIDATE                                           \
  if (res != 0) {                                          \
    printf("*** Error at line %i \n", __LINE__);           \
    printf("*** Error code = %i \n", base1->LastError());  \
  }

VoiceEngine* m_voe = NULL;
VoEBase* base1 = NULL;
VoECodec* codec = NULL;
VoEVolumeControl* volume = NULL;
VoERTP_RTCP* rtp_rtcp = NULL;
VoEAudioProcessing* apm = NULL;
VoENetwork* netw = NULL;
VoEFile* file = NULL;
VoEHardware* hardware = NULL;
VoENetEqStats* neteqst = NULL;

void RunTest(std::string out_path);

class MyObserver : public VoiceEngineObserver {
 public:
   virtual void CallbackOnError(int channel, int err_code);
};

void MyObserver::CallbackOnError(int channel, int err_code) {
  // Add printf for other error codes here
  if (err_code == VE_TYPING_NOISE_WARNING) {
    printf("  TYPING NOISE DETECTED \n");
  } else if (err_code == VE_TYPING_NOISE_OFF_WARNING) {
    printf("  TYPING NOISE OFF DETECTED \n");
  } else if (err_code == VE_RECEIVE_PACKET_TIMEOUT) {
    printf("  RECEIVE PACKET TIMEOUT \n");
  } else if (err_code == VE_PACKET_RECEIPT_RESTARTED) {
    printf("  PACKET RECEIPT RESTARTED \n");
  } else if (err_code == VE_RUNTIME_PLAY_WARNING) {
    printf("  RUNTIME PLAY WARNING \n");
  } else if (err_code == VE_RUNTIME_REC_WARNING) {
    printf("  RUNTIME RECORD WARNING \n");
  } else if (err_code == VE_RUNTIME_PLAY_ERROR) {
    printf("  RUNTIME PLAY ERROR \n");
  } else if (err_code == VE_RUNTIME_REC_ERROR) {
    printf("  RUNTIME RECORD ERROR \n");
  } else if (err_code == VE_REC_DEVICE_REMOVED) {
    printf("  RECORD DEVICE REMOVED \n");
  }
}

void SetStereoIfOpus(bool use_stereo, CodecInst* codec_params) {
  if (strncmp(codec_params->plname, "opus", 4) == 0) {
    if (use_stereo)
      codec_params->channels = 2;
    else
      codec_params->channels = 1;
  }
}

void PrintCodecs(bool opus_stereo) {
  CodecInst codec_params;
  for (int i = 0; i < codec->NumOfCodecs(); ++i) {
    int res = codec->GetCodec(i, codec_params);
    VALIDATE;
    SetStereoIfOpus(opus_stereo, &codec_params);
    printf("%2d. %3d  %s/%d/%" PRIuS " \n", i, codec_params.pltype,
           codec_params.plname, codec_params.plfreq, codec_params.channels);
  }
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  int res = 0;

  printf("Test started \n");

  m_voe = VoiceEngine::Create();
  base1 = VoEBase::GetInterface(m_voe);
  codec = VoECodec::GetInterface(m_voe);
  apm = VoEAudioProcessing::GetInterface(m_voe);
  volume = VoEVolumeControl::GetInterface(m_voe);
  rtp_rtcp = VoERTP_RTCP::GetInterface(m_voe);
  netw = VoENetwork::GetInterface(m_voe);
  file = VoEFile::GetInterface(m_voe);
  hardware = VoEHardware::GetInterface(m_voe);
  neteqst = VoENetEqStats::GetInterface(m_voe);

  MyObserver my_observer;

  std::unique_ptr<test::TraceToStderr> trace_to_stderr;
  if (!FLAGS_use_log_file) {
    trace_to_stderr.reset(new test::TraceToStderr);
  } else {
    const std::string trace_filename = test::OutputPath() + "webrtc_trace.txt";
    VoiceEngine::SetTraceFilter(kTraceAll);
    res = VoiceEngine::SetTraceFile(trace_filename.c_str());
    VALIDATE;
    res = VoiceEngine::SetTraceCallback(NULL);
    VALIDATE;
    printf("Outputting logs to file: %s\n", trace_filename.c_str());
  }

  printf("Init\n");
  res = base1->Init();
  if (res != 0) {
    printf("\nError calling Init: %d\n", base1->LastError());
    fflush(NULL);
    exit(1);
  }

  res = base1->RegisterVoiceEngineObserver(my_observer);
  VALIDATE;

  printf("Version\n");
  char tmp[1024];
  res = base1->GetVersion(tmp);
  VALIDATE;
  printf("%s\n", tmp);

  RunTest(test::OutputPath());

  printf("Terminate \n");

  base1->DeRegisterVoiceEngineObserver();

  res = base1->Terminate();
  VALIDATE;

  if (base1)
    base1->Release();

  if (codec)
    codec->Release();

  if (volume)
    volume->Release();

  if (rtp_rtcp)
    rtp_rtcp->Release();

  if (apm)
    apm->Release();

  if (netw)
    netw->Release();

  if (file)
    file->Release();

  if (hardware)
    hardware->Release();

  if (neteqst)
    neteqst->Release();

  VoiceEngine::Delete(m_voe);

  return 0;
}

void RunTest(std::string out_path) {
  int chan, res;
  CodecInst cinst;
  bool enable_aec = false;
  bool enable_agc = false;
  bool enable_cng = false;
  bool enable_ns = false;
  bool typing_detection = false;
  bool muted = false;
  bool opus_stereo = false;
  bool opus_dtx = false;
  bool experimental_ns_enabled = false;
  bool debug_recording_started = false;

  const std::string audio_filename =
      webrtc::test::ResourcePath("voice_engine/audio_long16", "pcm");

  const std::string play_filename = out_path + "recorded_playout.pcm";
  const std::string mic_filename = out_path + "recorded_mic.pcm";

  chan = base1->CreateChannel();
  if (chan < 0) {
    printf("************ Error code = %i\n", base1->LastError());
    fflush(NULL);
  }

  VoiceChannelTransport* voice_channel_transport(
      new VoiceChannelTransport(netw, chan));

  char ip[64];
  printf("1. 127.0.0.1 \n");
  printf("2. Specify IP \n");
  int ip_selection;
  ASSERT_EQ(1, scanf("%i", &ip_selection));

  if (ip_selection == 1) {
    strcpy(ip, "127.0.0.1");
  } else {
    printf("Specify remote IP: ");
    ASSERT_EQ(1, scanf("%s", ip));
  }

  int rPort;
  printf("Specify remote port (1=1234): ");
  ASSERT_EQ(1, scanf("%i", &rPort));
  if (1 == rPort)
    rPort = 1234;
  printf("Set Send port \n");

  printf("Set Send IP \n");
  res = voice_channel_transport->SetSendDestination(ip, rPort);
  VALIDATE;

  int lPort;
  printf("Specify local port (1=1234): ");
  ASSERT_EQ(1, scanf("%i", &lPort));
  if (1 == lPort)
    lPort = 1234;
  printf("Set Rec Port \n");

  res = voice_channel_transport->SetLocalReceiver(lPort);
  VALIDATE;

  printf("\n");
  PrintCodecs(opus_stereo);
  printf("Select send codec: ");
  int codec_selection;
  ASSERT_EQ(1, scanf("%i", &codec_selection));
  codec->GetCodec(codec_selection, cinst);

  printf("Set primary codec\n");
  SetStereoIfOpus(opus_stereo, &cinst);
  res = codec->SetSendCodec(chan, cinst);
  VALIDATE;

  const int kMaxNumChannels = 8;
  int channel_index = 0;
  std::vector<int> channels(kMaxNumChannels);
  std::vector<VoiceChannelTransport*> voice_channel_transports(kMaxNumChannels);

  for (int i = 0; i < kMaxNumChannels; ++i) {
    channels[i] = base1->CreateChannel();
    int port = rPort + (i + 1) * 2;

    voice_channel_transports[i] = new VoiceChannelTransport(netw, channels[i]);

    res = voice_channel_transports[i]->SetSendDestination(ip, port);
    VALIDATE;
    res = voice_channel_transports[i]->SetLocalReceiver(port);
    VALIDATE;
    res = codec->SetSendCodec(channels[i], cinst);
    VALIDATE;
  }

  // Call loop
  bool newcall = true;
  while (newcall) {
    int rd(-1), pd(-1);
    res = hardware->GetNumOfRecordingDevices(rd);
    VALIDATE;
    res = hardware->GetNumOfPlayoutDevices(pd);
    VALIDATE;

    char dn[128] = { 0 };
    char guid[128] = { 0 };
    printf("\nPlayout devices (%d): \n", pd);
    for (int j = 0; j < pd; ++j) {
      res = hardware->GetPlayoutDeviceName(j, dn, guid);
      VALIDATE;
      printf("  %d: %s \n", j, dn);
    }

    printf("Recording devices (%d): \n", rd);
    for (int j = 0; j < rd; ++j) {
      res = hardware->GetRecordingDeviceName(j, dn, guid);
      VALIDATE;
      printf("  %d: %s \n", j, dn);
    }

    printf("Select playout device: ");
    ASSERT_EQ(1, scanf("%d", &pd));
    res = hardware->SetPlayoutDevice(pd);
    VALIDATE;
    printf("Select recording device: ");
    ASSERT_EQ(1, scanf("%d", &rd));
    printf("Setting sound devices \n");
    res = hardware->SetRecordingDevice(rd);
    VALIDATE;

    res = codec->SetVADStatus(0, enable_cng);
    VALIDATE;

    res = apm->SetAgcStatus(enable_agc);
    VALIDATE;

    res = apm->SetEcStatus(enable_aec);
    VALIDATE;

    res = apm->SetNsStatus(enable_ns);
    VALIDATE;

    printf("\n1. Send, listen and playout \n");
    printf("2. Send only \n");
    printf("3. Listen and playout only \n");
    printf("Select transfer mode: ");
    int call_selection;
    ASSERT_EQ(1, scanf("%i", &call_selection));
    const bool send = !(call_selection == 3);
    const bool receive = !(call_selection == 2);

    if (receive) {
      printf("Start Playout \n");
      res = base1->StartPlayout(chan);
      VALIDATE;
    }

    if (send) {
      printf("Start Send \n");
      res = base1->StartSend(chan);
      VALIDATE;
    }

    printf("Getting mic volume \n");
    unsigned int vol = 999;
    res = volume->GetMicVolume(vol);
    VALIDATE;
    if ((vol > 255) || (vol < 1)) {
      printf("\n****ERROR in GetMicVolume");
    }

    int forever = 1;
    while (forever) {
      printf("\nSelect codec\n");
      PrintCodecs(opus_stereo);
      printf("\nOther actions\n");
      const int num_codecs = codec->NumOfCodecs();
      int option_index = num_codecs;
      printf("%i. Toggle CNG\n", option_index++);
      printf("%i. Toggle AGC\n", option_index++);
      printf("%i. Toggle NS\n", option_index++);
      printf("%i. Toggle experimental NS\n", option_index++);
      printf("%i. Toggle EC\n", option_index++);
      printf("%i. Select AEC\n", option_index++);
      printf("%i. Select AECM\n", option_index++);
      printf("%i. Get speaker volume\n", option_index++);
      printf("%i. Set speaker volume\n", option_index++);
      printf("%i. Get microphone volume\n", option_index++);
      printf("%i. Set microphone volume\n", option_index++);
      printf("%i. Play local file (audio_long16.pcm) \n", option_index++);
      printf("%i. Change playout device \n", option_index++);
      printf("%i. Change recording device \n", option_index++);
      printf("%i. Toggle receive-side AGC \n", option_index++);
      printf("%i. Toggle receive-side NS \n", option_index++);
      printf("%i. AGC status \n", option_index++);
      printf("%i. Toggle microphone mute \n", option_index++);
      printf("%i. Get last error code \n", option_index++);
      printf("%i. Toggle typing detection \n",
             option_index++);
      printf("%i. Record a PCM file \n", option_index++);
      printf("%i. Play a previously recorded PCM file locally \n",
             option_index++);
      printf("%i. Play a previously recorded PCM file as microphone \n",
             option_index++);
      printf("%i. Add an additional file-playing channel \n", option_index++);
      printf("%i. Remove a file-playing channel \n", option_index++);
      printf("%i. Toggle Opus stereo (Opus must be selected again to apply "
             "the setting) \n", option_index++);
      printf("%i. Set Opus maximum playback rate \n", option_index++);
      printf("%i. Toggle Opus DTX \n", option_index++);
      printf("%i. Set bit rate (only take effect on codecs that allow the "
             "change) \n", option_index++);
      printf("%i. Toggle AECdump recording \n", option_index++);

      printf("Select action or %i to stop the call: ", option_index);
      int option_selection;
      ASSERT_EQ(1, scanf("%i", &option_selection));

      option_index = num_codecs;
      if (option_selection < option_index) {
        res = codec->GetCodec(option_selection, cinst);
        VALIDATE;
        SetStereoIfOpus(opus_stereo, &cinst);
        printf("Set primary codec\n");
        res = codec->SetSendCodec(chan, cinst);
        VALIDATE;
      } else if (option_selection == option_index++) {
        enable_cng = !enable_cng;
        res = codec->SetVADStatus(0, enable_cng);
        VALIDATE;
        if (enable_cng)
          printf("\n CNG is now on! \n");
        else
          printf("\n CNG is now off! \n");
      } else if (option_selection == option_index++) {
        enable_agc = !enable_agc;
        res = apm->SetAgcStatus(enable_agc);
        VALIDATE;
        if (enable_agc)
          printf("\n AGC is now on! \n");
        else
          printf("\n AGC is now off! \n");
      } else if (option_selection == option_index++) {
        enable_ns = !enable_ns;
        res = apm->SetNsStatus(enable_ns);
        VALIDATE;
        if (enable_ns)
          printf("\n NS is now on! \n");
        else
          printf("\n NS is now off! \n");
      } else if (option_selection == option_index++) {
        experimental_ns_enabled = !experimental_ns_enabled;
        Config config;
        config.Set<ExperimentalNs>(new ExperimentalNs(experimental_ns_enabled));
        base1->audio_processing()->SetExtraOptions(config);
        if (experimental_ns_enabled) {
          printf("\n Experimental NS is now on!\n");
        } else {
          printf("\n Experimental NS is now off!\n");
        }
      } else if (option_selection == option_index++) {
        enable_aec = !enable_aec;
        res = apm->SetEcStatus(enable_aec, kEcUnchanged);
        VALIDATE;
        if (enable_aec)
          printf("\n Echo control is now on! \n");
        else
          printf("\n Echo control is now off! \n");
      } else if (option_selection == option_index++) {
        res = apm->SetEcStatus(enable_aec, kEcAec);
        VALIDATE;
        printf("\n AEC selected! \n");
        if (enable_aec)
          printf(" (Echo control is on)\n");
        else
          printf(" (Echo control is off)\n");
      } else if (option_selection == option_index++) {
        res = apm->SetEcStatus(enable_aec, kEcAecm);
        VALIDATE;
        printf("\n AECM selected! \n");
        if (enable_aec)
          printf(" (Echo control is on)\n");
        else
          printf(" (Echo control is off)\n");
      } else if (option_selection == option_index++) {
        unsigned vol(0);
        res = volume->GetSpeakerVolume(vol);
        VALIDATE;
        printf("\n Speaker Volume is %d \n", vol);
      } else if (option_selection == option_index++) {
        printf("Level: ");
        int level;
        ASSERT_EQ(1, scanf("%i", &level));
        res = volume->SetSpeakerVolume(level);
        VALIDATE;
      } else if (option_selection == option_index++) {
        unsigned vol(0);
        res = volume->GetMicVolume(vol);
        VALIDATE;
        printf("\n Microphone Volume is %d \n", vol);
      } else if (option_selection == option_index++) {
        printf("Level: ");
        int level;
        ASSERT_EQ(1, scanf("%i", &level));
        res = volume->SetMicVolume(level);
        VALIDATE;
      } else if (option_selection == option_index++) {
        res = file->StartPlayingFileLocally(0, audio_filename.c_str());
        VALIDATE;
      } else if (option_selection == option_index++) {
        // change the playout device with current call
        int num_pd(-1);
        res = hardware->GetNumOfPlayoutDevices(num_pd);
        VALIDATE;

        char dn[128] = { 0 };
        char guid[128] = { 0 };

        printf("\nPlayout devices (%d): \n", num_pd);
        for (int i = 0; i < num_pd; ++i) {
          res = hardware->GetPlayoutDeviceName(i, dn, guid);
          VALIDATE;
          printf("  %d: %s \n", i, dn);
        }
        printf("Select playout device: ");
        ASSERT_EQ(1, scanf("%d", &num_pd));
        // Will use plughw for hardware devices
        res = hardware->SetPlayoutDevice(num_pd);
        VALIDATE;
      } else if (option_selection == option_index++) {
        // change the recording device with current call
        int num_rd(-1);

        res = hardware->GetNumOfRecordingDevices(num_rd);
        VALIDATE;

        char dn[128] = { 0 };
        char guid[128] = { 0 };

        printf("Recording devices (%d): \n", num_rd);
        for (int i = 0; i < num_rd; ++i) {
          res = hardware->GetRecordingDeviceName(i, dn, guid);
          VALIDATE;
          printf("  %d: %s \n", i, dn);
        }

        printf("Select recording device: ");
        ASSERT_EQ(1, scanf("%d", &num_rd));
        printf("Setting sound devices \n");
        // Will use plughw for hardware devices
        res = hardware->SetRecordingDevice(num_rd);
        VALIDATE;
      } else if (option_selection == option_index++) {
        AgcModes agcmode;
        bool enable;
        res = apm->GetAgcStatus(enable, agcmode);
        VALIDATE
            printf("\n AGC enable is %d, mode is %d \n", enable, agcmode);
      } else if (option_selection == option_index++) {
        // Toggle Mute on Microphone
        res = volume->GetInputMute(chan, muted);
        VALIDATE;
        muted = !muted;
        res = volume->SetInputMute(chan, muted);
        VALIDATE;
        if (muted)
          printf("\n Microphone is now on mute! \n");
        else
          printf("\n Microphone is no longer on mute! \n");
      } else if (option_selection == option_index++) {
        // Get the last error code and print to screen
        int err_code = 0;
        err_code = base1->LastError();
        if (err_code != -1)
          printf("\n The last error code was %i.\n", err_code);
      } else if (option_selection == option_index++) {
        typing_detection= !typing_detection;
        res = apm->SetTypingDetectionStatus(typing_detection);
        VALIDATE;
        if (typing_detection)
          printf("\n Typing detection is now on!\n");
        else
          printf("\n Typing detection is now off!\n");
      } else if (option_selection == option_index++) {
        int stop_record = 1;
        int file_source = 1;
        printf("\n Select source of recorded file. ");
        printf("\n 1. Record from microphone to file ");
        printf("\n 2. Record from playout to file ");
        printf("\n Enter your selection: \n");
        ASSERT_EQ(1, scanf("%i", &file_source));
        if (file_source == 1) {
          printf("\n Start recording microphone as %s \n",
                 mic_filename.c_str());
          res = file->StartRecordingMicrophone(mic_filename.c_str());
          VALIDATE;
        } else {
          printf("\n Start recording playout as %s \n", play_filename.c_str());
          res = file->StartRecordingPlayout(chan, play_filename.c_str());
          VALIDATE;
        }
        while (stop_record != 0) {
          printf("\n Type 0 to stop recording file \n");
          ASSERT_EQ(1, scanf("%i", &stop_record));
        }
        if (file_source == 1) {
          res = file->StopRecordingMicrophone();
          VALIDATE;
        } else {
          res = file->StopRecordingPlayout(chan);
          VALIDATE;
        }
        printf("\n File finished recording \n");
      } else if (option_selection == option_index++) {
        int file_type = 1;
        int stop_play = 1;
        printf("\n Select a file to play locally in a loop.");
        printf("\n 1. Play %s", mic_filename.c_str());
        printf("\n 2. Play %s", play_filename.c_str());
        printf("\n Enter your selection\n");
        ASSERT_EQ(1, scanf("%i", &file_type));
        if (file_type == 1)  {
          printf("\n Start playing %s locally in a loop\n",
                 mic_filename.c_str());
          res = file->StartPlayingFileLocally(chan, mic_filename.c_str(), true);
          VALIDATE;
        } else {
          printf("\n Start playing %s locally in a loop\n",
                 play_filename.c_str());
          res = file->StartPlayingFileLocally(chan, play_filename.c_str(),
                                              true);
          VALIDATE;
        }
        while (stop_play != 0) {
          printf("\n Type 0 to stop playing file\n");
          ASSERT_EQ(1, scanf("%i", &stop_play));
        }
        res = file->StopPlayingFileLocally(chan);
        VALIDATE;
      } else if (option_selection == option_index++) {
        int file_type = 1;
        int stop_play = 1;
        printf("\n Select a file to play as microphone in a loop.");
        printf("\n 1. Play %s", mic_filename.c_str());
        printf("\n 2. Play %s", play_filename.c_str());
        printf("\n Enter your selection\n");
        ASSERT_EQ(1, scanf("%i", &file_type));
        if (file_type == 1)  {
          printf("\n Start playing %s as mic in a loop\n",
                 mic_filename.c_str());
          res = file->StartPlayingFileAsMicrophone(chan, mic_filename.c_str(),
                                                   true);
          VALIDATE;
        } else {
          printf("\n Start playing %s as mic in a loop\n",
                 play_filename.c_str());
          res = file->StartPlayingFileAsMicrophone(chan, play_filename.c_str(),
                                                   true);
          VALIDATE;
        }
        while (stop_play != 0) {
          printf("\n Type 0 to stop playing file\n");
          ASSERT_EQ(1, scanf("%i", &stop_play));
        }
        res = file->StopPlayingFileAsMicrophone(chan);
        VALIDATE;
      } else if (option_selection == option_index++) {
        if (channel_index < kMaxNumChannels) {
          res = base1->StartPlayout(channels[channel_index]);
          VALIDATE;
          res = base1->StartSend(channels[channel_index]);
          VALIDATE;
          res = file->StartPlayingFileAsMicrophone(channels[channel_index],
                                                   audio_filename.c_str(),
                                                   true,
                                                   false);
          VALIDATE;
          channel_index++;
          printf("Using %d additional channels\n", channel_index);
        } else {
          printf("Max number of channels reached\n");
        }
      } else if (option_selection == option_index++) {
        if (channel_index > 0) {
          channel_index--;
          res = file->StopPlayingFileAsMicrophone(channels[channel_index]);
          VALIDATE;
          res = base1->StopSend(channels[channel_index]);
          VALIDATE;
          res = base1->StopPlayout(channels[channel_index]);
          VALIDATE;
          printf("Using %d additional channels\n", channel_index);
        } else {
          printf("All additional channels stopped\n");
        }
      } else if (option_selection == option_index++) {
        opus_stereo = !opus_stereo;
        if (opus_stereo)
          printf("\n Opus stereo enabled (select Opus again to apply the "
                 "setting). \n");
        else
          printf("\n Opus mono enabled (select Opus again to apply the "
                 "setting). \n");
      } else if (option_selection == option_index++) {
        printf("\n Input maxium playback rate in Hz: ");
        int max_playback_rate;
        ASSERT_EQ(1, scanf("%i", &max_playback_rate));
        res = codec->SetOpusMaxPlaybackRate(chan, max_playback_rate);
        VALIDATE;
      } else if (option_selection == option_index++) {
        opus_dtx = !opus_dtx;
        res = codec->SetOpusDtx(chan, opus_dtx);
        VALIDATE;
        printf("Opus DTX %s.\n", opus_dtx ? "enabled" : "disabled");
      } else if (option_selection == option_index++) {
        res = codec->GetSendCodec(chan, cinst);
        VALIDATE;
        printf("Current bit rate is %i bps, set to: ", cinst.rate);
        int new_bitrate_bps;
        ASSERT_EQ(1, scanf("%i", &new_bitrate_bps));
        res = codec->SetBitRate(chan, new_bitrate_bps);
        VALIDATE;
      } else if (option_selection == option_index++) {
        const char* kDebugFileName = "audio.aecdump";
        if (debug_recording_started) {
          apm->StopDebugRecording();
          printf("Debug recording named %s stopped\n", kDebugFileName);
        } else {
          apm->StartDebugRecording(kDebugFileName);
          printf("Debug recording named %s started\n", kDebugFileName);
        }
        debug_recording_started = !debug_recording_started;
      } else {
        break;
      }
    }

    if (debug_recording_started) {
      apm->StopDebugRecording();
    }

    if (send) {
      printf("Stop Send \n");
      res = base1->StopSend(chan);
      VALIDATE;
    }

    if (receive) {
      printf("Stop Playout \n");
      res = base1->StopPlayout(chan);
      VALIDATE;
    }

    while (channel_index > 0) {
      --channel_index;
      res = file->StopPlayingFileAsMicrophone(channels[channel_index]);
      VALIDATE;
      res = base1->StopSend(channels[channel_index]);
      VALIDATE;
      res = base1->StopPlayout(channels[channel_index]);
      VALIDATE;
    }

    printf("\n1. New call \n");
    printf("2. Quit \n");
    printf("Select action: ");
    int end_option;
    ASSERT_EQ(1, scanf("%i", &end_option));
    newcall = (end_option == 1);
    // Call loop
  }

  // Transports should be deleted before channel deletion.
  delete voice_channel_transport;
  for (int i = 0; i < kMaxNumChannels; ++i) {
    delete voice_channel_transports[i];
    voice_channel_transports[i] = NULL;
  }

  printf("Delete channels \n");
  res = base1->DeleteChannel(chan);
  VALIDATE;

  for (int i = 0; i < kMaxNumChannels; ++i) {
    res = base1->DeleteChannel(channels[i]);
    VALIDATE;
  }
}
