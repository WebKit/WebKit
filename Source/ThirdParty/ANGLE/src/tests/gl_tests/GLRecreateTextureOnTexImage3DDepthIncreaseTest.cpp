//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GLRecreateTextureOnTexImage3DDepthIncreaseTest:
//   White box tests for the recreateTextureOnTexImage3dDepthIncrease workaround of the GL backend
//   on drivers that do not support glCopyImageSubData.  When the workaround is enabled,
//   glTexImage3D must never increase the depth of a level of a mutable 3D texture in place in the
//   native driver.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"
// 'None' is defined as 'struct None {};' in
// third_party/googletest/src/googletest/include/gtest/internal/gtest-type-util.h.
// But 'None' is also defined as a numeric constant 0L in <X11/X.h>.
// So we need to include ANGLETest.h first to avoid this conflict.

#include <vector>

#include "common/angleutils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

using namespace angle;

namespace
{
constexpr GLsizei kSize = 4;

// Makes glCopyImageSubData unavailable to the GL backend while in scope, like on drivers that
// support neither OpenGL 4.3 / OpenGL ES 3.2 nor any of the copy image extensions.
class ScopedNoCopyImageSubData final : angle::NonCopyable
{
  public:
    explicit ScopedNoCopyImageSubData(const rx::FunctionsGL *functions)
        : mFunctions(const_cast<rx::FunctionsGL *>(functions)),
          mCopyImageSubData(mFunctions->copyImageSubData)
    {
        mFunctions->copyImageSubData = nullptr;
    }
    ~ScopedNoCopyImageSubData() { mFunctions->copyImageSubData = mCopyImageSubData; }

  private:
    rx::FunctionsGL *mFunctions;
    decltype(rx::FunctionsGL::copyImageSubData) mCopyImageSubData;
};

class GLRecreateTextureOnTexImage3DDepthIncreaseTest : public ANGLETest<>
{
  protected:
    GLRecreateTextureOnTexImage3DDepthIncreaseTest()
    {
        setWindowWidth(kSize);
        setWindowHeight(kSize);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    gl::Context *hackContext() const
    {
        egl::Display *display   = static_cast<egl::Display *>(getEGLWindow()->getDisplay());
        gl::ContextID contextID = {
            static_cast<GLuint>(reinterpret_cast<uintptr_t>(getEGLWindow()->getContext()))};
        return display->getContextRef(contextID).get();
    }

    const rx::FunctionsGL *getFunctionsGL() const
    {
        // Hack the angle!
        return rx::GetImplAs<rx::ContextGL>(hackContext())->getFunctions();
    }

    GLuint getNativeTextureID(GLuint texture) const
    {
        return rx::GetImplAs<rx::TextureGL>(hackContext()->getTexture({texture}))->getTextureID();
    }

    // Defines level 0 of |texture| as kSize x kSize x 2 filled with red, and level 1 as
    // kSize/2 x kSize/2 x 1 filled with green.
    void defineTwoLevels(GLuint texture)
    {
        glBindTexture(GL_TEXTURE_3D, texture);
        const std::vector<GLColor> pixelsRed(kSize * kSize * 2, GLColor::red);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kSize, kSize, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelsRed.data());
        const std::vector<GLColor> pixelsGreen((kSize / 2) * (kSize / 2), GLColor::green);
        glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, kSize / 2, kSize / 2, 1, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixelsGreen.data());
    }

    // Redefines |level| of the 3D texture bound to GL_TEXTURE_3D with the size it has in
    // defineTwoLevels(), except for a depth of 4, filled with blue.
    void increaseLevelDepth(GLint level)
    {
        const GLsizei width = kSize >> level;
        const std::vector<GLColor> pixelsBlue(width * width * 4, GLColor::blue);
        glTexImage3D(GL_TEXTURE_3D, level, GL_RGBA8, width, width, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelsBlue.data());
    }

    // Verifies that |level| of |texture| is |width| x |width| x |depth| and filled with |color|.
    void verifyLevel(GLuint texture, GLint level, GLsizei width, GLsizei depth, GLColor color)
    {
        // Attaching a level other than the base level requires the texture to be mipmap complete.
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, level);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, level);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        for (GLint layer = 0; layer < depth; ++layer)
        {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, level, layer);
            ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
            EXPECT_PIXEL_RECT_EQ(0, 0, width, width, color);
        }

        // The level does not have any more layers.
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, level, depth);
        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

