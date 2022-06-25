/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_receive_stream2.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_decoder_factory.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "call/rtp_stream_receiver_controller.h"
#include "common_video/test/utilities.h"
#include "media/base/fake_video_renderer.h"
#include "media/engine/fake_webrtc_call.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/encoded_frame.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/clock.h"
#include "test/fake_decoder.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/run_loop.h"
#include "test/time_controller/simulated_time_controller.h"
#include "test/video_decoder_proxy_factory.h"
#include "video/call_stats2.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::WithoutArgs;

constexpr int kDefaultTimeOutMs = 50;

class FrameObjectFake : public EncodedFrame {
 public:
  void SetPayloadType(uint8_t payload_type) { _payloadType = payload_type; }

  void SetRotation(const VideoRotation& rotation) { rotation_ = rotation; }

  void SetNtpTime(int64_t ntp_time_ms) { ntp_time_ms_ = ntp_time_ms; }

  int64_t ReceivedTime() const override { return 0; }

  int64_t RenderTime() const override { return _renderTimeMs; }
};

}  // namespace

class VideoReceiveStream2Test : public ::testing::Test {
 public:
  VideoReceiveStream2Test()
      : task_queue_factory_(CreateDefaultTaskQueueFactory()),
        h264_decoder_factory_(&mock_h264_video_decoder_),
        config_(&mock_transport_, &h264_decoder_factory_),
        call_stats_(Clock::GetRealTimeClock(), loop_.task_queue()) {}
  ~VideoReceiveStream2Test() override {
    if (video_receive_stream_)
      video_receive_stream_->UnregisterFromTransport();
  }

  void SetUp() override {
    constexpr int kDefaultNumCpuCores = 2;
    config_.rtp.remote_ssrc = 1111;
    config_.rtp.local_ssrc = 2222;
    config_.renderer = &fake_renderer_;
    VideoReceiveStream::Decoder h264_decoder;
    h264_decoder.payload_type = 99;
    h264_decoder.video_format = SdpVideoFormat("H264");
    h264_decoder.video_format.parameters.insert(
        {"sprop-parameter-sets", "Z0IACpZTBYmI,aMljiA=="});
    config_.decoders.clear();
    config_.decoders.push_back(h264_decoder);

    clock_ = Clock::GetRealTimeClock();
    timing_ = new VCMTiming(clock_);

    video_receive_stream_ =
        std::make_unique<webrtc::internal::VideoReceiveStream2>(
            task_queue_factory_.get(), &fake_call_, kDefaultNumCpuCores,
            &packet_router_, config_.Copy(), &call_stats_, clock_, timing_,
            &nack_periodic_processor_);
    video_receive_stream_->RegisterWithTransport(
        &rtp_stream_receiver_controller_);
  }

 protected:
  test::RunLoop loop_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  test::VideoDecoderProxyFactory h264_decoder_factory_;
  NackPeriodicProcessor nack_periodic_processor_;
  VideoReceiveStream::Config config_;
  internal::CallStats call_stats_;
  MockVideoDecoder mock_h264_video_decoder_;
  cricket::FakeVideoRenderer fake_renderer_;
  cricket::FakeCall fake_call_;
  MockTransport mock_transport_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  std::unique_ptr<webrtc::internal::VideoReceiveStream2> video_receive_stream_;
  Clock* clock_;
  VCMTiming* timing_;
};

TEST_F(VideoReceiveStream2Test, CreateFrameFromH264FmtpSpropAndIdr) {
  constexpr uint8_t idr_nalu[] = {0x05, 0xFF, 0xFF, 0xFF};
  RtpPacketToSend rtppacket(nullptr);
  uint8_t* payload = rtppacket.AllocatePayload(sizeof(idr_nalu));
  memcpy(payload, idr_nalu, sizeof(idr_nalu));
  rtppacket.SetMarker(true);
  rtppacket.SetSsrc(1111);
  rtppacket.SetPayloadType(99);
  rtppacket.SetSequenceNumber(1);
  rtppacket.SetTimestamp(0);
  rtc::Event init_decode_event;
  EXPECT_CALL(mock_h264_video_decoder_, Configure).WillOnce(WithoutArgs([&] {
    init_decode_event.Set();
    return true;
  }));
  EXPECT_CALL(mock_h264_video_decoder_, RegisterDecodeCompleteCallback(_));
  video_receive_stream_->Start();
  EXPECT_CALL(mock_h264_video_decoder_, Decode(_, false, _));
  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(rtppacket.data(), rtppacket.size()));
  rtp_stream_receiver_controller_.OnRtpPacket(parsed_packet);
  EXPECT_CALL(mock_h264_video_decoder_, Release());
  // Make sure the decoder thread had a chance to run.
  init_decode_event.Wait(kDefaultTimeOutMs);
}

