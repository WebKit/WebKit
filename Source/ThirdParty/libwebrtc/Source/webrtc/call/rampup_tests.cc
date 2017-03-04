/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rampup_tests.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/perf_test.h"

namespace webrtc {
namespace {

static const int64_t kPollIntervalMs = 20;
static const int kExpectedHighVideoBitrateBps = 80000;
static const int kExpectedHighAudioBitrateBps = 30000;
static const int kLowBandwidthLimitBps = 20000;
static const int kExpectedLowBitrateBps = 20000;

std::vector<uint32_t> GenerateSsrcs(size_t num_streams, uint32_t ssrc_offset) {
  std::vector<uint32_t> ssrcs;
  for (size_t i = 0; i != num_streams; ++i)
    ssrcs.push_back(static_cast<uint32_t>(ssrc_offset + i));
  return ssrcs;
}
}  // namespace

RampUpTester::RampUpTester(size_t num_video_streams,
                           size_t num_audio_streams,
                           size_t num_flexfec_streams,
                           unsigned int start_bitrate_bps,
                           int64_t min_run_time_ms,
                           const std::string& extension_type,
                           bool rtx,
                           bool red,
                           bool report_perf_stats)
    : EndToEndTest(test::CallTest::kLongTimeoutMs),
      stop_event_(false, false),
      clock_(Clock::GetRealTimeClock()),
      num_video_streams_(num_video_streams),
      num_audio_streams_(num_audio_streams),
      num_flexfec_streams_(num_flexfec_streams),
      rtx_(rtx),
      red_(red),
      sender_call_(nullptr),
      send_stream_(nullptr),
      start_bitrate_bps_(start_bitrate_bps),
      min_run_time_ms_(min_run_time_ms),
      report_perf_stats_(report_perf_stats),
      expected_bitrate_bps_(0),
      test_start_ms_(-1),
      ramp_up_finished_ms_(-1),
      extension_type_(extension_type),
      video_ssrcs_(GenerateSsrcs(num_video_streams_, 100)),
      video_rtx_ssrcs_(GenerateSsrcs(num_video_streams_, 200)),
      audio_ssrcs_(GenerateSsrcs(num_audio_streams_, 300)),
      poller_thread_(&BitrateStatsPollingThread,
                     this,
                     "BitrateStatsPollingThread") {
  if (red_)
    EXPECT_EQ(0u, num_flexfec_streams_);
  EXPECT_LE(num_audio_streams_, 1u);
}

RampUpTester::~RampUpTester() {
}

Call::Config RampUpTester::GetSenderCallConfig() {
  Call::Config call_config(&event_log_);
  if (start_bitrate_bps_ != 0) {
    call_config.bitrate_config.start_bitrate_bps = start_bitrate_bps_;
  }
  call_config.bitrate_config.min_bitrate_bps = 10000;
  return call_config;
}

void RampUpTester::OnVideoStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {
  send_stream_ = send_stream;
}

test::PacketTransport* RampUpTester::CreateSendTransport(Call* sender_call) {
  send_transport_ = new test::PacketTransport(sender_call, this,
                                              test::PacketTransport::kSender,
                                              forward_transport_config_);
  return send_transport_;
}

size_t RampUpTester::GetNumVideoStreams() const {
  return num_video_streams_;
}

size_t RampUpTester::GetNumAudioStreams() const {
  return num_audio_streams_;
}

size_t RampUpTester::GetNumFlexfecStreams() const {
  return num_flexfec_streams_;
}

class RampUpTester::VideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  VideoStreamFactory() {}

 private:
  std::vector<VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const VideoEncoderConfig& encoder_config) override {
    std::vector<VideoStream> streams =
        test::CreateVideoStreams(width, height, encoder_config);
    if (encoder_config.number_of_streams == 1) {
      streams[0].target_bitrate_bps = streams[0].max_bitrate_bps = 2000000;
    }
    return streams;
  }
};

