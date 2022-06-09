//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MulithreadingTest.cpp : Tests of multithreaded rendering

#include "platform/FeaturesVk.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/MultiThreadSteps.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/test_utils.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace angle
{

class MultithreadingTest : public ANGLETest
{
  public:
    static constexpr uint32_t kSize = 512;

  protected:
    MultithreadingTest()
    {
        setWindowWidth(kSize);
        setWindowHeight(kSize);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    bool hasFenceSyncExtension() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), "EGL_KHR_fence_sync");
    }
    bool hasGLSyncExtension() const { return IsGLExtensionEnabled("GL_OES_EGL_sync"); }

    EGLContext createMultithreadedContext(EGLWindow *window, EGLContext shareCtx)
    {
        EGLint attribs[] = {EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE, mVirtualizationGroup++,
                            EGL_NONE};
        if (!IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(),
                                          "EGL_ANGLE_context_virtualization"))
        {
            attribs[0] = EGL_NONE;
        }

        return window->createContext(shareCtx, attribs);
    }

    void runMultithreadedGLTest(
        std::function<void(EGLSurface surface, size_t threadIndex)> testBody,
        size_t threadCount)
    {
        std::mutex mutex;

        EGLWindow *window = getEGLWindow();
        EGLDisplay dpy    = window->getDisplay();
        EGLConfig config  = window->getConfig();

        constexpr EGLint kPBufferSize = 256;

        std::vector<std::thread> threads(threadCount);
        for (size_t threadIdx = 0; threadIdx < threadCount; threadIdx++)
        {
            threads[threadIdx] = std::thread([&, threadIdx]() {
                EGLSurface surface = EGL_NO_SURFACE;
                EGLContext ctx     = EGL_NO_CONTEXT;

                {
                    std::lock_guard<decltype(mutex)> lock(mutex);

                    // Initialize the pbuffer and context
                    EGLint pbufferAttributes[] = {
                        EGL_WIDTH, kPBufferSize, EGL_HEIGHT, kPBufferSize, EGL_NONE, EGL_NONE,
                    };
                    surface = eglCreatePbufferSurface(dpy, config, pbufferAttributes);
                    EXPECT_EGL_SUCCESS();

                    ctx = createMultithreadedContext(window, EGL_NO_CONTEXT);
                    EXPECT_NE(EGL_NO_CONTEXT, ctx);

                    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));
                    EXPECT_EGL_SUCCESS();
                }

                testBody(surface, threadIdx);

                {
                    std::lock_guard<decltype(mutex)> lock(mutex);

                    // Clean up
                    EXPECT_EGL_TRUE(
                        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
                    EXPECT_EGL_SUCCESS();

                    eglDestroySurface(dpy, surface);
                    eglDestroyContext(dpy, ctx);
                }
            });
        }

        for (std::thread &thread : threads)
        {
            thread.join();
        }
    }

    std::atomic<EGLint> mVirtualizationGroup;
};

class MultithreadingTestES3 : public MultithreadingTest
{
  public:
    void textureThreadFunction(bool useDraw);
    void mainThreadDraw(bool useDraw);

  protected:
    MultithreadingTestES3()
        : mTexture2D(0), mExitThread(false), mMainThreadSyncObj(NULL), mSecondThreadSyncObj(NULL)
    {
        setWindowWidth(kSize);
        setWindowHeight(kSize);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    GLuint create2DTexture()
    {
        GLuint texture2D;
        glGenTextures(1, &texture2D);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        EXPECT_GL_NO_ERROR();
        return texture2D;
    }

    void testSetUp() override { mTexture2D = create2DTexture(); }

    void testTearDown() override
    {
        if (mTexture2D)
        {
            glDeleteTextures(1, &mTexture2D);
        }
    }

    std::mutex mutex;
    GLuint mTexture2D;
    std::atomic<bool> mExitThread;
    std::atomic<bool> mDrawGreen;  // Toggle drawing green or red
    std::atomic<GLsync> mMainThreadSyncObj;
    std::atomic<GLsync> mSecondThreadSyncObj;
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

// Test that multiple threads can clear and readback pixels successfully at the same time
TEST_P(MultithreadingTest, MultiContextClear)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());

