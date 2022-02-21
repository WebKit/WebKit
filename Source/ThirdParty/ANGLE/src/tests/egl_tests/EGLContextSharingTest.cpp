//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLContextSharingTest.cpp:
//   Tests relating to shared Contexts.

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"
#include "test_utils/MultiThreadSteps.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"

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

    EGLContext mContexts[2] = {EGL_NO_CONTEXT, EGL_NO_CONTEXT};
    GLuint mTexture;
};

class EGLContextSharingTestNoFixture : public EGLContextSharingTest
{
  public:
    EGLContextSharingTestNoFixture() : EGLContextSharingTest() {}

    void testSetUp() override
    {
        mOsWindow     = OSWindow::New();
        mMajorVersion = GetParam().majorVersion;
    }

    void testTearDown() override
    {
        if (mDisplay != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (mSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(mDisplay, mSurface);
                ASSERT_EGL_SUCCESS();
                mSurface = EGL_NO_SURFACE;
            }

            for (auto &context : mContexts)
            {
                SafeDestroyContext(mDisplay, context);
            }

            eglTerminate(mDisplay);
            mDisplay = EGL_NO_DISPLAY;
            ASSERT_EGL_SUCCESS();
            eglReleaseThread();
            ASSERT_EGL_SUCCESS();
        }

        mOsWindow->destroy();
        OSWindow::Delete(&mOsWindow);
        ASSERT_EGL_SUCCESS() << "Error during test TearDown";
    }

    bool chooseConfig(EGLConfig *config) const
    {
        bool result          = false;
        EGLint count         = 0;
        EGLint clientVersion = mMajorVersion == 3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT;
        EGLint attribs[]     = {EGL_RED_SIZE,
                            8,
                            EGL_GREEN_SIZE,
                            8,
                            EGL_BLUE_SIZE,
                            8,
                            EGL_ALPHA_SIZE,
                            8,
                            EGL_RENDERABLE_TYPE,
                            clientVersion,
                            EGL_SURFACE_TYPE,
                            EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                            EGL_NONE};

        result = eglChooseConfig(mDisplay, attribs, config, 1, &count);
        EXPECT_EGL_TRUE(result && (count > 0));
        return result;
    }

    bool createContext(EGLConfig config, EGLContext *context)
    {
        bool result      = false;
        EGLint attribs[] = {EGL_CONTEXT_MAJOR_VERSION, mMajorVersion, EGL_NONE};

        *context = eglCreateContext(mDisplay, config, nullptr, attribs);
        result   = (*context != EGL_NO_CONTEXT);
        EXPECT_TRUE(result);
        return result;
    }

    bool createWindowSurface(EGLConfig config, EGLNativeWindowType win, EGLSurface *surface)
    {
        bool result      = false;
        EGLint attribs[] = {EGL_NONE};

        *surface = eglCreateWindowSurface(mDisplay, config, win, attribs);
        result   = (*surface != EGL_NO_SURFACE);
        EXPECT_TRUE(result);
        return result;
    }

    bool createPbufferSurface(EGLDisplay dpy,
                              EGLConfig config,
                              EGLint width,
                              EGLint height,
                              EGLSurface *surface)
    {
        bool result      = false;
        EGLint attribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};