TEST_F(VideoReceiveStream2Test, PlayoutDelay) {
  const VideoPlayoutDelay kPlayoutDelayMs = {123, 321};
  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->SetId(0);
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_EQ(kPlayoutDelayMs.min_ms, timing_->min_playout_delay());
  EXPECT_EQ(kPlayoutDelayMs.max_ms, timing_->max_playout_delay());

  // Check that the biggest minimum delay is chosen.
  video_receive_stream_->SetMinimumPlayoutDelay(400);
  EXPECT_EQ(400, timing_->min_playout_delay());

  // Check base minimum delay validation.
  EXPECT_FALSE(video_receive_stream_->SetBaseMinimumPlayoutDelayMs(12345));
  EXPECT_FALSE(video_receive_stream_->SetBaseMinimumPlayoutDelayMs(-1));
  EXPECT_TRUE(video_receive_stream_->SetBaseMinimumPlayoutDelayMs(500));
  EXPECT_EQ(500, timing_->min_playout_delay());

  // Check that intermidiate values are remembered and the biggest remembered
  // is chosen.
  video_receive_stream_->SetBaseMinimumPlayoutDelayMs(0);
  EXPECT_EQ(400, timing_->min_playout_delay());

  video_receive_stream_->SetMinimumPlayoutDelay(0);
  EXPECT_EQ(123, timing_->min_playout_delay());
}

TEST_F(VideoReceiveStream2Test, PlayoutDelayPreservesDefaultMaxValue) {
  const int default_max_playout_latency = timing_->max_playout_delay();
  const VideoPlayoutDelay kPlayoutDelayMs = {123, -1};

  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->SetId(0);
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));

  // Ensure that -1 preserves default maximum value from `timing_`.
  EXPECT_EQ(kPlayoutDelayMs.min_ms, timing_->min_playout_delay());
  EXPECT_NE(kPlayoutDelayMs.max_ms, timing_->max_playout_delay());
  EXPECT_EQ(default_max_playout_latency, timing_->max_playout_delay());
}

TEST_F(VideoReceiveStream2Test, PlayoutDelayPreservesDefaultMinValue) {
  const int default_min_playout_latency = timing_->min_playout_delay();
  const VideoPlayoutDelay kPlayoutDelayMs = {-1, 321};

  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->SetId(0);
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));

  // Ensure that -1 preserves default minimum value from `timing_`.
  EXPECT_NE(kPlayoutDelayMs.min_ms, timing_->min_playout_delay());
  EXPECT_EQ(kPlayoutDelayMs.max_ms, timing_->max_playout_delay());
  EXPECT_EQ(default_min_playout_latency, timing_->min_playout_delay());
}

TEST_F(VideoReceiveStream2Test, MaxCompositionDelayNotSetByDefault) {
  // Default with no playout delay set.
  std::unique_ptr<FrameObjectFake> test_frame0(new FrameObjectFake());
  test_frame0->SetId(0);
  video_receive_stream_->OnCompleteFrame(std::move(test_frame0));
  EXPECT_FALSE(timing_->MaxCompositionDelayInFrames());

  // Max composition delay not set for playout delay 0,0.
  std::unique_ptr<FrameObjectFake> test_frame1(new FrameObjectFake());
  test_frame1->SetId(1);
  test_frame1->SetPlayoutDelay({0, 0});
  video_receive_stream_->OnCompleteFrame(std::move(test_frame1));
  EXPECT_FALSE(timing_->MaxCompositionDelayInFrames());

  // Max composition delay not set for playout delay X,Y, where X,Y>0.
  std::unique_ptr<FrameObjectFake> test_frame2(new FrameObjectFake());
  test_frame2->SetId(2);
  test_frame2->SetPlayoutDelay({10, 30});
  video_receive_stream_->OnCompleteFrame(std::move(test_frame2));
  EXPECT_FALSE(timing_->MaxCompositionDelayInFrames());
}

