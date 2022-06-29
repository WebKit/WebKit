//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MultiThreadSteps.cpp:
//   Synchronization help for tests that use multiple threads.

#include "MultiThreadSteps.h"

#include "gtest/gtest.h"
#include "util/EGLWindow.h"

namespace angle
{

void RunLockStepThreads(EGLWindow *window, size_t threadCount, LockStepThreadFunc threadFuncs[])
{
    EGLDisplay dpy   = window->getDisplay();
    EGLConfig config = window->getConfig();

    constexpr EGLint kPBufferSize = 256;
    // Initialize the pbuffer and context
    EGLint pbufferAttributes[] = {
        EGL_WIDTH, kPBufferSize, EGL_HEIGHT, kPBufferSize, EGL_NONE, EGL_NONE,
    };

    std::vector<EGLSurface> surfaces(threadCount);
    std::vector<EGLContext> contexts(threadCount);

    // Create N surfaces and shared contexts, one for each thread
    for (size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex)
    {
        surfaces[threadIndex] = eglCreatePbufferSurface(dpy, config, pbufferAttributes);
        EXPECT_EQ(eglGetError(), EGL_SUCCESS);
        contexts[threadIndex] =
            window->createContext(threadIndex == 0 ? EGL_NO_CONTEXT : contexts[0], nullptr);
        EXPECT_NE(EGL_NO_CONTEXT, contexts[threadIndex]) << threadIndex;
    }

    std::vector<std::thread> threads(threadCount);

    // Run the threads
    for (size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex)
    {
        threads[threadIndex] = std::thread(std::move(threadFuncs[threadIndex]), dpy,
                                           surfaces[threadIndex], contexts[threadIndex]);
    }

    // Wait for them to finish
    for (size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex)
    {
        threads[threadIndex].join();

        // Clean up
        eglDestroySurface(dpy, surfaces[threadIndex]);
        eglDestroyContext(dpy, contexts[threadIndex]);
    }
}
}  // namespace angle
