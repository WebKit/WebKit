//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLSyncTest.cpp:
//   Tests of EGL_KHR_fence_sync and EGL_KHR_wait_sync extensions.

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_configs.h"
#include "util/EGLWindow.h"

using namespace angle;

class EGLSyncTest : public ANGLETest
{
  protected:
    bool hasFenceSyncExtension() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), "EGL_KHR_fence_sync");
    }
    bool hasWaitSyncExtension() const
    {
        return hasFenceSyncExtension() &&
               IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), "EGL_KHR_wait_sync");
    }
    bool hasGLSyncExtension() const { return IsGLExtensionEnabled("GL_OES_EGL_sync"); }
};

// Test error cases for all EGL_KHR_fence_sync functions
TEST_P(EGLSyncTest, FenceSyncErrors)
{
    ANGLE_SKIP_TEST_IF(!hasFenceSyncExtension());

    EGLDisplay display = getEGLWindow()->getDisplay();

    // If the client API doesn't have the necessary extension, test that sync creation fails and
    // ignore the rest of the tests.
    if (!hasGLSyncExtension())
    {
        EXPECT_EQ(EGL_NO_SYNC_KHR, eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nullptr));
        EXPECT_EGL_ERROR(EGL_BAD_MATCH);
    }

    ANGLE_SKIP_TEST_IF(!hasGLSyncExtension());

    EGLContext context     = eglGetCurrentContext();
    EGLSurface drawSurface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface readSurface = eglGetCurrentSurface(EGL_READ);

    EXPECT_NE(context, EGL_NO_CONTEXT);
    EXPECT_NE(drawSurface, EGL_NO_SURFACE);
    EXPECT_NE(readSurface, EGL_NO_SURFACE);

    // CreateSync with no attribute shouldn't cause an error
    EGLSyncKHR sync = eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC_KHR);

    EXPECT_EGL_TRUE(eglDestroySyncKHR(display, sync));

    // CreateSync with empty attribute shouldn't cause an error
    const EGLint emptyAttributes[] = {EGL_NONE};
    sync                           = eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, emptyAttributes);
    EXPECT_NE(sync, EGL_NO_SYNC_KHR);

    // DestroySync generates BAD_PARAMETER if the sync is not valid
    EXPECT_EGL_FALSE(eglDestroySyncKHR(display, reinterpret_cast<EGLSyncKHR>(20)));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // CreateSync generates BAD_DISPLAY if display is not valid
    EXPECT_EQ(EGL_NO_SYNC_KHR, eglCreateSyncKHR(EGL_NO_DISPLAY, EGL_SYNC_FENCE_KHR, nullptr));
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    // CreateSync generates BAD_ATTRIBUTE if attribute is neither nullptr nor empty.
    const EGLint nonEmptyAttributes[] = {
        EGL_CL_EVENT_HANDLE,
        0,
        EGL_NONE,
    };
    EXPECT_EQ(EGL_NO_SYNC_KHR, eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nonEmptyAttributes));
    EXPECT_EGL_ERROR(EGL_BAD_ATTRIBUTE);

    // CreateSync generates BAD_ATTRIBUTE if type is not valid
    EXPECT_EQ(EGL_NO_SYNC_KHR, eglCreateSyncKHR(display, 0, nullptr));
    EXPECT_EGL_ERROR(EGL_BAD_ATTRIBUTE);

    // CreateSync generates BAD_MATCH if no context is current
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    EXPECT_EQ(EGL_NO_SYNC_KHR, eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nullptr));
    EXPECT_EGL_ERROR(EGL_BAD_MATCH);
    eglMakeCurrent(display, drawSurface, readSurface, context);

    // ClientWaitSync generates EGL_BAD_PARAMETER if the sync object is not valid
    EXPECT_EGL_FALSE(eglClientWaitSyncKHR(display, reinterpret_cast<EGLSyncKHR>(30), 0, 0));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // GetSyncAttrib generates EGL_BAD_PARAMETER if the sync object is not valid, and value is not
    // modified
    constexpr EGLint kSentinelAttribValue = 123456789;
    EGLint attribValue                    = kSentinelAttribValue;
    EXPECT_EGL_FALSE(eglGetSyncAttribKHR(display, reinterpret_cast<EGLSyncKHR>(40),
                                         EGL_SYNC_TYPE_KHR, &attribValue));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    EXPECT_EQ(attribValue, kSentinelAttribValue);

    // GetSyncAttrib generates EGL_BAD_ATTRIBUTE if the attribute is not valid, and value is not
    // modified
    EXPECT_EGL_FALSE(eglGetSyncAttribKHR(display, sync, EGL_CL_EVENT_HANDLE, &attribValue));
    EXPECT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
    EXPECT_EQ(attribValue, kSentinelAttribValue);

    // GetSyncAttrib generates EGL_BAD_MATCH if the attribute is valid for sync, but not the
    // particular sync type. We don't have such a case at the moment.

    EXPECT_EGL_TRUE(eglDestroySyncKHR(display, sync));
}

