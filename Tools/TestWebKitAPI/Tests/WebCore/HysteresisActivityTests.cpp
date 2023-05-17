/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "PlatformUtilities.h"
#include "Test.h"
#include <pal/HysteresisActivity.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>

namespace TestWebKitAPI {

static const Seconds hysteresisDuration { 2_ms };

static bool hysteresisTestDone;
static bool endTestInCallback;

TEST(HysteresisActivity, BasicState)
{
    PAL::HysteresisActivity activity([](PAL::HysteresisState state) {
        if (endTestInCallback)
            hysteresisTestDone = true;
    }, hysteresisDuration);

    hysteresisTestDone = false;
    endTestInCallback = false;

    EXPECT_EQ(activity.state(), PAL::HysteresisState::Stopped);
    activity.start();
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Started);

    endTestInCallback = true;
    activity.stop();
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Started);

    TestWebKitAPI::Util::run(&hysteresisTestDone);
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Stopped);
}

TEST(HysteresisActivity, ImpulseState)
{
    PAL::HysteresisActivity activity([](PAL::HysteresisState state) {
        if (endTestInCallback)
            hysteresisTestDone = true;
    }, hysteresisDuration);

    hysteresisTestDone = false;
    endTestInCallback = false;

    activity.impulse();
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Started);

    endTestInCallback = true;
    TestWebKitAPI::Util::run(&hysteresisTestDone);
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Stopped);

    hysteresisTestDone = false;
    endTestInCallback = false;

    activity.start();
    activity.impulse();
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Started);
    activity.stop();
    endTestInCallback = true;
    TestWebKitAPI::Util::run(&hysteresisTestDone);
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Stopped);

    hysteresisTestDone = false;
    endTestInCallback = false;

    activity.impulse();
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Started);
    endTestInCallback = true;
    TestWebKitAPI::Util::run(&hysteresisTestDone);
    EXPECT_EQ(activity.state(), PAL::HysteresisState::Stopped);
}

TEST(HysteresisActivity, StateInCallback)
{
    std::optional<PAL::HysteresisState> callbackState;
    PAL::HysteresisActivity activity([&callbackState](PAL::HysteresisState state) {
        callbackState = state;
        if (endTestInCallback)
            hysteresisTestDone = true;
    }, hysteresisDuration);

    hysteresisTestDone = false;
    endTestInCallback = false;

    activity.start();
    EXPECT_TRUE(callbackState);
    EXPECT_EQ(*callbackState, PAL::HysteresisState::Started);

    callbackState = { };
    activity.stop();
    endTestInCallback = true;
    TestWebKitAPI::Util::run(&hysteresisTestDone);
    EXPECT_TRUE(callbackState);
    EXPECT_EQ(*callbackState, PAL::HysteresisState::Stopped);
}

TEST(HysteresisActivity, ImpulseStateInCallback)
{
    std::optional<PAL::HysteresisState> callbackState;
    PAL::HysteresisActivity activity([&callbackState](PAL::HysteresisState state) {
        callbackState = state;
        if (endTestInCallback)
            hysteresisTestDone = true;
    }, hysteresisDuration);

    hysteresisTestDone = false;
    endTestInCallback = false;

    activity.impulse();
    EXPECT_TRUE(callbackState);
    EXPECT_EQ(*callbackState, PAL::HysteresisState::Started);

    callbackState = { };
    endTestInCallback = true;
    TestWebKitAPI::Util::run(&hysteresisTestDone);
    EXPECT_TRUE(callbackState);
    EXPECT_EQ(*callbackState, PAL::HysteresisState::Stopped);
}

TEST(HysteresisActivity, ActiveInCallback)
{
    struct CallbackData {
        std::optional<PAL::HysteresisState> state;
        PAL::HysteresisActivity* activity;
    } callbackData;

    PAL::HysteresisActivity activity([&callbackData](PAL::HysteresisState) {
        callbackData.state = callbackData.activity->state();
        if (endTestInCallback)
            hysteresisTestDone = true;
    }, hysteresisDuration);

    callbackData.activity = &activity;

    hysteresisTestDone = false;
    endTestInCallback = false;

    activity.impulse();
    EXPECT_TRUE(callbackData.state);
    EXPECT_EQ(*callbackData.state, PAL::HysteresisState::Started);

    callbackData.state = { };
    endTestInCallback = true;
    TestWebKitAPI::Util::run(&hysteresisTestDone);
    EXPECT_TRUE(callbackData.state);
    EXPECT_EQ(*callbackData.state, PAL::HysteresisState::Stopped);
}

} // namespace TestWebKitAPI