TEST_F(VideoReceiveStream2Test, MaxCompositionDelaySetFromMaxPlayoutDelay) {
  // Max composition delay set if playout delay X,Y, where X=0,Y>0.
  const VideoPlayoutDelay kPlayoutDelayMs = {0, 50};
  const int kExpectedMaxCompositionDelayInFrames = 3;  // ~50 ms at 60 fps.
  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->SetId(0);
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_EQ(kExpectedMaxCompositionDelayInFrames,
            timing_->MaxCompositionDelayInFrames());
}

class VideoReceiveStream2TestWithFakeDecoder : public ::testing::Test {
 public:
  VideoReceiveStream2TestWithFakeDecoder()
      : fake_decoder_factory_(
            []() { return std::make_unique<test::FakeDecoder>(); }),
        task_queue_factory_(CreateDefaultTaskQueueFactory()),
        config_(&mock_transport_, &fake_decoder_factory_),
        call_stats_(Clock::GetRealTimeClock(), loop_.task_queue()) {}
  ~VideoReceiveStream2TestWithFakeDecoder() override {
    if (video_receive_stream_)
      video_receive_stream_->UnregisterFromTransport();
  }

  void SetUp() override {
    config_.rtp.remote_ssrc = 1111;
    config_.rtp.local_ssrc = 2222;
    config_.renderer = &fake_renderer_;
    VideoReceiveStream::Decoder fake_decoder;
    fake_decoder.payload_type = 99;
    fake_decoder.video_format = SdpVideoFormat("VP8");
    config_.decoders.push_back(fake_decoder);
    clock_ = Clock::GetRealTimeClock();
    ReCreateReceiveStream(VideoReceiveStream::RecordingState());
  }

  void ReCreateReceiveStream(VideoReceiveStream::RecordingState state) {
    constexpr int kDefaultNumCpuCores = 2;
    if (video_receive_stream_) {
      video_receive_stream_->UnregisterFromTransport();
      video_receive_stream_ = nullptr;
    }
    timing_ = new VCMTiming(clock_);
    video_receive_stream_.reset(new webrtc::internal::VideoReceiveStream2(
        task_queue_factory_.get(), &fake_call_, kDefaultNumCpuCores,
        &packet_router_, config_.Copy(), &call_stats_, clock_, timing_,
        &nack_periodic_processor_));
    video_receive_stream_->RegisterWithTransport(
        &rtp_stream_receiver_controller_);
    video_receive_stream_->SetAndGetRecordingState(std::move(state), false);
  }

 protected:
  test::RunLoop loop_;
  test::FunctionVideoDecoderFactory fake_decoder_factory_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  NackPeriodicProcessor nack_periodic_processor_;
  VideoReceiveStream::Config config_;
  internal::CallStats call_stats_;
  cricket::FakeVideoRenderer fake_renderer_;
  MockTransport mock_transport_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  cricket::FakeCall fake_call_;
  std::unique_ptr<webrtc::internal::VideoReceiveStream2> video_receive_stream_;
  Clock* clock_;
  VCMTiming* timing_;
};

TEST_F(VideoReceiveStream2TestWithFakeDecoder, PassesNtpTime) {
  const int64_t kNtpTimestamp = 12345;
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->SetId(0);
  test_frame->SetNtpTime(kNtpTimestamp);

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));
  EXPECT_EQ(kNtpTimestamp, fake_renderer_.ntp_time_ms());
}

TEST_F(VideoReceiveStream2TestWithFakeDecoder, PassesRotation) {
  const webrtc::VideoRotation kRotation = webrtc::kVideoRotation_180;
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->SetId(0);
  test_frame->SetRotation(kRotation);

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));

  EXPECT_EQ(kRotation, fake_renderer_.rotation());
}

TEST_F(VideoReceiveStream2TestWithFakeDecoder, PassesPacketInfos) {
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->SetId(0);
  RtpPacketInfos packet_infos = CreatePacketInfos(3);
  test_frame->SetPacketInfos(packet_infos);

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));

  EXPECT_THAT(fake_renderer_.packet_infos(), ElementsAreArray(packet_infos));
}

