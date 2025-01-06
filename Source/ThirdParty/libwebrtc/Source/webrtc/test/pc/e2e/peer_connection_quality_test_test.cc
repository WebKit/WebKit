/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/peer_connection_quality_test.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "api/test/create_network_emulation_manager.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/units/time_delta.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::testing::Eq;
using ::testing::Test;

using ::webrtc::webrtc_pc_e2e::PeerConfigurer;

// Remove files and directories in a directory non-recursively.
void CleanDir(absl::string_view dir, size_t expected_output_files_count) {
  std::optional<std::vector<std::string>> dir_content =
      test::ReadDirectory(dir);
  if (expected_output_files_count == 0) {
    ASSERT_FALSE(dir_content.has_value()) << "Empty directory is expected";
  } else {
    ASSERT_TRUE(dir_content.has_value()) << "Test directory is empty!";
    EXPECT_EQ(dir_content->size(), expected_output_files_count);
    for (const auto& entry : *dir_content) {
      if (test::DirExists(entry)) {
        EXPECT_TRUE(test::RemoveDir(entry))
            << "Failed to remove sub directory: " << entry;
      } else if (test::FileExists(entry)) {
        EXPECT_TRUE(test::RemoveFile(entry))
            << "Failed to remove file: " << entry;
      } else {
        FAIL() << "Can't remove unknown file type: " << entry;
      }
    }
  }
  EXPECT_TRUE(test::RemoveDir(dir)) << "Failed to remove directory: " << dir;
}

class PeerConnectionE2EQualityTestTest : public Test {
 protected:
  ~PeerConnectionE2EQualityTestTest() override = default;

  void SetUp() override {
    // Create an empty temporary directory for this test.
    test_directory_ = test::JoinFilename(
        test::OutputPath(),
        "TestDir_PeerConnectionE2EQualityTestTest_" +
            std::string(
                testing::UnitTest::GetInstance()->current_test_info()->name()));
    test::CreateDir(test_directory_);
  }

  void TearDown() override {
    CleanDir(test_directory_, expected_output_files_count_);
  }

  void ExpectOutputFilesCount(size_t count) {
    expected_output_files_count_ = count;
  }

  std::string test_directory_;
  size_t expected_output_files_count_ = 0;
};

TEST_F(PeerConnectionE2EQualityTestTest, OutputVideoIsDumpedWhenRequested) {
  std::unique_ptr<NetworkEmulationManager> network_emulation =
      CreateNetworkEmulationManager({.time_mode = TimeMode::kSimulated});
  PeerConnectionE2EQualityTest fixture(
      "test_case", *network_emulation->time_controller(),
      /*audio_quality_analyzer=*/nullptr, /*video_quality_analyzer=*/nullptr,
      test::GetGlobalMetricsLogger());

  EmulatedEndpoint* alice_endpoint =
      network_emulation->CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_emulation->CreateEndpoint(EmulatedEndpointConfig());

  network_emulation->CreateRoute(
      alice_endpoint, {network_emulation->CreateUnconstrainedEmulatedNode()},
      bob_endpoint);
  network_emulation->CreateRoute(
      bob_endpoint, {network_emulation->CreateUnconstrainedEmulatedNode()},
      alice_endpoint);

  EmulatedNetworkManagerInterface* alice_network =
      network_emulation->CreateEmulatedNetworkManagerInterface(
          {alice_endpoint});
  EmulatedNetworkManagerInterface* bob_network =
      network_emulation->CreateEmulatedNetworkManagerInterface({bob_endpoint});

  VideoConfig alice_video("alice_video", 320, 180, 15);
  alice_video.output_dump_options = VideoDumpOptions(test_directory_);
  PeerConfigurer alice(alice_network->network_dependencies());
  alice.SetName("alice");
  alice.AddVideoConfig(std::move(alice_video));
  fixture.AddPeer(std::make_unique<PeerConfigurer>(std::move(alice)));

  PeerConfigurer bob(bob_network->network_dependencies());
  bob.SetName("bob");
  fixture.AddPeer(std::make_unique<PeerConfigurer>(std::move(bob)));

  fixture.Run(RunParams(TimeDelta::Seconds(2)));

  auto frame_reader = test::CreateY4mFrameReader(
      test::JoinFilename(test_directory_, "alice_video_bob_320x180_15.y4m"));
  EXPECT_THAT(frame_reader->num_frames(), Eq(31));  // 2 seconds 15 fps + 1

  ExpectOutputFilesCount(1);
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
