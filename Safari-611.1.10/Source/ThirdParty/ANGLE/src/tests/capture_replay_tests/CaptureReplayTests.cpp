//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CaptureReplayTest.cpp:
//   Application that runs replay for testing of capture replay
//

#include "common/system_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/frame_capture_utils.h"
#include "util/EGLPlatformParameters.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"

#include <stdint.h>
#include <string.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "util/frame_capture_test_utils.h"

// Build the right context header based on replay ID
// This will expand to "angle_capture_context<#>.h"
#include ANGLE_MACRO_STRINGIZE(ANGLE_CAPTURE_REPLAY_COMPOSITE_TESTS_HEADER)

constexpr char kResultTag[] = "*RESULT";

class CaptureReplayTests
{
  public:
    CaptureReplayTests(EGLint glesMajorVersion, EGLint glesMinorVersion)
        : mOSWindow(nullptr), mEGLWindow(nullptr)
    {
        mPlatformParams.renderer   = EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
        mPlatformParams.deviceType = EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;

        // Load EGL library so we can initialize the display.
        mEntryPointsLib.reset(
            angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME, angle::SearchType::ApplicationDir));
        mEGLWindow = EGLWindow::New(glesMajorVersion, glesMinorVersion);
        mOSWindow  = OSWindow::New();
        mOSWindow->disableErrorMessageDialog();
    }

    ~CaptureReplayTests()
    {
        EGLWindow::Delete(&mEGLWindow);
        OSWindow::Delete(&mOSWindow);
    }

    bool initializeTest(uint32_t testIndex, TestTraceInfo &testTraceInfo)
    {
        if (!mOSWindow->initialize(testTraceInfo.testName, testTraceInfo.replayDrawSurfaceWidth,
                                   testTraceInfo.replayDrawSurfaceHeight))
        {
            return false;
        }
        mOSWindow->disableErrorMessageDialog();
        mOSWindow->setVisible(true);

        ConfigParameters configParams;
        configParams.redBits     = testTraceInfo.defaultFramebufferRedBits;
        configParams.greenBits   = testTraceInfo.defaultFramebufferGreenBits;
        configParams.blueBits    = testTraceInfo.defaultFramebufferBlueBits;
        configParams.alphaBits   = testTraceInfo.defaultFramebufferAlphaBits;
        configParams.depthBits   = testTraceInfo.defaultFramebufferDepthBits;
        configParams.stencilBits = testTraceInfo.defaultFramebufferStencilBits;
        if (!mEGLWindow->initializeGL(mOSWindow, mEntryPointsLib.get(),
                                      angle::GLESDriverType::AngleEGL, mPlatformParams,
                                      configParams))
        {
            mOSWindow->destroy();
            return false;
        }
        // Disable vsync
        if (!mEGLWindow->setSwapInterval(0))
        {
            cleanupTest();
            return false;
        }
        // Set CWD to executable directory.
        std::string exeDir = angle::GetExecutableDirectory();
        if (!angle::SetCWD(exeDir.c_str()))
        {
            cleanupTest();
            return false;
        }
        if (testTraceInfo.isBinaryDataCompressed)
        {
            SetBinaryDataDecompressCallback(testIndex, angle::DecompressBinaryData);
        }
        SetBinaryDataDir(testIndex, ANGLE_CAPTURE_REPLAY_TEST_DATA_DIR);

        SetupContextReplay(testIndex);
        return true;
    }

    void cleanupTest()
    {
        mEGLWindow->destroyGL();
        mOSWindow->destroy();
    }

    void swap() { mEGLWindow->swap(); }

    int runTest(uint32_t testIndex, TestTraceInfo &testTraceInfo)
    {
        if (!initializeTest(testIndex, testTraceInfo))
        {
            return -1;
        }

        for (uint32_t frame = testTraceInfo.replayFrameStart; frame <= testTraceInfo.replayFrameEnd;
             frame++)
        {
            ReplayContextFrame(testIndex, frame);
            gl::Context *context = static_cast<gl::Context *>(mEGLWindow->getContext());
            gl::BinaryOutputStream bos;
            if (angle::SerializeContext(&bos, context) != angle::Result::Continue)
            {
                cleanupTest();
                return -1;
            }
            bool isEqual = compareSerializedContexts(testIndex, frame, bos.getData());
            if (!isEqual)
            {
                cleanupTest();
                return -1;
            }
            swap();
        }
        cleanupTest();
        return 0;
    }

    int run()
    {
        for (size_t i = 0; i < testTraceInfos.size(); i++)
        {
            int result = runTest(static_cast<uint32_t>(i), testTraceInfos[i]);
            std::cout << kResultTag << " " << testTraceInfos[i].testName << " " << result << "\n";
        }
        return 0;
    }

  private:
    bool compareSerializedContexts(uint32_t testIndex,
                                   uint32_t frame,
                                   const std::vector<uint8_t> &replaySerializedContextState)
    {

        return memcmp(replaySerializedContextState.data(),
                      GetSerializedContextState(testIndex, frame),
                      replaySerializedContextState.size()) == 0;
    }

    OSWindow *mOSWindow;
    EGLWindow *mEGLWindow;
    EGLPlatformParameters mPlatformParams;
    // Handle to the entry point binding library.
    std::unique_ptr<angle::Library> mEntryPointsLib;
};

int main(int argc, char **argv)
{
    // TODO (nguyenmh): http://anglebug.com/4759: initialize app with arguments taken from cmdline
    const EGLint glesMajorVersion = 2;
    const GLint glesMinorVersion  = 0;
    CaptureReplayTests app(glesMajorVersion, glesMinorVersion);
    return app.run();
}
