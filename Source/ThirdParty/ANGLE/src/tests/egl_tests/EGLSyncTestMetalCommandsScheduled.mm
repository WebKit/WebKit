// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLSyncTestCommandsScheduled:
//   Tests pertaining to EGL_ANGLE_metal_commands_scheduled_sync extension.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

#include <Metal/Metal.h>
#include <thread>

using namespace angle;

class EGLSyncTestMetalCommandsScheduled : public ANGLETest<>
{
  protected:
    bool hasCommandsScheduledSyncExtension() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(),
                                            "EGL_ANGLE_metal_commands_scheduled_sync");
    }

    void waitForCommandsScheduled(EGLDisplay display, EGLSync sync)
    {
        // Don't wait forever to make sure the test terminates
        constexpr GLuint64 kTimeout = 1000'000'000ul;  // 1 second
        EXPECT_EQ(EGL_CONDITION_SATISFIED,
                  eglClientWaitSync(display, sync, EGL_SYNC_FLUSH_COMMANDS_BIT, kTimeout));

        EGLAttrib value = 0;
        EXPECT_EGL_TRUE(eglGetSyncAttrib(display, sync, EGL_SYNC_STATUS, &value));
        EXPECT_EQ(value, EGL_SIGNALED);
    }

    void recordCommands()
    {
        // Create work to do
        ANGLE_GL_PROGRAM(redProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        drawQuad(redProgram, essl1_shaders::PositionAttrib(), 0.5f);
    }
};

// Test that a sync object is signaled after commands are scheduled.
TEST_P(EGLSyncTestMetalCommandsScheduled, SingleThread)
{
    ANGLE_SKIP_TEST_IF(!hasCommandsScheduledSyncExtension());

    recordCommands();

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLSync sync       = eglCreateSync(display, EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC);

    waitForCommandsScheduled(display, sync);

    EXPECT_EGL_TRUE(eglDestroySync(display, sync));
}

// Test that a sync object can be waited on from another thread.
TEST_P(EGLSyncTestMetalCommandsScheduled, MultiThread)
{
    ANGLE_SKIP_TEST_IF(!hasCommandsScheduledSyncExtension());

    recordCommands();

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLSync sync       = eglCreateSync(display, EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC);

    std::thread thread([&]() { waitForCommandsScheduled(display, sync); });
    thread.join();

    EXPECT_EGL_TRUE(eglDestroySync(display, sync));
}

// Test that a sync object can be waited on after the work is done.
TEST_P(EGLSyncTestMetalCommandsScheduled, AfterFinish)
{
    ANGLE_SKIP_TEST_IF(!hasCommandsScheduledSyncExtension());

    recordCommands();

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLSync sync       = eglCreateSync(display, EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC);

    glFinish();

    waitForCommandsScheduled(display, sync);

    EXPECT_EGL_TRUE(eglDestroySync(display, sync));
}

// Test that a sync object is signaled even if no work is done.
TEST_P(EGLSyncTestMetalCommandsScheduled, NoWork)
{
    ANGLE_SKIP_TEST_IF(!hasCommandsScheduledSyncExtension());

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLSync sync       = eglCreateSync(display, EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC);

    waitForCommandsScheduled(display, sync);

    EXPECT_EGL_TRUE(eglDestroySync(display, sync));
}

// Test that there's no crash if a sync object is destroyed before its command buffer is scheduled.
TEST_P(EGLSyncTestMetalCommandsScheduled, DestroyBeforeScheduled)
{
    ANGLE_SKIP_TEST_IF(!hasCommandsScheduledSyncExtension());

    recordCommands();

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLSync sync1      = eglCreateSync(display, EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE, nullptr);
    EXPECT_NE(sync1, EGL_NO_SYNC);

    // Create and destroy another sync object immediately - it will add a callback to the commands
    // scheduled handler, but the callback shouldn't crash even after the sync object is destroyed.
    EGLSync sync2 = eglCreateSync(display, EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE, nullptr);
    EXPECT_NE(sync2, EGL_NO_SYNC);
    EXPECT_EGL_TRUE(eglDestroySync(display, sync2));

    waitForCommandsScheduled(display, sync1);

    EXPECT_EGL_TRUE(eglDestroySync(display, sync1));
}

ANGLE_INSTANTIATE_TEST(EGLSyncTestMetalCommandsScheduled, ES2_METAL(), ES3_METAL());
// This test suite is not instantiated on non-Metal backends and OSes.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLSyncTestMetalCommandsScheduled);
