/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_video_sender.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "call/rtp_transport_controller_send.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/video_coding/fec_controller_default.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/event.h"
#include "rtc_base/rate_limiter.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "video/call_stats.h"
#include "video/send_delay_stats.h"
#include "video/send_statistics_proxy.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::Unused;

namespace webrtc {
namespace {
const int8_t kPayloadType = 96;
const uint32_t kSsrc1 = 12345;
const uint32_t kSsrc2 = 23456;
const uint32_t kRtxSsrc1 = 34567;
const uint32_t kRtxSsrc2 = 45678;
const int16_t kInitialPictureId1 = 222;
const int16_t kInitialPictureId2 = 44;
const int16_t kInitialTl0PicIdx1 = 99;
const int16_t kInitialTl0PicIdx2 = 199;
const int64_t kRetransmitWindowSizeMs = 500;
const int kTransportsSequenceExtensionId = 7;

class MockRtcpIntraFrameObserver : public RtcpIntraFrameObserver {
 public:
  MOCK_METHOD1(OnReceivedIntraFrameRequest, void(uint32_t));
};

RtpSenderObservers CreateObservers(
    RtcpRttStats* rtcp_rtt_stats,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpStatisticsCallback* rtcp_stats,
    ReportBlockDataObserver* report_block_data_observer,
    StreamDataCountersCallback* rtp_stats,
    BitrateStatisticsObserver* bitrate_observer,
    FrameCountObserver* frame_count_observer,
    RtcpPacketTypeCounterObserver* rtcp_type_observer,
    SendSideDelayObserver* send_delay_observer,
    SendPacketObserver* send_packet_observer) {
  RtpSenderObservers observers;
  observers.rtcp_rtt_stats = rtcp_rtt_stats;
  observers.intra_frame_callback = intra_frame_callback;
  observers.rtcp_loss_notification_observer = nullptr;
  observers.rtcp_stats = rtcp_stats;
  observers.report_block_data_observer = report_block_data_observer;
  observers.rtp_stats = rtp_stats;
  observers.bitrate_observer = bitrate_observer;
  observers.frame_count_observer = frame_count_observer;
  observers.rtcp_type_observer = rtcp_type_observer;
  observers.send_delay_observer = send_delay_observer;
  observers.send_packet_observer = send_packet_observer;
  return observers;
}

BitrateConstraints GetBitrateConfig() {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 30000;
  bitrate_config.start_bitrate_bps = 300000;
  bitrate_config.max_bitrate_bps = 3000000;
  return bitrate_config;
}

VideoSendStream::Config CreateVideoSendStreamConfig(
    Transport* transport,
    const std::vector<uint32_t>& ssrcs,
    const std::vector<uint32_t>& rtx_ssrcs,
    int payload_type) {
  VideoSendStream::Config config(transport);
  config.rtp.ssrcs = ssrcs;
  config.rtp.rtx.ssrcs = rtx_ssrcs;
  config.rtp.payload_type = payload_type;
  config.rtp.rtx.payload_type = payload_type + 1;
  config.rtp.nack.rtp_history_ms = 1000;
  config.rtp.extensions.emplace_back(RtpExtension::kTransportSequenceNumberUri,
                                     kTransportsSequenceExtensionId);
  return config;
}

class RtpVideoSenderTestFixture {
 public:
  RtpVideoSenderTestFixture(
      const std::vector<uint32_t>& ssrcs,
      const std::vector<uint32_t>& rtx_ssrcs,
      int payload_type,
      const std::map<uint32_t, RtpPayloadState>& suspended_payload_states,
      FrameCountObserver* frame_count_observer)
      : clock_(1000000),
        config_(CreateVideoSendStreamConfig(&transport_,
                                            ssrcs,
                                            rtx_ssrcs,
                                            payload_type)),
        send_delay_stats_(&clock_),
        bitrate_config_(GetBitrateConfig()),
        task_queue_factory_(CreateDefaultTaskQueueFactory()),
        transport_controller_(&clock_,
                              &event_log_,
                              nullptr,
                              nullptr,
                              bitrate_config_,
                              ProcessThread::Create("PacerThread"),
                              task_queue_factory_.get()),
        process_thread_(ProcessThread::Create("test_thread")),
        call_stats_(&clock_, process_thread_.get()),
        stats_proxy_(&clock_,
                     config_,
                     VideoEncoderConfig::ContentType::kRealtimeVideo),
        retransmission_rate_limiter_(&clock_, kRetransmitWindowSizeMs) {
    std::map<uint32_t, RtpState> suspended_ssrcs;
    router_ = absl::make_unique<RtpVideoSender>(
        &clock_, suspended_ssrcs, suspended_payload_states, config_.rtp,
        config_.rtcp_report_interval_ms, &transport_,
        CreateObservers(&call_stats_, &encoder_feedback_, &stats_proxy_,
                        &stats_proxy_, &stats_proxy_, &stats_proxy_,
                        frame_count_observer, &stats_proxy_, &stats_proxy_,
                        &send_delay_stats_),
        &transport_controller_, &event_log_, &retransmission_rate_limiter_,
        absl::make_unique<FecControllerDefault>(&clock_), nullptr,
        CryptoOptions{});
  }
  RtpVideoSenderTestFixture(
      const std::vector<uint32_t>& ssrcs,
      const std::vector<uint32_t>& rtx_ssrcs,
      int payload_type,
      const std::map<uint32_t, RtpPayloadState>& suspended_payload_states)
      : RtpVideoSenderTestFixture(ssrcs,
                                  rtx_ssrcs,
                                  payload_type,
                                  suspended_payload_states,
                                  /*frame_count_observer=*/nullptr) {}