        *surface = eglCreatePbufferSurface(dpy, config, attribs);
        result   = (*surface != EGL_NO_SURFACE);
        EXPECT_TRUE(result);
        return result;
    }

    OSWindow *mOsWindow;
    EGLDisplay mDisplay  = EGL_NO_DISPLAY;
    EGLSurface mSurface  = EGL_NO_SURFACE;
    const EGLint kWidth  = 64;
    const EGLint kHeight = 64;
    EGLint mMajorVersion = 0;
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

    // Check whether extension's supported to avoid clearing the EGL error state
    // after failed context creation.
    bool extensionEnabled =
        IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_display_texture_share_group");

    // Test creating two contexts in the global share group
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    mContexts[1] = eglCreateContext(display, config, mContexts[1], inShareGroupContextAttribs);

    if (!extensionEnabled)
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
    mContexts[0] = EGL_NO_CONTEXT;

    // Try creating a context that is not in the global share group but tries to share with a
    // context that is
    const EGLint notInShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_FALSE, EGL_NONE};
    mContexts[0] = eglCreateContext(display, config, mContexts[1], notInShareGroupContextAttribs);
    ASSERT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
    ASSERT_TRUE(mContexts[0] == EGL_NO_CONTEXT);
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
    GLTexture textureFromCtx0;
    glBindTexture(GL_TEXTURE_2D, textureFromCtx0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    GLBuffer bufferFromCtx0;
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
    ASSERT_GL_NO_ERROR();

    // Call readpixels on the texture to verify that the backend has proper support
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureFromCtx0, 0);

    GLubyte pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    ASSERT_GL_NO_ERROR();

    // Switch back to context 0 and delete the buffer
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    ASSERT_EGL_SUCCESS();
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
    GLTexture textureFromCtx0;
    glBindTexture(GL_TEXTURE_2D, textureFromCtx0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    // Switch to context 1 and verify that the texture is accessible
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[1]));
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    // Destroy both contexts, the texture should be cleaned up automatically
    ASSERT_EGL_TRUE(eglDestroyContext(display, mContexts[0]));
    mContexts[0] = EGL_NO_CONTEXT;
    ASSERT_EGL_TRUE(eglDestroyContext(display, mContexts[1]));
    mContexts[1] = EGL_NO_CONTEXT;

    // Unmake current, so the context can be released.
    ASSERT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

    // Create a new context and verify it cannot access the texture previously created
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));

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
    // GL Fences require GLES 3.0+
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

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

        ctx[t] = window->createContext(t == 0 ? EGL_NO_CONTEXT : ctx[0], nullptr);
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
    std::atomic<GLsync> deletingThreadSyncObj;
    std::atomic<GLsync> continuingThreadSyncObj;

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

    std::thread deletingThread = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface[0], surface[0], ctx[0]));
        EXPECT_EGL_SUCCESS();

        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Start));

        // Draw using the shared texture.
        drawQuad(program[0].get(), essl1_shaders::PositionAttrib(), 0.5f);

        deletingThreadSyncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        ASSERT_GL_NO_ERROR();
        // Force the fence to be created
        glFlush();

        // Wait for the other thread to also draw using the shared texture.
        threadSynchronization.nextStep(Step::Thread0Draw);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread1Draw));

        ASSERT_TRUE(continuingThreadSyncObj != nullptr);
        glWaitSync(continuingThreadSyncObj, 0, GL_TIMEOUT_IGNORED);
        ASSERT_GL_NO_ERROR();
        glDeleteSync(continuingThreadSyncObj);
        ASSERT_GL_NO_ERROR();
        continuingThreadSyncObj = nullptr;

        // Delete this thread's framebuffer (reader of the shared texture).
        fbo[0].reset();

        // Wait for the other thread to use the shared texture again before unbinding the
        // context (so no implicit flush happens).
        threadSynchronization.nextStep(Step::Thread0Delete);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Finish));

        EXPECT_GL_NO_ERROR();
        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_EGL_SUCCESS();
    });

    std::thread continuingThread = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        EXPECT_EGL_TRUE(eglMakeCurrent(dpy, surface[1], surface[1], ctx[1]));
        EXPECT_EGL_SUCCESS();

        // Wait for first thread to draw using the shared texture.
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0Draw));

        ASSERT_TRUE(deletingThreadSyncObj != nullptr);
        glWaitSync(deletingThreadSyncObj, 0, GL_TIMEOUT_IGNORED);
        ASSERT_GL_NO_ERROR();
        glDeleteSync(deletingThreadSyncObj);
        ASSERT_GL_NO_ERROR();
        deletingThreadSyncObj = nullptr;

        // Draw using the shared texture.
        drawQuad(program[0].get(), essl1_shaders::PositionAttrib(), 0.5f);

        continuingThreadSyncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        ASSERT_GL_NO_ERROR();
        // Force the fence to be created
        glFlush();

        // Wait for the other thread to delete its framebuffer.
        threadSynchronization.nextStep(Step::Thread1Draw);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0Delete));

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

        threadSynchronization.nextStep(Step::Finish);

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