void RampUpTester::ModifyVideoConfigs(
    VideoSendStream::Config* send_config,
    std::vector<VideoReceiveStream::Config>* receive_configs,
    VideoEncoderConfig* encoder_config) {
  send_config->suspend_below_min_bitrate = true;
  encoder_config->number_of_streams = num_video_streams_;
  encoder_config->max_bitrate_bps = 2000000;
  encoder_config->video_stream_factory =
      new rtc::RefCountedObject<RampUpTester::VideoStreamFactory>();
  if (num_video_streams_ == 1) {
    // For single stream rampup until 1mbps
    expected_bitrate_bps_ = kSingleStreamTargetBps;
  } else {
    // For multi stream rampup until all streams are being sent. That means
    // enough bitrate to send all the target streams plus the min bitrate of
    // the last one.
    std::vector<VideoStream> streams = test::CreateVideoStreams(
        test::CallTest::kDefaultWidth, test::CallTest::kDefaultHeight,
        *encoder_config);
    expected_bitrate_bps_ = streams.back().min_bitrate_bps;
    for (size_t i = 0; i < streams.size() - 1; ++i) {
      expected_bitrate_bps_ += streams[i].target_bitrate_bps;
    }
  }

  send_config->rtp.extensions.clear();

  bool remb;
  bool transport_cc;
  if (extension_type_ == RtpExtension::kAbsSendTimeUri) {
    remb = true;
    transport_cc = false;
    send_config->rtp.extensions.push_back(
        RtpExtension(extension_type_.c_str(), kAbsSendTimeExtensionId));
  } else if (extension_type_ == RtpExtension::kTransportSequenceNumberUri) {
    remb = false;
    transport_cc = true;
    send_config->rtp.extensions.push_back(RtpExtension(
        extension_type_.c_str(), kTransportSequenceNumberExtensionId));
  } else {
    remb = true;
    transport_cc = false;
    send_config->rtp.extensions.push_back(RtpExtension(
        extension_type_.c_str(), kTransmissionTimeOffsetExtensionId));
  }

  send_config->rtp.nack.rtp_history_ms = test::CallTest::kNackRtpHistoryMs;
  send_config->rtp.ssrcs = video_ssrcs_;
  if (rtx_) {
    send_config->rtp.rtx.payload_type = test::CallTest::kSendRtxPayloadType;
    send_config->rtp.rtx.ssrcs = video_rtx_ssrcs_;
  }
  if (red_) {
    send_config->rtp.ulpfec.ulpfec_payload_type =
        test::CallTest::kUlpfecPayloadType;
    send_config->rtp.ulpfec.red_payload_type = test::CallTest::kRedPayloadType;
    if (rtx_) {
      send_config->rtp.ulpfec.red_rtx_payload_type =
          test::CallTest::kRtxRedPayloadType;
    }
  }

  size_t i = 0;
  for (VideoReceiveStream::Config& recv_config : *receive_configs) {
    recv_config.rtp.remb = remb;
    recv_config.rtp.transport_cc = transport_cc;
    recv_config.rtp.extensions = send_config->rtp.extensions;

    recv_config.rtp.remote_ssrc = video_ssrcs_[i];
    recv_config.rtp.nack.rtp_history_ms = send_config->rtp.nack.rtp_history_ms;

    if (red_) {
      recv_config.rtp.ulpfec.red_payload_type =
          send_config->rtp.ulpfec.red_payload_type;
      recv_config.rtp.ulpfec.ulpfec_payload_type =
          send_config->rtp.ulpfec.ulpfec_payload_type;
      if (rtx_) {
        recv_config.rtp.ulpfec.red_rtx_payload_type =
            send_config->rtp.ulpfec.red_rtx_payload_type;
      }
    }

    if (rtx_) {
      recv_config.rtp.rtx_ssrc = video_rtx_ssrcs_[i];
      recv_config.rtp
          .rtx_payload_types[send_config->encoder_settings.payload_type] =
          send_config->rtp.rtx.payload_type;
    }
    ++i;
  }

  RTC_DCHECK_LE(num_flexfec_streams_, 1);
  if (num_flexfec_streams_ == 1) {
    send_config->rtp.flexfec.payload_type = test::CallTest::kFlexfecPayloadType;
    send_config->rtp.flexfec.ssrc = test::CallTest::kFlexfecSendSsrc;
    send_config->rtp.flexfec.protected_media_ssrcs = {video_ssrcs_[0]};
  }
}

