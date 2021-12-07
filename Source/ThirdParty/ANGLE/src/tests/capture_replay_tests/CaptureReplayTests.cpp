//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CaptureReplayTest.cpp:
//   Application that runs replay for testing of capture replay
//

#include "common/debug.h"
#include "common/system_utils.h"
#include "util/EGLPlatformParameters.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"

#include <stdint.h>
#include <string.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "util/capture/frame_capture_test_utils.h"

constexpr char kResultTag[] = "*RESULT";
constexpr char kTracePath[] = ANGLE_CAPTURE_REPLAY_TEST_NAMES_PATH;

class CaptureReplayTests
{
  public:
    CaptureReplayTests()
    {
        // Load EGL library so we can initialize the display.
        mEntryPointsLib.reset(
            angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME, angle::SearchType::ModuleDir));

        mOSWindow = OSWindow::New();
        mOSWindow->disableErrorMessageDialog();
    }

    ~CaptureReplayTests()
    {
        EGLWindow::Delete(&mEGLWindow);
        OSWindow::Delete(&mOSWindow);
    }

    bool initializeTest(const std::string &execDir, const angle::TraceInfo &traceInfo)
    {
        if (!mOSWindow->initialize(traceInfo.name, traceInfo.drawSurfaceWidth,
                                   traceInfo.drawSurfaceHeight))
        {
            return false;
        }

        mOSWindow->disableErrorMessageDialog();
        mOSWindow->setVisible(true);

        if (mEGLWindow && !mEGLWindow->isContextVersion(traceInfo.contextClientMajorVersion,
                                                        traceInfo.contextClientMinorVersion))
        {
            EGLWindow::Delete(&mEGLWindow);
            mEGLWindow = nullptr;
        }

        if (!mEGLWindow)
        {
            mEGLWindow = EGLWindow::New(traceInfo.contextClientMajorVersion,
                                        traceInfo.contextClientMinorVersion);
        }

        ConfigParameters configParams;
        configParams.redBits     = traceInfo.configRedBits;
        configParams.greenBits   = traceInfo.configGreenBits;
        configParams.blueBits    = traceInfo.configBlueBits;
        configParams.alphaBits   = traceInfo.configAlphaBits;
        configParams.depthBits   = traceInfo.configDepthBits;
        configParams.stencilBits = traceInfo.configStencilBits;

        configParams.clientArraysEnabled   = traceInfo.areClientArraysEnabled;
        configParams.bindGeneratesResource = traceInfo.isBindGeneratesResourcesEnabled;
        configParams.webGLCompatibility    = traceInfo.isWebGLCompatibilityEnabled;
        configParams.robustResourceInit    = traceInfo.isRobustResourceInitEnabled;

        mPlatformParams.renderer                 = traceInfo.displayPlatformType;
        mPlatformParams.deviceType               = traceInfo.displayDeviceType;
        mPlatformParams.forceInitShaderVariables = EGL_TRUE;

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

        // Load trace
        mTraceLibrary.reset(new angle::TraceLibrary(traceInfo.name));
        if (!mTraceLibrary->valid())
        {
            std::cout << "Failed to load trace library: " << traceInfo.name << "\n";
            return false;
        }

        if (traceInfo.isBinaryDataCompressed)
        {
            mTraceLibrary->setBinaryDataDecompressCallback(angle::DecompressBinaryData);
        }

        std::stringstream binaryPathStream;
        binaryPathStream << execDir << angle::GetPathSeparator()
                         << ANGLE_CAPTURE_REPLAY_TEST_DATA_DIR;

        mTraceLibrary->setBinaryDataDir(binaryPathStream.str().c_str());

        mTraceLibrary->setupReplay();
        return true;
    }

    void cleanupTest()
    {
        mTraceLibrary.reset(nullptr);
        mEGLWindow->destroyGL();
        mOSWindow->destroy();
    }

    void swap() { mEGLWindow->swap(); }

    int runTest(const std::string &exeDir, const angle::TraceInfo &traceInfo)
    {
        if (!initializeTest(exeDir, traceInfo))
        {
            return -1;
        }

        for (uint32_t frame = traceInfo.frameStart; frame <= traceInfo.frameEnd; frame++)
        {
            mTraceLibrary->replayFrame(frame);

            const char *replayedSerializedState =
                reinterpret_cast<const char *>(glGetString(GL_SERIALIZED_CONTEXT_STRING_ANGLE));
            const char *capturedSerializedState = mTraceLibrary->getSerializedContextState(frame);

            bool isEqual =
                (capturedSerializedState && replayedSerializedState)
                    ? compareSerializedContexts(replayedSerializedState, capturedSerializedState)
                    : (capturedSerializedState == replayedSerializedState);

            // Swap always to allow RenderDoc/other tools to capture frames.
            swap();
            if (!isEqual)
            {
                std::ostringstream replayName;
                replayName << exeDir << angle::GetPathSeparator() << traceInfo.name
                           << "_ContextReplayed" << frame << ".json";

                std::ofstream debugReplay(replayName.str());
                debugReplay << (replayedSerializedState ? replayedSerializedState : "") << "\n";

                std::ostringstream captureName;
                captureName << exeDir << angle::GetPathSeparator() << traceInfo.name
                            << "_ContextCaptured" << frame << ".json";
                std::ofstream debugCapture(captureName.str());

                debugCapture << (capturedSerializedState ? capturedSerializedState : "") << "\n";

                cleanupTest();
                return -1;
            }
        }
        cleanupTest();
        return 0;
    }

    int run()
    {
        std::string startingDirectory = angle::GetCWD().value();

        // Set CWD to executable directory.
        std::string exeDir = angle::GetExecutableDirectory();

        std::vector<std::string> traces;

        std::stringstream tracePathStream;
        tracePathStream << exeDir << angle::GetPathSeparator() << kTracePath;

        if (!angle::LoadTraceNamesFromJSON(tracePathStream.str(), &traces))
        {
            std::cout << "Unable to load trace names from " << kTracePath << "\n";
            return 1;
        }

        for (const std::string &trace : traces)
        {
            std::stringstream traceJsonPathStream;
            traceJsonPathStream << exeDir << angle::GetPathSeparator()
                                << ANGLE_CAPTURE_REPLAY_TEST_DATA_DIR << angle::GetPathSeparator()
                                << trace << ".json";
            std::string traceJsonPath = traceJsonPathStream.str();

            int result                 = -1;
            angle::TraceInfo traceInfo = {};
            if (!angle::LoadTraceInfoFromJSON(trace, traceJsonPath, &traceInfo))
            {
                std::cout << "Unable to load trace data: " << traceJsonPath << "\n";
            }
            else
            {
                result = runTest(exeDir, traceInfo);
            }
            std::cout << kResultTag << " " << trace << " " << result << "\n";
        }

        angle::SetCWD(startingDirectory.c_str());
        return 0;
    }

  private:
    bool compareSerializedContexts(const char *capturedSerializedContextState,
                                   const char *replaySerializedContextState)
    {

        return !strcmp(replaySerializedContextState, capturedSerializedContextState);
    }

    OSWindow *mOSWindow   = nullptr;
    EGLWindow *mEGLWindow = nullptr;
    EGLPlatformParameters mPlatformParams;
    // Handle to the entry point binding library.
    std::unique_ptr<angle::Library> mEntryPointsLib;
    std::unique_ptr<angle::TraceLibrary> mTraceLibrary;
};

int main(int argc, char **argv)
{
    CaptureReplayTests app;
    return app.run();
}
