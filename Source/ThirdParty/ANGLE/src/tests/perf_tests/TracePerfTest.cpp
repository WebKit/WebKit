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
#include "tests/perf_tests/ANGLEPerfTestArgs.h"
#include "tests/perf_tests/DrawCallPerfParams.h"
#include "util/egl_loader_autogen.h"
#include "util/frame_capture_test_utils.h"
#include "util/png_utils.h"

#include "restricted_traces/restricted_traces_autogen.h"

#include <cassert>
#include <functional>
#include <sstream>

using namespace angle;
using namespace egl_platform;

namespace
{
struct TracePerfParams final : public RenderTestParams
{
    // Common default options
    TracePerfParams()
    {
        majorVersion = 3;
        minorVersion = 1;

        // Display the frame after every drawBenchmark invocation
        iterationsPerStep = 1;
    }

    std::string story() const override
    {
        std::stringstream strstr;
        strstr << RenderTestParams::story() << "_" << GetTraceInfo(testID).name;
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

    void onReplayFramebufferChange(GLenum target, GLuint framebuffer);
    void onReplayInvalidateFramebuffer(GLenum target,
                                       GLsizei numAttachments,
                                       const GLenum *attachments);
    void onReplayInvalidateSubFramebuffer(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum *attachments,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height);
    void onReplayDrawBuffers(GLsizei n, const GLenum *bufs);
    void onReplayReadBuffer(GLenum src);
    void onReplayDiscardFramebufferEXT(GLenum target,
                                       GLsizei numAttachments,
                                       const GLenum *attachments);

    bool isDefaultFramebuffer(GLenum target) const;

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
    bool mUseTimestampQueries      = false;
    GLuint mOffscreenFramebuffer   = 0;
    GLuint mOffscreenTexture       = 0;
    GLuint mOffscreenDepthStencil  = 0;
    int mWindowWidth               = 0;
    int mWindowHeight              = 0;
    GLuint mDrawFramebufferBinding = 0;
    GLuint mReadFramebufferBinding = 0;
};

class TracePerfTest;
TracePerfTest *gCurrentTracePerfTest = nullptr;

// Don't forget to include KHRONOS_APIENTRY in override methods. Neccessary on Win/x86.
void KHRONOS_APIENTRY BindFramebufferProc(GLenum target, GLuint framebuffer)
{
    gCurrentTracePerfTest->onReplayFramebufferChange(target, framebuffer);
}

void KHRONOS_APIENTRY InvalidateFramebufferProc(GLenum target,
                                                GLsizei numAttachments,
                                                const GLenum *attachments)
{
    gCurrentTracePerfTest->onReplayInvalidateFramebuffer(target, numAttachments, attachments);
}

void KHRONOS_APIENTRY InvalidateSubFramebufferProc(GLenum target,
                                                   GLsizei numAttachments,
                                                   const GLenum *attachments,
                                                   GLint x,
                                                   GLint y,
                                                   GLsizei width,
                                                   GLsizei height)
{
    gCurrentTracePerfTest->onReplayInvalidateSubFramebuffer(target, numAttachments, attachments, x,
                                                            y, width, height);
}

void KHRONOS_APIENTRY DrawBuffersProc(GLsizei n, const GLenum *bufs)
{
    gCurrentTracePerfTest->onReplayDrawBuffers(n, bufs);
}

void KHRONOS_APIENTRY ReadBufferProc(GLenum src)
{
    gCurrentTracePerfTest->onReplayReadBuffer(src);
}

void KHRONOS_APIENTRY DiscardFramebufferEXTProc(GLenum target,
                                                GLsizei numAttachments,
                                                const GLenum *attachments)
{
    gCurrentTracePerfTest->onReplayDiscardFramebufferEXT(target, numAttachments, attachments);
}

angle::GenericProc KHRONOS_APIENTRY TraceLoadProc(const char *procName)
{
    if (strcmp(procName, "glBindFramebuffer") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(BindFramebufferProc);
    }
    if (strcmp(procName, "glInvalidateFramebuffer") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(InvalidateFramebufferProc);
    }
    if (strcmp(procName, "glInvalidateSubFramebuffer") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(InvalidateSubFramebufferProc);
    }
    if (strcmp(procName, "glDrawBuffers") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(DrawBuffersProc);
    }
    if (strcmp(procName, "glReadBuffer") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(ReadBufferProc);
    }
    if (strcmp(procName, "glDiscardFramebufferEXT") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(DiscardFramebufferEXTProc);
    }
    return gCurrentTracePerfTest->getGLWindow()->getProcAddress(procName);
}

TracePerfTest::TracePerfTest()
    : ANGLERenderTest("TracePerf", GetParam(), "ms"), mStartFrame(0), mEndFrame(0)
{
    const TracePerfParams &param = GetParam();

    // TODO: http://anglebug.com/4533 This fails after the upgrade to the 26.20.100.7870 driver.
    if (IsWindows() && IsIntel() && param.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE &&
        param.testID == RestrictedTraceID::manhattan_10)
    {
        mSkipTest = true;
    }

    // TODO: http://anglebug.com/4731 Fails on older Intel drivers. Passes in newer.
    if (IsWindows() && IsIntel() && param.driver != GLESDriverType::AngleEGL &&
        param.testID == RestrictedTraceID::angry_birds_2_1500)
    {
        mSkipTest = true;
    }

    if (param.surfaceType != SurfaceType::Window && !gEnableAllTraceTests)
    {
        printf("Test skipped. Use --enable-all-trace-tests to run.\n");
        mSkipTest = true;
    }

    if (param.eglParameters.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE &&
        !gEnableAllTraceTests)
    {
        printf("Test skipped. Use --enable-all-trace-tests to run.\n");
        mSkipTest = true;
    }

    if (param.testID == RestrictedTraceID::cod_mobile)
    {
        // TODO: http://anglebug.com/4967 Vulkan: GL_EXT_color_buffer_float not supported on Pixel 2
        // The COD:Mobile trace uses a framebuffer attachment with:
        //   format = GL_RGB
        //   type = GL_UNSIGNED_INT_10F_11F_11F_REV
        // That combination is only renderable if GL_EXT_color_buffer_float is supported.
        // It happens to not be supported on Pixel 2's Vulkan driver.
        addExtensionPrerequisite("GL_EXT_color_buffer_float");

        // TODO: http://anglebug.com/4731 This extension is missing on older Intel drivers.
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (param.testID == RestrictedTraceID::brawl_stars)
    {
        addExtensionPrerequisite("GL_EXT_shadow_samplers");
    }

    if (param.testID == RestrictedTraceID::free_fire)
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (param.testID == RestrictedTraceID::marvel_contest_of_champions)
    {
        addExtensionPrerequisite("GL_EXT_color_buffer_half_float");
    }

    if (param.testID == RestrictedTraceID::world_of_tanks_blitz)
    {
        addExtensionPrerequisite("GL_EXT_disjoint_timer_query");
    }

    if (param.testID == RestrictedTraceID::dragon_ball_legends)
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    // We already swap in TracePerfTest::drawBenchmark, no need to swap again in the harness.
    disableTestHarnessSwap();

    gCurrentTracePerfTest = this;
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

    trace_angle::LoadGLES(TraceLoadProc);

    const TraceInfo &traceInfo = GetTraceInfo(params.testID);
    mStartFrame                = traceInfo.startFrame;
    mEndFrame                  = traceInfo.endFrame;
    SetBinaryDataDecompressCallback(params.testID, DecompressBinaryData);

    setStepsPerRunLoopStep(mEndFrame - mStartFrame + 1);

    std::stringstream testDataDirStr;
    testDataDirStr << ANGLE_TRACE_DATA_DIR << "/" << traceInfo.name;
    std::string testDataDir = testDataDirStr.str();
    SetBinaryDataDir(params.testID, testDataDir.c_str());

    mWindowWidth  = mTestParams.windowWidth;
    mWindowHeight = mTestParams.windowHeight;

    if (IsAndroid())
    {
        // On Android, set the orientation used by the app, based on width/height
        getWindow()->setOrientation(mTestParams.windowWidth, mTestParams.windowHeight);
    }

    // If we're rendering offscreen we set up a default backbuffer.
    if (params.surfaceType == SurfaceType::Offscreen)
    {
        if (!IsAndroid())
        {
            mWindowWidth *= 4;
            mWindowHeight *= 4;
        }

        glGenFramebuffers(1, &mOffscreenFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mOffscreenFramebuffer);

        // Hard-code RGBA8/D24S8. This should be specified in the trace info.
        glGenTextures(1, &mOffscreenTexture);
        glBindTexture(GL_TEXTURE_2D, mOffscreenTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWindowWidth, mWindowHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);

        glGenRenderbuffers(1, &mOffscreenDepthStencil);
        glBindRenderbuffer(GL_RENDERBUFFER, mOffscreenDepthStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWindowWidth, mWindowHeight);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mOffscreenTexture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  mOffscreenDepthStencil);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  mOffscreenDepthStencil);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    // Potentially slow. Can load a lot of resources.
    SetupReplay(params.testID);

    glFinish();

    ASSERT_TRUE(mEndFrame > mStartFrame);

    getWindow()->ignoreSizeEvents();
    getWindow()->setVisible(true);

    // If we're re-tracing, trigger capture start after setup. This ensures the Setup function gets
    // recaptured into another Setup function and not merged with the first frame.
    if (angle::gStartTraceAfterSetup)
    {
        angle::SetEnvironmentVar("ANGLE_CAPTURE_TRIGGER", "0");
        getGLWindow()->swap();
    }
}

#undef TRACE_TEST_CASE

void TracePerfTest::destroyBenchmark()
{
    if (mOffscreenTexture != 0)
    {
        glDeleteTextures(1, &mOffscreenTexture);
        mOffscreenTexture = 0;
    }

    if (mOffscreenDepthStencil != 0)
    {
        glDeleteRenderbuffers(1, &mOffscreenDepthStencil);
        mOffscreenDepthStencil = 0;
    }

    if (mOffscreenFramebuffer != 0)
    {
        glDeleteFramebuffers(1, &mOffscreenFramebuffer);
        mOffscreenFramebuffer = 0;
    }

    // In order for the next test to load, restore the working directory
    angle::SetCWD(mStartingDirectory.c_str());
}

void TracePerfTest::sampleTime()
{
    if (mUseTimestampQueries)
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
    constexpr uint32_t kFramesPerX  = 6;
    constexpr uint32_t kFramesPerY  = 4;
    constexpr uint32_t kFramesPerXY = kFramesPerY * kFramesPerX;

    const uint32_t kOffscreenOffsetX =
        static_cast<uint32_t>(static_cast<double>(mTestParams.windowWidth) / 3.0f);
    const uint32_t kOffscreenOffsetY =
        static_cast<uint32_t>(static_cast<double>(mTestParams.windowHeight) / 3.0f);
    const uint32_t kOffscreenWidth  = kOffscreenOffsetX;
    const uint32_t kOffscreenHeight = kOffscreenOffsetY;

    const uint32_t kOffscreenFrameWidth = static_cast<uint32_t>(
        static_cast<double>(kOffscreenWidth / static_cast<double>(kFramesPerX)));
    const uint32_t kOffscreenFrameHeight = static_cast<uint32_t>(
        static_cast<double>(kOffscreenHeight / static_cast<double>(kFramesPerY)));

    const TracePerfParams &params = GetParam();

    // Add a time sample from GL and the host.
    sampleTime();

    uint32_t endFrame = mEndFrame;
    if (gMaxStepsPerformed > 0)
    {
        endFrame =
            std::min(endFrame, gMaxStepsPerformed - mTotalNumStepsPerformed - 1 + mStartFrame);
        mStepsPerRunLoopStep = endFrame - mStartFrame + 1;
    }

    for (uint32_t frame = mStartFrame; frame <= mEndFrame; ++frame)
    {
        char frameName[32];
        sprintf(frameName, "Frame %u", frame);
        beginInternalTraceEvent(frameName);

        startGpuTimer();
        ReplayFrame(params.testID, frame);
        stopGpuTimer();

        if (params.surfaceType != SurfaceType::Offscreen)
        {
            getGLWindow()->swap();
        }
        else
        {
            GLint currentDrawFBO, currentReadFBO;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFBO);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &currentReadFBO);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, mOffscreenFramebuffer);