    auto testBody = [](EGLSurface surface, size_t thread) {
        constexpr size_t kIterationsPerThread = 32;
        for (size_t iteration = 0; iteration < kIterationsPerThread; iteration++)
        {
            // Base the clear color on the thread and iteration indexes so every clear color is
            // unique
            const GLColor color(static_cast<GLubyte>(thread % 255),
                                static_cast<GLubyte>(iteration % 255), 0, 255);
            const angle::Vector4 floatColor = color.toNormalizedVector();

            glClearColor(floatColor[0], floatColor[1], floatColor[2], floatColor[3]);
            EXPECT_GL_NO_ERROR();

            glClear(GL_COLOR_BUFFER_BIT);
            EXPECT_GL_NO_ERROR();

            EXPECT_PIXEL_COLOR_EQ(0, 0, color);
        }
    };
    runMultithreadedGLTest(testBody, 72);
}

// Verify that threads can interleave eglDestroyContext and draw calls without
// any crashes.
TEST_P(MultithreadingTest, MultiContextDeleteDraw)
{
    // Skip this test on non-D3D11 backends, as it has the potential to time-out
    // and this test was originally intended to catch a crash on the D3D11 backend.
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());
    ANGLE_SKIP_TEST_IF(!IsD3D11());

    EGLWindow *window = getEGLWindow();
    EGLDisplay dpy    = window->getDisplay();
    EGLConfig config  = window->getConfig();

    std::thread t1 = std::thread([&]() {
        // 5000 is chosen here as it reliably reproduces the former crash.
        for (int i = 0; i < 5000; i++)
        {
            EGLContext ctx1 = createMultithreadedContext(window, EGL_NO_CONTEXT);
            EGLContext ctx2 = createMultithreadedContext(window, EGL_NO_CONTEXT);

            EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx2));
            EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx1));

            EXPECT_EGL_TRUE(eglDestroyContext(dpy, ctx2));
            EXPECT_EGL_TRUE(eglDestroyContext(dpy, ctx1));
        }
    });

    std::thread t2 = std::thread([&]() {
        EGLint pbufferAttributes[] = {
            EGL_WIDTH, 256, EGL_HEIGHT, 256, EGL_NONE, EGL_NONE,
        };

        EGLSurface surface = eglCreatePbufferSurface(dpy, config, pbufferAttributes);
        EXPECT_EGL_SUCCESS();

        auto ctx = createMultithreadedContext(window, EGL_NO_CONTEXT);
        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));

        constexpr size_t kIterationsPerThread = 512;
        constexpr size_t kDrawsPerIteration   = 512;

        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        glUseProgram(program);

        GLint colorLocation = glGetUniformLocation(program, essl1_shaders::ColorUniform());

        auto quadVertices = GetQuadVertices();

        GLBuffer vertexBuffer;
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, quadVertices.data(), GL_STATIC_DRAW);

        GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
        glEnableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        for (size_t iteration = 0; iteration < kIterationsPerThread; iteration++)
        {
            const GLColor color(static_cast<GLubyte>(15151 % 255),
                                static_cast<GLubyte>(iteration % 255), 0, 255);
            const angle::Vector4 floatColor = color.toNormalizedVector();
            glUniform4fv(colorLocation, 1, floatColor.data());
            for (size_t draw = 0; draw < kDrawsPerIteration; draw++)
            {
                EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    });

    t1.join();
    t2.join();
}

// Test that multiple threads can draw and readback pixels successfully at the same time
TEST_P(MultithreadingTest, MultiContextDraw)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());

    ANGLE_SKIP_TEST_IF(isSwiftshader());

    auto testBody = [](EGLSurface surface, size_t thread) {
        constexpr size_t kIterationsPerThread = 32;
        constexpr size_t kDrawsPerIteration   = 500;

        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        glUseProgram(program);

        GLint colorLocation = glGetUniformLocation(program, essl1_shaders::ColorUniform());

        auto quadVertices = GetQuadVertices();

        GLBuffer vertexBuffer;
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, quadVertices.data(), GL_STATIC_DRAW);

        GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
        glEnableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

        for (size_t iteration = 0; iteration < kIterationsPerThread; iteration++)
        {
            // Base the clear color on the thread and iteration indexes so every clear color is
            // unique
            const GLColor color(static_cast<GLubyte>(thread % 255),
                                static_cast<GLubyte>(iteration % 255), 0, 255);
            const angle::Vector4 floatColor = color.toNormalizedVector();
            glUniform4fv(colorLocation, 1, floatColor.data());

            for (size_t draw = 0; draw < kDrawsPerIteration; draw++)
            {
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            EXPECT_PIXEL_COLOR_EQ(0, 0, color);
        }
    };
    runMultithreadedGLTest(testBody, 4);
}

