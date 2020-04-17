//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLContextSharingTest.cpp:
//   Tests relating to shared Contexts.

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

#include <condition_variable>
#include <mutex>
#include <thread>

using namespace angle;

namespace
{

EGLBoolean SafeDestroyContext(EGLDisplay display, EGLContext &context)
{
    EGLBoolean result = EGL_TRUE;
    if (context != EGL_NO_CONTEXT)
    {
        result  = eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    return result;
}

class EGLContextSharingTest : public ANGLETest
{
  public:
    EGLContextSharingTest() : mContexts{EGL_NO_CONTEXT, EGL_NO_CONTEXT}, mTexture(0) {}

    void testTearDown() override
    {
        glDeleteTextures(1, &mTexture);

        EGLDisplay display = getEGLWindow()->getDisplay();

        if (display != EGL_NO_DISPLAY)
        {
            for (auto &context : mContexts)
            {
                SafeDestroyContext(display, context);
            }
        }

        // Set default test state to not give an error on shutdown.
        getEGLWindow()->makeCurrent();
    }

    EGLContext mContexts[2];
    GLuint mTexture;
};

// Tests that creating resources works after freeing the share context.
TEST_P(EGLContextSharingTest, BindTextureAfterShareContextFree)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLConfig config   = getEGLWindow()->getConfig();
    EGLSurface surface = getEGLWindow()->getSurface();

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION,
                                     getEGLWindow()->getClientMajorVersion(), EGL_NONE};

    mContexts[0] = eglCreateContext(display, config, nullptr, contextAttribs);
    ASSERT_EGL_SUCCESS();
    ASSERT_TRUE(mContexts[0] != EGL_NO_CONTEXT);
    mContexts[1] = eglCreateContext(display, config, mContexts[1], contextAttribs);
    ASSERT_EGL_SUCCESS();
    ASSERT_TRUE(mContexts[1] != EGL_NO_CONTEXT);

    ASSERT_EGL_TRUE(SafeDestroyContext(display, mContexts[0]));
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[1]));
    ASSERT_EGL_SUCCESS();

    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    ASSERT_GL_NO_ERROR();
}

// Tests the creation of contexts using EGL_ANGLE_display_texture_share_group
TEST_P(EGLContextSharingTest, DisplayShareGroupContextCreation)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLConfig config   = getEGLWindow()->getConfig();

    const EGLint inShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_TRUE, EGL_NONE};

    // Test creating two contexts in the global share group
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    mContexts[1] = eglCreateContext(display, config, mContexts[1], inShareGroupContextAttribs);

    if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_display_texture_share_group"))
    {
        // Make sure an error is generated and early-exit
        ASSERT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
        ASSERT_EQ(EGL_NO_CONTEXT, mContexts[0]);
        return;
    }

    ASSERT_EGL_SUCCESS();

    ASSERT_NE(EGL_NO_CONTEXT, mContexts[0]);
    ASSERT_NE(EGL_NO_CONTEXT, mContexts[1]);
    eglDestroyContext(display, mContexts[0]);

    // Try creating a context that is not in the global share group but tries to share with a
    // context that is
    const EGLint notInShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_FALSE, EGL_NONE};
    mContexts[1] = eglCreateContext(display, config, mContexts[1], notInShareGroupContextAttribs);
    ASSERT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
    ASSERT_TRUE(mContexts[1] == EGL_NO_CONTEXT);
}