void RampUpTester::ModifyAudioConfigs(
    AudioSendStream::Config* send_config,
    std::vector<AudioReceiveStream::Config>* receive_configs) {
  if (num_audio_streams_ == 0)
    return;

  EXPECT_NE(RtpExtension::kTimestampOffsetUri, extension_type_)
      << "Audio BWE not supported with toffset.";
  EXPECT_NE(RtpExtension::kAbsSendTimeUri, extension_type_)
      << "Audio BWE not supported with abs-send-time.";

  send_config->rtp.ssrc = audio_ssrcs_[0];
  send_config->rtp.extensions.clear();

  send_config->min_bitrate_bps = 6000;
  send_config->max_bitrate_bps = 60000;

  bool transport_cc = false;
  if (extension_type_ == RtpExtension::kTransportSequenceNumberUri) {
    transport_cc = true;
    send_config->rtp.extensions.push_back(RtpExtension(
        extension_type_.c_str(), kTransportSequenceNumberExtensionId));
  }

  for (AudioReceiveStream::Config& recv_config : *receive_configs) {
    recv_config.rtp.transport_cc = transport_cc;
    recv_config.rtp.extensions = send_config->rtp.extensions;
    recv_config.rtp.remote_ssrc = send_config->rtp.ssrc;
  }
}

void RampUpTester::ModifyFlexfecConfigs(
    std::vector<FlexfecReceiveStream::Config>* receive_configs) {
  if (num_flexfec_streams_ == 0)
    return;
  RTC_DCHECK_EQ(1, num_flexfec_streams_);
  (*receive_configs)[0].payload_type = test::CallTest::kFlexfecPayloadType;
  (*receive_configs)[0].remote_ssrc = test::CallTest::kFlexfecSendSsrc;
  (*receive_configs)[0].protected_media_ssrcs = {video_ssrcs_[0]};
  (*receive_configs)[0].local_ssrc = video_ssrcs_[0];
  if (extension_type_ == RtpExtension::kAbsSendTimeUri) {
    (*receive_configs)[0].transport_cc = false;
    (*receive_configs)[0].rtp_header_extensions.push_back(
        RtpExtension(extension_type_.c_str(), kAbsSendTimeExtensionId));
  } else if (extension_type_ == RtpExtension::kTransportSequenceNumberUri) {
    (*receive_configs)[0].transport_cc = true;
    (*receive_configs)[0].rtp_header_extensions.push_back(RtpExtension(
        extension_type_.c_str(), kTransportSequenceNumberExtensionId));
  }
}

void RampUpTester::OnCallsCreated(Call* sender_call, Call* receiver_call) {
  sender_call_ = sender_call;
}

void RampUpTester::BitrateStatsPollingThread(void* obj) {
  static_cast<RampUpTester*>(obj)->PollStats();
}

void RampUpTester::PollStats() {
  do {
    if (sender_call_) {
      Call::Stats stats = sender_call_->GetStats();

      EXPECT_GE(stats.send_bandwidth_bps, start_bitrate_bps_);
      EXPECT_GE(expected_bitrate_bps_, 0);
      if (stats.send_bandwidth_bps >= expected_bitrate_bps_ &&
          (min_run_time_ms_ == -1 ||
           clock_->TimeInMilliseconds() - test_start_ms_ >= min_run_time_ms_)) {
        ramp_up_finished_ms_ = clock_->TimeInMilliseconds();
        observation_complete_.Set();
      }
    }
  } while (!stop_event_.Wait(kPollIntervalMs));
}

void RampUpTester::ReportResult(const std::string& measurement,
                                size_t value,
                                const std::string& units) const {
  webrtc::test::PrintResult(
      measurement, "",
      ::testing::UnitTest::GetInstance()->current_test_info()->name(), value,
      units, false);
}

void RampUpTester::AccumulateStats(const VideoSendStream::StreamStats& stream,
                                   size_t* total_packets_sent,
                                   size_t* total_sent,
                                   size_t* padding_sent,
                                   size_t* media_sent) const {
  *total_packets_sent += stream.rtp_stats.transmitted.packets +
                         stream.rtp_stats.retransmitted.packets +
                         stream.rtp_stats.fec.packets;
  *total_sent += stream.rtp_stats.transmitted.TotalBytes() +
                 stream.rtp_stats.retransmitted.TotalBytes() +
                 stream.rtp_stats.fec.TotalBytes();
  *padding_sent += stream.rtp_stats.transmitted.padding_bytes +
                   stream.rtp_stats.retransmitted.padding_bytes +
                   stream.rtp_stats.fec.padding_bytes;
  *media_sent += stream.rtp_stats.MediaPayloadBytes();
}