// Test that multiple threads can draw and read back pixels correctly.
// Using eglSwapBuffers stresses race conditions around use of QueueSerials.
TEST_P(MultithreadingTest, MultiContextDrawWithSwapBuffers)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());

    // http://anglebug.com/5099
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    EGLWindow *window = getEGLWindow();
    EGLDisplay dpy    = window->getDisplay();

    auto testBody = [dpy](EGLSurface surface, size_t thread) {
        constexpr size_t kIterationsPerThread = 100;
        constexpr size_t kDrawsPerIteration   = 10;

        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        glUseProgram(program);

        GLint colorLocation = glGetUniformLocation(program, essl1_shaders::ColorUniform());

        auto quadVertices = GetQuadVertices();

        GLBuffer vertexBuffer;
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, quadVertices.data(), GL_STATIC_DRAW);

        GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
        glEnableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

        for (size_t iteration = 0; iteration < kIterationsPerThread; iteration++)
        {
            // Base the clear color on the thread and iteration indexes so every clear color is
            // unique
            const GLColor color(static_cast<GLubyte>(thread % 255),
                                static_cast<GLubyte>(iteration % 255), 0, 255);
            const angle::Vector4 floatColor = color.toNormalizedVector();
            glUniform4fv(colorLocation, 1, floatColor.data());

            for (size_t draw = 0; draw < kDrawsPerIteration; draw++)
            {
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            EXPECT_EGL_TRUE(eglSwapBuffers(dpy, surface));
            EXPECT_EGL_SUCCESS();

            EXPECT_PIXEL_COLOR_EQ(0, 0, color);
        }
    };
    runMultithreadedGLTest(testBody, 32);
}

// Test that ANGLE handles multiple threads creating and destroying resources (vertex buffer in this
// case). Disable defer_flush_until_endrenderpass so that glFlush will issue work to GPU in order to
// maximize the chance we resources can be destroyed at the wrong time.
TEST_P(MultithreadingTest, MultiContextCreateAndDeleteResources)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());

    EGLWindow *window = getEGLWindow();
    EGLDisplay dpy    = window->getDisplay();

    auto testBody = [dpy](EGLSurface surface, size_t thread) {
        constexpr size_t kIterationsPerThread = 32;
        constexpr size_t kDrawsPerIteration   = 1;

        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        glUseProgram(program);

        GLint colorLocation = glGetUniformLocation(program, essl1_shaders::ColorUniform());

        auto quadVertices = GetQuadVertices();

        for (size_t iteration = 0; iteration < kIterationsPerThread; iteration++)
        {
            GLBuffer vertexBuffer;
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, quadVertices.data(),
                         GL_STATIC_DRAW);

            GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
            glEnableVertexAttribArray(positionLocation);
            glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

            // Base the clear color on the thread and iteration indexes so every clear color is
            // unique
            const GLColor color(static_cast<GLubyte>(thread % 255),
                                static_cast<GLubyte>(iteration % 255), 0, 255);
            const angle::Vector4 floatColor = color.toNormalizedVector();
            glUniform4fv(colorLocation, 1, floatColor.data());

            for (size_t draw = 0; draw < kDrawsPerIteration; draw++)
            {
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            EXPECT_EGL_TRUE(eglSwapBuffers(dpy, surface));
            EXPECT_EGL_SUCCESS();

            EXPECT_PIXEL_COLOR_EQ(0, 0, color);
        }
        glFinish();
    };
    runMultithreadedGLTest(testBody, 32);
}

TEST_P(MultithreadingTest, MultiCreateContext)
{
    // Supported by CGL, GLX, and WGL (https://anglebug.com/4725)
    // Not supported on Ozone (https://crbug.com/1103009)
    ANGLE_SKIP_TEST_IF(!(IsWindows() || IsLinux() || IsOSX()) || IsOzone());

    EGLWindow *window  = getEGLWindow();
    EGLDisplay dpy     = window->getDisplay();
    EGLContext ctx     = window->getContext();
    EGLSurface surface = window->getSurface();

    // Un-makeCurrent the test window's context
    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    EXPECT_EGL_SUCCESS();

    constexpr size_t kThreadCount = 16;
    std::atomic<uint32_t> barrier(0);
    std::vector<std::thread> threads(kThreadCount);
    std::vector<EGLContext> contexts(kThreadCount);
    for (size_t threadIdx = 0; threadIdx < kThreadCount; threadIdx++)
    {
        threads[threadIdx] = std::thread([&, threadIdx]() {
            contexts[threadIdx] = EGL_NO_CONTEXT;
            {
                contexts[threadIdx] = createMultithreadedContext(window, EGL_NO_CONTEXT);
                EXPECT_NE(EGL_NO_CONTEXT, contexts[threadIdx]);

                barrier++;
            }

            while (barrier < kThreadCount)
            {
            }

            {
                EXPECT_TRUE(eglDestroyContext(dpy, contexts[threadIdx]));
            }
        });
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }

    // Re-make current the test window's context for teardown.
    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));
    EXPECT_EGL_SUCCESS();
}