// Tests the sharing of textures using EGL_ANGLE_display_texture_share_group
TEST_P(EGLContextSharingTest, DisplayShareGroupObjectSharing)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_display_texture_share_group"))
    {
        std::cout << "Test skipped because EGL_ANGLE_display_texture_share_group is not present."
                  << std::endl;
        return;
    }

    EGLConfig config   = getEGLWindow()->getConfig();
    EGLSurface surface = getEGLWindow()->getSurface();

    const EGLint inShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_TRUE, EGL_NONE};

    // Create two contexts in the global share group but not in the same context share group
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    mContexts[1] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);

    ASSERT_EGL_SUCCESS();

    ASSERT_NE(EGL_NO_CONTEXT, mContexts[0]);
    ASSERT_NE(EGL_NO_CONTEXT, mContexts[1]);

    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    ASSERT_EGL_SUCCESS();

    // Create a texture and buffer in ctx 0
    GLuint textureFromCtx0 = 0;
    glGenTextures(1, &textureFromCtx0);
    glBindTexture(GL_TEXTURE_2D, textureFromCtx0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    GLuint bufferFromCtx0 = 0;
    glGenBuffers(1, &bufferFromCtx0);
    glBindBuffer(GL_ARRAY_BUFFER, bufferFromCtx0);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    ASSERT_GL_TRUE(glIsBuffer(bufferFromCtx0));

    ASSERT_GL_NO_ERROR();

    // Switch to context 1 and verify that the texture is accessible but the buffer is not
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[1]));
    ASSERT_EGL_SUCCESS();

    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    ASSERT_GL_FALSE(glIsBuffer(bufferFromCtx0));
    glDeleteBuffers(1, &bufferFromCtx0);
    ASSERT_GL_NO_ERROR();

    // Call readpixels on the texture to verify that the backend has proper support
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureFromCtx0, 0);

    GLubyte pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    ASSERT_GL_NO_ERROR();

    glDeleteFramebuffers(1, &fbo);

    glDeleteTextures(1, &textureFromCtx0);
    ASSERT_GL_NO_ERROR();
    ASSERT_GL_FALSE(glIsTexture(textureFromCtx0));

    // Switch back to context 0 and delete the buffer
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    ASSERT_EGL_SUCCESS();

    ASSERT_GL_TRUE(glIsBuffer(bufferFromCtx0));
    glDeleteBuffers(1, &bufferFromCtx0);
    ASSERT_GL_NO_ERROR();
}

// Tests that shared textures using EGL_ANGLE_display_texture_share_group are released when the last
// context is destroyed
TEST_P(EGLContextSharingTest, DisplayShareGroupReleasedWithLastContext)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_display_texture_share_group"))
    {
        std::cout << "Test skipped because EGL_ANGLE_display_texture_share_group is not present."
                  << std::endl;
        return;
    }

    EGLConfig config   = getEGLWindow()->getConfig();
    EGLSurface surface = getEGLWindow()->getSurface();

    const EGLint inShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_TRUE, EGL_NONE};

    // Create two contexts in the global share group but not in the same context share group
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    mContexts[1] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);

    // Create a texture and buffer in ctx 0
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    GLuint textureFromCtx0 = 0;
    glGenTextures(1, &textureFromCtx0);
    glBindTexture(GL_TEXTURE_2D, textureFromCtx0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    // Switch to context 1 and verify that the texture is accessible
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[1]));
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    // Destroy both contexts, the texture should be cleaned up automatically
    ASSERT_EGL_TRUE(eglDestroyContext(display, mContexts[0]));
    ASSERT_EGL_TRUE(eglDestroyContext(display, mContexts[1]));

    // Create a new context and verify it cannot access the texture previously created
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);

    ASSERT_GL_FALSE(glIsTexture(textureFromCtx0));
}

// Tests that deleting an object on one Context doesn't destroy it ahead-of-time. Mostly focused
// on the Vulkan back-end where we manage object lifetime manually.
TEST_P(EGLContextSharingTest, TextureLifetime)
{
    EGLWindow *eglWindow = getEGLWindow();
    EGLConfig config     = getEGLWindow()->getConfig();
    EGLDisplay display   = getEGLWindow()->getDisplay();

    // Create a pbuffer surface for use with a shared context.
    EGLSurface surface     = eglWindow->getSurface();
    EGLContext mainContext = eglWindow->getContext();

    // Initialize a shared context.
    mContexts[0] = eglCreateContext(display, config, mainContext, nullptr);
    ASSERT_NE(mContexts[0], EGL_NO_CONTEXT);

    // Create a Texture on the shared context.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));

    constexpr GLsizei kTexSize                  = 2;
    const GLColor kTexData[kTexSize * kTexSize] = {GLColor::red, GLColor::green, GLColor::blue,
                                                   GLColor::yellow};
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kTexData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Make the main Context current and draw with the texture.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mainContext));

    glBindTexture(GL_TEXTURE_2D, tex);
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
    glUseProgram(program);

    // No uniform update because the update seems to hide the error on Vulkan.

    // Enqueue the draw call.
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();

    // Delete the texture in the main context to orphan it.
    // Do not read back the data to keep the commands in the graph.
    tex.reset();

    // Bind and delete the test context. This should trigger texture garbage collection.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    SafeDestroyContext(display, mContexts[0]);

    // Bind the main context to clean up the test.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mainContext));
}

