/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/analyzing_video_sinks_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "api/test/pclf/media_configuration.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::testing::Eq;

// Asserts equality of the main fields of the video config. We don't compare
// the full config due to the lack of equality definition for a lot of subtypes.
void AssertConfigsAreEquals(const VideoConfig& actual,
                            const VideoConfig& expected) {
  EXPECT_THAT(actual.stream_label, Eq(expected.stream_label));
  EXPECT_THAT(actual.width, Eq(expected.width));
  EXPECT_THAT(actual.height, Eq(expected.height));
  EXPECT_THAT(actual.fps, Eq(expected.fps));
}

TEST(AnalyzingVideoSinksHelperTest, ConfigsCanBeAdded) {
  VideoConfig config("alice_video", /*width=*/1280, /*height=*/720, /*fps=*/30);

  AnalyzingVideoSinksHelper helper;
  helper.AddConfig("alice", config);

  std::optional<std::pair<std::string, VideoConfig>> registred_config =
      helper.GetPeerAndConfig("alice_video");
  ASSERT_TRUE(registred_config.has_value());
  EXPECT_THAT(registred_config->first, Eq("alice"));
  AssertConfigsAreEquals(registred_config->second, config);
}

TEST(AnalyzingVideoSinksHelperTest, AddingForExistingLabelWillOverwriteValue) {
  VideoConfig config_before("alice_video", /*width=*/1280, /*height=*/720,
                            /*fps=*/30);
  VideoConfig config_after("alice_video", /*width=*/640, /*height=*/360,
                           /*fps=*/15);

  AnalyzingVideoSinksHelper helper;
  helper.AddConfig("alice", config_before);

  std::optional<std::pair<std::string, VideoConfig>> registred_config =
      helper.GetPeerAndConfig("alice_video");
  ASSERT_TRUE(registred_config.has_value());
  EXPECT_THAT(registred_config->first, Eq("alice"));
  AssertConfigsAreEquals(registred_config->second, config_before);

  helper.AddConfig("alice", config_after);

  registred_config = helper.GetPeerAndConfig("alice_video");
  ASSERT_TRUE(registred_config.has_value());
  EXPECT_THAT(registred_config->first, Eq("alice"));
  AssertConfigsAreEquals(registred_config->second, config_after);
}

TEST(AnalyzingVideoSinksHelperTest, ConfigsCanBeRemoved) {
  VideoConfig config("alice_video", /*width=*/1280, /*height=*/720, /*fps=*/30);

  AnalyzingVideoSinksHelper helper;
  helper.AddConfig("alice", config);

  ASSERT_TRUE(helper.GetPeerAndConfig("alice_video").has_value());

  helper.RemoveConfig("alice_video");
  ASSERT_FALSE(helper.GetPeerAndConfig("alice_video").has_value());
}

TEST(AnalyzingVideoSinksHelperTest, RemoveOfNonExistingConfigDontCrash) {
  AnalyzingVideoSinksHelper helper;
  helper.RemoveConfig("alice_video");
}

TEST(AnalyzingVideoSinksHelperTest, ClearRemovesAllConfigs) {
  VideoConfig config1("alice_video", /*width=*/640, /*height=*/360, /*fps=*/30);
  VideoConfig config2("bob_video", /*width=*/640, /*height=*/360, /*fps=*/30);

  AnalyzingVideoSinksHelper helper;
  helper.AddConfig("alice", config1);
  helper.AddConfig("bob", config2);

  ASSERT_TRUE(helper.GetPeerAndConfig("alice_video").has_value());
  ASSERT_TRUE(helper.GetPeerAndConfig("bob_video").has_value());

  helper.Clear();
  ASSERT_FALSE(helper.GetPeerAndConfig("alice_video").has_value());
  ASSERT_FALSE(helper.GetPeerAndConfig("bob_video").has_value());
}

struct TestVideoFrameWriterFactory {
  int closed_writers_count = 0;
  int deleted_writers_count = 0;

  std::unique_ptr<test::VideoFrameWriter> CreateWriter() {
    return std::make_unique<TestVideoFrameWriter>(this);
  }

 private:
  class TestVideoFrameWriter : public test::VideoFrameWriter {
   public:
    explicit TestVideoFrameWriter(TestVideoFrameWriterFactory* factory)
        : factory_(factory) {}
    ~TestVideoFrameWriter() override { factory_->deleted_writers_count++; }

    bool WriteFrame(const VideoFrame& frame) override { return true; }

    void Close() override { factory_->closed_writers_count++; }

   private:
    TestVideoFrameWriterFactory* factory_;
  };
};

TEST(AnalyzingVideoSinksHelperTest, RemovingWritersCloseAndDestroyAllOfThem) {
  TestVideoFrameWriterFactory factory;

  AnalyzingVideoSinksHelper helper;
  test::VideoFrameWriter* writer1 =
      helper.AddVideoWriter(factory.CreateWriter());
  test::VideoFrameWriter* writer2 =
      helper.AddVideoWriter(factory.CreateWriter());

  helper.CloseAndRemoveVideoWriters({writer1, writer2});

  EXPECT_THAT(factory.closed_writers_count, Eq(2));
  EXPECT_THAT(factory.deleted_writers_count, Eq(2));
}

TEST(AnalyzingVideoSinksHelperTest, ClearCloseAndDestroyAllWriters) {
  TestVideoFrameWriterFactory factory;

  AnalyzingVideoSinksHelper helper;
  helper.AddVideoWriter(factory.CreateWriter());
  helper.AddVideoWriter(factory.CreateWriter());

  helper.Clear();

  EXPECT_THAT(factory.closed_writers_count, Eq(2));
  EXPECT_THAT(factory.deleted_writers_count, Eq(2));
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