  RtpVideoSender* router() { return router_.get(); }
  MockTransport& transport() { return transport_; }
  SimulatedClock& clock() { return clock_; }

 private:
  NiceMock<MockTransport> transport_;
  NiceMock<MockRtcpIntraFrameObserver> encoder_feedback_;
  SimulatedClock clock_;
  RtcEventLogNull event_log_;
  VideoSendStream::Config config_;
  SendDelayStats send_delay_stats_;
  BitrateConstraints bitrate_config_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  RtpTransportControllerSend transport_controller_;
  std::unique_ptr<ProcessThread> process_thread_;
  CallStats call_stats_;
  SendStatisticsProxy stats_proxy_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<RtpVideoSender> router_;
};
}  // namespace

TEST(RtpVideoSenderTest, SendOnOneModule) {
  constexpr uint8_t kPayload = 'a';
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(1);
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  encoded_image.SetEncodedData(EncodedImageBuffer::Create(&kPayload, 1));

  RtpVideoSenderTestFixture test({kSsrc1}, {kRtxSsrc1}, kPayloadType, {});
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  test.router()->SetActive(true);
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  test.router()->SetActive(false);
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  test.router()->SetActive(true);
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);
}

TEST(RtpVideoSenderTest, SendSimulcastSetActive) {
  constexpr uint8_t kPayload = 'a';
  EncodedImage encoded_image_1;
  encoded_image_1.SetTimestamp(1);
  encoded_image_1.capture_time_ms_ = 2;
  encoded_image_1._frameType = VideoFrameType::kVideoFrameKey;
  encoded_image_1.SetEncodedData(EncodedImageBuffer::Create(&kPayload, 1));

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, {kRtxSsrc1, kRtxSsrc2},
                                 kPayloadType, {});

  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  test.router()->SetActive(true);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);

  EncodedImage encoded_image_2(encoded_image_1);
  encoded_image_2.SetSpatialIndex(1);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_2, &codec_info, nullptr)
                .error);

  // Inactive.
  test.router()->SetActive(false);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_2, &codec_info, nullptr)
                .error);
}

