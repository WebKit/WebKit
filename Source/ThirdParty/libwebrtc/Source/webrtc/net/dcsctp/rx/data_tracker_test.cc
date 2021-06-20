/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/rx/data_tracker.h"

#include <cstdint>
#include <initializer_list>
#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/timer/fake_timeout.h"
#include "net/dcsctp/timer/timer.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr size_t kArwnd = 10000;
constexpr TSN kInitialTSN(11);

class DataTrackerTest : public testing::Test {
 protected:
  DataTrackerTest()
      : timeout_manager_([this]() { return now_; }),
        timer_manager_([this]() { return timeout_manager_.CreateTimeout(); }),
        timer_(timer_manager_.CreateTimer(
            "test/delayed_ack",
            []() { return absl::nullopt; },
            TimerOptions(DurationMs(0)))),
        buf_("log: ", timer_.get(), kInitialTSN) {}

  void Observer(std::initializer_list<uint32_t> tsns) {
    for (const uint32_t tsn : tsns) {
      buf_.Observe(TSN(tsn), AnyDataChunk::ImmediateAckFlag(false));
    }
  }

  TimeMs now_ = TimeMs(0);
  FakeTimeoutManager timeout_manager_;
  TimerManager timer_manager_;
  std::unique_ptr<Timer> timer_;
  DataTracker buf_;
};

TEST_F(DataTrackerTest, Empty) {
  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(10));
  EXPECT_THAT(sack.gap_ack_blocks(), IsEmpty());
  EXPECT_THAT(sack.duplicate_tsns(), IsEmpty());
}

TEST_F(DataTrackerTest, ObserverSingleInOrderPacket) {
  Observer({11});
  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(11));
  EXPECT_THAT(sack.gap_ack_blocks(), IsEmpty());
  EXPECT_THAT(sack.duplicate_tsns(), IsEmpty());
}

TEST_F(DataTrackerTest, ObserverManyInOrderMovesCumulativeTsnAck) {
  Observer({11, 12, 13});
  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(13));
  EXPECT_THAT(sack.gap_ack_blocks(), IsEmpty());
  EXPECT_THAT(sack.duplicate_tsns(), IsEmpty());
}

TEST_F(DataTrackerTest, ObserveOutOfOrderMovesCumulativeTsnAck) {
  Observer({12, 13, 14, 11});
  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(14));
  EXPECT_THAT(sack.gap_ack_blocks(), IsEmpty());
  EXPECT_THAT(sack.duplicate_tsns(), IsEmpty());
}

TEST_F(DataTrackerTest, SingleGap) {
  Observer({12});
  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(10));
  EXPECT_THAT(sack.gap_ack_blocks(), ElementsAre(SackChunk::GapAckBlock(2, 2)));
  EXPECT_THAT(sack.duplicate_tsns(), IsEmpty());
}

TEST_F(DataTrackerTest, ExampleFromRFC4960Section334) {
  Observer({11, 12, 14, 15, 17});
  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(12));
  EXPECT_THAT(sack.gap_ack_blocks(), ElementsAre(SackChunk::GapAckBlock(2, 3),
                                                 SackChunk::GapAckBlock(5, 5)));
  EXPECT_THAT(sack.duplicate_tsns(), IsEmpty());
}

TEST_F(DataTrackerTest, AckAlreadyReceivedChunk) {
  Observer({11});
  SackChunk sack1 = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack1.cumulative_tsn_ack(), TSN(11));
  EXPECT_THAT(sack1.gap_ack_blocks(), IsEmpty());

  // Receive old chunk
  Observer({8});
  SackChunk sack2 = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack2.cumulative_tsn_ack(), TSN(11));
  EXPECT_THAT(sack2.gap_ack_blocks(), IsEmpty());
}

TEST_F(DataTrackerTest, DoubleSendRetransmittedChunk) {
  Observer({11, 13, 14, 15});
  SackChunk sack1 = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack1.cumulative_tsn_ack(), TSN(11));
  EXPECT_THAT(sack1.gap_ack_blocks(),
              ElementsAre(SackChunk::GapAckBlock(2, 4)));

  // Fill in the hole.
  Observer({12, 16, 17, 18});
  SackChunk sack2 = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack2.cumulative_tsn_ack(), TSN(18));
  EXPECT_THAT(sack2.gap_ack_blocks(), IsEmpty());

  // Receive chunk 12 again.
  Observer({12, 19, 20, 21});
  SackChunk sack3 = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack3.cumulative_tsn_ack(), TSN(21));
  EXPECT_THAT(sack3.gap_ack_blocks(), IsEmpty());
}