TEST_F(VideoReceiveStream2TestWithFakeDecoder, RenderedFrameUpdatesGetSources) {
  constexpr uint32_t kSsrc = 1111;
  constexpr uint32_t kCsrc = 9001;
  constexpr uint32_t kRtpTimestamp = 12345;

  // Prepare one video frame with per-packet information.
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->SetId(0);
  RtpPacketInfos packet_infos;
  {
    RtpPacketInfos::vector_type infos;

    RtpPacketInfo info;
    info.set_ssrc(kSsrc);
    info.set_csrcs({kCsrc});
    info.set_rtp_timestamp(kRtpTimestamp);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(5000));
    infos.push_back(info);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(3000));
    infos.push_back(info);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(2000));
    infos.push_back(info);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(1000));
    infos.push_back(info);

    packet_infos = RtpPacketInfos(std::move(infos));
  }
  test_frame->SetPacketInfos(packet_infos);

  // Start receive stream.
  video_receive_stream_->Start();
  EXPECT_THAT(video_receive_stream_->GetSources(), IsEmpty());

  // Render one video frame.
  int64_t timestamp_ms_min = clock_->TimeInMilliseconds();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));
  int64_t timestamp_ms_max = clock_->TimeInMilliseconds();

  // Verify that the per-packet information is passed to the renderer.
  EXPECT_THAT(fake_renderer_.packet_infos(), ElementsAreArray(packet_infos));

  // Verify that the per-packet information also updates `GetSources()`.
  std::vector<RtpSource> sources = video_receive_stream_->GetSources();
  ASSERT_THAT(sources, SizeIs(2));
  {
    auto it = std::find_if(sources.begin(), sources.end(),
                           [](const RtpSource& source) {
                             return source.source_type() == RtpSourceType::SSRC;
                           });
    ASSERT_NE(it, sources.end());

    EXPECT_EQ(it->source_id(), kSsrc);
    EXPECT_EQ(it->source_type(), RtpSourceType::SSRC);
    EXPECT_EQ(it->rtp_timestamp(), kRtpTimestamp);
    EXPECT_GE(it->timestamp_ms(), timestamp_ms_min);
    EXPECT_LE(it->timestamp_ms(), timestamp_ms_max);
  }
  {
    auto it = std::find_if(sources.begin(), sources.end(),
                           [](const RtpSource& source) {
                             return source.source_type() == RtpSourceType::CSRC;
                           });
    ASSERT_NE(it, sources.end());

    EXPECT_EQ(it->source_id(), kCsrc);
    EXPECT_EQ(it->source_type(), RtpSourceType::CSRC);
    EXPECT_EQ(it->rtp_timestamp(), kRtpTimestamp);
    EXPECT_GE(it->timestamp_ms(), timestamp_ms_min);
    EXPECT_LE(it->timestamp_ms(), timestamp_ms_max);
  }
}

std::unique_ptr<FrameObjectFake> MakeFrameWithResolution(
    VideoFrameType frame_type,
    int picture_id,
    int width,
    int height) {
  auto frame = std::make_unique<FrameObjectFake>();
  frame->SetPayloadType(99);
  frame->SetId(picture_id);
  frame->SetFrameType(frame_type);
  frame->_encodedWidth = width;
  frame->_encodedHeight = height;
  return frame;
}

std::unique_ptr<FrameObjectFake> MakeFrame(VideoFrameType frame_type,
                                           int picture_id) {
  return MakeFrameWithResolution(frame_type, picture_id, 320, 240);
}

TEST_F(VideoReceiveStream2TestWithFakeDecoder,
       PassesFrameWhenEncodedFramesCallbackSet) {
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_->Start();
  // Expect a keyframe request to be generated
  EXPECT_CALL(mock_transport_, SendRtcp);
  EXPECT_CALL(callback, Call);
  video_receive_stream_->SetAndGetRecordingState(
      VideoReceiveStream::RecordingState(callback.AsStdFunction()), true);
  video_receive_stream_->OnCompleteFrame(
      MakeFrame(VideoFrameType::kVideoFrameKey, 0));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));
  video_receive_stream_->Stop();
}

