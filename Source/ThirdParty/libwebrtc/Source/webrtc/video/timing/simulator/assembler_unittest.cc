/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/assembler.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "api/sequence_checker.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/thread_annotations.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/test/matchers.h"
#include "video/timing/simulator/test/simulated_time_test_fixture.h"

namespace webrtc::video_timing_simulator {
namespace {

constexpr uint8_t kPayloadType = 96;
constexpr uint32_t kSsrc = 123456;
constexpr int kPayloadSize = 1000;

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Property;

class MockAssemblerEvents : public AssemblerEvents {
 public:
  MOCK_METHOD(void,
              OnAssembledFrame,
              (const EncodedFrame& assembled_frame),
              (override));
};

class MockAssembledFrameCallback : public AssembledFrameCallback {
 public:
  MOCK_METHOD(void,
              OnAssembledFrame,
              (std::unique_ptr<EncodedFrame> encoded_frame),
              (override));
};

class RtpPacketReceivedGenerator {
 public:
  struct BuildRtpPacketsForFrameOptions {
    int num_packets;
    bool is_keyframe;
  };

  RtpPacketReceivedGenerator()
      : rtp_header_extension_map_(
            ParsedRtcEventLog::GetDefaultHeaderExtensionMap()),
        arrival_time_(Timestamp::Seconds(10000)),
        rtp_seq_num_(0),
        rtp_timestamp_(0),
        frame_id_(0) {
    frame_dependency_structure_.num_decode_targets = 1;
    frame_dependency_structure_.templates = {
        FrameDependencyTemplate().Dtis("S"),
        FrameDependencyTemplate().Dtis("S").FrameDiffs({1}),
    };
  }

  std::vector<RtpPacketReceived> BuildRtpPacketsForFrame(
      BuildRtpPacketsForFrameOptions options) {
    std::vector<RtpPacketReceived> rtp_packets;
    for (int i = 0; i < options.num_packets; ++i) {
      bool is_first_packet_in_frame = (i == 0);
      bool is_last_packet_in_frame = (i == options.num_packets - 1);

      RtpPacketReceived rtp_packet = BuildBaseRtpPacketReceived();
      rtp_packet.set_arrival_time(arrival_time_);
      // RTP header.
      rtp_packet.SetMarker(is_last_packet_in_frame);
      rtp_packet.SetSequenceNumber(rtp_seq_num_);
      rtp_packet.SetTimestamp(rtp_timestamp_);
      // RTP header extension.
      DependencyDescriptor dependency_descriptor;
      dependency_descriptor.first_packet_in_frame = is_first_packet_in_frame;
      dependency_descriptor.last_packet_in_frame = is_last_packet_in_frame;
      dependency_descriptor.frame_number = frame_id_;
      dependency_descriptor.frame_dependencies =
          frame_dependency_structure_.templates[options.is_keyframe ? 0 : 1];
      if (options.is_keyframe && is_first_packet_in_frame) {
        dependency_descriptor.attached_structure =
            std::make_unique<FrameDependencyStructure>(
                frame_dependency_structure_);
      }
      rtp_packet.SetExtension<RtpDependencyDescriptorExtension>(
          frame_dependency_structure_, dependency_descriptor);
      EXPECT_TRUE(rtp_packet.HasExtension<RtpDependencyDescriptorExtension>());
      // Payload.
      rtp_packet.AllocatePayload(kPayloadSize);

      rtp_packets.push_back(rtp_packet);

      // Increment packet state.
      arrival_time_ += TimeDelta::Millis(33) / options.num_packets;  // ~30 fps.
      ++rtp_seq_num_;
    }

    // Increment frame state.
    rtp_timestamp_ += 3000;  // 30 fps.
    ++frame_id_;

    return rtp_packets;
  }

 private:
  RtpPacketReceived BuildBaseRtpPacketReceived() {
    RtpPacketReceived rtp_packet(&rtp_header_extension_map_);
    rtp_packet.SetPayloadType(kPayloadType);
    rtp_packet.SetSsrc(kSsrc);
    return rtp_packet;
  }

  const RtpHeaderExtensionMap rtp_header_extension_map_;
  FrameDependencyStructure frame_dependency_structure_;

  Timestamp arrival_time_;
  int rtp_seq_num_;
  uint32_t rtp_timestamp_;
  int frame_id_;
};

class AssemblerTest : public SimulatedTimeTestFixture {
 protected:
  AssemblerTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      assembler_ = std::make_unique<Assembler>(env_, kSsrc, &assembler_events_,
                                               &assembled_frame_cb_);
    });
  }
  ~AssemblerTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      assembler_.reset();
    });
  }

  void InsertPacket(const RtpPacketReceived& rtp_packet) {
    SendTask([this, &rtp_packet]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      assembler_->InsertPacket(rtp_packet);
    });
  }

  // Helpers.
  RtpPacketReceivedGenerator rtp_packet_generator_;

  // Expectations.
  // The two callbacks are called sequentially and have almost the same
  // signature, so we only expect on `assembled_frame_cb_` in the tests.
  NiceMock<MockAssemblerEvents> assembler_events_;
  MockAssembledFrameCallback assembled_frame_cb_;

  // Object under test.
  std::unique_ptr<Assembler> assembler_ RTC_GUARDED_BY(queue_ptr_);
};

TEST_F(AssemblerTest, AssemblesSinglePacketKeyframe) {
  std::vector<RtpPacketReceived> rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 1, .is_keyframe = true});

  EXPECT_CALL(assembled_frame_cb_, OnAssembledFrame(EncodedFramePtrWithId(0)));
  InsertPacket(rtp_packets[0]);
}

