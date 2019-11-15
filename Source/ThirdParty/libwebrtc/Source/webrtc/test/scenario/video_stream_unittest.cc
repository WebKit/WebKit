/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <atomic>

#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {
namespace {
using Capture = VideoStreamConfig::Source::Capture;
using ContentType = VideoStreamConfig::Encoder::ContentType;
using Codec = VideoStreamConfig::Encoder::Codec;
using CodecImpl = VideoStreamConfig::Encoder::Implementation;
}  // namespace

TEST(VideoStreamTest, ReceivesFramesFromFileBasedStreams) {
  TimeDelta kRunTime = TimeDelta::ms(500);
  std::vector<int> kFrameRates = {15, 30};
  std::deque<std::atomic<int>> frame_counts(2);
  frame_counts[0] = 0;
  frame_counts[1] = 0;
  {
    Scenario s;
    auto route =
        s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                       {s.CreateSimulationNode(NetworkSimulationConfig())},
                       s.CreateClient("callee", CallClientConfig()),
                       {s.CreateSimulationNode(NetworkSimulationConfig())});

    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->hooks.frame_pair_handlers = {
          [&](const VideoFramePair&) { frame_counts[0]++; }};
      c->source.capture = Capture::kVideoFile;
      c->source.video_file.name = "foreman_cif";
      c->source.video_file.width = 352;
      c->source.video_file.height = 288;
      c->source.framerate = kFrameRates[0];
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP8;
    });
    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->hooks.frame_pair_handlers = {
          [&](const VideoFramePair&) { frame_counts[1]++; }};
      c->source.capture = Capture::kImageSlides;
      c->source.slides.images.crop.width = 320;
      c->source.slides.images.crop.height = 240;
      c->source.framerate = kFrameRates[1];
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP9;
    });
    s.RunFor(kRunTime);
  }
  std::vector<int> expected_counts;
  for (int fps : kFrameRates)
    expected_counts.push_back(
        static_cast<int>(kRunTime.seconds<double>() * fps * 0.8));

  EXPECT_GE(frame_counts[0], expected_counts[0]);
  EXPECT_GE(frame_counts[1], expected_counts[1]);
}

TEST(VideoStreamTest, RecievesVp8SimulcastFrames) {
  TimeDelta kRunTime = TimeDelta::ms(500);
  int kFrameRate = 30;

  std::deque<std::atomic<int>> frame_counts(3);
  frame_counts[0] = 0;
  frame_counts[1] = 0;
  frame_counts[2] = 0;
  {
    Scenario s;
    auto route =
        s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                       {s.CreateSimulationNode(NetworkSimulationConfig())},
                       s.CreateClient("callee", CallClientConfig()),
                       {s.CreateSimulationNode(NetworkSimulationConfig())});
    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      // TODO(srte): Replace with code checking for all simulcast streams when
      // there's a hook available for that.
      c->hooks.frame_pair_handlers = {[&](const VideoFramePair& info) {
        frame_counts[info.layer_id]++;
        RTC_DCHECK(info.decoded);
        printf("%i: [%3i->%3i, %i], %i->%i, \n", info.layer_id, info.capture_id,
               info.decode_id, info.repeated, info.captured->width(),
               info.decoded->width());
      }};
      c->source.framerate = kFrameRate;
      // The resolution must be high enough to allow smaller layers to be
      // created.
      c->source.generator.width = 1024;
      c->source.generator.height = 768;
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP8;
      // By enabling multiple spatial layers, simulcast will be enabled for VP8.
      c->encoder.layers.spatial = 3;
    });
    s.RunFor(kRunTime);
  }

  // Using high error margin to avoid flakyness.
  const int kExpectedCount =
      static_cast<int>(kRunTime.seconds<double>() * kFrameRate * 0.5);

  EXPECT_GE(frame_counts[0], kExpectedCount);
  EXPECT_GE(frame_counts[1], kExpectedCount);
  EXPECT_GE(frame_counts[2], kExpectedCount);
}

TEST(VideoStreamTest, SendsNacksOnLoss) {
  Scenario s;
  auto route =
      s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                     {s.CreateSimulationNode([](NetworkSimulationConfig* c) {
                       c->loss_rate = 0.2;
                     })},
                     s.CreateClient("callee", CallClientConfig()),
                     {s.CreateSimulationNode(NetworkSimulationConfig())});
  // NACK retransmissions are enabled by default.
  auto video = s.CreateVideoStream(route->forward(), VideoStreamConfig());
  s.RunFor(TimeDelta::seconds(1));
  auto stream_stats = video->send()->GetStats().substreams.begin()->second;
  EXPECT_GT(stream_stats.rtp_stats.retransmitted.packets, 0u);
}

TEST(VideoStreamTest, SendsFecWithUlpFec) {
  Scenario s;
  auto route =
      s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                     {s.CreateSimulationNode([](NetworkSimulationConfig* c) {
                       c->loss_rate = 0.1;
                     })},
                     s.CreateClient("callee", CallClientConfig()),
                     {s.CreateSimulationNode(NetworkSimulationConfig())});
  auto video = s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
    // We do not allow NACK+ULPFEC for generic codec, using VP8.
    c->encoder.codec = VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
    c->stream.use_ulpfec = true;
  });
  s.RunFor(TimeDelta::seconds(5));
  VideoSendStream::Stats video_stats = video->send()->GetStats();
  EXPECT_GT(video_stats.substreams.begin()->second.rtp_stats.fec.packets, 0u);
}
TEST(VideoStreamTest, SendsFecWithFlexFec) {
  Scenario s;
  auto route =
      s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                     {s.CreateSimulationNode([](NetworkSimulationConfig* c) {
                       c->loss_rate = 0.1;
                     })},
                     s.CreateClient("callee", CallClientConfig()),
                     {s.CreateSimulationNode(NetworkSimulationConfig())});
  auto video = s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
    c->stream.use_flexfec = true;
  });
  s.RunFor(TimeDelta::seconds(5));
  VideoSendStream::Stats video_stats = video->send()->GetStats();
  EXPECT_GT(video_stats.substreams.begin()->second.rtp_stats.fec.packets, 0u);
}
}  // namespace test
}  // namespace webrtc
