// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Minimal gtest entry point for the webrtc_unittests aggregate test binary.
// Mirrors the placeholder used by legacy_stats_collector_unittest_main.cc.
// Calls webrtc::metrics::Enable() before RUN_ALL_TESTS(), matching upstream's
// test/test_main_lib.cc -- without it, RTC_HISTOGRAM_* macros silently no-op
// because their factory calls find g_rtc_histogram_map == nullptr, leaving
// metrics tests reading an empty `histograms` map (and dereferencing end()).
// Replace with upstream test/test_main.cc once helper closure is in place.

#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"

int main(int argc, char** argv) {
    webrtc::metrics::Enable();
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
