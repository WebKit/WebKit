//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TracePerf:
//   Performance test for ANGLE replaying traces.
//

#include <gtest/gtest.h>
#include "common/system_utils.h"
#include "tests/perf_tests/ANGLEPerfTest.h"
#include "util/egl_loader_autogen.h"

#include "restricted_traces/trex_1300_1310/trex_1300_1310_capture_context1.h"
#include "restricted_traces/trex_200_210/trex_200_210_capture_context1.h"
#include "restricted_traces/trex_800_810/trex_800_810_capture_context1.h"
#include "restricted_traces/trex_900_910/trex_900_910_capture_context1.h"

#include <cassert>
#include <functional>
#include <sstream>

using namespace angle;
using namespace egl_platform;

namespace
{

enum class TracePerfTestID
{
    TRex200,
    TRex800,
    TRex900,
    TRex1300,
};

struct TracePerfParams final : public RenderTestParams
{
    // Common default options
    TracePerfParams()
    {
        majorVersion = 2;
        minorVersion = 0;
        windowWidth  = 1920;
        windowHeight = 1080;
        trackGpuTime = true;

        // Display the frame after every drawBenchmark invocation
        iterationsPerStep = 1;
    }

    std::string story() const override
    {
        std::stringstream strstr;

        strstr << RenderTestParams::story();

        switch (testID)
        {
            case TracePerfTestID::TRex200:
                strstr << "_trex_200";
                break;
            case TracePerfTestID::TRex800:
                strstr << "_trex_800";
                break;
            case TracePerfTestID::TRex900:
                strstr << "_trex_900";
                break;
            case TracePerfTestID::TRex1300:
                strstr << "_trex_1300";
                break;
            default:
                assert(0);
                break;
        }

        return strstr.str();
    }

    TracePerfTestID testID;
};

std::ostream &operator<<(std::ostream &os, const TracePerfParams &params)
{
    os << params.backendAndStory().substr(1);
    return os;
}

class TracePerfTest : public ANGLERenderTest, public ::testing::WithParamInterface<TracePerfParams>
{
  public:
    TracePerfTest();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

    uint32_t mStartFrame;
    uint32_t mEndFrame;
    std::function<void(uint32_t)> mReplayFunc;
};

TracePerfTest::TracePerfTest()
    : ANGLERenderTest("TracePerf", GetParam()), mStartFrame(0), mEndFrame(0)
{}

void TracePerfTest::initializeBenchmark()
{
    const auto &params = GetParam();

    // TODO: Note the start and end frames in the trace
    //       i.e. mStartFrame = trex_200_210::kReplayFrameStart
    switch (params.testID)
    {
        // For each case, bootstrap the trace
        case TracePerfTestID::TRex200:
            mStartFrame = 200;
            mEndFrame   = 210;
            mReplayFunc = trex_200_210::ReplayContext1Frame;
            trex_200_210::SetBinaryDataDir(ANGLE_TRACE_DATA_DIR_trex_200_210);
            trex_200_210::SetupContext1Replay();
            break;
        case TracePerfTestID::TRex800:
            mStartFrame = 800;
            mEndFrame   = 810;
            mReplayFunc = trex_800_810::ReplayContext1Frame;
            trex_800_810::SetBinaryDataDir(ANGLE_TRACE_DATA_DIR_trex_800_810);
            trex_800_810::SetupContext1Replay();
            break;
        case TracePerfTestID::TRex900:
            mStartFrame = 900;
            mEndFrame   = 910;
            mReplayFunc = trex_900_910::ReplayContext1Frame;
            trex_900_910::SetBinaryDataDir(ANGLE_TRACE_DATA_DIR_trex_900_910);
            trex_900_910::SetupContext1Replay();
            break;
        case TracePerfTestID::TRex1300:
            mStartFrame = 1300;
            mEndFrame   = 1310;
            mReplayFunc = trex_1300_1310::ReplayContext1Frame;
            trex_1300_1310::SetBinaryDataDir(ANGLE_TRACE_DATA_DIR_trex_1300_1310);
            trex_1300_1310::SetupContext1Replay();
            break;
        default:
            assert(0);
            break;
    }

    ASSERT_TRUE(mEndFrame > mStartFrame);

    getWindow()->setVisible(true);
}

void TracePerfTest::destroyBenchmark() {}

void TracePerfTest::drawBenchmark()
{
    startGpuTimer();

    for (uint32_t frame = mStartFrame; frame < mEndFrame; ++frame)
    {
        mReplayFunc(frame);
        getGLWindow()->swap();
    }

    stopGpuTimer();
}

TracePerfParams TRexReplayPerfOpenGLOrGLESParams_200_210()
{
    TracePerfParams params;
    params.eglParameters = OPENGL_OR_GLES();
    params.testID        = TracePerfTestID::TRex200;
    return params;
}

TracePerfParams TRexReplayPerfOpenGLOrGLESParams_800_810()
{
    TracePerfParams params;
    params.eglParameters = OPENGL_OR_GLES();
    params.testID        = TracePerfTestID::TRex800;
    return params;
}

TracePerfParams TRexReplayPerfOpenGLOrGLESParams_900_910()
{
    TracePerfParams params;
    params.eglParameters = OPENGL_OR_GLES();
    params.testID        = TracePerfTestID::TRex900;
    return params;
}

TracePerfParams TRexReplayPerfOpenGLOrGLESParams_1300_1310()
{
    TracePerfParams params;
    params.eglParameters = OPENGL_OR_GLES();
    params.testID        = TracePerfTestID::TRex1300;
    return params;
}

TracePerfParams TRexReplayPerfVulkanParams_200_210()
{
    TracePerfParams params;
    params.eglParameters = VULKAN();
    params.testID        = TracePerfTestID::TRex200;
    return params;
}

TracePerfParams TRexReplayPerfVulkanParams_800_810()
{
    TracePerfParams params;
    params.eglParameters = VULKAN();
    params.testID        = TracePerfTestID::TRex800;
    return params;
}

TracePerfParams TRexReplayPerfVulkanParams_900_910()
{
    TracePerfParams params;
    params.eglParameters = VULKAN();
    params.testID        = TracePerfTestID::TRex900;
    return params;
}

TracePerfParams TRexReplayPerfVulkanParams_1300_1310()
{
    TracePerfParams params;
    params.eglParameters = VULKAN();
    params.testID        = TracePerfTestID::TRex1300;
    return params;
}

TEST_P(TracePerfTest, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(TracePerfTest,
                       TRexReplayPerfOpenGLOrGLESParams_200_210(),
                       TRexReplayPerfOpenGLOrGLESParams_800_810(),
                       TRexReplayPerfOpenGLOrGLESParams_900_910(),
                       TRexReplayPerfOpenGLOrGLESParams_1300_1310(),
                       TRexReplayPerfVulkanParams_200_210(),
                       TRexReplayPerfVulkanParams_800_810(),
                       TRexReplayPerfVulkanParams_900_910(),
                       TRexReplayPerfVulkanParams_1300_1310());

}  // anonymous namespace