// Test that eglTerminate() with a thread doesn't cause other threads to crash.
TEST_P(EGLContextSharingTestNoFixture, EglTerminateMultiThreaded)
{
    // http://anglebug.com/6208
    // The following EGL calls led to a crash in eglMakeCurrent():
    //
    // Thread A: eglMakeCurrent(context A)
    // Thread B: eglDestroyContext(context A)
    //        B: eglTerminate() <<--- this release context A
    // Thread A: eglMakeCurrent(context B)

    EGLint dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
    mDisplay           = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                        reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
    EXPECT_TRUE(mDisplay != EGL_NO_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));

    EGLConfig config = EGL_NO_CONFIG_KHR;
    EXPECT_TRUE(chooseConfig(&config));

    mOsWindow->initialize("EGLContextSharingTestNoFixture", kWidth, kHeight);
    EXPECT_TRUE(createWindowSurface(config, mOsWindow->getNativeWindow(), &mSurface));
    ASSERT_EGL_SUCCESS() << "eglCreateWindowSurface failed.";

    EXPECT_TRUE(createContext(config, &mContexts[0]));
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[0]));

    // Must be after the eglMakeCurrent() so renderer string is initialized.
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());
    // TODO(http://www.anglebug.com/6304): Fails with OpenGL ES backend.
    ANGLE_SKIP_TEST_IF(IsOpenGLES());

    EXPECT_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

    // Synchronization tools to ensure the two threads are interleaved as designed by this test.
    std::mutex mutex;
    std::condition_variable condVar;

    enum class Step
    {
        Start,
        Thread0Clear,
        Thread1Terminate,
        Thread0MakeCurrentContext1,
        Finish,
        Abort,
    };
    Step currentStep = Step::Start;

    std::thread thread0 = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Start));

        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[0]));

        // Clear and read back to make sure thread 0 uses context 0.
        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

        // Wait for thread 1 to clear.
        threadSynchronization.nextStep(Step::Thread0Clear);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread1Terminate));

        // First Display was terminated, so we need to create a new one to create a new Context.
        mDisplay = eglGetPlatformDisplayEXT(
            EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        EXPECT_TRUE(mDisplay != EGL_NO_DISPLAY);
        EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));
        config = EGL_NO_CONFIG_KHR;
        EXPECT_TRUE(chooseConfig(&config));
        EXPECT_TRUE(createContext(config, &mContexts[1]));

        // Thread1's terminate call will make mSurface an invalid handle, recreate a new surface
        EXPECT_TRUE(createPbufferSurface(mDisplay, config, 1280, 720, &mSurface));
        ASSERT_EGL_SUCCESS() << "eglCreatePbufferSurface failed.";

        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[1]));

        // Clear and read back to make sure thread 0 uses context 1.
        glClearColor(1.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(0, 0, 255, 255, 0, 255);

        // Cleanup
        EXPECT_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_TRUE(SafeDestroyContext(mDisplay, mContexts[1]));
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
        EXPECT_EGL_SUCCESS();

        threadSynchronization.nextStep(Step::Thread0MakeCurrentContext1);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Finish));
    });

    std::thread thread1 = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        // Wait for thread 0 to clear.
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0Clear));

        // Destroy context 0 while thread1 has it current.
        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_TRUE(SafeDestroyContext(mDisplay, mContexts[0]));
        EXPECT_EGL_SUCCESS();
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
        EXPECT_EGL_SUCCESS();

        // Wait for the thread 0 to make a new context and glClear().
        threadSynchronization.nextStep(Step::Thread1Terminate);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0MakeCurrentContext1));

        threadSynchronization.nextStep(Step::Finish);
    });

    thread0.join();
    thread1.join();

    ASSERT_NE(currentStep, Step::Abort);
}

// Test that eglDestoryContext() can be called multiple times on the same Context without causing
// errors.
TEST_P(EGLContextSharingTestNoFixture, EglDestoryContextManyTimesSameContext)
{
    EGLint dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
    mDisplay           = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                        reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
    EXPECT_TRUE(mDisplay != EGL_NO_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));

    EGLConfig config = EGL_NO_CONFIG_KHR;
    EXPECT_TRUE(chooseConfig(&config));

    mOsWindow->initialize("EGLContextSharingTestNoFixture", kWidth, kHeight);
    EXPECT_TRUE(createWindowSurface(config, mOsWindow->getNativeWindow(), &mSurface));
    ASSERT_EGL_SUCCESS() << "eglCreateWindowSurface failed.";

    EXPECT_TRUE(createContext(config, &mContexts[0]));
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[0]));

    // Must be after the eglMakeCurrent() so renderer string is initialized.
    ANGLE_SKIP_TEST_IF(!platformSupportsMultithreading());
    // TODO(http://www.anglebug.com/6304): Fails with OpenGL ES backend.
    ANGLE_SKIP_TEST_IF(IsOpenGLES());

    EXPECT_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

    // Synchronization tools to ensure the two threads are interleaved as designed by this test.
    std::mutex mutex;
    std::condition_variable condVar;

    enum class Step
    {
        Start,
        Thread0Clear,
        Thread1Terminate,
        Thread0MakeCurrentContext1,
        Finish,
        Abort,
    };
    Step currentStep = Step::Start;

    std::thread thread0 = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Start));

        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[0]));

        // Clear and read back to make sure thread 0 uses context 0.
        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

        // Wait for thread 1 to clear.
        threadSynchronization.nextStep(Step::Thread0Clear);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread1Terminate));

        // First Display was terminated, so we need to create a new one to create a new Context.
        mDisplay = eglGetPlatformDisplayEXT(
            EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        EXPECT_TRUE(mDisplay != EGL_NO_DISPLAY);
        EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));
        config = EGL_NO_CONFIG_KHR;
        EXPECT_TRUE(chooseConfig(&config));
        EXPECT_TRUE(createContext(config, &mContexts[1]));

        // Thread1's terminate call will make mSurface an invalid handle, recreate a new surface
        EXPECT_TRUE(createPbufferSurface(mDisplay, config, 1280, 720, &mSurface));
        ASSERT_EGL_SUCCESS() << "eglCreatePbufferSurface failed.";

        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[1]));
        EXPECT_EGL_SUCCESS();

        // Clear and read back to make sure thread 0 uses context 1.
        glClearColor(1.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(0, 0, 255, 255, 0, 255);

        // Cleanup
        EXPECT_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        EXPECT_TRUE(SafeDestroyContext(mDisplay, mContexts[1]));
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
        EXPECT_EGL_SUCCESS();

        threadSynchronization.nextStep(Step::Thread0MakeCurrentContext1);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Finish));
    });

    std::thread thread1 = std::thread([&]() {
        ThreadSynchronization<Step> threadSynchronization(&currentStep, &mutex, &condVar);

        // Wait for thread 0 to clear.
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0Clear));

        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

        // Destroy context 0 5 times while thread1 has it current.
        EXPECT_EGL_TRUE(eglDestroyContext(mDisplay, mContexts[0]));
        EXPECT_EGL_TRUE(eglDestroyContext(mDisplay, mContexts[0]));
        EXPECT_EGL_TRUE(eglDestroyContext(mDisplay, mContexts[0]));
        EXPECT_EGL_TRUE(eglDestroyContext(mDisplay, mContexts[0]));
        EXPECT_EGL_TRUE(eglDestroyContext(mDisplay, mContexts[0]));
        mContexts[0] = EGL_NO_CONTEXT;

        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
        EXPECT_EGL_SUCCESS();

        // Wait for the thread 0 to make a new context and glClear().
        threadSynchronization.nextStep(Step::Thread1Terminate);
        ASSERT_TRUE(threadSynchronization.waitForStep(Step::Thread0MakeCurrentContext1));

        threadSynchronization.nextStep(Step::Finish);
    });

    thread0.join();
    thread1.join();

    ASSERT_NE(currentStep, Step::Abort);
}

