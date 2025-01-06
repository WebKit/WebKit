/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for DelayManager class.

#include "modules/audio_coding/neteq/delay_manager.h"

#include "api/neteq/tick_timer.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using test::ExplicitKeyValueConfig;

TEST(DelayManagerTest, UpdateNormal) {
  TickTimer tick_timer;
  DelayManager dm(DelayManager::Config(ExplicitKeyValueConfig("")),
                  &tick_timer);
  for (int i = 0; i < 50; ++i) {
    dm.Update(0, false);
    tick_timer.Increment(2);
  }
  EXPECT_EQ(20, dm.TargetDelayMs());
}

}  // namespace
}  // namespace webrtc
