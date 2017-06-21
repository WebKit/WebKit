/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <queue>

#include "webrtc/base/format_macros.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fakes/conference_transport.h"

namespace webrtc {
namespace {

const int kRttMs = 25;

bool IsNear(int ref, int comp, int error) {
  return (ref - comp <= error) && (comp - ref >= -error);
}

void CreateSilenceFile(const std::string& silence_file, int sample_rate_hz) {
  FILE* fid = fopen(silence_file.c_str(), "wb");
  int16_t zero = 0;
  for (int i = 0; i < sample_rate_hz; ++i) {
    // Write 1 second, but it does not matter since the file will be looped.
    fwrite(&zero, sizeof(int16_t), 1, fid);
  }
  fclose(fid);
}

}  // namespace

namespace voetest {

TEST(VoeConferenceTest, RttAndStartNtpTime) {
  struct Stats {
    Stats(int64_t rtt_receiver_1, int64_t rtt_receiver_2, int64_t ntp_delay)
        : rtt_receiver_1_(rtt_receiver_1),
          rtt_receiver_2_(rtt_receiver_2),
          ntp_delay_(ntp_delay) {
    }
    int64_t rtt_receiver_1_;
    int64_t rtt_receiver_2_;
    int64_t ntp_delay_;
  };

  const std::string input_file =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  const webrtc::FileFormats kInputFormat = webrtc::kFileFormatPcm32kHzFile;

  const int kDelayMs = 987;
  ConferenceTransport trans;
  trans.SetRtt(kRttMs);

  unsigned int id_1 = trans.AddStream(input_file, kInputFormat);
  unsigned int id_2 = trans.AddStream(input_file, kInputFormat);

  EXPECT_TRUE(trans.StartPlayout(id_1));
  // Start NTP time is the time when a stream is played out, rather than
  // when it is added.
  webrtc::SleepMs(kDelayMs);
  EXPECT_TRUE(trans.StartPlayout(id_2));

  const int kMaxRunTimeMs = 25000;
  const int kNeedSuccessivePass = 3;
  const int kStatsRequestIntervalMs = 1000;
  const int kStatsBufferSize = 3;

  int64_t deadline = rtc::TimeAfter(kMaxRunTimeMs);
  // Run the following up to |kMaxRunTimeMs| milliseconds.
  int successive_pass = 0;
  webrtc::CallStatistics stats_1;
  webrtc::CallStatistics stats_2;
  std::queue<Stats> stats_buffer;

  while (rtc::TimeMillis() < deadline &&
         successive_pass < kNeedSuccessivePass) {
    webrtc::SleepMs(kStatsRequestIntervalMs);

    EXPECT_TRUE(trans.GetReceiverStatistics(id_1, &stats_1));
    EXPECT_TRUE(trans.GetReceiverStatistics(id_2, &stats_2));

    // It is not easy to verify the NTP time directly. We verify it by testing
    // the difference of two start NTP times.
    int64_t captured_start_ntp_delay = stats_2.capture_start_ntp_time_ms_ -
        stats_1.capture_start_ntp_time_ms_;

    // For the checks of RTT and start NTP time, We allow 10% accuracy.
    if (IsNear(kRttMs, stats_1.rttMs, kRttMs / 10 + 1) &&
        IsNear(kRttMs, stats_2.rttMs, kRttMs / 10 + 1) &&
        IsNear(kDelayMs, captured_start_ntp_delay, kDelayMs / 10 + 1)) {
      successive_pass++;
    } else {
      successive_pass = 0;
    }
    if (stats_buffer.size() >= kStatsBufferSize) {
      stats_buffer.pop();
    }
    stats_buffer.push(Stats(stats_1.rttMs, stats_2.rttMs,
                            captured_start_ntp_delay));
  }

  EXPECT_GE(successive_pass, kNeedSuccessivePass) << "Expected to get RTT and"
      " start NTP time estimate within 10% of the correct value over "
      << kStatsRequestIntervalMs * kNeedSuccessivePass / 1000
      << " seconds.";
  if (successive_pass < kNeedSuccessivePass) {
    printf("The most recent values (RTT for receiver 1, RTT for receiver 2, "
        "NTP delay between receiver 1 and 2) are (from oldest):\n");
    while (!stats_buffer.empty()) {
      Stats stats = stats_buffer.front();
      printf("(%" PRId64 ", %" PRId64 ", %" PRId64 ")\n", stats.rtt_receiver_1_,
             stats.rtt_receiver_2_, stats.ntp_delay_);
      stats_buffer.pop();
    }
  }
}


TEST(VoeConferenceTest, ReceivedPackets) {
  const int kPackets = 50;
  const int kPacketDurationMs = 20;  // Correspond to Opus.

  const std::string input_file =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  const webrtc::FileFormats kInputFormat = webrtc::kFileFormatPcm32kHzFile;

  const std::string silence_file =
      webrtc::test::TempFilename(webrtc::test::OutputPath(), "silence");
  CreateSilenceFile(silence_file, 32000);

  {
    ConferenceTransport trans;
    // Add silence to stream 0, so that it will be filtered out.
    unsigned int id_0 = trans.AddStream(silence_file, kInputFormat);
    unsigned int id_1 = trans.AddStream(input_file, kInputFormat);
    unsigned int id_2 = trans.AddStream(input_file, kInputFormat);
    unsigned int id_3 = trans.AddStream(input_file, kInputFormat);

    EXPECT_TRUE(trans.StartPlayout(id_0));
    EXPECT_TRUE(trans.StartPlayout(id_1));
    EXPECT_TRUE(trans.StartPlayout(id_2));
    EXPECT_TRUE(trans.StartPlayout(id_3));

    webrtc::SleepMs(kPacketDurationMs * kPackets);

    webrtc::CallStatistics stats_0;
    webrtc::CallStatistics stats_1;
    webrtc::CallStatistics stats_2;
    webrtc::CallStatistics stats_3;
    EXPECT_TRUE(trans.GetReceiverStatistics(id_0, &stats_0));
    EXPECT_TRUE(trans.GetReceiverStatistics(id_1, &stats_1));
    EXPECT_TRUE(trans.GetReceiverStatistics(id_2, &stats_2));
    EXPECT_TRUE(trans.GetReceiverStatistics(id_3, &stats_3));

    // We expect stream 0 to be filtered out totally, but since it may join the
    // call earlier than other streams and the beginning packets might have got
    // through. So we only expect |packetsReceived| to be close to zero.
    EXPECT_NEAR(stats_0.packetsReceived, 0, 2);
    // We expect |packetsReceived| to match |kPackets|, but the actual value
    // depends on the sleep timer. So we allow a small off from |kPackets|.
    EXPECT_NEAR(stats_1.packetsReceived, kPackets, 2);
    EXPECT_NEAR(stats_2.packetsReceived, kPackets, 2);
    EXPECT_NEAR(stats_3.packetsReceived, kPackets, 2);
  }

  remove(silence_file.c_str());
}

}  // namespace voetest
}  // namespace webrtc
