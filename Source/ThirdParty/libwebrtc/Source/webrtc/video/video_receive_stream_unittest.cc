/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_receive_stream.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/video_codecs/video_decoder.h"
#include "call/rtp_stream_receiver_controller.h"
#include "common_video/test/utilities.h"
#include "media/base/fake_video_renderer.h"
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
#include "test/time_controller/simulated_time_controller.h"
#include "test/video_decoder_proxy_factory.h"
#include "video/call_stats.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr int kDefaultTimeOutMs = 50;

class MockTransport : public Transport {
 public:
  MOCK_METHOD(bool,
              SendRtp,
              (const uint8_t*, size_t length, const PacketOptions& options),
              (override));
  MOCK_METHOD(bool, SendRtcp, (const uint8_t*, size_t length), (override));
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MOCK_METHOD(int32_t,
              InitDecode,
              (const VideoCodec*, int32_t number_of_cores),
              (override));
  MOCK_METHOD(int32_t,
              Decode,
              (const EncodedImage& input,
               bool missing_frames,
               int64_t render_time_ms),
              (override));
  MOCK_METHOD(int32_t,
              RegisterDecodeCompleteCallback,
              (DecodedImageCallback*),
              (override));
  MOCK_METHOD(int32_t, Release, (), (override));
  const char* ImplementationName() const { return "MockVideoDecoder"; }
};

class FrameObjectFake : public video_coding::EncodedFrame {
 public:
  void SetPayloadType(uint8_t payload_type) { _payloadType = payload_type; }

  void SetRotation(const VideoRotation& rotation) { rotation_ = rotation; }

  void SetNtpTime(int64_t ntp_time_ms) { ntp_time_ms_ = ntp_time_ms; }

  int64_t ReceivedTime() const override { return 0; }

  int64_t RenderTime() const override { return _renderTimeMs; }
};

}  // namespace

class VideoReceiveStreamTest : public ::testing::Test {
 public:
  VideoReceiveStreamTest()
      : process_thread_(ProcessThread::Create("TestThread")),
        task_queue_factory_(CreateDefaultTaskQueueFactory()),
        config_(&mock_transport_),
        call_stats_(Clock::GetRealTimeClock(), process_thread_.get()),
        h264_decoder_factory_(&mock_h264_video_decoder_) {}

  void SetUp() {
    constexpr int kDefaultNumCpuCores = 2;
    config_.rtp.remote_ssrc = 1111;
    config_.rtp.local_ssrc = 2222;
    config_.renderer = &fake_renderer_;
    config_.decoder_factory = &h264_decoder_factory_;
    VideoReceiveStream::Decoder h264_decoder;
    h264_decoder.payload_type = 99;
    h264_decoder.video_format = SdpVideoFormat("H264");
    h264_decoder.video_format.parameters.insert(
        {"sprop-parameter-sets", "Z0IACpZTBYmI,aMljiA=="});
    config_.decoders.push_back(h264_decoder);

    clock_ = Clock::GetRealTimeClock();
    timing_ = new VCMTiming(clock_);

    video_receive_stream_ =
        std::make_unique<webrtc::internal::VideoReceiveStream>(
            task_queue_factory_.get(), &rtp_stream_receiver_controller_,
            kDefaultNumCpuCores, &packet_router_, config_.Copy(),
            process_thread_.get(), &call_stats_, clock_, timing_);
  }

 protected:
  std::unique_ptr<ProcessThread> process_thread_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  VideoReceiveStream::Config config_;
  CallStats call_stats_;
  MockVideoDecoder mock_h264_video_decoder_;
  test::VideoDecoderProxyFactory h264_decoder_factory_;
  cricket::FakeVideoRenderer fake_renderer_;
  MockTransport mock_transport_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  std::unique_ptr<webrtc::internal::VideoReceiveStream> video_receive_stream_;
  Clock* clock_;
  VCMTiming* timing_;
};

