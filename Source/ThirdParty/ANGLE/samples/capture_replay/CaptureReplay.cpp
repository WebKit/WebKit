//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CaptureReplay: Template for replaying a frame capture with ANGLE.

#include "SampleApplication.h"

#include <functional>

#define ANGLE_MACRO_STRINGIZE_AUX(a) #a
#define ANGLE_MACRO_STRINGIZE(a) ANGLE_MACRO_STRINGIZE_AUX(a)
#define ANGLE_MACRO_CONCAT_AUX(a, b) a##b
#define ANGLE_MACRO_CONCAT(a, b) ANGLE_MACRO_CONCAT_AUX(a, b)

// Build the right context header based on replay ID
// This will expand to "angle_capture_context<#>.h"
#include ANGLE_MACRO_STRINGIZE(ANGLE_CAPTURE_REPLAY_SAMPLE_HEADER)

// Assign the context numbered functions based on GN arg selecting replay ID
std::function<void()> SetupContextReplay = reinterpret_cast<void (*)()>(
    ANGLE_MACRO_CONCAT(SetupContext,
                       ANGLE_MACRO_CONCAT(ANGLE_CAPTURE_REPLAY_SAMPLE_CONTEXT_ID, Replay)));
std::function<void(int)> ReplayContextFrame = reinterpret_cast<void (*)(int)>(
    ANGLE_MACRO_CONCAT(ReplayContext,
                       ANGLE_MACRO_CONCAT(ANGLE_CAPTURE_REPLAY_SAMPLE_CONTEXT_ID, Frame)));

class CaptureReplaySample : public SampleApplication
{
  public:
    CaptureReplaySample(int argc, char **argv)
        : SampleApplication("CaptureReplaySample", argc, argv, 3, 0)
    {}

    bool initialize() override
    {
        // Set CWD to executable directory.
        std::string exeDir = angle::GetExecutableDirectory();
        if (!angle::SetCWD(exeDir.c_str()))
            return false;
        SetBinaryDataDir(ANGLE_CAPTURE_REPLAY_SAMPLE_DATA_DIR);
        SetupContextReplay();

        eglSwapInterval(getDisplay(), 1);
        return true;
    }

    void destroy() override {}

    void draw() override
    {
        // Compute the current frame, looping from kReplayFrameStart to kReplayFrameEnd.
        uint32_t frame =
            kReplayFrameStart + (mCurrentFrame % (kReplayFrameEnd - kReplayFrameStart));
        ReplayContextFrame(frame);
        mCurrentFrame++;
    }

  private:
    uint32_t mCurrentFrame = 0;
};

int main(int argc, char **argv)
{
    CaptureReplaySample app(argc, argv);
    return app.run();
}