// Tests how setting individual rtp modules to active affects the overall
// behavior of the payload router. First sets one module to active and checks
// that outgoing data can be sent on this module, and checks that no data can
// be sent if both modules are inactive.
TEST(RtpVideoSenderTest, SendSimulcastSetActiveModules) {
  constexpr uint8_t kPayload = 'a';
  EncodedImage encoded_image_1;
  encoded_image_1.SetTimestamp(1);
  encoded_image_1.capture_time_ms_ = 2;
  encoded_image_1._frameType = VideoFrameType::kVideoFrameKey;
  encoded_image_1.SetEncodedData(EncodedImageBuffer::Create(&kPayload, 1));

  EncodedImage encoded_image_2(encoded_image_1);
  encoded_image_2.SetSpatialIndex(1);

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, {kRtxSsrc1, kRtxSsrc2},
                                 kPayloadType, {});
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  // Only setting one stream to active will still set the payload router to
  // active and allow sending data on the active stream.
  std::vector<bool> active_modules({true, false});
  test.router()->SetActiveModules(active_modules);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);

  // Setting both streams to inactive will turn the payload router to
  // inactive.
  active_modules = {false, false};
  test.router()->SetActiveModules(active_modules);
  // An incoming encoded image will not ask the module to send outgoing data
  // because the payload router is inactive.
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);
}

TEST(RtpVideoSenderTest, CreateWithNoPreviousStates) {
  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, {kRtxSsrc1, kRtxSsrc2},
                                 kPayloadType, {});
  test.router()->SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      test.router()->GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_NE(initial_states.find(kSsrc1), initial_states.end());
  EXPECT_NE(initial_states.find(kSsrc2), initial_states.end());
}

TEST(RtpVideoSenderTest, CreateWithPreviousStates) {
  const int64_t kState1SharedFrameId = 123;
  const int64_t kState2SharedFrameId = 234;
  RtpPayloadState state1;
  state1.picture_id = kInitialPictureId1;
  state1.tl0_pic_idx = kInitialTl0PicIdx1;
  state1.shared_frame_id = kState1SharedFrameId;
  RtpPayloadState state2;
  state2.picture_id = kInitialPictureId2;
  state2.tl0_pic_idx = kInitialTl0PicIdx2;
  state2.shared_frame_id = kState2SharedFrameId;
  std::map<uint32_t, RtpPayloadState> states = {{kSsrc1, state1},
                                                {kSsrc2, state2}};

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, {kRtxSsrc1, kRtxSsrc2},
                                 kPayloadType, states);
  test.router()->SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      test.router()->GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_EQ(kInitialPictureId1, initial_states[kSsrc1].picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1, initial_states[kSsrc1].tl0_pic_idx);
  EXPECT_EQ(kInitialPictureId2, initial_states[kSsrc2].picture_id);
  EXPECT_EQ(kInitialTl0PicIdx2, initial_states[kSsrc2].tl0_pic_idx);
  EXPECT_EQ(kState2SharedFrameId, initial_states[kSsrc1].shared_frame_id);
  EXPECT_EQ(kState2SharedFrameId, initial_states[kSsrc2].shared_frame_id);
}

TEST(RtpVideoSenderTest, FrameCountCallbacks) {
  class MockFrameCountObserver : public FrameCountObserver {
   public:
    MOCK_METHOD2(FrameCountUpdated,
                 void(const FrameCounts& frame_counts, uint32_t ssrc));
  } callback;

  RtpVideoSenderTestFixture test({kSsrc1}, {kRtxSsrc1}, kPayloadType, {},
                                 &callback);

  constexpr uint8_t kPayload = 'a';
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(1);
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  encoded_image.SetEncodedData(EncodedImageBuffer::Create(&kPayload, 1));

  encoded_image._frameType = VideoFrameType::kVideoFrameKey;

  // No callbacks when not active.
  EXPECT_CALL(callback, FrameCountUpdated).Times(0);
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);
  ::testing::Mock::VerifyAndClearExpectations(&callback);

  test.router()->SetActive(true);

  FrameCounts frame_counts;
  EXPECT_CALL(callback, FrameCountUpdated(_, kSsrc1))
      .WillOnce(SaveArg<0>(&frame_counts));
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  EXPECT_EQ(1, frame_counts.key_frames);
  EXPECT_EQ(0, frame_counts.delta_frames);

  ::testing::Mock::VerifyAndClearExpectations(&callback);

  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;
  EXPECT_CALL(callback, FrameCountUpdated(_, kSsrc1))
      .WillOnce(SaveArg<0>(&frame_counts));
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  EXPECT_EQ(1, frame_counts.key_frames);
  EXPECT_EQ(1, frame_counts.delta_frames);
}