TEST_F(VideoReceiveStreamTest, CreateFrameFromH264FmtpSpropAndIdr) {
  constexpr uint8_t idr_nalu[] = {0x05, 0xFF, 0xFF, 0xFF};
  RtpPacketToSend rtppacket(nullptr);
  uint8_t* payload = rtppacket.AllocatePayload(sizeof(idr_nalu));
  memcpy(payload, idr_nalu, sizeof(idr_nalu));
  rtppacket.SetMarker(true);
  rtppacket.SetSsrc(1111);
  rtppacket.SetPayloadType(99);
  rtppacket.SetSequenceNumber(1);
  rtppacket.SetTimestamp(0);
  rtc::Event init_decode_event_;
  EXPECT_CALL(mock_h264_video_decoder_, InitDecode(_, _))
      .WillOnce(Invoke([&init_decode_event_](const VideoCodec* config,
                                             int32_t number_of_cores) {
        init_decode_event_.Set();
        return 0;
      }));
  EXPECT_CALL(mock_h264_video_decoder_, RegisterDecodeCompleteCallback(_));
  video_receive_stream_->Start();
  EXPECT_CALL(mock_h264_video_decoder_, Decode(_, false, _));
  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(rtppacket.data(), rtppacket.size()));
  rtp_stream_receiver_controller_.OnRtpPacket(parsed_packet);
  EXPECT_CALL(mock_h264_video_decoder_, Release());
  // Make sure the decoder thread had a chance to run.
  init_decode_event_.Wait(kDefaultTimeOutMs);
}

TEST_F(VideoReceiveStreamTest, PlayoutDelay) {
  const VideoPlayoutDelay kPlayoutDelayMs = {123, 321};
  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->id.picture_id = 0;
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

TEST_F(VideoReceiveStreamTest, PlayoutDelayPreservesDefaultMaxValue) {
  const int default_max_playout_latency = timing_->max_playout_delay();
  const VideoPlayoutDelay kPlayoutDelayMs = {123, -1};

  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->id.picture_id = 0;
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));

  // Ensure that -1 preserves default maximum value from |timing_|.
  EXPECT_EQ(kPlayoutDelayMs.min_ms, timing_->min_playout_delay());
  EXPECT_NE(kPlayoutDelayMs.max_ms, timing_->max_playout_delay());
  EXPECT_EQ(default_max_playout_latency, timing_->max_playout_delay());
}

TEST_F(VideoReceiveStreamTest, PlayoutDelayPreservesDefaultMinValue) {
  const int default_min_playout_latency = timing_->min_playout_delay();
  const VideoPlayoutDelay kPlayoutDelayMs = {-1, 321};

  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->id.picture_id = 0;
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));

  // Ensure that -1 preserves default minimum value from |timing_|.
  EXPECT_NE(kPlayoutDelayMs.min_ms, timing_->min_playout_delay());
  EXPECT_EQ(kPlayoutDelayMs.max_ms, timing_->max_playout_delay());
  EXPECT_EQ(default_min_playout_latency, timing_->min_playout_delay());
}

class VideoReceiveStreamTestWithFakeDecoder : public ::testing::Test {
 public:
  VideoReceiveStreamTestWithFakeDecoder()
      : fake_decoder_factory_(
            []() { return std::make_unique<test::FakeDecoder>(); }),
        process_thread_(ProcessThread::Create("TestThread")),
        task_queue_factory_(CreateDefaultTaskQueueFactory()),
        config_(&mock_transport_),
        call_stats_(Clock::GetRealTimeClock(), process_thread_.get()) {}

  void SetUp() {
    config_.rtp.remote_ssrc = 1111;
    config_.rtp.local_ssrc = 2222;
    config_.renderer = &fake_renderer_;
    config_.decoder_factory = &fake_decoder_factory_;
    VideoReceiveStream::Decoder fake_decoder;
    fake_decoder.payload_type = 99;
    fake_decoder.video_format = SdpVideoFormat("VP8");
    config_.decoders.push_back(fake_decoder);
    clock_ = Clock::GetRealTimeClock();
    ReCreateReceiveStream(VideoReceiveStream::RecordingState());
  }

