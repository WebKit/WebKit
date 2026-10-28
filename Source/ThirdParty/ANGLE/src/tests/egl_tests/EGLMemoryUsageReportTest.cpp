//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLMemoryUsageReportTest:
//   Tests pertaining to EGL_ANGLE_memory_usage_report extension.
//

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

using namespace angle;

class EGLMemoryUsageReportTest : public ANGLETest<>
{
  protected:
    bool hasEGLDisplayExtension(const char *extname) const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), extname);
    }

    bool hasMemoryUsageReportExtension() const
    {
        return hasEGLDisplayExtension("EGL_ANGLE_memory_usage_report");
    }

    uint64_t getMemoryUsage(EGLDisplay display, EGLContext context)
    {
        GLint parts[2];
        EXPECT_EGL_TRUE(eglQueryContext(display, context, EGL_CONTEXT_MEMORY_USAGE_ANGLE, parts));

        return (static_cast<uint64_t>(parts[0]) & 0xffffffff) |
               ((static_cast<uint64_t>(parts[1]) & 0xffffffff) << 32);
    }

    // Enough object names that the latest ones are stored in the hash map of the resource map
    // instead of its flat arrays.  Buffer names are in flat arrays below 0x7800 when the share
    // context lock is enabled, and other names, or all names without the lock, below 0x3000.
    static constexpr GLsizei kHashedObjectNameCount = 0x7800 + 16;
};

// Basic memory usage queries
TEST_P(EGLMemoryUsageReportTest, BasicQuery)
{
    ANGLE_SKIP_TEST_IF(!hasMemoryUsageReportExtension());

    constexpr GLint kTextureDim              = 1024;
    constexpr GLuint kTextureSize            = kTextureDim * kTextureDim * 4;
    constexpr GLuint kBufferSize             = 4096;
    constexpr GLint kRenderbufferDim         = 512;
    constexpr GLuint kRenderbufferSize       = kRenderbufferDim * kRenderbufferDim * 4;
    constexpr GLuint kTotalObjectsMemorySize = kBufferSize + kRenderbufferSize + kTextureSize;

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLContext context = getEGLWindow()->getContext();

    uint64_t memorySize1 = getMemoryUsage(display, context);
    uint64_t memorySize2;

    {
        GLBuffer buffer;
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, kBufferSize, nullptr, GL_STATIC_DRAW);

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kTextureDim, kTextureDim);

        GLRenderbuffer rbo;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kRenderbufferDim, kRenderbufferDim);

        memorySize2 = getMemoryUsage(display, context);

        EXPECT_EQ(memorySize2 - memorySize1, kTotalObjectsMemorySize);
    }

    uint64_t memorySize3 = getMemoryUsage(display, context);
    EXPECT_EQ(memorySize2 - memorySize3, kTotalObjectsMemorySize);
}

// Test that querying memory usage of a context that is not current works.
TEST_P(EGLMemoryUsageReportTest, QueryNonCurrentContext)
{
    ANGLE_SKIP_TEST_IF(!hasMemoryUsageReportExtension());

    constexpr GLint kTextureDim   = 1024;
    constexpr GLuint kTextureSize = kTextureDim * kTextureDim * 4;

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    // Create a 2nd context.
    EGLContext context2 = window->createContext(EGL_NO_CONTEXT, nullptr);

    uint64_t context2MemorySize1 = getMemoryUsage(display, context2);

    {
        EXPECT_EGL_TRUE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context2));
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kTextureDim, kTextureDim);

        // Make default context current.
        EXPECT_TRUE(window->makeCurrent());
        EXPECT_NE(eglGetCurrentContext(), context2);

        // Query 2nd context's memory size
        uint64_t context2MemorySize2 = getMemoryUsage(display, context2);

        EXPECT_EQ(context2MemorySize2 - context2MemorySize1, kTextureSize);
    }

    EXPECT_EGL_TRUE(eglDestroyContext(display, context2));
}

// Test that memory usage can be queried while many generated buffer names are not bound.
TEST_P(EGLMemoryUsageReportTest, QueryWithManyUnboundBufferNames)
{
    ANGLE_SKIP_TEST_IF(!hasMemoryUsageReportExtension());

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLContext context = getEGLWindow()->getContext();

    uint64_t memorySize1 = getMemoryUsage(display, context);

    std::vector<GLuint> buffers(kHashedObjectNameCount);
    glGenBuffers(kHashedObjectNameCount, buffers.data());
    ASSERT_GL_NO_ERROR();

    uint64_t memorySize2 = getMemoryUsage(display, context);
    EXPECT_EQ(memorySize2, memorySize1);

    glDeleteBuffers(kHashedObjectNameCount, buffers.data());
    ASSERT_GL_NO_ERROR();
}

// Test that memory usage can be queried while many generated texture names are not bound.
TEST_P(EGLMemoryUsageReportTest, QueryWithManyUnboundTextureNames)
{
    ANGLE_SKIP_TEST_IF(!hasMemoryUsageReportExtension());

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLContext context = getEGLWindow()->getContext();

    uint64_t memorySize1 = getMemoryUsage(display, context);

    std::vector<GLuint> textures(kHashedObjectNameCount);
    glGenTextures(kHashedObjectNameCount, textures.data());
    ASSERT_GL_NO_ERROR();

    uint64_t memorySize2 = getMemoryUsage(display, context);
    EXPECT_EQ(memorySize2, memorySize1);

    glDeleteTextures(kHashedObjectNameCount, textures.data());
    ASSERT_GL_NO_ERROR();
}

// Test that memory usage can be queried while many generated renderbuffer names are not bound.
TEST_P(EGLMemoryUsageReportTest, QueryWithManyUnboundRenderbufferNames)
{
    ANGLE_SKIP_TEST_IF(!hasMemoryUsageReportExtension());

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLContext context = getEGLWindow()->getContext();

    uint64_t memorySize1 = getMemoryUsage(display, context);

    std::vector<GLuint> renderbuffers(kHashedObjectNameCount);
    glGenRenderbuffers(kHashedObjectNameCount, renderbuffers.data());
    ASSERT_GL_NO_ERROR();

    uint64_t memorySize2 = getMemoryUsage(display, context);
    EXPECT_EQ(memorySize2, memorySize1);

    glDeleteRenderbuffers(kHashedObjectNameCount, renderbuffers.data());
    ASSERT_GL_NO_ERROR();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLMemoryUsageReportTest);
ANGLE_INSTANTIATE_TEST_ES3(EGLMemoryUsageReportTest);