// Integration test verifying that ack of packet via TransportFeedback means
// that the packet is removed from RtpPacketHistory and won't be retransmitted
// again.
// TODO(crbug.com/webrtc/10873): Re-enable on iOS
#if defined(WEBRTC_IOS)
TEST(RtpVideoSenderTest, DISABLED_DoesNotRetrasmitAckedPackets) {
#else
TEST(RtpVideoSenderTest, DoesNotRetrasmitAckedPackets) {
#endif
  const int64_t kTimeoutMs = 500;

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, {kRtxSsrc1, kRtxSsrc2},
                                 kPayloadType, {});
  test.router()->SetActive(true);

  constexpr uint8_t kPayload = 'a';
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(1);
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  encoded_image.SetEncodedData(EncodedImageBuffer::Create(&kPayload, 1));

  // Send two tiny images, mapping to two RTP packets. Capture sequence numbers.
  rtc::Event event;
  std::vector<uint16_t> rtp_sequence_numbers;
  std::vector<uint16_t> transport_sequence_numbers;
  EXPECT_CALL(test.transport(), SendRtp)
      .Times(2)
      .WillRepeatedly(
          [&event, &rtp_sequence_numbers, &transport_sequence_numbers](
              const uint8_t* packet, size_t length,
              const PacketOptions& options) {
            RtpPacket rtp_packet;
            EXPECT_TRUE(rtp_packet.Parse(packet, length));
            rtp_sequence_numbers.push_back(rtp_packet.SequenceNumber());
            transport_sequence_numbers.push_back(options.packet_id);
            if (transport_sequence_numbers.size() == 2) {
              event.Set();
            }
            return true;
          });
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);
  encoded_image.SetTimestamp(2);
  encoded_image.capture_time_ms_ = 3;
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);
  const int64_t send_time_ms = test.clock().TimeInMilliseconds();

  test.clock().AdvanceTimeMilliseconds(33);

  ASSERT_TRUE(event.Wait(kTimeoutMs));

  // Construct a NACK message for requesting retransmission of both packet.
  rtcp::Nack nack;
  nack.SetMediaSsrc(kSsrc1);
  nack.SetPacketIds(rtp_sequence_numbers);
  rtc::Buffer nack_buffer = nack.Build();

  std::vector<uint16_t> retransmitted_rtp_sequence_numbers;
  EXPECT_CALL(test.transport(), SendRtp)
      .Times(2)
      .WillRepeatedly([&event, &retransmitted_rtp_sequence_numbers](
                          const uint8_t* packet, size_t length,
                          const PacketOptions& options) {
        RtpPacket rtp_packet;
        EXPECT_TRUE(rtp_packet.Parse(packet, length));
        EXPECT_EQ(rtp_packet.Ssrc(), kRtxSsrc1);
        // Capture the retransmitted sequence number from the RTX header.
        rtc::ArrayView<const uint8_t> payload = rtp_packet.payload();
        retransmitted_rtp_sequence_numbers.push_back(
            ByteReader<uint16_t>::ReadBigEndian(payload.data()));
        if (retransmitted_rtp_sequence_numbers.size() == 2) {
          event.Set();
        }
        return true;
      });
  test.router()->DeliverRtcp(nack_buffer.data(), nack_buffer.size());
  ASSERT_TRUE(event.Wait(kTimeoutMs));

  // Verify that both packets were retransmitted.
  EXPECT_EQ(retransmitted_rtp_sequence_numbers, rtp_sequence_numbers);

  // Simulate transport feedback indicating fist packet received, next packet
  // lost.
  PacketFeedback received_packet_feedback(test.clock().TimeInMilliseconds(),
                                          transport_sequence_numbers[0]);
  received_packet_feedback.rtp_sequence_number = rtp_sequence_numbers[0];
  received_packet_feedback.ssrc = kSsrc1;
  received_packet_feedback.send_time_ms = send_time_ms;

  PacketFeedback lost_packet_feedback(PacketFeedback::kNotReceived,
                                      transport_sequence_numbers[1]);
  lost_packet_feedback.rtp_sequence_number = rtp_sequence_numbers[1];
  lost_packet_feedback.ssrc = kSsrc1;
  lost_packet_feedback.send_time_ms = send_time_ms;
  std::vector<PacketFeedback> feedback_vector = {received_packet_feedback,
                                                 lost_packet_feedback};

  test.router()->OnPacketFeedbackVector(feedback_vector);

  // Advance time to make sure retransmission would be allowed and try again.
  // This time the retransmission should not happen for the first packet since
  // the history has been notified of the ack and removed the packet. The
  // second packet, included in the feedback but not marked as received, should
  // still be retransmitted.
  test.clock().AdvanceTimeMilliseconds(33);
  EXPECT_CALL(test.transport(), SendRtp)
      .WillOnce([&event, &lost_packet_feedback](const uint8_t* packet,
                                                size_t length,
                                                const PacketOptions& options) {
        RtpPacket rtp_packet;
        EXPECT_TRUE(rtp_packet.Parse(packet, length));
        EXPECT_EQ(rtp_packet.Ssrc(), kRtxSsrc1);
        // Capture the retransmitted sequence number from the RTX header.
        rtc::ArrayView<const uint8_t> payload = rtp_packet.payload();
        EXPECT_EQ(lost_packet_feedback.rtp_sequence_number,
                  ByteReader<uint16_t>::ReadBigEndian(payload.data()));
        event.Set();
        return true;
      });
  test.router()->DeliverRtcp(nack_buffer.data(), nack_buffer.size());
  ASSERT_TRUE(event.Wait(kTimeoutMs));
}

