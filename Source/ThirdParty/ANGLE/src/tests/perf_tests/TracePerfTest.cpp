//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TracePerf:
//   Performance test for ANGLE replaying traces.
//

#include <gtest/gtest.h>
#include "common/PackedEnums.h"
#include "common/system_utils.h"
#include "tests/perf_tests/ANGLEPerfTest.h"
#include "tests/perf_tests/DrawCallPerfParams.h"
#include "util/egl_loader_autogen.h"
#include "util/frame_capture_utils.h"
#include "util/png_utils.h"

#include "restricted_traces/restricted_traces_autogen.h"

#include <cassert>
#include <functional>
#include <sstream>

using namespace angle;
using namespace egl_platform;

namespace
{
void FramebufferChangeCallback(void *userData, GLenum target, GLuint framebuffer);

struct TracePerfParams final : public RenderTestParams
{
    // Common default options
    TracePerfParams()
    {
        majorVersion = 3;
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
        strstr << RenderTestParams::story() << "_" << kTraceInfos[testID].name;
        return strstr.str();
    }

    RestrictedTraceID testID;
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

    void onFramebufferChange(GLenum target, GLuint framebuffer);

    uint32_t mStartFrame;
    uint32_t mEndFrame;

    double getHostTimeFromGLTime(GLint64 glTime);

  private:
    struct QueryInfo
    {
        GLuint beginTimestampQuery;
        GLuint endTimestampQuery;
        GLuint framebuffer;
    };

    struct TimeSample
    {
        GLint64 glTime;
        double hostTime;
    };

    void sampleTime();
    void saveScreenshot(const std::string &screenshotName) override;

    // For tracking RenderPass/FBO change timing.
    QueryInfo mCurrentQuery = {};
    std::vector<QueryInfo> mRunningQueries;
    std::vector<TimeSample> mTimeline;

    std::string mStartingDirectory;
};

TracePerfTest::TracePerfTest()
    : ANGLERenderTest("TracePerf", GetParam()), mStartFrame(0), mEndFrame(0)
{
    // TODO(anglebug.com/4533) This fails after the upgrade to the 26.20.100.7870 driver.
    if (IsWindows() && IsIntel() &&
        GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE &&
        GetParam().testID == RestrictedTraceID::manhattan_10)
    {
        mSkipTest = true;
    }
}

void TracePerfTest::initializeBenchmark()
{
    const auto &params = GetParam();

    mStartingDirectory = angle::GetCWD().value();

    // To load the trace data path correctly we set the CWD to the executable dir.
    if (!IsAndroid())
    {
        std::string exeDir = angle::GetExecutableDirectory();
        angle::SetCWD(exeDir.c_str());
    }

    const TraceInfo &traceInfo = kTraceInfos[params.testID];
    mStartFrame                = traceInfo.startFrame;
    mEndFrame                  = traceInfo.endFrame;
    SetBinaryDataDecompressCallback(params.testID, DecompressBinaryData);

    std::stringstream testDataDirStr;
    testDataDirStr << ANGLE_TRACE_DATA_DIR << "/" << traceInfo.name;
    std::string testDataDir = testDataDirStr.str();
    SetBinaryDataDir(params.testID, testDataDir.c_str());

    // Potentially slow. Can load a lot of resources.
    SetupReplay(params.testID);

    ASSERT_TRUE(mEndFrame > mStartFrame);

    getWindow()->setVisible(true);
}

#undef TRACE_TEST_CASE

void TracePerfTest::destroyBenchmark()
{
    // In order for the next test to load, restore the working directory
    angle::SetCWD(mStartingDirectory.c_str());
}

void TracePerfTest::sampleTime()
{
    if (mIsTimestampQueryAvailable)
    {
        GLint64 glTime;
        // glGetInteger64vEXT is exported by newer versions of the timer query extensions.
        // Unfortunately only the core EP is exposed by some desktop drivers (e.g. NVIDIA).
        if (glGetInteger64vEXT)
        {
            glGetInteger64vEXT(GL_TIMESTAMP_EXT, &glTime);
        }
        else
        {
            glGetInteger64v(GL_TIMESTAMP_EXT, &glTime);
        }
        mTimeline.push_back({glTime, angle::GetHostTimeSeconds()});
    }
}

void TracePerfTest::drawBenchmark()
{
    // Add a time sample from GL and the host.
    sampleTime();

    startGpuTimer();

    for (uint32_t frame = mStartFrame; frame < mEndFrame; ++frame)
    {
        char frameName[32];
        sprintf(frameName, "Frame %u", frame);
        beginInternalTraceEvent(frameName);

        ReplayFrame(GetParam().testID, frame);
        getGLWindow()->swap();

        endInternalTraceEvent(frameName);
    }

    ResetReplay(GetParam().testID);

    // Process any running queries once per iteration.
    for (size_t queryIndex = 0; queryIndex < mRunningQueries.size();)
    {
        const QueryInfo &query = mRunningQueries[queryIndex];

        GLuint endResultAvailable = 0;
        glGetQueryObjectuivEXT(query.endTimestampQuery, GL_QUERY_RESULT_AVAILABLE,
                               &endResultAvailable);

        if (endResultAvailable == GL_TRUE)
        {
            char fboName[32];
            sprintf(fboName, "FBO %u", query.framebuffer);

            GLint64 beginTimestamp = 0;
            glGetQueryObjecti64vEXT(query.beginTimestampQuery, GL_QUERY_RESULT, &beginTimestamp);
            glDeleteQueriesEXT(1, &query.beginTimestampQuery);
            double beginHostTime = getHostTimeFromGLTime(beginTimestamp);
            beginGLTraceEvent(fboName, beginHostTime);

            GLint64 endTimestamp = 0;
            glGetQueryObjecti64vEXT(query.endTimestampQuery, GL_QUERY_RESULT, &endTimestamp);
            glDeleteQueriesEXT(1, &query.endTimestampQuery);
            double endHostTime = getHostTimeFromGLTime(endTimestamp);
            endGLTraceEvent(fboName, endHostTime);

            mRunningQueries.erase(mRunningQueries.begin() + queryIndex);
        }
        else
        {
            queryIndex++;
        }
    }

    stopGpuTimer();
}

// Converts a GL timestamp into a host-side CPU time aligned with "GetHostTimeSeconds".
// This check is necessary to line up sampled trace events in a consistent timeline.
// Uses a linear interpolation from a series of samples. We do a blocking call to sample
// both host and GL time once per swap. We then find the two closest GL timestamps and
// interpolate the host times between them to compute our result. If we are past the last
// GL timestamp we sample a new data point pair.
double TracePerfTest::getHostTimeFromGLTime(GLint64 glTime)
{
    // Find two samples to do a lerp.
    size_t firstSampleIndex = mTimeline.size() - 1;
    while (firstSampleIndex > 0)
    {
        if (mTimeline[firstSampleIndex].glTime < glTime)
        {
            break;
        }
        firstSampleIndex--;
    }

    // Add an extra sample if we're missing an ending sample.
    if (firstSampleIndex == mTimeline.size() - 1)
    {
        sampleTime();
    }

    const TimeSample &start = mTimeline[firstSampleIndex];
    const TimeSample &end   = mTimeline[firstSampleIndex + 1];

    // Note: we have observed in some odd cases later timestamps producing values that are
    // smaller than preceding timestamps. This bears further investigation.

    // Compute the scaling factor for the lerp.
    double glDelta = static_cast<double>(glTime - start.glTime);
    double glRange = static_cast<double>(end.glTime - start.glTime);
    double t       = glDelta / glRange;

    // Lerp(t1, t2, t)
    double hostRange = end.hostTime - start.hostTime;
    return mTimeline[firstSampleIndex].hostTime + hostRange * t;
}

// Callback from the perf tests.
void TracePerfTest::onFramebufferChange(GLenum target, GLuint framebuffer)
{
    if (!mIsTimestampQueryAvailable)
        return;

    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER)
        return;