void RampUpTester::TriggerTestDone() {
  RTC_DCHECK_GE(test_start_ms_, 0);

  // TODO(holmer): Add audio send stats here too when those APIs are available.
  if (!send_stream_)
    return;

  VideoSendStream::Stats send_stats = send_stream_->GetStats();

  size_t total_packets_sent = 0;
  size_t total_sent = 0;
  size_t padding_sent = 0;
  size_t media_sent = 0;
  for (uint32_t ssrc : video_ssrcs_) {
    AccumulateStats(send_stats.substreams[ssrc], &total_packets_sent,
                    &total_sent, &padding_sent, &media_sent);
  }

  size_t rtx_total_packets_sent = 0;
  size_t rtx_total_sent = 0;
  size_t rtx_padding_sent = 0;
  size_t rtx_media_sent = 0;
  for (uint32_t rtx_ssrc : video_rtx_ssrcs_) {
    AccumulateStats(send_stats.substreams[rtx_ssrc], &rtx_total_packets_sent,
                    &rtx_total_sent, &rtx_padding_sent, &rtx_media_sent);
  }

  if (report_perf_stats_) {
    ReportResult("ramp-up-total-packets-sent", total_packets_sent, "packets");
    ReportResult("ramp-up-total-sent", total_sent, "bytes");
    ReportResult("ramp-up-media-sent", media_sent, "bytes");
    ReportResult("ramp-up-padding-sent", padding_sent, "bytes");
    ReportResult("ramp-up-rtx-total-packets-sent", rtx_total_packets_sent,
                 "packets");
    ReportResult("ramp-up-rtx-total-sent", rtx_total_sent, "bytes");
    ReportResult("ramp-up-rtx-media-sent", rtx_media_sent, "bytes");
    ReportResult("ramp-up-rtx-padding-sent", rtx_padding_sent, "bytes");
    if (ramp_up_finished_ms_ >= 0) {
      ReportResult("ramp-up-time", ramp_up_finished_ms_ - test_start_ms_,
                   "milliseconds");
    }
    ReportResult("ramp-up-average-network-latency",
                 send_transport_->GetAverageDelayMs(), "milliseconds");
  }
}

void RampUpTester::PerformTest() {
  test_start_ms_ = clock_->TimeInMilliseconds();
  poller_thread_.Start();
  EXPECT_TRUE(Wait()) << "Timed out while waiting for ramp-up to complete.";
  TriggerTestDone();
  stop_event_.Set();
  poller_thread_.Stop();
}

RampUpDownUpTester::RampUpDownUpTester(size_t num_video_streams,
                                       size_t num_audio_streams,
                                       size_t num_flexfec_streams,
                                       unsigned int start_bitrate_bps,
                                       const std::string& extension_type,
                                       bool rtx,
                                       bool red,
                                       const std::vector<int>& loss_rates)
    : RampUpTester(num_video_streams,
                   num_audio_streams,
                   num_flexfec_streams,
                   start_bitrate_bps,
                   0,
                   extension_type,
                   rtx,
                   red,
                   true),
      link_rates_({GetHighLinkCapacity(), kLowBandwidthLimitBps / 1000,
                   GetHighLinkCapacity(), 0}),
      test_state_(kFirstRampup),
      next_state_(kTransitionToNextState),
      state_start_ms_(clock_->TimeInMilliseconds()),
      interval_start_ms_(clock_->TimeInMilliseconds()),
      sent_bytes_(0),
      loss_rates_(loss_rates) {
  forward_transport_config_.link_capacity_kbps = link_rates_[test_state_];
  forward_transport_config_.queue_delay_ms = 100;
  forward_transport_config_.loss_percent = loss_rates_[test_state_];
}

RampUpDownUpTester::~RampUpDownUpTester() {}

