//
//  Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLSyncTestMetalSharedEvent:
//   Tests pertaining to EGL_ANGLE_sync_mtl_shared_event extension.
//

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"
#include "util/EGLWindow.h"

#include <Metal/Metal.h>

using namespace angle;

class EGLSyncTestMetalSharedEvent : public ANGLETest
{
  protected:
    id<MTLSharedEvent> createMetalSharedEvent() const
    {
        id<MTLDevice> device = getMetalDevice();
        return [device newSharedEvent];
    }

    id<MTLDevice> getMetalDevice() const
    {
        EGLAttrib angleDevice = 0;
        EXPECT_EGL_TRUE(
            eglQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice));

        EGLAttrib device = 0;
        EXPECT_EGL_TRUE(
            eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                    EGL_METAL_DEVICE_ANGLE, &device));

        return (__bridge id<MTLDevice>)reinterpret_cast<void *>(device);
    }

    bool hasEGLDisplayExtension(const char* extname) const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), extname);
    }

    bool hasFenceSyncExtension() const
    {
        return hasEGLDisplayExtension("EGL_KHR_fence_sync");
    }

    bool hasGLSyncExtension() const { return IsGLExtensionEnabled("GL_OES_EGL_sync"); }

    bool hasSyncMetalSharedEventExtension() const
    {
        return hasEGLDisplayExtension("EGL_ANGLE_metal_shared_event_sync");
    }
};

TEST_P(EGLSyncTestMetalSharedEvent, BasicOperations)
{
    EXPECT_EGL_TRUE(hasSyncMetalSharedEventExtension());

    EGLWindow *window = getEGLWindow();

    id<MTLSharedEvent> sharedEvent = createMetalSharedEvent();
    sharedEvent.label = @"TestSharedEvent";

    EGLDisplay display = window->getDisplay();

    EGLAttrib syncAttribs[] = {EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE, (EGLAttrib)sharedEvent, EGL_NONE};

    EGLSync sync = eglCreateSync(display, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, syncAttribs);
    EXPECT_NE(sync, EGL_NO_SYNC);

    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_EGL_TRUE(eglWaitSync(display, sync, 0));

    glFlush();

    glClear(GL_COLOR_BUFFER_BIT);

    // Don't wait forever to make sure the test terminates
    constexpr GLuint64 kTimeout = 1'000'000'000;  // 1 second
    EGLAttrib value             = 0;
    ASSERT_EQ(EGL_CONDITION_SATISFIED,
              eglClientWaitSync(display, sync, EGL_SYNC_FLUSH_COMMANDS_BIT, kTimeout));

    for (size_t i = 0; i < 20; i++)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EQ(
            EGL_CONDITION_SATISFIED,
            eglClientWaitSync(display, sync, EGL_SYNC_FLUSH_COMMANDS_BIT, EGL_FOREVER));
        EXPECT_EGL_TRUE(eglGetSyncAttrib(display, sync, EGL_SYNC_STATUS, &value));
        EXPECT_EQ(value, EGL_SIGNALED);
    }

    EXPECT_EGL_TRUE(eglDestroySync(display, sync));

    sharedEvent = nil;
}

ANGLE_INSTANTIATE_TEST(EGLSyncTestMetalSharedEvent,
                       ES2_METAL(),
                       ES3_METAL());
