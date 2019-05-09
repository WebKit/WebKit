//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MulithreadingTest.cpp : Tests of multithreaded rendering

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

#include <mutex>
#include <thread>

namespace angle
{

class MultithreadingTest : public ANGLETest
{
  protected:
    MultithreadingTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    bool platformSupportsMultithreading() const { return (IsOpenGLES() && IsAndroid()); }
};

// Test that it's possible to make one context current on different threads
TEST_P(MultithreadingTest, MakeCurrentSingleContext)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());

    std::mutex mutex;

    EGLWindow *window  = getEGLWindow();
    EGLDisplay dpy     = window->getDisplay();
    EGLContext ctx     = window->getContext();
    EGLSurface surface = window->getSurface();

    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    EXPECT_EGL_SUCCESS();

    constexpr size_t kThreadCount = 16;
    std::array<std::thread, kThreadCount> threads;
    for (std::thread &thread : threads)
    {
        thread = std::thread([&]() {
            std::lock_guard<decltype(mutex)> lock(mutex);

            EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));
            EXPECT_EGL_SUCCESS();

            EXPECT_EGL_TRUE(eglSwapBuffers(dpy, surface));
            EXPECT_EGL_SUCCESS();

            EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
            EXPECT_EGL_SUCCESS();
        });
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }

    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));
    EXPECT_EGL_SUCCESS();
}

// Test that it's possible to make one multiple contexts current on different threads simultaneously
TEST_P(MultithreadingTest, MakeCurrentMultiContext)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());
    ANGLE_SKIP_TEST_IF(IsWindows() && IsOpenGL() && IsAMD());

    std::mutex mutex;

    EGLWindow *window = getEGLWindow();
    EGLDisplay dpy    = window->getDisplay();
    EGLConfig config  = window->getConfig();

    constexpr size_t kThreadCount         = 16;
    constexpr size_t kIterationsPerThread = 16;

    constexpr EGLint kPBufferSize = 256;

    std::array<std::thread, kThreadCount> threads;
    for (size_t thread = 0; thread < kThreadCount; thread++)
    {
        threads[thread] = std::thread([&, thread]() {
            EGLSurface pbuffer = EGL_NO_SURFACE;
            EGLConfig ctx      = EGL_NO_CONTEXT;

            {
                std::lock_guard<decltype(mutex)> lock(mutex);

                // Initialize the pbuffer and context
                EGLint pbufferAttributes[] = {
                    EGL_WIDTH, kPBufferSize, EGL_HEIGHT, kPBufferSize, EGL_NONE, EGL_NONE,
                };
                pbuffer = eglCreatePbufferSurface(dpy, config, pbufferAttributes);
                EXPECT_EGL_SUCCESS();

                ctx = window->createContext(EGL_NO_CONTEXT);
                EXPECT_NE(EGL_NO_CONTEXT, ctx);

                EXPECT_EGL_TRUE(eglMakeCurrent(dpy, pbuffer, pbuffer, ctx));
                EXPECT_EGL_SUCCESS();
            }

            for (size_t iteration = 0; iteration < kIterationsPerThread; iteration++)
            {
                std::lock_guard<decltype(mutex)> lock(mutex);

                // Base the clear color on the thread and iteration indexes so every clear color is
                // unique
                const GLColor color(static_cast<GLubyte>(thread), static_cast<GLubyte>(iteration),
                                    0, 255);
                const angle::Vector4 floatColor = color.toNormalizedVector();

                glClearColor(floatColor[0], floatColor[1], floatColor[2], floatColor[3]);
                EXPECT_GL_NO_ERROR();

                glClear(GL_COLOR_BUFFER_BIT);
                EXPECT_GL_NO_ERROR();

                EXPECT_PIXEL_COLOR_EQ(0, 0, color);
            }

            {
                std::lock_guard<decltype(mutex)> lock(mutex);

                // Clean up
                EXPECT_EGL_TRUE(
                    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
                EXPECT_EGL_SUCCESS();

                eglDestroySurface(dpy, pbuffer);
                eglDestroyContext(dpy, ctx);
            }
        });
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }
}

// TODO(geofflang): Test sharing a program between multiple shared contexts on multiple threads

ANGLE_INSTANTIATE_TEST(MultithreadingTest,
                       WithNoVirtualContexts(ES2_OPENGL()),
                       WithNoVirtualContexts(ES3_OPENGL()),
                       WithNoVirtualContexts(ES2_OPENGLES()),
                       WithNoVirtualContexts(ES3_OPENGLES()));

}  // namespace angle