TEST_F(VideoReceiveStream2TestWithFakeDecoder,
       MovesEncodedFrameDispatchStateWhenReCreating) {
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_->Start();
  // Expect a key frame request over RTCP.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(1);
  video_receive_stream_->SetAndGetRecordingState(
      VideoReceiveStream::RecordingState(callback.AsStdFunction()), true);
  video_receive_stream_->Stop();
  VideoReceiveStream::RecordingState old_state =
      video_receive_stream_->SetAndGetRecordingState(
          VideoReceiveStream::RecordingState(), false);
  ReCreateReceiveStream(std::move(old_state));
  video_receive_stream_->Stop();
}

class VideoReceiveStream2TestWithSimulatedClock
    : public ::testing::TestWithParam<int> {
 public:
  class FakeRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    void SignalDoneAfterFrames(int num_frames_received) {
      signal_after_frame_count_ = num_frames_received;
      if (frame_count_ == signal_after_frame_count_)
        event_.Set();
    }

    void OnFrame(const webrtc::VideoFrame& frame) override {
      if (++frame_count_ == signal_after_frame_count_)
        event_.Set();
    }

    void WaitUntilDone() { event_.Wait(rtc::Event::kForever); }

   private:
    int signal_after_frame_count_ = std::numeric_limits<int>::max();
    int frame_count_ = 0;
    rtc::Event event_;
  };

  class FakeDecoder2 : public test::FakeDecoder {
   public:
    explicit FakeDecoder2(std::function<void()> decode_callback)
        : callback_(decode_callback) {}

    int32_t Decode(const EncodedImage& input,
                   bool missing_frames,
                   int64_t render_time_ms) override {
      int32_t result =
          FakeDecoder::Decode(input, missing_frames, render_time_ms);
      callback_();
      return result;
    }

   private:
    std::function<void()> callback_;
  };

  static VideoReceiveStream::Config GetConfig(
      Transport* transport,
      VideoDecoderFactory* decoder_factory,
      rtc::VideoSinkInterface<webrtc::VideoFrame>* renderer) {
    VideoReceiveStream::Config config(transport, decoder_factory);
    config.rtp.remote_ssrc = 1111;
    config.rtp.local_ssrc = 2222;
    config.rtp.nack.rtp_history_ms = GetParam();  // rtx-time.
    config.renderer = renderer;
    VideoReceiveStream::Decoder fake_decoder;
    fake_decoder.payload_type = 99;
    fake_decoder.video_format = SdpVideoFormat("VP8");
    config.decoders.push_back(fake_decoder);
    return config;
  }

  VideoReceiveStream2TestWithSimulatedClock()
      : time_controller_(Timestamp::Millis(4711)),
        fake_decoder_factory_([this] {
          return std::make_unique<FakeDecoder2>([this] { OnFrameDecoded(); });
        }),
        config_(GetConfig(&mock_transport_,
                          &fake_decoder_factory_,
                          &fake_renderer_)),
        call_stats_(time_controller_.GetClock(), loop_.task_queue()),
        video_receive_stream_(time_controller_.GetTaskQueueFactory(),
                              &fake_call_,
                              /*num_cores=*/2,
                              &packet_router_,
                              config_.Copy(),
                              &call_stats_,
                              time_controller_.GetClock(),
                              new VCMTiming(time_controller_.GetClock()),
                              &nack_periodic_processor_) {
    video_receive_stream_.RegisterWithTransport(
        &rtp_stream_receiver_controller_);
    video_receive_stream_.Start();
  }

  ~VideoReceiveStream2TestWithSimulatedClock() override {
    video_receive_stream_.UnregisterFromTransport();
  }

  void OnFrameDecoded() { event_->Set(); }

  void PassEncodedFrameAndWait(std::unique_ptr<EncodedFrame> frame) {
    event_ = std::make_unique<rtc::Event>();
    // This call will eventually end up in the Decoded method where the
    // event is set.
    video_receive_stream_.OnCompleteFrame(std::move(frame));
    event_->Wait(rtc::Event::kForever);
  }

 protected:
  GlobalSimulatedTimeController time_controller_;
  test::RunLoop loop_;
  test::FunctionVideoDecoderFactory fake_decoder_factory_;
  MockTransport mock_transport_;
  FakeRenderer fake_renderer_;
  cricket::FakeCall fake_call_;
  NackPeriodicProcessor nack_periodic_processor_;
  VideoReceiveStream::Config config_;
  internal::CallStats call_stats_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  webrtc::internal::VideoReceiveStream2 video_receive_stream_;
  std::unique_ptr<rtc::Event> event_;
};