// Tests that deleting an object on one Context doesn't destroy it ahead-of-time. Mostly focused
// on the Vulkan back-end where we manage object lifetime manually.
TEST_P(EGLContextSharingTest, SamplerLifetime)
{
    EGLWindow *eglWindow = getEGLWindow();
    EGLConfig config     = getEGLWindow()->getConfig();
    EGLDisplay display   = getEGLWindow()->getDisplay();

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!IsEGLDisplayExtensionEnabled(display, "EGL_KHR_create_context"));

    // Create a pbuffer surface for use with a shared context.
    EGLSurface surface     = eglWindow->getSurface();
    EGLContext mainContext = eglWindow->getContext();

    std::vector<EGLint> contextAttributes;
    contextAttributes.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
    contextAttributes.push_back(getClientMajorVersion());
    contextAttributes.push_back(EGL_NONE);

    // Initialize a shared context.
    mContexts[0] = eglCreateContext(display, config, mainContext, contextAttributes.data());
    ASSERT_NE(mContexts[0], EGL_NO_CONTEXT);

    // Create a Texture on the shared context. Also create a Sampler object.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));

    constexpr GLsizei kTexSize                  = 2;
    const GLColor kTexData[kTexSize * kTexSize] = {GLColor::red, GLColor::green, GLColor::blue,
                                                   GLColor::yellow};
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kTexData);

    GLSampler sampler;
    glBindSampler(0, sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Make the main Context current and draw with the texture and sampler.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mainContext));

    glBindTexture(GL_TEXTURE_2D, tex);
    glBindSampler(0, sampler);
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
    glUseProgram(program);

    // No uniform update because the update seems to hide the error on Vulkan.

    // Enqueue the draw call.
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();

    // Delete the texture and sampler in the main context to orphan them.
    // Do not read back the data to keep the commands in the graph.
    tex.reset();
    sampler.reset();

    // Bind and delete the test context. This should trigger texture garbage collection.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    SafeDestroyContext(display, mContexts[0]);

    // Bind the main context to clean up the test.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mainContext));
}