  void ReCreateReceiveStream(VideoReceiveStream::RecordingState state) {
    constexpr int kDefaultNumCpuCores = 2;
    video_receive_stream_ = nullptr;
    timing_ = new VCMTiming(clock_);
    video_receive_stream_.reset(new webrtc::internal::VideoReceiveStream(
        task_queue_factory_.get(), &rtp_stream_receiver_controller_,
        kDefaultNumCpuCores, &packet_router_, config_.Copy(),
        process_thread_.get(), &call_stats_, clock_, timing_));
    video_receive_stream_->SetAndGetRecordingState(std::move(state), false);
  }

 protected:
  test::FunctionVideoDecoderFactory fake_decoder_factory_;
  std::unique_ptr<ProcessThread> process_thread_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  VideoReceiveStream::Config config_;
  CallStats call_stats_;
  cricket::FakeVideoRenderer fake_renderer_;
  MockTransport mock_transport_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  std::unique_ptr<webrtc::internal::VideoReceiveStream> video_receive_stream_;
  Clock* clock_;
  VCMTiming* timing_;
};

TEST_F(VideoReceiveStreamTestWithFakeDecoder, PassesNtpTime) {
  const int64_t kNtpTimestamp = 12345;
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->id.picture_id = 0;
  test_frame->SetNtpTime(kNtpTimestamp);

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));
  EXPECT_EQ(kNtpTimestamp, fake_renderer_.ntp_time_ms());
}

TEST_F(VideoReceiveStreamTestWithFakeDecoder, PassesRotation) {
  const webrtc::VideoRotation kRotation = webrtc::kVideoRotation_180;
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->id.picture_id = 0;
  test_frame->SetRotation(kRotation);

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));

  EXPECT_EQ(kRotation, fake_renderer_.rotation());
}

TEST_F(VideoReceiveStreamTestWithFakeDecoder, PassesPacketInfos) {
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->id.picture_id = 0;
  RtpPacketInfos packet_infos = CreatePacketInfos(3);
  test_frame->SetPacketInfos(packet_infos);

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_TRUE(fake_renderer_.WaitForRenderedFrame(kDefaultTimeOutMs));

  EXPECT_THAT(fake_renderer_.packet_infos(), ElementsAreArray(packet_infos));
}