void RampUpDownUpTester::PollStats() {
  do {
    if (send_stream_) {
      webrtc::VideoSendStream::Stats stats = send_stream_->GetStats();
      int transmit_bitrate_bps = 0;
      for (auto it : stats.substreams) {
        transmit_bitrate_bps += it.second.total_bitrate_bps;
      }
      EvolveTestState(transmit_bitrate_bps, stats.suspended);
    } else if (num_audio_streams_ > 0 && sender_call_ != nullptr) {
      // An audio send stream doesn't have bitrate stats, so the call send BW is
      // currently used instead.
      int transmit_bitrate_bps = sender_call_->GetStats().send_bandwidth_bps;
      EvolveTestState(transmit_bitrate_bps, false);
    }
  } while (!stop_event_.Wait(kPollIntervalMs));
}

Call::Config RampUpDownUpTester::GetReceiverCallConfig() {
  Call::Config config(&event_log_);
  config.bitrate_config.min_bitrate_bps = 10000;
  return config;
}

std::string RampUpDownUpTester::GetModifierString() const {
  std::string str("_");
  if (num_video_streams_ > 0) {
    std::ostringstream s;
    s << num_video_streams_;
    str += s.str();
    str += "stream";
    str += (num_video_streams_ > 1 ? "s" : "");
    str += "_";
  }
  if (num_audio_streams_ > 0) {
    std::ostringstream s;
    s << num_audio_streams_;
    str += s.str();
    str += "stream";
    str += (num_audio_streams_ > 1 ? "s" : "");
    str += "_";
  }
  str += (rtx_ ? "" : "no");
  str += "rtx";
  return str;
}

int RampUpDownUpTester::GetExpectedHighBitrate() const {
  int expected_bitrate_bps = 0;
  if (num_audio_streams_ > 0)
    expected_bitrate_bps += kExpectedHighAudioBitrateBps;
  if (num_video_streams_ > 0)
    expected_bitrate_bps += kExpectedHighVideoBitrateBps;
  return expected_bitrate_bps;
}

int RampUpDownUpTester::GetHighLinkCapacity() const {
  return 4 * GetExpectedHighBitrate() / (3 * 1000);
}

size_t RampUpDownUpTester::GetFecBytes() const {
  size_t flex_fec_bytes = 0;
  if (num_flexfec_streams_ > 0) {
    webrtc::VideoSendStream::Stats stats = send_stream_->GetStats();
    for (const auto& kv : stats.substreams)
      flex_fec_bytes += kv.second.rtp_stats.fec.TotalBytes();
  }
  return flex_fec_bytes;
}

bool RampUpDownUpTester::ExpectingFec() const {
  return num_flexfec_streams_ > 0 && forward_transport_config_.loss_percent > 0;
}

void RampUpDownUpTester::EvolveTestState(int bitrate_bps, bool suspended) {
  int64_t now = clock_->TimeInMilliseconds();
  switch (test_state_) {
    case kFirstRampup:
      EXPECT_FALSE(suspended);
      if (bitrate_bps >= GetExpectedHighBitrate()) {
        webrtc::test::PrintResult("ramp_up_down_up", GetModifierString(),
                                  "first_rampup", now - state_start_ms_, "ms",
                                  false);
        // Apply loss during the transition between states if FEC is enabled.
        forward_transport_config_.loss_percent = loss_rates_[test_state_];
        test_state_ = kTransitionToNextState;
        next_state_ = kLowRate;
      }
      break;
    case kLowRate: {
      // Audio streams are never suspended.
      bool check_suspend_state = num_video_streams_ > 0;
      if (bitrate_bps < kExpectedLowBitrateBps &&
          suspended == check_suspend_state) {
        webrtc::test::PrintResult("ramp_up_down_up", GetModifierString(),
                                  "rampdown", now - state_start_ms_, "ms",
                                  false);
        // Apply loss during the transition between states if FEC is enabled.
        forward_transport_config_.loss_percent = loss_rates_[test_state_];
        test_state_ = kTransitionToNextState;
        next_state_ = kSecondRampup;
      }
      break;
    }
    case kSecondRampup:
      if (bitrate_bps >= GetExpectedHighBitrate() && !suspended) {
        webrtc::test::PrintResult("ramp_up_down_up", GetModifierString(),
                                  "second_rampup", now - state_start_ms_, "ms",
                                  false);
        ReportResult("ramp-up-down-up-average-network-latency",
                     send_transport_->GetAverageDelayMs(), "milliseconds");
        // Apply loss during the transition between states if FEC is enabled.
        forward_transport_config_.loss_percent = loss_rates_[test_state_];
        test_state_ = kTransitionToNextState;
        next_state_ = kTestEnd;
      }
      break;
    case kTestEnd:
      observation_complete_.Set();
      break;
    case kTransitionToNextState:
      if (!ExpectingFec() || GetFecBytes() > 0) {
        test_state_ = next_state_;
        forward_transport_config_.link_capacity_kbps = link_rates_[test_state_];
        // No loss while ramping up and down as it may affect the BWE
        // negatively, making the test flaky.
        forward_transport_config_.loss_percent = 0;
        state_start_ms_ = now;
        interval_start_ms_ = now;
        sent_bytes_ = 0;
        send_transport_->SetConfig(forward_transport_config_);
      }
      break;
  }
}

