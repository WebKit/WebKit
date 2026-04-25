//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TracePerfTestCL:
//   Performance test for ANGLE CL replaying traces.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "tests/perf_tests/ANGLEComputeTestCL.h"
#include "tests/perf_tests/TracePerfTest.h"

#if defined(ANGLE_PLATFORM_ANDROID)
#    include "util/android/AndroidWindow.h"
#endif

namespace angle
{
class TracePerfTestCL : public ANGLEComputeTestCL
{
  public:
    TracePerfTestCL(std::unique_ptr<const TracePerfParams> &params)
        : ANGLEComputeTestCL("TracePerf", *params.get(), "ms"),
          mParams(std::move(params)),
          mStartFrame(0),
          mEndFrame(0)
    {}

    void initializeBenchmark() override;
    void drawBenchmark() override;
    void destroyBenchmark() override;

    uint32_t frameCount() const
    {
        const TraceInfo &traceInfo = mParams->traceInfo;
        return traceInfo.frameEnd - traceInfo.frameStart + 1;
    }

    int getStepAlignment() const override
    {
        // Align step counts to the number of frames in a trace.
        return static_cast<int>(frameCount());
    }

    void TestBody() override { run(); }

    bool traceNameIs(const char *name) const
    {
        return strncmp(name, mParams->traceInfo.name, kTraceInfoMaxNameLen) == 0;
    }

  private:
    std::unique_ptr<const TracePerfParams> mParams;

    uint32_t mStartFrame;
    uint32_t mEndFrame;
    uint32_t mCurrentFrame     = 0;
    uint32_t mCurrentIteration = 0;
    uint32_t mTotalFrameCount  = 0;
    std::unique_ptr<TraceLibrary> mTraceReplay;
};

ANGLEPerfTest *CreateTracePerfTestCL(std::unique_ptr<const TracePerfParams> params)
{
    return new TracePerfTestCL(params);
}

bool FindTraceTestDataPath(const char *traceName, char *testDataDirOut, size_t maxDataDirLen)
{
    char relativeTestDataDir[kMaxPath] = {};
    snprintf(relativeTestDataDir, kMaxPath, "%s%c%s", kTraceTestFolder, GetPathSeparator(),
             traceName);
    return angle::FindTestDataPath(relativeTestDataDir, testDataDirOut, maxDataDirLen);
}

std::string FindTraceGzPath(const std::string &traceName)
{
    std::stringstream pathStream;

    char genDir[kMaxPath] = {};
    if (!angle::FindTestDataPath("gen", genDir, kMaxPath))
    {
        return "";
    }
    pathStream << genDir << angle::GetPathSeparator() << "tracegz_" << traceName << ".gz";

    return pathStream.str();
}

void TracePerfTestCL::initializeBenchmark()
{
    const TraceInfo &traceInfo = mParams->traceInfo;

    char testDataDir[kMaxPath] = {};
    if (!FindTraceTestDataPath(traceInfo.name, testDataDir, kMaxPath))
    {
        failTest("Could not find test data folder.");
        return;
    }

    std::string baseDir = "";
#if defined(ANGLE_TRACE_EXTERNAL_BINARIES)
    baseDir += AndroidWindow::GetApplicationDirectory() + "/angle_traces/";
#endif

    if (gTraceInterpreter)
    {
        mTraceReplay.reset(new TraceLibrary("angle_trace_interpreter", traceInfo, baseDir));
        if (strcmp(gTraceInterpreter, "gz") == 0)
        {
            std::string traceGzPath = FindTraceGzPath(traceInfo.name);
            if (traceGzPath.empty())
            {
                failTest("Could not find trace gz.");
                return;
            }
            mTraceReplay->setTraceGzPath(traceGzPath);
        }
    }
    else
    {
        std::stringstream traceNameStr;
        traceNameStr << "angle_restricted_traces_" << traceInfo.name;
        std::string traceName = traceNameStr.str();
        mTraceReplay.reset(new TraceLibrary(traceNameStr.str(), traceInfo, baseDir));
    }

    if (!mTraceReplay->valid())
    {
        failTest("Could not load trace.");
        return;
    }

    mStartFrame = traceInfo.frameStart;
    mEndFrame   = traceInfo.frameEnd;
    mTraceReplay->setBinaryDataDir(testDataDir);
    mTraceReplay->setReplayResourceMode(gIncludeInactiveResources);
    if (gScreenshotDir)
    {
        mTraceReplay->setDebugOutputDir(gScreenshotDir);
    }

    mCurrentFrame     = mStartFrame;
    mCurrentIteration = mStartFrame;

    // Potentially slow. Can load a lot of resources.
    mTraceReplay->setupReplay();
    ASSERT_GE(mEndFrame, mStartFrame);
}

void TracePerfTestCL::destroyBenchmark()
{
    mTraceReplay->finishReplay();
    mTraceReplay.reset(nullptr);
}

void TracePerfTestCL::drawBenchmark()
{
    if (mCurrentFrame == mStartFrame)
    {
        mTraceReplay->setupFirstFrame();
    }

    char frameName[32];
    snprintf(frameName, sizeof(frameName), "Frame %u", mCurrentFrame);

    atraceCounter("TraceFrameIndex", mCurrentFrame);
    mTraceReplay->replayFrame(mCurrentFrame);

    updatePerfCounters();

    mTotalFrameCount++;

    if (mCurrentFrame == mEndFrame)
    {
        mTraceReplay->resetReplay();
        mCurrentFrame = mStartFrame;
    }
    else
    {
        mCurrentFrame++;
    }

    // Always iterated for saving screenshots after reset
    mCurrentIteration++;
}

}  // namespace angle