// Test that deleting an object reading from a shared object in one context doesn't cause the other
// context to crash.  Mostly focused on the Vulkan back-end where we track resource dependencies in
// a graph.
TEST_P(EGLContextSharingTest, DeleteReaderOfSharedTexture)
{
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());

    // Bug in the Vulkan backend. http://anglebug.com/4130
    ANGLE_SKIP_TEST_IF(IsVulkan());

    // Initialize contexts
    EGLWindow *window = getEGLWindow();
    EGLDisplay dpy    = window->getDisplay();
    EGLConfig config  = window->getConfig();

    constexpr size_t kThreadCount    = 2;
    EGLSurface surface[kThreadCount] = {EGL_NO_SURFACE, EGL_NO_SURFACE};
    EGLContext ctx[kThreadCount]     = {EGL_NO_CONTEXT, EGL_NO_CONTEXT};

    EGLint pbufferAttributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE, EGL_NONE};

    for (size_t t = 0; t < kThreadCount; ++t)
    {
        surface[t] = eglCreatePbufferSurface(dpy, config, pbufferAttributes);
        EXPECT_EGL_SUCCESS();

        ctx[t] = window->createContext(t == 0 ? EGL_NO_CONTEXT : ctx[0]);
        EXPECT_NE(EGL_NO_CONTEXT, ctx[t]);
    }

    // Initialize test resources.  They are done outside the threads to reduce the sources of
    // errors and thus deadlock-free teardown.
    ASSERT_EGL_TRUE(eglMakeCurrent(dpy, surface[1], surface[1], ctx[1]));

    // Shared texture to read from.
    constexpr GLsizei kTexSize = 1;
    const GLColor kTexData     = GLColor::red;

    GLTexture sharedTex;
    glBindTexture(GL_TEXTURE_2D, sharedTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 &kTexData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Resources for each context.
    GLRenderbuffer renderbuffer[kThreadCount];
    GLFramebuffer fbo[kThreadCount];
    GLProgram program[kThreadCount];

    for (size_t t = 0; t < kThreadCount; ++t)
    {
        ASSERT_EGL_TRUE(eglMakeCurrent(dpy, surface[t], surface[t], ctx[t]));

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer[t]);
        constexpr int kRenderbufferSize = 4;
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kRenderbufferSize, kRenderbufferSize);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[t]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  renderbuffer[t]);

        glBindTexture(GL_TEXTURE_2D, sharedTex);
        program[t].makeRaster(essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
        ASSERT_TRUE(program[t].valid());
    }

    EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

    // Synchronization tools to ensure the two threads are interleaved as designed by this test.
    std::mutex mutex;
    std::condition_variable condVar;

    enum class Step
    {
        Start,
        Thread0Draw,
        Thread1Draw,
        Thread0Delete,
        Finish,
        Abort,
    };
    Step currentStep = Step::Start;

    // Helper functions to synchronize the threads so that the operations are executed in the
    // specific order the test is written for.
    auto waitForStep = [&](Step waitStep) -> bool {
        std::unique_lock<std::mutex> lock(mutex);
        while (currentStep != waitStep)
        {
            // If necessary, abort execution as the other thread has encountered a GL error.
            if (currentStep == Step::Abort)
            {
                return false;
            }
            condVar.wait(lock);
        }

        return true;
    };
    auto nextStep = [&](Step newStep) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            currentStep = newStep;
        }
        condVar.notify_one();
    };

    class AbortOnFailure
    {
      public:
        AbortOnFailure(Step *currentStep, std::mutex *mutex, std::condition_variable *condVar)
            : mCurrentStep(currentStep), mMutex(mutex), mCondVar(condVar)
        {}

        ~AbortOnFailure()
        {
            bool isAborting = false;
            {
                std::unique_lock<std::mutex> lock(*mMutex);
                isAborting = *mCurrentStep != Step::Finish;

                if (isAborting)
                {
                    *mCurrentStep = Step::Abort;
                }
            }
            mCondVar->notify_all();
        }

      private:
        Step *mCurrentStep;
        std::mutex *mMutex;
        std::condition_variable *mCondVar;
    };

    std::thread deletingThread = std::thread([&]() {
        AbortOnFailure abortOnFailure(&currentStep, &mutex, &condVar);

        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface[0], surface[0], ctx[0]));
        EXPECT_EGL_SUCCESS();

        ASSERT_TRUE(waitForStep(Step::Start));

        // Draw using the shared texture.
        drawQuad(program[0].get(), essl1_shaders::PositionAttrib(), 0.5f);

        // Wait for the other thread to also draw using the shared texture.
        nextStep(Step::Thread0Draw);
        ASSERT_TRUE(waitForStep(Step::Thread1Draw));

        // Delete this thread's framebuffer (reader of the shared texture).
        fbo[0].reset();

        // Flush to make sure the graph nodes associated with this context are deleted.
        glFlush();

        // Wait for the other thread to use the shared texture again before unbinding the
        // context (so no implicit flush happens).
        nextStep(Step::Thread0Delete);
        ASSERT_TRUE(waitForStep(Step::Finish));

        EXPECT_GL_NO_ERROR();
        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_EGL_SUCCESS();
    });

    std::thread continuingThread = std::thread([&]() {
        AbortOnFailure abortOnFailure(&currentStep, &mutex, &condVar);

        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface[1], surface[1], ctx[1]));
        EXPECT_EGL_SUCCESS();

        // Wait for first thread to draw using the shared texture.
        ASSERT_TRUE(waitForStep(Step::Thread0Draw));

        // Draw using the shared texture.
        drawQuad(program[0].get(), essl1_shaders::PositionAttrib(), 0.5f);

        // Wait for the other thread to delete its framebuffer.
        nextStep(Step::Thread1Draw);
        ASSERT_TRUE(waitForStep(Step::Thread0Delete));

        // Write to the shared texture differently, so a dependency is created from the previous
        // readers of the shared texture (the two framebuffers of the two threads) to the new
        // commands being recorded for the shared texture.
        //
        // If the backend attempts to create a dependency from nodes associated with the
        // previous readers of the texture to the new node that will contain the following
        // commands, there will be a use-after-free error.
        const GLColor kTexData2 = GLColor::green;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTexSize, kTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     &kTexData2);
        drawQuad(program[0].get(), essl1_shaders::PositionAttrib(), 0.5f);

        nextStep(Step::Finish);

        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_EGL_SUCCESS();
    });

    deletingThread.join();
    continuingThread.join();

    ASSERT_NE(currentStep, Step::Abort);

    // Clean up
    for (size_t t = 0; t < kThreadCount; ++t)
    {
        eglDestroySurface(dpy, surface[t]);
        eglDestroyContext(dpy, ctx[t]);
    }
}
}  // anonymous namespace

ANGLE_INSTANTIATE_TEST(EGLContextSharingTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_VULKAN(),
                       ES3_VULKAN());