TEST_F(DataTrackerTest, ForwardTsnSimple) {
  // Messages (11, 12, 13), (14, 15) - first message expires.
  Observer({11, 12, 15});

  buf_.HandleForwardTsn(TSN(13));

  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(13));
  EXPECT_THAT(sack.gap_ack_blocks(), ElementsAre(SackChunk::GapAckBlock(2, 2)));
}

TEST_F(DataTrackerTest, ForwardTsnSkipsFromGapBlock) {
  // Messages (11, 12, 13), (14, 15) - first message expires.
  Observer({11, 12, 14});

  buf_.HandleForwardTsn(TSN(13));

  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(14));
  EXPECT_THAT(sack.gap_ack_blocks(), IsEmpty());
}

TEST_F(DataTrackerTest, ExampleFromRFC3758) {
  buf_.HandleForwardTsn(TSN(102));

  Observer({102, 104, 105, 107});

  buf_.HandleForwardTsn(TSN(103));

  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(105));
  EXPECT_THAT(sack.gap_ack_blocks(), ElementsAre(SackChunk::GapAckBlock(2, 2)));
}

TEST_F(DataTrackerTest, EmptyAllAcks) {
  Observer({11, 13, 14, 15});

  buf_.HandleForwardTsn(TSN(100));

  SackChunk sack = buf_.CreateSelectiveAck(kArwnd);
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(100));
  EXPECT_THAT(sack.gap_ack_blocks(), IsEmpty());
}

TEST_F(DataTrackerTest, SetsArwndCorrectly) {
  SackChunk sack1 = buf_.CreateSelectiveAck(/*a_rwnd=*/100);
  EXPECT_EQ(sack1.a_rwnd(), 100u);

  SackChunk sack2 = buf_.CreateSelectiveAck(/*a_rwnd=*/101);
  EXPECT_EQ(sack2.a_rwnd(), 101u);
}

TEST_F(DataTrackerTest, WillIncreaseCumAckTsn) {
  EXPECT_EQ(buf_.last_cumulative_acked_tsn(), TSN(10));
  EXPECT_FALSE(buf_.will_increase_cum_ack_tsn(TSN(10)));
  EXPECT_TRUE(buf_.will_increase_cum_ack_tsn(TSN(11)));
  EXPECT_FALSE(buf_.will_increase_cum_ack_tsn(TSN(12)));

  Observer({11, 12, 13, 14, 15});
  EXPECT_EQ(buf_.last_cumulative_acked_tsn(), TSN(15));
  EXPECT_FALSE(buf_.will_increase_cum_ack_tsn(TSN(15)));
  EXPECT_TRUE(buf_.will_increase_cum_ack_tsn(TSN(16)));
  EXPECT_FALSE(buf_.will_increase_cum_ack_tsn(TSN(17)));
}

TEST_F(DataTrackerTest, ForceShouldSendSackImmediately) {
  EXPECT_FALSE(buf_.ShouldSendAck());

  buf_.ForceImmediateSack();

  EXPECT_TRUE(buf_.ShouldSendAck());
}

TEST_F(DataTrackerTest, WillAcceptValidTSNs) {
  // The initial TSN is always one more than the last, which is our base.
  TSN last_tsn = TSN(*kInitialTSN - 1);
  int limit = static_cast<int>(DataTracker::kMaxAcceptedOutstandingFragments);

  for (int i = -limit; i <= limit; ++i) {
    EXPECT_TRUE(buf_.IsTSNValid(TSN(*last_tsn + i)));
  }
}

TEST_F(DataTrackerTest, WillNotAcceptInvalidTSNs) {
  // The initial TSN is always one more than the last, which is our base.
  TSN last_tsn = TSN(*kInitialTSN - 1);

  size_t limit = DataTracker::kMaxAcceptedOutstandingFragments;
  EXPECT_FALSE(buf_.IsTSNValid(TSN(*last_tsn + limit + 1)));
  EXPECT_FALSE(buf_.IsTSNValid(TSN(*last_tsn - (limit + 1))));
  EXPECT_FALSE(buf_.IsTSNValid(TSN(*last_tsn + 65536)));
  EXPECT_FALSE(buf_.IsTSNValid(TSN(*last_tsn - 65536)));
  EXPECT_FALSE(buf_.IsTSNValid(TSN(*last_tsn + 0x8000000)));
  EXPECT_FALSE(buf_.IsTSNValid(TSN(*last_tsn - 0x8000000)));
}

}  // namespace
}  // namespace dcsctp