            uint32_t frames  = getNumStepsPerformed() + (frame - mStartFrame);
            uint32_t frameX  = (frames % kFramesPerXY) % kFramesPerX;
            uint32_t frameY  = (frames % kFramesPerXY) / kFramesPerX;
            uint32_t windowX = kOffscreenOffsetX + frameX * kOffscreenFrameWidth;
            uint32_t windowY = kOffscreenOffsetY + frameY * kOffscreenFrameHeight;

            if (angle::gVerboseLogging)
            {
                printf("Frame %d: x %d y %d (screen x %d, screen y %d)\n", frames, frameX, frameY,
                       windowX, windowY);
            }

            GLboolean scissorTest = GL_FALSE;
            glGetBooleanv(GL_SCISSOR_TEST, &scissorTest);

            if (scissorTest)
            {
                glDisable(GL_SCISSOR_TEST);
            }

            glBlitFramebuffer(0, 0, mWindowWidth, mWindowHeight, windowX, windowY,
                              windowX + kOffscreenFrameWidth, windowY + kOffscreenFrameHeight,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);

            if (frameX == kFramesPerX - 1 && frameY == kFramesPerY - 1)
            {
                getGLWindow()->swap();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            if (scissorTest)
            {
                glEnable(GL_SCISSOR_TEST);
            }
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, currentDrawFBO);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, currentReadFBO);
        }

        endInternalTraceEvent(frameName);

        // Check for abnormal exit.
        if (!mRunning)
        {
            return;
        }
    }

    ResetReplay(params.testID);

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

