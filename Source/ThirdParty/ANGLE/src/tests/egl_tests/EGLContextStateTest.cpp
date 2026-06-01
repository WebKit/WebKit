//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLContextStateTest.cpp:
//   Tests relating to Context state.

#include <gtest/gtest.h>

#include "common/tls.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/MultiThreadSteps.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/test_utils.h"

using namespace angle;

namespace
{

class EGLContextStateTest : public ANGLETest<>
{
  public:
    void testSetUp() override
    {
        EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};

        mDisplay = eglGetPlatformDisplay(GetEglPlatform(),
                                         reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        EXPECT_TRUE(mDisplay != EGL_NO_DISPLAY);
        EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));

        int nConfigs = 0;
        ASSERT_EGL_TRUE(eglGetConfigs(mDisplay, nullptr, 0, &nConfigs));
        ASSERT_GE(nConfigs, 1);

        int nReturnedConfigs = 0;
        std::vector<EGLConfig> configs(nConfigs);
        ASSERT_EGL_TRUE(eglGetConfigs(mDisplay, configs.data(), nConfigs, &nReturnedConfigs));
        ASSERT_EQ(nConfigs, nReturnedConfigs);
        mConfig = configs[0];
    }

    EGLContext createContext(EGLConfig config)
    {
        EGLint attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE,
                            mVirtualizationGroup++, EGL_NONE};
        if (!IsEGLDisplayExtensionEnabled(mDisplay, "EGL_ANGLE_context_virtualization"))
        {
            attribs[2] = EGL_NONE;
        }

        EGLContext context = eglCreateContext(mDisplay, config, nullptr, attribs);
        EXPECT_NE(context, EGL_NO_CONTEXT);
        return context;
    }

    void testTearDown() override {}

    EGLDisplay mDisplay;
    EGLConfig mConfig;
    std::atomic<EGLint> mVirtualizationGroup;
};

// Test eglMakeCurrent() and eglGetCurrentContext() on multiple threads.
TEST_P(EGLContextStateTest, MultipleThreads)
{
    EGLContext contexts[2] = {EGL_NO_CONTEXT, EGL_NO_CONTEXT};

    contexts[0] = createContext(mConfig);
    contexts[1] = createContext(mConfig);

    {
        EXPECT_EQ(eglGetCurrentContext(), EGL_NO_CONTEXT);
        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, contexts[0]));
        EXPECT_EQ(eglGetCurrentContext(), contexts[0]);
        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, contexts[1]));
        EXPECT_EQ(eglGetCurrentContext(), contexts[1]);
    }

    std::thread thread([this, contexts]() {
        auto prevContext = eglGetCurrentContext();
        EXPECT_EQ(prevContext, EGL_NO_CONTEXT);
        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, contexts[0]));
        EXPECT_EQ(eglGetCurrentContext(), contexts[0]);
        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, prevContext));
        EXPECT_EQ(eglGetCurrentContext(), prevContext);
    });
    thread.join();
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}

ANGLE_INSTANTIATE_TEST(EGLContextStateTest,
                       WithNoFixture(ES2_D3D9()),
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES2_METAL()),
                       WithNoFixture(ES3_METAL()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES2_OPENGLES()),
                       WithNoFixture(ES3_OPENGLES()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));

}  // namespace