void MultithreadingTestES3::textureThreadFunction(bool useDraw)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay dpy     = window->getDisplay();
    EGLConfig config   = window->getConfig();
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext ctx     = EGL_NO_CONTEXT;

    // Initialize the pbuffer and context
    EGLint pbufferAttributes[] = {
        EGL_WIDTH, kSize, EGL_HEIGHT, kSize, EGL_NONE, EGL_NONE,
    };
    surface = eglCreatePbufferSurface(dpy, config, pbufferAttributes);
    EXPECT_EGL_SUCCESS();
    EXPECT_NE(EGL_NO_SURFACE, surface);

    ctx = createMultithreadedContext(window, window->getContext());
    EXPECT_NE(EGL_NO_CONTEXT, ctx);

    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));
    EXPECT_EGL_SUCCESS();

    std::vector<GLColor> greenColor(kSize * kSize, GLColor::green);
    std::vector<GLColor> redColor(kSize * kSize, GLColor::red);
    ANGLE_GL_PROGRAM(greenProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    ANGLE_GL_PROGRAM(redProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());

    glBindTexture(GL_TEXTURE_2D, mTexture2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture2D, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    mSecondThreadSyncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    ASSERT_GL_NO_ERROR();
    // Force the fence to be created
    glFlush();

    // Draw something
    while (!mExitThread)
    {
        std::lock_guard<decltype(mutex)> lock(mutex);

        if (mMainThreadSyncObj != nullptr)
        {
            glWaitSync(mMainThreadSyncObj, 0, GL_TIMEOUT_IGNORED);
            ASSERT_GL_NO_ERROR();
            glDeleteSync(mSecondThreadSyncObj);
            ASSERT_GL_NO_ERROR();
            mMainThreadSyncObj = nullptr;
        }
        else
        {
            continue;
        }

        glBindTexture(GL_TEXTURE_2D, mTexture2D);
        ASSERT_GL_NO_ERROR();

        if (mDrawGreen)
        {
            if (useDraw)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                drawQuad(greenProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             greenColor.data());
            }
            ASSERT_GL_NO_ERROR();
        }
        else
        {
            if (useDraw)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                drawQuad(redProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             redColor.data());
            }
            ASSERT_GL_NO_ERROR();
        }

        ASSERT_EQ(mSecondThreadSyncObj.load(), nullptr);
        mSecondThreadSyncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        ASSERT_GL_NO_ERROR();
        // Force the fence to be created
        glFlush();

        mDrawGreen = !mDrawGreen;
    }

    // Clean up
    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    EXPECT_EGL_SUCCESS();

    eglDestroySurface(dpy, surface);
    eglDestroyContext(dpy, ctx);
}

// Test fence sync with multiple threads drawing
void MultithreadingTestES3::mainThreadDraw(bool useDraw)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());

    EGLWindow *window  = getEGLWindow();
    EGLDisplay dpy     = window->getDisplay();
    EGLContext ctx     = window->getContext();
    EGLSurface surface = window->getSurface();
    // Use odd numbers so we bounce between red and green in the final image
    constexpr int kNumIterations = 5;
    constexpr int kNumDraws      = 5;

    mDrawGreen = false;

    std::thread textureThread(&MultithreadingTestES3::textureThreadFunction, this, true);

    ANGLE_GL_PROGRAM(texProgram, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());

    for (int iterations = 0; iterations < kNumIterations; ++iterations)
    {
        for (int draws = 0; draws < kNumDraws;)
        {
            std::lock_guard<decltype(mutex)> lock(mutex);

            if (mSecondThreadSyncObj != nullptr)
            {
                glWaitSync(mSecondThreadSyncObj, 0, GL_TIMEOUT_IGNORED);
                ASSERT_GL_NO_ERROR();
                glDeleteSync(mSecondThreadSyncObj);
                ASSERT_GL_NO_ERROR();
                mSecondThreadSyncObj = nullptr;
            }
            else
            {
                continue;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, mTexture2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glUseProgram(texProgram);
            drawQuad(texProgram.get(), std::string(essl1_shaders::PositionAttrib()), 0.0f);

            ASSERT_EQ(mMainThreadSyncObj.load(), nullptr);
            mMainThreadSyncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            ASSERT_GL_NO_ERROR();
            // Force the fence to be created
            glFlush();

            ++draws;
        }

        ASSERT_GL_NO_ERROR();
        swapBuffers();
    }

    mExitThread = true;
    textureThread.join();

    ASSERT_GL_NO_ERROR();
    GLColor color;
    if (mDrawGreen)
    {
        color = GLColor::green;
    }
    else
    {
        color = GLColor::red;
    }
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, color);

    // Re-make current the test window's context for teardown.
    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, ctx));
    EXPECT_EGL_SUCCESS();
}

