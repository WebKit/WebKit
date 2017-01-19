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
#include <string>

#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_initialization_fixture.h"

namespace webrtc {
namespace {

const int16_t kLimiterHeadroom = 29204;  // == -1 dbFS
const int16_t kInt16Max = 0x7fff;
const int kPayloadType = 105;
const int kInSampleRateHz = 16000;  // Input file taken as 16 kHz by default.
const int kRecSampleRateHz = 16000;  // Recorded with 16 kHz L16.
const int kTestDurationMs = 3000;
const CodecInst kCodecL16 = {kPayloadType, "L16", 16000, 160, 1, 256000};
const CodecInst kCodecOpus = {kPayloadType, "opus", 48000, 960, 1, 32000};

}  // namespace

class MixingTest : public AfterInitializationFixture {
 protected:
  MixingTest()
      : output_filename_(test::OutputPath() + "mixing_test_output.pcm") {
  }
  void SetUp() {
    transport_ = new LoopBackTransport(voe_network_, 0);
  }
  void TearDown() {
    delete transport_;
  }

  // Creates and mixes |num_remote_streams| which play a file "as microphone"
  // with |num_local_streams| which play a file "locally", using a constant
  // amplitude of |input_value|. The local streams manifest as "anonymous"
  // mixing participants, meaning they will be mixed regardless of the number
  // of participants. (A stream is a VoiceEngine "channel").
  //
  // The mixed output is verified to always fall between |max_output_value| and
  // |min_output_value|, after a startup phase.
  //
  // |num_remote_streams_using_mono| of the remote streams use mono, with the
  // remainder using stereo.
  void RunMixingTest(int num_remote_streams,
                     int num_local_streams,
                     int num_remote_streams_using_mono,
                     bool real_audio,
                     int16_t input_value,
                     int16_t max_output_value,
                     int16_t min_output_value,
                     const CodecInst& codec_inst) {
    ASSERT_LE(num_remote_streams_using_mono, num_remote_streams);

    if (real_audio) {
      input_filename_ = test::ResourcePath("voice_engine/audio_long16", "pcm");
    } else {
      input_filename_ = test::OutputPath() + "mixing_test_input.pcm";
      GenerateInputFile(input_value);
    }

    std::vector<int> local_streams(num_local_streams);
    for (size_t i = 0; i < local_streams.size(); ++i) {
      local_streams[i] = voe_base_->CreateChannel();
      EXPECT_NE(-1, local_streams[i]);
    }
    StartLocalStreams(local_streams);
    TEST_LOG("Playing %d local streams.\n", num_local_streams);

    std::vector<int> remote_streams(num_remote_streams);
    for (size_t i = 0; i < remote_streams.size(); ++i) {
      remote_streams[i] = voe_base_->CreateChannel();
      EXPECT_NE(-1, remote_streams[i]);
    }
    StartRemoteStreams(remote_streams, num_remote_streams_using_mono,
                       codec_inst);
    TEST_LOG("Playing %d remote streams.\n", num_remote_streams);

    // Give it plenty of time to get started.
    SleepMs(1000);

    // Start recording the mixed output and wait.
    EXPECT_EQ(0, voe_file_->StartRecordingPlayout(-1 /* record meeting */,
        output_filename_.c_str()));
    SleepMs(kTestDurationMs);
    while (GetFileDurationMs(output_filename_.c_str()) < kTestDurationMs) {
      SleepMs(200);
    }
    EXPECT_EQ(0, voe_file_->StopRecordingPlayout(-1));

    StopLocalStreams(local_streams);
    StopRemoteStreams(remote_streams);

    if (!real_audio) {
      VerifyMixedOutput(max_output_value, min_output_value);
    }
  }

 private:
  // Generate input file with constant values equal to |input_value|. The file
  // will be twice the duration of the test.
  void GenerateInputFile(int16_t input_value) {
    FILE* input_file = fopen(input_filename_.c_str(), "wb");
    ASSERT_TRUE(input_file != NULL);
    for (int i = 0; i < kInSampleRateHz / 1000 * (kTestDurationMs * 2); i++) {
      ASSERT_EQ(1u, fwrite(&input_value, sizeof(input_value), 1, input_file));
    }
    ASSERT_EQ(0, fclose(input_file));
  }

  void VerifyMixedOutput(int16_t max_output_value, int16_t min_output_value) {
    // Verify the mixed output.
    FILE* output_file = fopen(output_filename_.c_str(), "rb");
    ASSERT_TRUE(output_file != NULL);
    int16_t output_value = 0;
    int samples_read = 0;
    while (fread(&output_value, sizeof(output_value), 1, output_file) == 1) {
      samples_read++;
      std::ostringstream trace_stream;
      trace_stream << samples_read << " samples read";
      SCOPED_TRACE(trace_stream.str());
      ASSERT_LE(output_value, max_output_value);
      ASSERT_GE(output_value, min_output_value);
    }
    // Ensure we've at least recorded half as much file as the duration of the
    // test. We have to use a relaxed tolerance here due to filesystem flakiness
    // on the bots.
    ASSERT_GE((samples_read * 1000.0) / kRecSampleRateHz, kTestDurationMs);
    // Ensure we read the entire file.
    ASSERT_NE(0, feof(output_file));
    ASSERT_EQ(0, fclose(output_file));
  }

  // Start up local streams ("anonymous" participants).
  void StartLocalStreams(const std::vector<int>& streams) {
    for (size_t i = 0; i < streams.size(); ++i) {
      EXPECT_EQ(0, voe_base_->StartPlayout(streams[i]));
      EXPECT_EQ(0, voe_file_->StartPlayingFileLocally(streams[i],
          input_filename_.c_str(), true));
    }
  }

