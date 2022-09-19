/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_stream_state.h"

#include <set>

#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(StreamStateTest, PopFrontAndFrontIndependentForEachPeer) {
  StreamState state(/*sender=*/0,
                    /*receivers=*/std::set<size_t>{1, 2},
                    Timestamp::Seconds(1));
  state.PushBack(/*frame_id=*/1);
  state.PushBack(/*frame_id=*/2);

  EXPECT_EQ(state.Front(/*peer=*/1), 1);
  EXPECT_EQ(state.PopFront(/*peer=*/1), 1);
  EXPECT_EQ(state.Front(/*peer=*/1), 2);
  EXPECT_EQ(state.PopFront(/*peer=*/1), 2);
  EXPECT_EQ(state.Front(/*peer=*/2), 1);
  EXPECT_EQ(state.PopFront(/*peer=*/2), 1);
  EXPECT_EQ(state.Front(/*peer=*/2), 2);
  EXPECT_EQ(state.PopFront(/*peer=*/2), 2);
}

TEST(StreamStateTest, IsEmpty) {
  StreamState state(/*sender=*/0,
                    /*receivers=*/std::set<size_t>{1, 2},
                    Timestamp::Seconds(1));
  state.PushBack(/*frame_id=*/1);

  EXPECT_FALSE(state.IsEmpty(/*peer=*/1));

  state.PopFront(/*peer=*/1);

  EXPECT_TRUE(state.IsEmpty(/*peer=*/1));
}

TEST(StreamStateTest, PopFrontForOnlyOnePeerDontChangeAliveFramesCount) {
  StreamState state(/*sender=*/0,
                    /*receivers=*/std::set<size_t>{1, 2},
                    Timestamp::Seconds(1));
  state.PushBack(/*frame_id=*/1);
  state.PushBack(/*frame_id=*/2);

  EXPECT_EQ(state.GetAliveFramesCount(), 2lu);

  state.PopFront(/*peer=*/1);
  state.PopFront(/*peer=*/1);

  EXPECT_EQ(state.GetAliveFramesCount(), 2lu);
}

TEST(StreamStateTest, PopFrontForAllPeersReducesAliveFramesCount) {
  StreamState state(/*sender=*/0,
                    /*receivers=*/std::set<size_t>{1, 2},
                    Timestamp::Seconds(1));
  state.PushBack(/*frame_id=*/1);
  state.PushBack(/*frame_id=*/2);

  EXPECT_EQ(state.GetAliveFramesCount(), 2lu);

  state.PopFront(/*peer=*/1);
  state.PopFront(/*peer=*/2);

  EXPECT_EQ(state.GetAliveFramesCount(), 1lu);
}

TEST(StreamStateTest, RemovePeerForLastExpectedReceiverUpdatesAliveFrames) {
  StreamState state(/*sender=*/0,
                    /*receivers=*/std::set<size_t>{1, 2},
                    Timestamp::Seconds(1));
  state.PushBack(/*frame_id=*/1);
  state.PushBack(/*frame_id=*/2);

  state.PopFront(/*peer=*/1);

  EXPECT_EQ(state.GetAliveFramesCount(), 2lu);

  state.RemovePeer(/*peer=*/2);

  EXPECT_EQ(state.GetAliveFramesCount(), 1lu);
}

TEST(StreamStateTest, MarkNextAliveFrameAsDeadDecreseAliveFramesCount) {
  StreamState state(/*sender=*/0,
                    /*receivers=*/std::set<size_t>{1, 2},
                    Timestamp::Seconds(1));
  state.PushBack(/*frame_id=*/1);
  state.PushBack(/*frame_id=*/2);

  EXPECT_EQ(state.GetAliveFramesCount(), 2lu);

  state.MarkNextAliveFrameAsDead();

  EXPECT_EQ(state.GetAliveFramesCount(), 1lu);
}

TEST(StreamStateTest, MarkNextAliveFrameAsDeadDoesntAffectFrontFrameForPeer) {
  StreamState state(/*sender=*/0,
                    /*receivers=*/std::set<size_t>{1, 2},
                    Timestamp::Seconds(1));
  state.PushBack(/*frame_id=*/1);
  state.PushBack(/*frame_id=*/2);

  EXPECT_EQ(state.Front(/*peer=*/1), 1);

  state.MarkNextAliveFrameAsDead();

  EXPECT_EQ(state.Front(/*peer=*/1), 1);
}

}  // namespace
}  // namespace webrtc
