/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/bbr.h"

#include <time.h>
#include <algorithm>
#include <utility>

#include "webrtc/base/logging.h"
#include "webrtc/modules/congestion_controller/delay_based_bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"

namespace webrtc {
namespace testing {
namespace bwe {
BbrBweSender::BbrBweSender() : BweSender() {
  // Initially enter Startup mode.
  EnterStartup();
}

BbrBweSender::~BbrBweSender() {}

int BbrBweSender::GetFeedbackIntervalMs() const {
  return 0;
}

void BbrBweSender::GiveFeedback(const FeedbackPacket& feedback) {}

bool BbrBweSender::UpdateBandwidthAndMinRtt() {
  return false;
}

void BbrBweSender::EnterStartup() {}

void BbrBweSender::TryExitingStartup() {}

void BbrBweSender::TryExitingDrain(int64_t now) {}

void BbrBweSender::EnterProbeBw(int64_t now) {}

void BbrBweSender::TryUpdatingCyclePhase(int64_t now) {}

void BbrBweSender::EnterProbeRtt(int64_t now) {}

void BbrBweSender::TryExitingProbeRtt(int64_t now) {}

int64_t BbrBweSender::TimeUntilNextProcess() {
  return 100;
}

void BbrBweSender::OnPacketsSent(const Packets& packets) {}

void BbrBweSender::Process() {}

BbrBweReceiver::BbrBweReceiver(int flow_id)
    : BweReceiver(flow_id, kReceivingRateTimeWindowMs),
      clock_(0),
      packet_feedbacks_() {}

BbrBweReceiver::~BbrBweReceiver() {}

void BbrBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                   const MediaPacket& media_packet) {}

FeedbackPacket* BbrBweReceiver::GetFeedback(int64_t now_ms) {
  return nullptr;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
