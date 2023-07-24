/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/wrapping_active_ice_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "p2p/base/connection.h"
#include "p2p/base/mock_ice_agent.h"
#include "p2p/base/mock_ice_controller.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"

namespace {

using ::cricket::Connection;
using ::cricket::IceConfig;
using ::cricket::IceControllerFactoryArgs;
using ::cricket::IceControllerInterface;
using ::cricket::IceMode;
using ::cricket::IceRecheckEvent;
using ::cricket::IceSwitchReason;
using ::cricket::MockIceAgent;
using ::cricket::MockIceController;
using ::cricket::MockIceControllerFactory;
using ::cricket::NominationMode;
using ::cricket::WrappingActiveIceController;

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::Sequence;

using ::rtc::AutoThread;
using ::rtc::Event;
using ::rtc::ScopedFakeClock;
using ::webrtc::TimeDelta;

using NiceMockIceController = NiceMock<MockIceController>;

static const Connection* kConnection =
    reinterpret_cast<const Connection*>(0xabcd);
static const Connection* kConnectionTwo =
    reinterpret_cast<const Connection*>(0xbcde);
static const Connection* kConnectionThree =
    reinterpret_cast<const Connection*>(0xcdef);

static const std::vector<const Connection*> kEmptyConnsList =
    std::vector<const Connection*>();

static const TimeDelta kTick = TimeDelta::Millis(1);

TEST(WrappingActiveIceControllerTest, CreateLegacyIceControllerFromFactory) {
  AutoThread main;
  MockIceAgent agent;
  IceControllerFactoryArgs args;
  MockIceControllerFactory legacy_controller_factory;
  EXPECT_CALL(legacy_controller_factory, RecordIceControllerCreated()).Times(1);
  WrappingActiveIceController controller(&agent, &legacy_controller_factory,
                                         args);
}

TEST(WrappingActiveIceControllerTest, PassthroughIceControllerInterface) {
  AutoThread main;
  MockIceAgent agent;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();
  WrappingActiveIceController controller(&agent, std::move(will_move));

  IceConfig config{};
  EXPECT_CALL(*wrapped, SetIceConfig(Ref(config)));
  controller.SetIceConfig(config);

  EXPECT_CALL(*wrapped,
              GetUseCandidateAttr(kConnection, NominationMode::AGGRESSIVE,
                                  IceMode::ICEMODE_LITE))
      .WillOnce(Return(true));
  EXPECT_TRUE(controller.GetUseCandidateAttribute(
      kConnection, NominationMode::AGGRESSIVE, IceMode::ICEMODE_LITE));

  EXPECT_CALL(*wrapped, AddConnection(kConnection));
  controller.OnConnectionAdded(kConnection);

  EXPECT_CALL(*wrapped, OnConnectionDestroyed(kConnection));
  controller.OnConnectionDestroyed(kConnection);

  EXPECT_CALL(*wrapped, SetSelectedConnection(kConnection));
  controller.OnConnectionSwitched(kConnection);

  EXPECT_CALL(*wrapped, MarkConnectionPinged(kConnection));
  controller.OnConnectionPinged(kConnection);

  EXPECT_CALL(*wrapped, FindNextPingableConnection())
      .WillOnce(Return(kConnection));
  EXPECT_EQ(controller.FindNextPingableConnection(), kConnection);
}

TEST(WrappingActiveIceControllerTest, HandlesImmediateSwitchRequest) {
  AutoThread main;
  ScopedFakeClock clock;
  NiceMock<MockIceAgent> agent;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  WrappingActiveIceController controller(&agent, std::move(will_move));

  IceSwitchReason reason = IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE;
  std::vector<const Connection*> conns_to_forget{kConnectionTwo};
  int recheck_delay_ms = 10;
  IceControllerInterface::SwitchResult switch_result{
      kConnection,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  // ICE controller should switch to given connection immediately.
  Sequence check_then_switch;
  EXPECT_CALL(*wrapped, ShouldSwitchConnection(reason, kConnection))
      .InSequence(check_then_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(agent, SwitchSelectedConnection(kConnection, reason))
      .InSequence(check_then_switch);
  EXPECT_CALL(agent, ForgetLearnedStateForConnections(
                         ElementsAreArray(conns_to_forget)));

  EXPECT_TRUE(controller.OnImmediateSwitchRequest(reason, kConnection));

  // No rechecks before recheck delay.
  clock.AdvanceTime(TimeDelta::Millis(recheck_delay_ms - 1));

  // ICE controller should recheck for best connection after the recheck delay.
  Sequence recheck_sort;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(recheck_sort);
  EXPECT_CALL(*wrapped,
              SortAndSwitchConnection(IceSwitchReason::ICE_CONTROLLER_RECHECK))
      .InSequence(recheck_sort)
      .WillOnce(Return(IceControllerInterface::SwitchResult{}));
  EXPECT_CALL(agent, ForgetLearnedStateForConnections(IsEmpty()));

  clock.AdvanceTime(kTick);
}

TEST(WrappingActiveIceControllerTest, HandlesImmediateSortAndSwitchRequest) {
  AutoThread main;
  ScopedFakeClock clock;
  NiceMock<MockIceAgent> agent;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  WrappingActiveIceController controller(&agent, std::move(will_move));

  IceSwitchReason reason = IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE;
  std::vector<const Connection*> conns_to_forget{kConnectionTwo};
  std::vector<const Connection*> conns_to_prune{kConnectionThree};
  int recheck_delay_ms = 10;
  IceControllerInterface::SwitchResult switch_result{
      kConnection,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  Sequence sort_and_switch;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(sort_and_switch);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(reason))
      .InSequence(sort_and_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(agent, SwitchSelectedConnection(kConnection, reason))
      .InSequence(sort_and_switch);
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(sort_and_switch)
      .WillOnce(Return(conns_to_prune));
  EXPECT_CALL(agent, PruneConnections(ElementsAreArray(conns_to_prune)))
      .InSequence(sort_and_switch);

  controller.OnImmediateSortAndSwitchRequest(reason);

  // No rechecks before recheck delay.
  clock.AdvanceTime(TimeDelta::Millis(recheck_delay_ms - 1));

  // ICE controller should recheck for best connection after the recheck delay.
  Sequence recheck_sort;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(recheck_sort);
  EXPECT_CALL(*wrapped,
              SortAndSwitchConnection(IceSwitchReason::ICE_CONTROLLER_RECHECK))
      .InSequence(recheck_sort)
      .WillOnce(Return(IceControllerInterface::SwitchResult{}));
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(recheck_sort)
      .WillOnce(Return(kEmptyConnsList));
  EXPECT_CALL(agent, PruneConnections(IsEmpty())).InSequence(recheck_sort);

  clock.AdvanceTime(kTick);
}

TEST(WrappingActiveIceControllerTest, HandlesSortAndSwitchRequest) {
  AutoThread main;
  ScopedFakeClock clock;

  // Block the main task queue until ready.
  Event init;
  TimeDelta init_delay = TimeDelta::Millis(10);
  main.PostTask([&init, &init_delay] { init.Wait(init_delay); });

  NiceMock<MockIceAgent> agent;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  WrappingActiveIceController controller(&agent, std::move(will_move));

  IceSwitchReason reason = IceSwitchReason::NETWORK_PREFERENCE_CHANGE;

  // No action should occur immediately
  EXPECT_CALL(agent, UpdateConnectionStates()).Times(0);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(_)).Times(0);
  EXPECT_CALL(agent, SwitchSelectedConnection(_, _)).Times(0);

  controller.OnSortAndSwitchRequest(reason);

  std::vector<const Connection*> conns_to_forget{kConnectionTwo};
  int recheck_delay_ms = 10;
  IceControllerInterface::SwitchResult switch_result{
      kConnection,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  // Sort and switch should take place as the subsequent task.
  Sequence sort_and_switch;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(sort_and_switch);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(reason))
      .InSequence(sort_and_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(agent, SwitchSelectedConnection(kConnection, reason))
      .InSequence(sort_and_switch);

  // Unblock the init task.
  clock.AdvanceTime(init_delay);
}

TEST(WrappingActiveIceControllerTest, StartPingingAfterSortAndSwitch) {
  AutoThread main;
  ScopedFakeClock clock;

  // Block the main task queue until ready.
  Event init;
  TimeDelta init_delay = TimeDelta::Millis(10);
  main.PostTask([&init, &init_delay] { init.Wait(init_delay); });

  NiceMock<MockIceAgent> agent;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  WrappingActiveIceController controller(&agent, std::move(will_move));

  // Pinging does not start automatically, unless triggered through a sort.
  EXPECT_CALL(*wrapped, HasPingableConnection()).Times(0);
  EXPECT_CALL(*wrapped, SelectConnectionToPing(_)).Times(0);
  EXPECT_CALL(agent, OnStartedPinging()).Times(0);

  controller.OnSortAndSwitchRequest(IceSwitchReason::DATA_RECEIVED);

  // Pinging does not start if no pingable connection.
  EXPECT_CALL(*wrapped, HasPingableConnection()).WillOnce(Return(false));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(_)).Times(0);
  EXPECT_CALL(agent, OnStartedPinging()).Times(0);

  // Unblock the init task.
  clock.AdvanceTime(init_delay);

  int recheck_delay_ms = 10;
  IceControllerInterface::PingResult ping_result(kConnection, recheck_delay_ms);

  // Pinging starts when there is a pingable connection.
  Sequence start_pinging;
  EXPECT_CALL(*wrapped, HasPingableConnection())
      .InSequence(start_pinging)
      .WillOnce(Return(true));
  EXPECT_CALL(agent, OnStartedPinging()).InSequence(start_pinging);
  EXPECT_CALL(agent, GetLastPingSentMs())
      .InSequence(start_pinging)
      .WillOnce(Return(123));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(123))
      .InSequence(start_pinging)
      .WillOnce(Return(ping_result));
  EXPECT_CALL(agent, SendPingRequest(kConnection)).InSequence(start_pinging);

  controller.OnSortAndSwitchRequest(IceSwitchReason::DATA_RECEIVED);
  clock.AdvanceTime(kTick);

  // ICE controller should recheck and ping after the recheck delay.
  // No ping should be sent if no connection selected to ping.
  EXPECT_CALL(agent, GetLastPingSentMs()).WillOnce(Return(456));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(456))
      .WillOnce(Return(IceControllerInterface::PingResult(
          /* connection= */ nullptr, recheck_delay_ms)));
  EXPECT_CALL(agent, SendPingRequest(kConnection)).Times(0);

  clock.AdvanceTime(TimeDelta::Millis(recheck_delay_ms));
}

}  // namespace