    // We have at most one active timestamp query at a time. This code will end the current
    // query and immediately start a new one.
    if (mCurrentQuery.beginTimestampQuery != 0)
    {
        glGenQueriesEXT(1, &mCurrentQuery.endTimestampQuery);
        glQueryCounterEXT(mCurrentQuery.endTimestampQuery, GL_TIMESTAMP_EXT);
        mRunningQueries.push_back(mCurrentQuery);
        mCurrentQuery = {};
    }

    ASSERT(mCurrentQuery.beginTimestampQuery == 0);

    glGenQueriesEXT(1, &mCurrentQuery.beginTimestampQuery);
    glQueryCounterEXT(mCurrentQuery.beginTimestampQuery, GL_TIMESTAMP_EXT);
    mCurrentQuery.framebuffer = framebuffer;
}

void TracePerfTest::saveScreenshot(const std::string &screenshotName)
{
    // Render a single frame.
    RestrictedTraceID testID   = GetParam().testID;
    const TraceInfo &traceInfo = kTraceInfos[testID];
    ReplayFrame(testID, traceInfo.startFrame);

    // RGBA 4-byte data.
    uint32_t pixelCount = mTestParams.windowWidth * mTestParams.windowHeight;
    std::vector<uint8_t> pixelData(pixelCount * 4);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadPixels(0, 0, mTestParams.windowWidth, mTestParams.windowHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelData.data());

    // Convert to RGB and flip y.
    std::vector<uint8_t> rgbData(pixelCount * 3);
    for (EGLint y = 0; y < mTestParams.windowHeight; ++y)
    {
        for (EGLint x = 0; x < mTestParams.windowWidth; ++x)
        {
            EGLint srcPixel = x + y * mTestParams.windowWidth;
            EGLint dstPixel = x + (mTestParams.windowHeight - y - 1) * mTestParams.windowWidth;
            memcpy(&rgbData[dstPixel * 3], &pixelData[srcPixel * 4], 3);
        }
    }

    angle::SavePNGRGB(screenshotName.c_str(), "ANGLE Screenshot", mTestParams.windowWidth,
                      mTestParams.windowHeight, rgbData);

    // Finish the frame loop.
    for (uint32_t nextFrame = traceInfo.startFrame + 1; nextFrame < traceInfo.endFrame; ++nextFrame)
    {
        ReplayFrame(testID, nextFrame);
    }
    getGLWindow()->swap();
    glFinish();
}

ANGLE_MAYBE_UNUSED void FramebufferChangeCallback(void *userData, GLenum target, GLuint framebuffer)
{
    reinterpret_cast<TracePerfTest *>(userData)->onFramebufferChange(target, framebuffer);
}

TEST_P(TracePerfTest, Run)
{
    run();
}

TracePerfParams CombineTestID(const TracePerfParams &in, RestrictedTraceID id)
{
    TracePerfParams out = in;
    out.testID          = id;
    return out;
}

using namespace params;
using P = TracePerfParams;

std::vector<P> gTestsWithID =
    CombineWithValues({P()}, AllEnums<RestrictedTraceID>(), CombineTestID);
std::vector<P> gTestsWithRenderer = CombineWithFuncs(gTestsWithID, {Vulkan<P>, EGL<P>});
ANGLE_INSTANTIATE_TEST_ARRAY(TracePerfTest, gTestsWithRenderer);

}  // anonymous namespace