// Test that glFenceSync/glWaitSync works correctly with multithreading.
// Main thread: Samples from the shared texture to draw to the default FBO.
// Secondary (Texture) thread: Draws to the shared texture, which the Main thread samples from.
// The overall execution flow is:
// Main Thread:
// 1. Wait for the mSecondThreadSyncObj fence object to be created.
//    - This fence object is used by synchronize access to the shared texture by indicating that the
//    Secondary thread's draws to the texture have all completed and it's now safe to sample from
//    it.
// 2. Once the fence is created, add a glWaitSync(mSecondThreadSyncObj) to the command stream and
//    then delete it.
// 3. Draw, sampling from the shared texture.
// 4. Create a new mMainThreadSyncObj.
//    - This fence object is used to synchronize access to the shared texture by indicating that the
//    Main thread's draws are no longer sampling from the texture, so it's now safe for the
//    Secondary thread to draw to it again with a new color.
// Secondary (Texture) Thread:
// 1. Wait for the mMainThreadSyncObj fence object to be created.
// 2. Once the fence is created, add a glWaitSync(mMainThreadSyncObj) to the command stream and then
//    delete it.
// 3. Draw/Fill the texture.
// 4. Create a new mSecondThreadSyncObj.
//
// These threads loop for the specified number of iterations, drawing/sampling the shared texture
// with the necessary glFlush()s and occasional eglSwapBuffers() to mimic a real multithreaded GLES
// application.
TEST_P(MultithreadingTestES3, MultithreadFenceDraw)
{
    // http://anglebug.com/5418
    ANGLE_SKIP_TEST_IF(IsLinux() && IsVulkan() && (IsIntel() || isSwiftshader()));

    // Have the secondary thread use glDrawArrays()
    mainThreadDraw(true);
}

// Same as MultithreadFenceDraw, but with the secondary thread using glTexImage2D rather than
// glDrawArrays.
TEST_P(MultithreadingTestES3, MultithreadFenceTexImage)
{
    // http://anglebug.com/5418
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsVulkan());

    // http://anglebug.com/5439
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // Have the secondary thread use glTexImage2D()
    mainThreadDraw(false);
}

// Test that waiting on a sync object that hasn't been flushed and without a current context returns
// TIMEOUT_EXPIRED or CONDITION_SATISFIED, but doesn't generate an error or crash.
TEST_P(MultithreadingTest, NoFlushNoContextReturnsTimeout)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());
    ANGLE_SKIP_TEST_IF(!hasFenceSyncExtension() || !hasGLSyncExtension());

    std::mutex mutex;

    EGLWindow *window = getEGLWindow();
    EGLDisplay dpy    = window->getDisplay();

    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    EGLSyncKHR sync = eglCreateSyncKHR(dpy, EGL_SYNC_FENCE_KHR, nullptr);
    EXPECT_NE(sync, EGL_NO_SYNC_KHR);

    std::thread thread = std::thread([&]() {
        std::lock_guard<decltype(mutex)> lock(mutex);
        // Make sure there is no active context on this thread.
        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_EGL_SUCCESS();
        // Don't wait forever to make sure the test terminates
        constexpr GLuint64 kTimeout = 1'000'000'000;  // 1 second
        int result                  = eglClientWaitSyncKHR(dpy, sync, 0, kTimeout);
        // We typically expect to get back TIMEOUT_EXPIRED since the sync object was never flushed.
        // However, the OpenGL ES backend returns CONDITION_SATISFIED, which is also a passing
        // result.
        ASSERT_TRUE(result == EGL_TIMEOUT_EXPIRED_KHR || result == EGL_CONDITION_SATISFIED_KHR);
    });

    thread.join();

    EXPECT_EGL_TRUE(eglDestroySyncKHR(dpy, sync));
}