  void StopLocalStreams(const std::vector<int>& streams) {
    for (size_t i = 0; i < streams.size(); ++i) {
      EXPECT_EQ(0, voe_base_->StopPlayout(streams[i]));
      EXPECT_EQ(0, voe_base_->DeleteChannel(streams[i]));
    }
  }

  // Start up remote streams ("normal" participants).
  void StartRemoteStreams(const std::vector<int>& streams,
                          int num_remote_streams_using_mono,
                          const CodecInst& codec_inst) {
    for (int i = 0; i < num_remote_streams_using_mono; ++i) {
      // Add some delay between starting up the channels in order to give them
      // different energies in the "real audio" test and hopefully exercise
      // more code paths.
      SleepMs(50);
      StartRemoteStream(streams[i], codec_inst, 1234 + 2 * i);
    }

    // The remainder of the streams will use stereo.
    CodecInst codec_inst_stereo = codec_inst;
    codec_inst_stereo.channels = 2;
    codec_inst_stereo.pltype++;
    for (size_t i = num_remote_streams_using_mono; i < streams.size(); ++i) {
      StartRemoteStream(streams[i], codec_inst_stereo, 1234 + 2 * i);
    }
  }

  // Start up a single remote stream.
  void StartRemoteStream(int stream, const CodecInst& codec_inst, int port) {
    EXPECT_EQ(0, voe_codec_->SetRecPayloadType(stream, codec_inst));
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(stream, *transport_));
    EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(
                     stream, static_cast<unsigned int>(stream)));
    transport_->AddChannel(stream, stream);
    EXPECT_EQ(0, voe_base_->StartPlayout(stream));
    EXPECT_EQ(0, voe_codec_->SetSendCodec(stream, codec_inst));
    EXPECT_EQ(0, voe_base_->StartSend(stream));
    EXPECT_EQ(0, voe_file_->StartPlayingFileAsMicrophone(stream,
        input_filename_.c_str(), true));
  }

  void StopRemoteStreams(const std::vector<int>& streams) {
    for (size_t i = 0; i < streams.size(); ++i) {
      EXPECT_EQ(0, voe_base_->StopSend(streams[i]));
      EXPECT_EQ(0, voe_base_->StopPlayout(streams[i]));
      EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(streams[i]));
      EXPECT_EQ(0, voe_base_->DeleteChannel(streams[i]));
    }
  }

  int GetFileDurationMs(const char* file_name) {
    FILE* fid = fopen(file_name, "rb");
    EXPECT_FALSE(fid == NULL);
    fseek(fid, 0, SEEK_END);
    int size = ftell(fid);
    EXPECT_NE(-1, size);
    fclose(fid);
    // Divided by 2 due to 2 bytes/sample.
    return size * 1000 / kRecSampleRateHz / 2;
  }

  std::string input_filename_;
  const std::string output_filename_;
  LoopBackTransport* transport_;
};

// This test has no verification, but exercises additional code paths in a
// somewhat more realistic scenario using real audio. It can at least hunt for
// asserts and crashes.
TEST_F(MixingTest, MixManyChannelsForStress) {
  RunMixingTest(10, 0, 10, true, 0, 0, 0, kCodecL16);
}

TEST_F(MixingTest, MixManyChannelsForStressOpus) {
  RunMixingTest(10, 0, 10, true, 0, 0, 0, kCodecOpus);
}

// These tests assume a maximum of three mixed participants. We typically allow
// a +/- 10% range around the expected output level to account for distortion
// from coding and processing in the loopback chain.
TEST_F(MixingTest, FourChannelsWithOnlyThreeMixed) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 3;
  RunMixingTest(4, 0, 4, false, kInputValue, 1.1 * kExpectedOutput,
                0.9 * kExpectedOutput, kCodecL16);
}

// Ensure the mixing saturation protection is working. We can do this because
// the mixing limiter is given some headroom, so the expected output is less
// than full scale.
TEST_F(MixingTest, VerifySaturationProtection) {
  const int16_t kInputValue = 20000;
  const int16_t kExpectedOutput = kLimiterHeadroom;
  // If this isn't satisfied, we're not testing anything.
  ASSERT_GT(kInputValue * 3, kInt16Max);
  ASSERT_LT(1.1 * kExpectedOutput, kInt16Max);
  RunMixingTest(3, 0, 3, false, kInputValue, 1.1 * kExpectedOutput,
               0.9 * kExpectedOutput, kCodecL16);
}

TEST_F(MixingTest, SaturationProtectionHasNoEffectOnOneChannel) {
  const int16_t kInputValue = kInt16Max;
  const int16_t kExpectedOutput = kInt16Max;
  // If this isn't satisfied, we're not testing anything.
  ASSERT_GT(0.95 * kExpectedOutput, kLimiterHeadroom);
  // Tighter constraints are required here to properly test this.
  RunMixingTest(1, 0, 1, false, kInputValue, kExpectedOutput,
                0.95 * kExpectedOutput, kCodecL16);
}

TEST_F(MixingTest, VerifyAnonymousAndNormalParticipantMixing) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 2;
  RunMixingTest(1, 1, 1, false, kInputValue, 1.1 * kExpectedOutput,
                0.9 * kExpectedOutput, kCodecL16);
}

TEST_F(MixingTest, AnonymousParticipantsAreAlwaysMixed) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 4;
  RunMixingTest(3, 1, 3, false, kInputValue, 1.1 * kExpectedOutput,
                0.9 * kExpectedOutput, kCodecL16);
}

TEST_F(MixingTest, VerifyStereoAndMonoMixing) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 2;
  RunMixingTest(2, 0, 1, false, kInputValue, 1.1 * kExpectedOutput,
                // Lower than 0.9 due to observed flakiness on bots.
                0.8 * kExpectedOutput, kCodecL16);
}

}  // namespace webrtc