// Integration test verifying that retransmissions are sent for packets which
// can be detected as lost early, using transport wide feedback.
TEST(RtpVideoSenderTest, EarlyRetransmits) {
  const int64_t kTimeoutMs = 500;

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, {kRtxSsrc1, kRtxSsrc2},
                                 kPayloadType, {});
  test.router()->SetActive(true);

  constexpr uint8_t kPayload = 'a';
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(1);
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  encoded_image.Allocate(1);
  encoded_image.data()[0] = kPayload;
  encoded_image.set_size(1);
  encoded_image.SetSpatialIndex(0);

  CodecSpecificInfo codec_specific;
  codec_specific.codecType = VideoCodecType::kVideoCodecGeneric;

  // Send two tiny images, mapping to single RTP packets. Capture sequence
  // numbers.
  rtc::Event event;
  uint16_t frame1_rtp_sequence_number = 0;
  uint16_t frame1_transport_sequence_number = 0;
  EXPECT_CALL(test.transport(), SendRtp)
      .WillOnce([&event, &frame1_rtp_sequence_number,
                 &frame1_transport_sequence_number](
                    const uint8_t* packet, size_t length,
                    const PacketOptions& options) {
        RtpPacket rtp_packet;
        EXPECT_TRUE(rtp_packet.Parse(packet, length));
        frame1_rtp_sequence_number = rtp_packet.SequenceNumber();
        frame1_transport_sequence_number = options.packet_id;
        EXPECT_EQ(rtp_packet.Ssrc(), kSsrc1);
        event.Set();
        return true;
      });
  EXPECT_EQ(test.router()
                ->OnEncodedImage(encoded_image, &codec_specific, nullptr)
                .error,
            EncodedImageCallback::Result::OK);
  const int64_t send_time_ms = test.clock().TimeInMilliseconds();

  test.clock().AdvanceTimeMilliseconds(33);
  ASSERT_TRUE(event.Wait(kTimeoutMs));

  uint16_t frame2_rtp_sequence_number = 0;
  uint16_t frame2_transport_sequence_number = 0;
  encoded_image.SetSpatialIndex(1);
  EXPECT_CALL(test.transport(), SendRtp)
      .WillOnce([&event, &frame2_rtp_sequence_number,
                 &frame2_transport_sequence_number](
                    const uint8_t* packet, size_t length,
                    const PacketOptions& options) {
        RtpPacket rtp_packet;
        EXPECT_TRUE(rtp_packet.Parse(packet, length));
        frame2_rtp_sequence_number = rtp_packet.SequenceNumber();
        frame2_transport_sequence_number = options.packet_id;
        EXPECT_EQ(rtp_packet.Ssrc(), kSsrc2);
        event.Set();
        return true;
      });
  EXPECT_EQ(test.router()
                ->OnEncodedImage(encoded_image, &codec_specific, nullptr)
                .error,
            EncodedImageCallback::Result::OK);
  test.clock().AdvanceTimeMilliseconds(33);
  ASSERT_TRUE(event.Wait(kTimeoutMs));

  EXPECT_NE(frame1_transport_sequence_number, frame2_transport_sequence_number);

  // Inject a transport feedback where the packet for the first frame is lost,
  // expect a retransmission for it.
  EXPECT_CALL(test.transport(), SendRtp)
      .WillOnce([&event, &frame1_rtp_sequence_number](
                    const uint8_t* packet, size_t length,
                    const PacketOptions& options) {
        RtpPacket rtp_packet;
        EXPECT_TRUE(rtp_packet.Parse(packet, length));
        EXPECT_EQ(rtp_packet.Ssrc(), kRtxSsrc2);

        // Retransmitted sequence number from the RTX header should match
        // the lost packet.
        rtc::ArrayView<const uint8_t> payload = rtp_packet.payload();
        EXPECT_EQ(ByteReader<uint16_t>::ReadBigEndian(payload.data()),
                  frame1_rtp_sequence_number);
        event.Set();
        return true;
      });

  PacketFeedback first_packet_feedback(PacketFeedback::kNotReceived,
                                       frame1_transport_sequence_number);
  first_packet_feedback.rtp_sequence_number = frame1_rtp_sequence_number;
  first_packet_feedback.ssrc = kSsrc1;
  first_packet_feedback.send_time_ms = send_time_ms;

  PacketFeedback second_packet_feedback(test.clock().TimeInMilliseconds(),
                                        frame2_transport_sequence_number);
  first_packet_feedback.rtp_sequence_number = frame2_rtp_sequence_number;
  first_packet_feedback.ssrc = kSsrc2;
  first_packet_feedback.send_time_ms = send_time_ms + 33;

  std::vector<PacketFeedback> feedback_vector = {first_packet_feedback,
                                                 second_packet_feedback};

  test.router()->OnPacketFeedbackVector(feedback_vector);

  // Wait for pacer to run and send the RTX packet.
  test.clock().AdvanceTimeMilliseconds(33);
  ASSERT_TRUE(event.Wait(kTimeoutMs));
}

TEST(RtpVideoSenderTest, CanSetZeroBitrateWithOverhead) {
  test::ScopedFieldTrials trials("WebRTC-SendSideBwe-WithOverhead/Enabled/");
  RtpVideoSenderTestFixture test({kSsrc1}, {kRtxSsrc1}, kPayloadType, {});

  test.router()->OnBitrateUpdated(/*bitrate_bps*/ 0,
                                  /*fraction_loss*/ 0,
                                  /*rtt*/ 0,
                                  /*framerate*/ 0);
}

TEST(RtpVideoSenderTest, CanSetZeroBitrateWithoutOverhead) {
  RtpVideoSenderTestFixture test({kSsrc1}, {kRtxSsrc1}, kPayloadType, {});

  test.router()->OnBitrateUpdated(/*bitrate_bps*/ 0,
                                  /*fraction_loss*/ 0,
                                  /*rtt*/ 0,
                                  /*framerate*/ 0);
}
}  // namespace webrtc