class RampUpTest : public test::CallTest {
 public:
  RampUpTest() {}

  virtual ~RampUpTest() {
    EXPECT_EQ(nullptr, video_send_stream_);
    EXPECT_TRUE(video_receive_streams_.empty());
  }
};

static const uint32_t kStartBitrateBps = 60000;

TEST_F(RampUpTest, UpDownUpAbsSendTimeSimulcastRedRtx) {
  std::vector<int> loss_rates = {0, 0, 0, 0};
  RampUpDownUpTester test(3, 0, 0, kStartBitrateBps,
                          RtpExtension::kAbsSendTimeUri, true, true,
                          loss_rates);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpTransportSequenceNumberRtx) {
  std::vector<int> loss_rates = {0, 0, 0, 0};
  RampUpDownUpTester test(3, 0, 0, kStartBitrateBps,
                          RtpExtension::kTransportSequenceNumberUri, true,
                          false, loss_rates);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpTransportSequenceNumberPacketLoss) {
  std::vector<int> loss_rates = {20, 0, 0, 0};
  RampUpDownUpTester test(1, 0, 1, kStartBitrateBps,
                          RtpExtension::kTransportSequenceNumberUri, true,
                          false, loss_rates);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpAudioVideoTransportSequenceNumberRtx) {
  std::vector<int> loss_rates = {0, 0, 0, 0};
  RampUpDownUpTester test(3, 1, 0, kStartBitrateBps,
                          RtpExtension::kTransportSequenceNumberUri, true,
                          false, loss_rates);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, UpDownUpAudioTransportSequenceNumberRtx) {
  std::vector<int> loss_rates = {0, 0, 0, 0};
  RampUpDownUpTester test(0, 1, 0, kStartBitrateBps,
                          RtpExtension::kTransportSequenceNumberUri, true,
                          false, loss_rates);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TOffsetSimulcastRedRtx) {
  RampUpTester test(3, 0, 0, 0, 0, RtpExtension::kTimestampOffsetUri, true,
                    true, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AbsSendTime) {
  RampUpTester test(1, 0, 0, 0, 0, RtpExtension::kAbsSendTimeUri, false, false,
                    true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AbsSendTimeSimulcastRedRtx) {
  RampUpTester test(3, 0, 0, 0, 0, RtpExtension::kAbsSendTimeUri, true, true,
                    true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumber) {
  RampUpTester test(1, 0, 0, 0, 0, RtpExtension::kTransportSequenceNumberUri,
                    false, false, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumberSimulcast) {
  RampUpTester test(3, 0, 0, 0, 0, RtpExtension::kTransportSequenceNumberUri,
                    false, false, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, TransportSequenceNumberSimulcastRedRtx) {
  RampUpTester test(3, 0, 0, 0, 0, RtpExtension::kTransportSequenceNumberUri,
                    true, true, true);
  RunBaseTest(&test);
}

TEST_F(RampUpTest, AudioTransportSequenceNumber) {
  RampUpTester test(0, 1, 0, 300000, 10000,
                    RtpExtension::kTransportSequenceNumberUri, false, false,
                    false);
  RunBaseTest(&test);
}
}  // namespace webrtc