TEST_F(AssemblerTest, DoesNotAssembleSinglePacketDeltaFrame) {
  std::vector<RtpPacketReceived> rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 1, .is_keyframe = false});

  EXPECT_CALL(assembled_frame_cb_, OnAssembledFrame(_)).Times(0);
  InsertPacket(rtp_packets[0]);
}

TEST_F(AssemblerTest, AssemblesKeyframe) {
  std::vector<RtpPacketReceived> rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 3, .is_keyframe = true});

  EXPECT_CALL(assembled_frame_cb_, OnAssembledFrame(EncodedFramePtrWithId(0)));
  for (const auto& rtp_packet : rtp_packets) {
    InsertPacket(rtp_packet);
  }
}

TEST_F(AssemblerTest, DoesNotAssembleDeltaFrame) {
  std::vector<RtpPacketReceived> rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 3, .is_keyframe = false});

  EXPECT_CALL(assembled_frame_cb_, OnAssembledFrame(_)).Times(0);
  for (const auto& rtp_packet : rtp_packets) {
    InsertPacket(rtp_packet);
  }
}

TEST_F(AssemblerTest, DoesNotAssembleKeyframeWithMissingPackets) {
  std::vector<RtpPacketReceived> rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 3, .is_keyframe = true});

  EXPECT_CALL(assembled_frame_cb_, OnAssembledFrame(_)).Times(0);
  InsertPacket(rtp_packets[0]);
  InsertPacket(rtp_packets[2]);
}

TEST_F(AssemblerTest, AssemblesKeyframeWithReorderedPackets) {
  std::vector<RtpPacketReceived> rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 3, .is_keyframe = true});

  EXPECT_CALL(assembled_frame_cb_, OnAssembledFrame(EncodedFramePtrWithId(0)));
  InsertPacket(rtp_packets[0]);
  InsertPacket(rtp_packets[2]);
  InsertPacket(rtp_packets[1]);
}

TEST_F(AssemblerTest, AssemblesSinglePacketKeyframeAndDeltaFrame) {
  std::vector<RtpPacketReceived> key_rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 1, .is_keyframe = true});
  std::vector<RtpPacketReceived> delta_rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 1, .is_keyframe = false});

  {
    InSequence seq;
    EXPECT_CALL(assembled_frame_cb_,
                OnAssembledFrame(EncodedFramePtrWithId(0)));
    InsertPacket(key_rtp_packets[0]);
    EXPECT_CALL(assembled_frame_cb_,
                OnAssembledFrame(EncodedFramePtrWithId(1)));
    InsertPacket(delta_rtp_packets[0]);
  }
}

TEST_F(AssemblerTest, AssemblesKeyframeAndDeltaFrame) {
  std::vector<RtpPacketReceived> key_rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 5, .is_keyframe = true});
  std::vector<RtpPacketReceived> delta_rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 2, .is_keyframe = false});

  {
    InSequence seq;
    EXPECT_CALL(assembled_frame_cb_,
                OnAssembledFrame(EncodedFramePtrWithId(0)));
    for (const auto& rtp_packet : key_rtp_packets) {
      InsertPacket(rtp_packet);
    }
    EXPECT_CALL(assembled_frame_cb_,
                OnAssembledFrame(EncodedFramePtrWithId(1)));
    for (const auto& rtp_packet : delta_rtp_packets) {
      InsertPacket(rtp_packet);
    }
  }
}

TEST_F(AssemblerTest, DoesNotAssembleDeltaFrameAfterKeyframe) {
  std::vector<RtpPacketReceived> key_rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 5, .is_keyframe = true});
  std::vector<RtpPacketReceived> delta_rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 2, .is_keyframe = false});

  EXPECT_CALL(assembled_frame_cb_, OnAssembledFrame(EncodedFramePtrWithId(0)));
  for (const auto& rtp_packet : delta_rtp_packets) {
    InsertPacket(rtp_packet);
  }
  for (const auto& rtp_packet : key_rtp_packets) {
    InsertPacket(rtp_packet);
  }
}

TEST_F(AssemblerTest, AssemblesKeyframeAndDeltaFramesWithReorderedPacket) {
  std::vector<RtpPacketReceived> key_rtp_packets =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 5, .is_keyframe = true});
  std::vector<RtpPacketReceived> delta_rtp_packets1 =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 2, .is_keyframe = false});
  std::vector<RtpPacketReceived> delta_rtp_packets2 =
      rtp_packet_generator_.BuildRtpPacketsForFrame(
          {.num_packets = 2, .is_keyframe = false});

  {
    InSequence seq;
    EXPECT_CALL(assembled_frame_cb_,
                OnAssembledFrame(EncodedFramePtrWithId(0)));
    for (const auto& rtp_packet : key_rtp_packets) {
      InsertPacket(rtp_packet);
    }
    InsertPacket(delta_rtp_packets1[0]);
    InsertPacket(delta_rtp_packets2[0]);
    EXPECT_CALL(assembled_frame_cb_,
                OnAssembledFrame(EncodedFramePtrWithId(1)));
    InsertPacket(delta_rtp_packets1[1]);
    EXPECT_CALL(assembled_frame_cb_,
                OnAssembledFrame(EncodedFramePtrWithId(2)));
    InsertPacket(delta_rtp_packets2[1]);
  }
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