// Test that waiting on sync object that hasn't been flushed yet, but is later flushed by another
// thread, correctly returns when the fence is signalled without a timeout.
TEST_P(MultithreadingTest, CreateFenceThreadAClientWaitSyncThreadBDelayedFlush)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());
    ANGLE_SKIP_TEST_IF(!hasFenceSyncExtension() || !hasGLSyncExtension());
    // TODO: Fails on Pixel 4 with OpenGLES backend.
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    EGLWindow *window = getEGLWindow();
    EGLDisplay dpy    = window->getDisplay();
    EGLConfig config  = window->getConfig();
    EGLSurface surface;
    EGLContext context;
    constexpr EGLint kPBufferSize = 256;
    // Initialize the pbuffer and context
    EGLint pbufferAttributes[] = {
        EGL_WIDTH, kPBufferSize, EGL_HEIGHT, kPBufferSize, EGL_NONE, EGL_NONE,
    };

    // Create 2 surfaces, one for each thread
    surface = eglCreatePbufferSurface(dpy, config, pbufferAttributes);
    EXPECT_EGL_SUCCESS();
    // Create 2 shared contexts, one for each thread
    context = window->createContext(EGL_NO_CONTEXT, nullptr);
    EXPECT_NE(EGL_NO_CONTEXT, context);
    // Sync object
    EGLSyncKHR sync = EGL_NO_SYNC_KHR;

    // Synchronization tools to ensure the two threads are interleaved as designed by this test.
    std::mutex mutex;
    std::condition_variable condVar;

    enum class Step
    {
        Start,
        Thread0Clear,
        Thread1CreateFence,
        Thread0ClientWaitSync,
        Thread1Flush,
        Finish,
        Abort,
    };
    Step currentStep = Step::Start;

    std::thread thread0 = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Start));

        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

        // Do work.
        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        // Wait for thread 1 to clear.
        threadSynchronization.nextStep(Step::Thread0Clear);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread1CreateFence));

        // Wait on the sync object, but do *not* flush it, since the other thread will flush.
        constexpr GLuint64 kTimeout = 2'000'000'000;  // 1 second
        threadSynchronization.nextStep(Step::Thread0ClientWaitSync);
        ASSERT_EQ(EGL_CONDITION_SATISFIED_KHR, eglClientWaitSyncKHR(dpy, sync, 0, kTimeout));

        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Finish));
    });

    std::thread thread1 = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        // Wait for thread 0 to clear.
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0Clear));

        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface, surface, context));

        // Do work.
        glClearColor(0.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        sync = eglCreateSyncKHR(dpy, EGL_SYNC_FENCE_KHR, nullptr);
        EXPECT_NE(sync, EGL_NO_SYNC_KHR);

        // Wait for the thread 0 to eglClientWaitSyncKHR().
        threadSynchronization.nextStep(Step::Thread1CreateFence);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0ClientWaitSync));

        // Wait a little to give thread 1 time to wait on the sync object before flushing it.
        angle::Sleep(500);
        glFlush();

        // Clean up
        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

        threadSynchronization.nextStep(Step::Finish);
    });

    thread0.join();
    thread1.join();

    // Clean up
    if (surface != EGL_NO_SURFACE)
    {
        eglDestroySurface(dpy, surface);
    }
    if (context != EGL_NO_CONTEXT)
    {
        eglDestroyContext(dpy, context);
    }

    ASSERT_NE(currentStep, Step::Abort);
}

// TODO(geofflang): Test sharing a program between multiple shared contexts on multiple threads

ANGLE_INSTANTIATE_TEST(MultithreadingTest,
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES3_VULKAN(),
                       ES3_VULKAN_SWIFTSHADER(),
                       ES2_D3D11(),
                       ES3_D3D11());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MultithreadingTestES3);
ANGLE_INSTANTIATE_TEST(MultithreadingTestES3,
                       ES3_OPENGL(),
                       ES3_OPENGLES(),
                       ES3_VULKAN(),
                       ES3_VULKAN_SWIFTSHADER(),
                       ES3_D3D11());

}  // namespace angle