TEST_F(VideoReceiveStreamTestWithFakeDecoder, RenderedFrameUpdatesGetSources) {
  constexpr uint32_t kSsrc = 1111;
  constexpr uint32_t kCsrc = 9001;
  constexpr uint32_t kRtpTimestamp = 12345;

  // Prepare one video frame with per-packet information.
  auto test_frame = std::make_unique<FrameObjectFake>();
  test_frame->SetPayloadType(99);
  test_frame->id.picture_id = 0;
  RtpPacketInfos packet_infos;
  {
    RtpPacketInfos::vector_type infos;

    RtpPacketInfo info;
    info.set_ssrc(kSsrc);
    info.set_csrcs({kCsrc});
    info.set_rtp_timestamp(kRtpTimestamp);

    info.set_receive_time_ms(clock_->TimeInMilliseconds() - 5000);
    infos.push_back(info);

    info.set_receive_time_ms(clock_->TimeInMilliseconds() - 3000);
    infos.push_back(info);

    info.set_receive_time_ms(clock_->TimeInMilliseconds() - 2000);
    infos.push_back(info);

    info.set_receive_time_ms(clock_->TimeInMilliseconds() - 4000);
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

  // Verify that the per-packet information also updates |GetSources()|.
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

std::unique_ptr<FrameObjectFake> MakeFrame(VideoFrameType frame_type,
                                           int picture_id) {
  auto frame = std::make_unique<FrameObjectFake>();
  frame->SetPayloadType(99);
  frame->id.picture_id = picture_id;
  frame->SetFrameType(frame_type);
  return frame;
}

TEST_F(VideoReceiveStreamTestWithFakeDecoder,
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

TEST_F(VideoReceiveStreamTestWithFakeDecoder,
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

class VideoReceiveStreamTestWithSimulatedClock : public ::testing::Test {
 public:
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
    VideoReceiveStream::Config config(transport);
    config.rtp.remote_ssrc = 1111;
    config.rtp.local_ssrc = 2222;
    config.renderer = renderer;
    config.decoder_factory = decoder_factory;
    VideoReceiveStream::Decoder fake_decoder;
    fake_decoder.payload_type = 99;
    fake_decoder.video_format = SdpVideoFormat("VP8");
    config.decoders.push_back(fake_decoder);
    return config;
  }

  VideoReceiveStreamTestWithSimulatedClock()
      : time_controller_(Timestamp::Millis(4711)),
        fake_decoder_factory_([this] {
          return std::make_unique<FakeDecoder2>([this] { OnFrameDecoded(); });
        }),
        process_thread_(time_controller_.CreateProcessThread("ProcessThread")),
        config_(GetConfig(&mock_transport_,
                          &fake_decoder_factory_,
                          &fake_renderer_)),
        call_stats_(time_controller_.GetClock(), process_thread_.get()),
        video_receive_stream_(time_controller_.GetTaskQueueFactory(),
                              &rtp_stream_receiver_controller_,
                              /*num_cores=*/2,
                              &packet_router_,
                              config_.Copy(),
                              process_thread_.get(),
                              &call_stats_,
                              time_controller_.GetClock(),
                              new VCMTiming(time_controller_.GetClock())) {
    video_receive_stream_.Start();
  }

  void OnFrameDecoded() { event_->Set(); }

  void PassEncodedFrameAndWait(
      std::unique_ptr<video_coding::EncodedFrame> frame) {
      event_ = std::make_unique<rtc::Event>();
      // This call will eventually end up in the Decoded method where the
      // event is set.
      video_receive_stream_.OnCompleteFrame(std::move(frame));
      event_->Wait(rtc::Event::kForever);
  }

 protected:
  GlobalSimulatedTimeController time_controller_;
  test::FunctionVideoDecoderFactory fake_decoder_factory_;
  std::unique_ptr<ProcessThread> process_thread_;
  MockTransport mock_transport_;
  cricket::FakeVideoRenderer fake_renderer_;
  VideoReceiveStream::Config config_;
  CallStats call_stats_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  webrtc::internal::VideoReceiveStream video_receive_stream_;
  std::unique_ptr<rtc::Event> event_;
};

TEST_F(VideoReceiveStreamTestWithSimulatedClock,
       RequestsKeyFramesUntilKeyFrameReceived) {
  auto tick = TimeDelta::Millis(
      internal::VideoReceiveStream::kMaxWaitForKeyFrameMs / 2);
  EXPECT_CALL(mock_transport_, SendRtcp).Times(1);
  video_receive_stream_.GenerateKeyFrame();
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 0));
  time_controller_.AdvanceTime(tick);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 1));
  testing::Mock::VerifyAndClearExpectations(&mock_transport_);

  // T+200ms: still no key frame received, expect key frame request sent again.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(1);
  time_controller_.AdvanceTime(tick);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 2));
  testing::Mock::VerifyAndClearExpectations(&mock_transport_);

  // T+200ms: now send a key frame - we should not observe new key frame
  // requests after this.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(0);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameKey, 3));
  time_controller_.AdvanceTime(2 * tick);
  PassEncodedFrameAndWait(MakeFrame(VideoFrameType::kVideoFrameDelta, 4));
}

}  // namespace webrtc