TEST_P(VideoReceiveStream2TestWithSimulatedClock,
       RequestsKeyFramesUntilKeyFrameReceived) {
  auto tick = TimeDelta::Millis(GetParam() / 2);
  EXPECT_CALL(mock_transport_, SendRtcp).Times(1).WillOnce(Invoke([this]() {
    loop_.Quit();
    return 0;
  }));
  video_receive_stream_.GenerateKeyFrame();
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 0));
  time_controller_.AdvanceTime(tick);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 1));
  loop_.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_transport_);

  // T+keyframetimeout: still no key frame received, expect key frame request
  // sent again.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(1).WillOnce(Invoke([this]() {
    loop_.Quit();
    return 0;
  }));
  time_controller_.AdvanceTime(tick);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 2));
  loop_.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_transport_);

  // T+keyframetimeout: now send a key frame - we should not observe new key
  // frame requests after this.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(0);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameKey, 3));
  time_controller_.AdvanceTime(2 * tick);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 4));
  loop_.PostTask([this]() { loop_.Quit(); });
  loop_.Run();
}

TEST_P(VideoReceiveStream2TestWithSimulatedClock,
       DispatchesEncodedFrameSequenceStartingWithKeyframeWithoutResolution) {
  video_receive_stream_.Start();
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_.SetAndGetRecordingState(
      VideoReceiveStream::RecordingState(callback.AsStdFunction()),
      /*generate_key_frame=*/false);

  InSequence s;
  EXPECT_CALL(
      callback,
      Call(AllOf(
          Property(&RecordableEncodedFrame::resolution,
                   Field(&RecordableEncodedFrame::EncodedResolution::width,
                         test::FakeDecoder::kDefaultWidth)),
          Property(&RecordableEncodedFrame::resolution,
                   Field(&RecordableEncodedFrame::EncodedResolution::height,
                         test::FakeDecoder::kDefaultHeight)))));
  EXPECT_CALL(callback, Call);

  fake_renderer_.SignalDoneAfterFrames(2);
  PassEncodedFrameAndWait(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameKey, 0, 0, 0));
  PassEncodedFrameAndWait(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameDelta, 1, 0, 0));
  fake_renderer_.WaitUntilDone();

  video_receive_stream_.Stop();
}

TEST_P(VideoReceiveStream2TestWithSimulatedClock,
       DispatchesEncodedFrameSequenceStartingWithKeyframeWithResolution) {
  video_receive_stream_.Start();
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_.SetAndGetRecordingState(
      VideoReceiveStream::RecordingState(callback.AsStdFunction()),
      /*generate_key_frame=*/false);

  InSequence s;
  EXPECT_CALL(
      callback,
      Call(AllOf(
          Property(
              &RecordableEncodedFrame::resolution,
              Field(&RecordableEncodedFrame::EncodedResolution::width, 1080)),
          Property(&RecordableEncodedFrame::resolution,
                   Field(&RecordableEncodedFrame::EncodedResolution::height,
                         720)))));
  EXPECT_CALL(callback, Call);

  fake_renderer_.SignalDoneAfterFrames(2);
  PassEncodedFrameAndWait(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameKey, 0, 1080, 720));
  PassEncodedFrameAndWait(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameDelta, 1, 0, 0));
  fake_renderer_.WaitUntilDone();

  video_receive_stream_.Stop();
}

INSTANTIATE_TEST_SUITE_P(
    RtxTime,
    VideoReceiveStream2TestWithSimulatedClock,
    ::testing::Values(internal::VideoReceiveStream2::kMaxWaitForKeyFrameMs,
                      50 /*ms*/));

class VideoReceiveStream2TestWithLazyDecoderCreation : public ::testing::Test {
 public:
  VideoReceiveStream2TestWithLazyDecoderCreation()
      : task_queue_factory_(CreateDefaultTaskQueueFactory()),
        config_(&mock_transport_, &mock_h264_decoder_factory_),
        call_stats_(Clock::GetRealTimeClock(), loop_.task_queue()) {}

