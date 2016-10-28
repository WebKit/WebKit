//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// signal_utils_unittest:
//   Unit tests for signals and related utils.

#include <gtest/gtest.h>

#include "libANGLE/signal_utils.h"

using namespace angle;
using namespace testing;

namespace
{

struct SignalThing : public SignalReceiver
{
    void signal(SignalToken token) override { wasSignaled = true; }
    bool wasSignaled = false;
};

// Test that broadcast signals work.
TEST(SignalTest, BroadcastSignals)
{
    BroadcastChannel channel;
    SignalThing thing;
    ChannelBinding binding(&thing, 0u);

    binding.bind(&channel);
    ASSERT_FALSE(thing.wasSignaled);
    channel.signal();
    ASSERT_TRUE(thing.wasSignaled);
}

}  // anonymous namespace