// Test error cases for all EGL_KHR_wait_sync functions
TEST_P(EGLSyncTest, WaitSyncErrors)
{
    // The client API that shows support for eglWaitSyncKHR is the same as the one required for
    // eglCreateSyncKHR.  As such, there is no way to create a sync and not be able to wait on it.
    // This would have created an EGL_BAD_MATCH error.
    ANGLE_SKIP_TEST_IF(!hasWaitSyncExtension() || !hasGLSyncExtension());

    EGLDisplay display     = getEGLWindow()->getDisplay();
    EGLContext context     = eglGetCurrentContext();
    EGLSurface drawSurface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface readSurface = eglGetCurrentSurface(EGL_READ);

    EXPECT_NE(context, EGL_NO_CONTEXT);
    EXPECT_NE(drawSurface, EGL_NO_SURFACE);
    EXPECT_NE(readSurface, EGL_NO_SURFACE);

    EGLSyncKHR sync = eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC_KHR);

    // WaitSync generates BAD_MATCH if no context is current
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    EXPECT_EGL_FALSE(eglWaitSyncKHR(display, sync, 0));
    EXPECT_EGL_ERROR(EGL_BAD_MATCH);
    eglMakeCurrent(display, drawSurface, readSurface, context);

    // WaitSync generates BAD_PARAMETER if the sync is not valid
    EXPECT_EGL_FALSE(eglWaitSyncKHR(display, reinterpret_cast<EGLSyncKHR>(20), 0));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // WaitSync generates BAD_PARAMETER if flags is non-zero
    EXPECT_EGL_FALSE(eglWaitSyncKHR(display, sync, 1));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    EXPECT_EGL_TRUE(eglDestroySyncKHR(display, sync));
}

// Test usage of eglGetSyncAttribKHR
TEST_P(EGLSyncTest, GetSyncAttrib)
{
    ANGLE_SKIP_TEST_IF(!hasFenceSyncExtension() || !hasGLSyncExtension());

    EGLDisplay display = getEGLWindow()->getDisplay();

    EGLSyncKHR sync = eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC_KHR);

    // Fence sync attributes are:
    //
    // EGL_SYNC_TYPE_KHR: EGL_SYNC_FENCE_KHR
    // EGL_SYNC_STATUS_KHR: EGL_UNSIGNALED_KHR or EGL_SIGNALED_KHR
    // EGL_SYNC_CONDITION_KHR: EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR

    constexpr EGLint kSentinelAttribValue = 123456789;
    EGLint attribValue                    = kSentinelAttribValue;
    EXPECT_EGL_TRUE(eglGetSyncAttribKHR(display, sync, EGL_SYNC_TYPE_KHR, &attribValue));
    EXPECT_EQ(attribValue, EGL_SYNC_FENCE_KHR);

    attribValue = kSentinelAttribValue;
    EXPECT_EGL_TRUE(eglGetSyncAttribKHR(display, sync, EGL_SYNC_CONDITION_KHR, &attribValue));
    EXPECT_EQ(attribValue, EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR);

    attribValue = kSentinelAttribValue;
    EXPECT_EGL_TRUE(eglGetSyncAttribKHR(display, sync, EGL_SYNC_STATUS_KHR, &attribValue));

    // Hack around EXPECT_* not having an "either this or that" variant:
    if (attribValue != EGL_SIGNALED_KHR)
    {
        EXPECT_EQ(attribValue, EGL_UNSIGNALED_KHR);
    }

    EXPECT_EGL_TRUE(eglDestroySyncKHR(display, sync));
}

// Test that basic usage works and doesn't generate errors or crash
TEST_P(EGLSyncTest, BasicOperations)
{
    ANGLE_SKIP_TEST_IF(!hasFenceSyncExtension() || !hasGLSyncExtension());

    EGLDisplay display = getEGLWindow()->getDisplay();

    EGLSyncKHR sync = eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC_KHR);

    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_EGL_TRUE(eglWaitSyncKHR(display, sync, 0));

    glFlush();

    EGLint value           = 0;
    unsigned int loopCount = 0;

    // Use 'loopCount' to make sure the test doesn't get stuck in an infinite loop
    while (value != EGL_SIGNALED_KHR && loopCount <= 1000000)
    {
        loopCount++;
        EXPECT_EGL_TRUE(eglGetSyncAttribKHR(display, sync, EGL_SYNC_STATUS_KHR, &value));
    }

    ASSERT_EQ(value, EGL_SIGNALED_KHR);

    for (size_t i = 0; i < 20; i++)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EQ(
            EGL_CONDITION_SATISFIED_KHR,
            eglClientWaitSyncKHR(display, sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR));
        EXPECT_EGL_TRUE(eglGetSyncAttribKHR(display, sync, EGL_SYNC_STATUS_KHR, &value));
        EXPECT_EQ(value, EGL_SIGNALED_KHR);
    }

    EXPECT_EGL_TRUE(eglDestroySyncKHR(display, sync));
}

ANGLE_INSTANTIATE_TEST(EGLSyncTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN(),
                       ES3_VULKAN());