  ~VideoReceiveStream2TestWithLazyDecoderCreation() override {
    video_receive_stream_->UnregisterFromTransport();
  }

  void SetUp() override {
    webrtc::test::ScopedFieldTrials field_trials(
        "WebRTC-PreStreamDecoders/max:0/");
    constexpr int kDefaultNumCpuCores = 2;
    config_.rtp.remote_ssrc = 1111;
    config_.rtp.local_ssrc = 2222;
    config_.renderer = &fake_renderer_;
    VideoReceiveStream::Decoder h264_decoder;
    h264_decoder.payload_type = 99;
    h264_decoder.video_format = SdpVideoFormat("H264");
    h264_decoder.video_format.parameters.insert(
        {"sprop-parameter-sets", "Z0IACpZTBYmI,aMljiA=="});
    config_.decoders.clear();
    config_.decoders.push_back(h264_decoder);

    clock_ = Clock::GetRealTimeClock();
    timing_ = new VCMTiming(clock_);

    video_receive_stream_ =
        std::make_unique<webrtc::internal::VideoReceiveStream2>(
            task_queue_factory_.get(), &fake_call_, kDefaultNumCpuCores,
            &packet_router_, config_.Copy(), &call_stats_, clock_, timing_,
            &nack_periodic_processor_);
    video_receive_stream_->RegisterWithTransport(
        &rtp_stream_receiver_controller_);
  }

 protected:
  test::RunLoop loop_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  MockVideoDecoderFactory mock_h264_decoder_factory_;
  NackPeriodicProcessor nack_periodic_processor_;
  VideoReceiveStream::Config config_;
  internal::CallStats call_stats_;
  MockVideoDecoder mock_h264_video_decoder_;
  cricket::FakeVideoRenderer fake_renderer_;
  cricket::FakeCall fake_call_;
  MockTransport mock_transport_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  std::unique_ptr<webrtc::internal::VideoReceiveStream2> video_receive_stream_;
  Clock* clock_;
  VCMTiming* timing_;
};

TEST_F(VideoReceiveStream2TestWithLazyDecoderCreation, LazyDecoderCreation) {
  constexpr uint8_t idr_nalu[] = {0x05, 0xFF, 0xFF, 0xFF};
  RtpPacketToSend rtppacket(nullptr);
  uint8_t* payload = rtppacket.AllocatePayload(sizeof(idr_nalu));
  memcpy(payload, idr_nalu, sizeof(idr_nalu));
  rtppacket.SetMarker(true);
  rtppacket.SetSsrc(1111);
  rtppacket.SetPayloadType(99);
  rtppacket.SetSequenceNumber(1);
  rtppacket.SetTimestamp(0);

  // No decoder is created here.
  EXPECT_CALL(mock_h264_decoder_factory_, CreateVideoDecoder(_)).Times(0);
  video_receive_stream_->Start();

  EXPECT_CALL(mock_h264_decoder_factory_, CreateVideoDecoder(_))
      .WillOnce(Invoke([this](const SdpVideoFormat& format) {
        test::VideoDecoderProxyFactory h264_decoder_factory(
            &mock_h264_video_decoder_);
        return h264_decoder_factory.CreateVideoDecoder(format);
      }));
  rtc::Event init_decode_event;
  EXPECT_CALL(mock_h264_video_decoder_, Configure).WillOnce(WithoutArgs([&] {
    init_decode_event.Set();
    return true;
  }));
  EXPECT_CALL(mock_h264_video_decoder_, RegisterDecodeCompleteCallback(_));
  EXPECT_CALL(mock_h264_video_decoder_, Decode(_, false, _));
  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(rtppacket.data(), rtppacket.size()));
  rtp_stream_receiver_controller_.OnRtpPacket(parsed_packet);
  EXPECT_CALL(mock_h264_video_decoder_, Release());

  // Make sure the decoder thread had a chance to run.
  init_decode_event.Wait(kDefaultTimeOutMs);
}

TEST_F(VideoReceiveStream2TestWithLazyDecoderCreation,
       DeregisterDecoderThatsNotCreated) {
  // No decoder is created here.
  EXPECT_CALL(mock_h264_decoder_factory_, CreateVideoDecoder(_)).Times(0);
  video_receive_stream_->Start();
  video_receive_stream_->Stop();
}

}  // namespace webrtc