// Triggered when the replay calls glBindFramebuffer.
void TracePerfTest::onReplayFramebufferChange(GLenum target, GLuint framebuffer)
{
    if (framebuffer == 0 && GetParam().surfaceType == SurfaceType::Offscreen)
    {
        glBindFramebuffer(target, mOffscreenFramebuffer);
    }
    else
    {
        glBindFramebuffer(target, framebuffer);
    }

    switch (target)
    {
        case GL_FRAMEBUFFER:
            mDrawFramebufferBinding = framebuffer;
            mReadFramebufferBinding = framebuffer;
            break;
        case GL_DRAW_FRAMEBUFFER:
            mDrawFramebufferBinding = framebuffer;
            break;
        case GL_READ_FRAMEBUFFER:
            mReadFramebufferBinding = framebuffer;
            return;

        default:
            UNREACHABLE();
            break;
    }

    if (!mUseTimestampQueries)
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

bool TracePerfTest::isDefaultFramebuffer(GLenum target) const
{
    switch (target)
    {
        case GL_FRAMEBUFFER:
        case GL_DRAW_FRAMEBUFFER:
            return (mDrawFramebufferBinding == 0);

        case GL_READ_FRAMEBUFFER:
            return (mReadFramebufferBinding == 0);

        default:
            UNREACHABLE();
            return false;
    }
}

GLenum ConvertDefaultFramebufferEnum(GLenum value)
{
    switch (value)
    {
        case GL_NONE:
            return GL_NONE;
        case GL_BACK:
        case GL_COLOR:
            return GL_COLOR_ATTACHMENT0;
        case GL_DEPTH:
            return GL_DEPTH_ATTACHMENT;
        case GL_STENCIL:
            return GL_STENCIL_ATTACHMENT;
        case GL_DEPTH_STENCIL:
            return GL_DEPTH_STENCIL_ATTACHMENT;
        default:
            UNREACHABLE();
            return GL_NONE;
    }
}

std::vector<GLenum> ConvertDefaultFramebufferEnums(GLsizei numAttachments,
                                                   const GLenum *attachments)
{
    std::vector<GLenum> translatedAttachments;
    for (GLsizei attachmentIndex = 0; attachmentIndex < numAttachments; ++attachmentIndex)
    {
        GLenum converted = ConvertDefaultFramebufferEnum(attachments[attachmentIndex]);
        translatedAttachments.push_back(converted);
    }
    return translatedAttachments;
}

// Needs special handling to treat the 0 framebuffer in offscreen mode.
void TracePerfTest::onReplayInvalidateFramebuffer(GLenum target,
                                                  GLsizei numAttachments,
                                                  const GLenum *attachments)
{
    if (GetParam().surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
    {
        glInvalidateFramebuffer(target, numAttachments, attachments);
    }
    else
    {
        std::vector<GLenum> translatedAttachments =
            ConvertDefaultFramebufferEnums(numAttachments, attachments);
        glInvalidateFramebuffer(target, numAttachments, translatedAttachments.data());
    }
}

void TracePerfTest::onReplayInvalidateSubFramebuffer(GLenum target,
                                                     GLsizei numAttachments,
                                                     const GLenum *attachments,
                                                     GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height)
{
    if (GetParam().surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
    {
        glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
    }
    else
    {
        std::vector<GLenum> translatedAttachments =
            ConvertDefaultFramebufferEnums(numAttachments, attachments);
        glInvalidateSubFramebuffer(target, numAttachments, translatedAttachments.data(), x, y,
                                   width, height);
    }
}

void TracePerfTest::onReplayDrawBuffers(GLsizei n, const GLenum *bufs)
{
    if (GetParam().surfaceType != SurfaceType::Offscreen ||
        !isDefaultFramebuffer(GL_DRAW_FRAMEBUFFER))
    {
        glDrawBuffers(n, bufs);
    }
    else
    {
        std::vector<GLenum> translatedBufs = ConvertDefaultFramebufferEnums(n, bufs);
        glDrawBuffers(n, translatedBufs.data());
    }
}

void TracePerfTest::onReplayReadBuffer(GLenum src)
{
    if (GetParam().surfaceType != SurfaceType::Offscreen ||
        !isDefaultFramebuffer(GL_READ_FRAMEBUFFER))
    {
        glReadBuffer(src);
    }
    else
    {
        GLenum translated = ConvertDefaultFramebufferEnum(src);
        glReadBuffer(translated);
    }
}

void TracePerfTest::onReplayDiscardFramebufferEXT(GLenum target,
                                                  GLsizei numAttachments,
                                                  const GLenum *attachments)
{
    if (GetParam().surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
    {
        glDiscardFramebufferEXT(target, numAttachments, attachments);
    }
    else
    {
        std::vector<GLenum> translatedAttachments =
            ConvertDefaultFramebufferEnums(numAttachments, attachments);
        glDiscardFramebufferEXT(target, numAttachments, translatedAttachments.data());
    }
}

void TracePerfTest::saveScreenshot(const std::string &screenshotName)
{
    // Render a single frame.
    RestrictedTraceID testID   = GetParam().testID;
    const TraceInfo &traceInfo = GetTraceInfo(testID);
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

    if (!angle::SavePNGRGB(screenshotName.c_str(), "ANGLE Screenshot", mTestParams.windowWidth,
                           mTestParams.windowHeight, rgbData))
    {
        FAIL() << "Error saving screenshot: " << screenshotName;
    }
    else
    {
        printf("Saved screenshot: '%s'\n", screenshotName.c_str());
    }

    // Finish the frame loop.
    for (uint32_t nextFrame = traceInfo.startFrame + 1; nextFrame <= traceInfo.endFrame;
         ++nextFrame)
    {
        ReplayFrame(testID, nextFrame);
    }
    ResetReplay(testID);
    getGLWindow()->swap();
    glFinish();
}

TEST_P(TracePerfTest, Run)
{
    run();
}

TracePerfParams CombineTestID(const TracePerfParams &in, RestrictedTraceID id)
{
    const TraceInfo &traceInfo = GetTraceInfo(id);

    TracePerfParams out = in;
    out.testID          = id;
    out.windowWidth     = traceInfo.drawSurfaceWidth;
    out.windowHeight    = traceInfo.drawSurfaceHeight;
    return out;
}

bool NoAndroidMockICD(const TracePerfParams &in)
{
    return in.eglParameters.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE || !IsAndroid();
}

TracePerfParams CombineWithSurfaceType(const TracePerfParams &in, SurfaceType surfaceType)
{
    TracePerfParams out = in;
    out.surfaceType     = surfaceType;

    if (!IsAndroid() && surfaceType == SurfaceType::Offscreen)
    {
        out.windowWidth /= 4;
        out.windowHeight /= 4;
    }

    // We track GPU time only in frame-rate-limited cases.
    out.trackGpuTime = surfaceType == SurfaceType::WindowWithVSync;

    return out;
}

using namespace params;
using P = TracePerfParams;

std::vector<P> gTestsWithID =
    CombineWithValues({P()}, AllEnums<RestrictedTraceID>(), CombineTestID);
std::vector<P> gTestsWithSurfaceType =
    CombineWithValues(gTestsWithID,
                      {SurfaceType::Offscreen, SurfaceType::Window, SurfaceType::WindowWithVSync},
                      CombineWithSurfaceType);
std::vector<P> gTestsWithRenderer =
    CombineWithFuncs(gTestsWithSurfaceType,
                     {Vulkan<P>, VulkanMockICD<P>, VulkanSwiftShader<P>, Native<P>});
std::vector<P> gTestsWithoutMockICD = FilterWithFunc(gTestsWithRenderer, NoAndroidMockICD);
ANGLE_INSTANTIATE_TEST_ARRAY(TracePerfTest, gTestsWithoutMockICD);

}  // anonymous namespace