// Test that increasing the depth of the only level of a 3D texture recreates the native texture
// when glCopyImageSubData is unavailable, as there are no other levels to copy to the new texture.
TEST_P(GLRecreateTextureOnTexImage3DDepthIncreaseTest, SingleLevelWithoutCopyImageSubData)
{
    ScopedNoCopyImageSubData noCopyImageSubData(getFunctionsGL());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_3D, texture);
    const std::vector<GLColor> pixelsRed(kSize * kSize, GLColor::red);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kSize, kSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsRed.data());
    ASSERT_GL_NO_ERROR();
    const GLuint nativeTextureID = getNativeTextureID(texture);

    // Increase the depth from 1 to 4.
    increaseLevelDepth(0);
    ASSERT_GL_NO_ERROR();
    EXPECT_NE(getNativeTextureID(texture), nativeTextureID);

    verifyLevel(texture, 0, kSize, 4, GLColor::blue);
}

// Test that other levels defined with a zero extent do not prevent recreating the native texture
// when glCopyImageSubData is unavailable, as they do not have any contents to copy.
TEST_P(GLRecreateTextureOnTexImage3DDepthIncreaseTest, ZeroExtentLevelsWithoutCopyImageSubData)
{
    ScopedNoCopyImageSubData noCopyImageSubData(getFunctionsGL());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_3D, texture);
    const std::vector<GLColor> pixelsRed(kSize * kSize, GLColor::red);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kSize, kSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsRed.data());
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, kSize / 2, kSize / 2, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage3D(GL_TEXTURE_3D, 2, GL_RGBA8, 0, 0, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();
    const GLuint nativeTextureID = getNativeTextureID(texture);

    // Increase the depth of level 0 from 1 to 4.
    increaseLevelDepth(0);
    ASSERT_GL_NO_ERROR();
    EXPECT_NE(getNativeTextureID(texture), nativeTextureID);

    verifyLevel(texture, 0, kSize, 4, GLColor::blue);
}

// Test that increasing the depth of a level of a 3D texture fails when glCopyImageSubData is
// unavailable to copy the contents of the other levels to a new native texture.  The level must
// not be redefined in place instead, and the texture must be left unchanged.
TEST_P(GLRecreateTextureOnTexImage3DDepthIncreaseTest, MultipleLevelsWithoutCopyImageSubData)
{
    ScopedNoCopyImageSubData noCopyImageSubData(getFunctionsGL());

    for (GLint levelToIncrease : {0, 1})
    {
        GLTexture texture;
        defineTwoLevels(texture);
        ASSERT_GL_NO_ERROR();
        const GLuint nativeTextureID = getNativeTextureID(texture);

        increaseLevelDepth(levelToIncrease);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        EXPECT_EQ(getNativeTextureID(texture), nativeTextureID);

        verifyLevel(texture, 0, kSize, 2, GLColor::red);
        verifyLevel(texture, 1, kSize / 2, 1, GLColor::green);
    }
}

// Test that redefining a level of a 3D texture without increasing its depth neither recreates the
// native texture nor fails when glCopyImageSubData is unavailable, even if other levels are
// defined.
TEST_P(GLRecreateTextureOnTexImage3DDepthIncreaseTest, NoDepthIncreaseWithoutCopyImageSubData)
{
    ScopedNoCopyImageSubData noCopyImageSubData(getFunctionsGL());

    GLTexture texture;
    defineTwoLevels(texture);
    ASSERT_GL_NO_ERROR();
    const GLuint nativeTextureID = getNativeTextureID(texture);

    // Redefine level 0 with the same depth.
    const std::vector<GLColor> pixelsBlue(kSize * kSize * 2, GLColor::blue);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kSize, kSize, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsBlue.data());
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(getNativeTextureID(texture), nativeTextureID);

    // Redefine level 0 with a smaller depth.
    const std::vector<GLColor> pixelsYellow(kSize * kSize, GLColor::yellow);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kSize, kSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsYellow.data());
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(getNativeTextureID(texture), nativeTextureID);
}

// Disable VirtualizeBaseLevelAndMipchain, which manages the levels of the texture in separate
// native textures and bypasses the workaround.
ANGLE_INSTANTIATE_TEST(GLRecreateTextureOnTexImage3DDepthIncreaseTest,
                       ES3_OPENGL()
                           .enable(Feature::RecreateTextureOnTexImage3dDepthIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain),
                       ES3_OPENGLES()
                           .enable(Feature::RecreateTextureOnTexImage3dDepthIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLRecreateTextureOnTexImage3DDepthIncreaseTest);

}  // anonymous namespace