// Test that eglTerminate() can be called multiple times on the same Display while Contexts are
// still current without causing errors.
TEST_P(EGLContextSharingTestNoFixture, EglTerminateMultipleTimes)
{
    // https://bugs.chromium.org/p/skia/issues/detail?id=12413#c4
    // The following sequence caused a crash with the D3D backend in the Skia infra:
    //   eglDestroyContext(ctx0)
    //   eglDestroySurface(srf0)
    //   eglTerminate(shared-display)
    //   eglDestroyContext(ctx1) // completes the cleanup from the above terminate
    //   eglDestroySurface(srf1)
    //   eglTerminate(shared-display)

    EGLint dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
    mDisplay           = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                        reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
    EXPECT_TRUE(mDisplay != EGL_NO_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));

    EGLConfig config = EGL_NO_CONFIG_KHR;
    EXPECT_TRUE(chooseConfig(&config));

    mOsWindow->initialize("EGLContextSharingTestNoFixture", kWidth, kHeight);
    EXPECT_TRUE(createWindowSurface(config, mOsWindow->getNativeWindow(), &mSurface));
    EXPECT_TRUE(mSurface != EGL_NO_SURFACE);
    ASSERT_EGL_SUCCESS() << "eglCreateWindowSurface failed.";

    EXPECT_TRUE(createContext(config, &mContexts[0]));
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[0]));
    EXPECT_TRUE(createContext(config, &mContexts[1]));
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mSurface, mSurface, mContexts[1]));

    // Must be after the eglMakeCurrent() so renderer string is initialized.
    // TODO(http://www.anglebug.com/6304): Fails with Mac + OpenGL backend.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL());

    EXPECT_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

    eglDestroySurface(mDisplay, mSurface);
    mSurface = EGL_NO_SURFACE;
    EXPECT_EGL_TRUE(eglDestroyContext(mDisplay, mContexts[0]));
    mContexts[0] = EGL_NO_CONTEXT;
    eglTerminate(mDisplay);
    EXPECT_EGL_SUCCESS();

    eglDestroyContext(mDisplay, mContexts[1]);
    mContexts[1] = EGL_NO_CONTEXT;
    ASSERT_EGL_ERROR(EGL_NOT_INITIALIZED);
    eglTerminate(mDisplay);
    EXPECT_EGL_SUCCESS();
    mDisplay = EGL_NO_DISPLAY;
}
}  // anonymous namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLContextSharingTest);
ANGLE_INSTANTIATE_TEST(EGLContextSharingTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_METAL(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_VULKAN(),
                       ES3_VULKAN());

ANGLE_INSTANTIATE_TEST(EGLContextSharingTestNoFixture,
                       WithNoFixture(ES2_OPENGLES()),
                       WithNoFixture(ES3_OPENGLES()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));
