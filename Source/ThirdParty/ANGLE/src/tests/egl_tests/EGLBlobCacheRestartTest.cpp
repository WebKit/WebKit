//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLBlobCacheRestartTest:
//   Tests that what ANGLE stores through EGL_ANDROID_blob_cache serves a later Display, as it
//   serves a later process on Android.  The blob cache is util/test_blob_cache, which keeps its
//   entries on disk as Android's multifile blob cache does.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/test_blob_cache.h"

using namespace angle;

namespace
{
constexpr char kEGLExtName[] = "EGL_ANDROID_blob_cache";

class EGLBlobCacheRestartTest : public ANGLETest<>
{
  protected:
    EGLBlobCacheRestartTest()
    {
        // A new Display for every SetUp(), so that recreateTestFixture() is a restart: nothing
        // ANGLE keeps in memory survives it, only what it stored in the blob cache.
        forceNewDisplay();

        mCacheDir = CreateTestBlobCacheDirectory();
    }

    ~EGLBlobCacheRestartTest() override { RemoveTestBlobCacheDirectory(mCacheDir); }

    void testSetUp() override
    {
        // Every new Display gets the cache, as Android hands its blob cache to every process.
        // Reopening the directory is what a process restart looks like to the cache: it reads back
        // what is on disk.
        EGLDisplay display = getEGLWindow()->getDisplay();
        if (IsEGLDisplayExtensionEnabled(display, kEGLExtName))
        {
            InitTestBlobCache(display, eglSetBlobCacheFuncsANDROID, mCacheDir,
                              TestBlobCacheLimits());
            EXPECT_EGL_SUCCESS();
        }
    }

    std::string mCacheDir;
};

// A program linked before a restart is loaded from the blob cache after it.
TEST_P(EGLBlobCacheRestartTest, ProgramSurvivesRestart)
{
    ANGLE_SKIP_TEST_IF(!IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), kEGLExtName));
    // ANGLE only caches programs when it supports program binaries.
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_get_program_binary"));
    ANGLE_SKIP_TEST_IF(getEGLWindow()->isFeatureEnabled(Feature::DisableProgramCaching));

    {
        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        // Deleting the program makes sure its binary is cached, rather than waiting on tasks that
        // can run past the link.
    }
    EXPECT_GT(GetTestBlobCacheStats().stores, 0u) << "Nothing was stored in the blob cache";

    recreateTestFixture();

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_GT(GetTestBlobCacheStats().hits, 0u)
        << "The restarted Display loaded nothing from the blob cache";
}
}  // anonymous namespace

ANGLE_INSTANTIATE_TEST_ES2(EGLBlobCacheRestartTest);
