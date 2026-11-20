//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GLReattachTextureToFboTest:
//   White box tests for the reattachTextureToFboAfterLayerIncrease workaround of the GL backend.
//   When the workaround is enabled, a layered texture must be detached from framebuffers in the
//   native driver before a call that can increase its number of images.  The affected drivers
//   behave correctly otherwise, so this can only be verified by inspecting native GL state.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"
// 'None' is defined as 'struct None {};' in
// third_party/googletest/src/googletest/include/gtest/internal/gtest-type-util.h.
// But 'None' is also defined as a numeric constant 0L in <X11/X.h>.
// So we need to include ANGLETest.h first to avoid this conflict.

#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

using namespace angle;

namespace
{
constexpr GLsizei kSize = 4;

class GLReattachTextureToFboTest : public ANGLETest<>
{
  protected:
    GLReattachTextureToFboTest()
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

    rx::ContextGL *hackANGLE() const
    {
        // Hack the angle!
        return rx::GetImplAs<rx::ContextGL>(hackContext());
    }

    bool isWorkaroundEnabled()
    {
        return getEGLWindow()->isFeatureEnabled(Feature::ReattachTextureToFboAfterLayerIncrease);
    }

    GLuint getNativeTextureID(GLuint texture) const
    {
        return rx::GetImplAs<rx::TextureGL>(hackContext()->getTexture({texture}))->getTextureID();
    }

    // Returns the name of the texture attached to GL_COLOR_ATTACHMENT0 of |framebuffer| in the
    // native driver, bypassing ANGLE's frontend state tracking.  Returns 0 if no texture is
    // attached.
    GLuint getNativeColorAttachment0TextureID(GLuint framebuffer) const
    {
        rx::ContextGL *contextGL         = hackANGLE();
        const rx::FunctionsGL *functions = contextGL->getFunctions();
        const rx::FramebufferGL *framebufferGL =
            rx::GetImplAs<rx::FramebufferGL>(hackContext()->getFramebuffer({framebuffer}));

        // Bind through the state manager, which flags the framebuffer binding dirty bits so that
        // the application's bindings are restored on the next state sync.
        contextGL->getStateManager()->bindFramebuffer(GL_FRAMEBUFFER,
                                                      framebufferGL->getFramebufferID());

        GLint type = GL_NONE;
        functions->getFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        if (type != GL_TEXTURE)
        {
            return 0;
        }

        GLint name = 0;
        functions->getFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
        return static_cast<GLuint>(name);
    }

    // Verifies that the texture is attached to |framebuffer| in the native driver.  Must be called
    // after the framebuffer has been synced, e.g. by reading from it.
    void expectNativelyAttached(GLuint framebuffer, GLuint texture)
    {
        EXPECT_EQ(getNativeColorAttachment0TextureID(framebuffer), getNativeTextureID(texture));
    }

    // Verifies the native attachment state right after a call that may increase the number of
    // images of |texture|.  With the workaround, the texture must have been detached natively.
    // Without it, it must still be attached, which validates that the check is meaningful.
    void expectNativelyDetachedIfWorkaroundEnabled(GLuint framebuffer, GLuint texture)
    {
        const GLuint expected = isWorkaroundEnabled() ? 0u : getNativeTextureID(texture);
        EXPECT_EQ(getNativeColorAttachment0TextureID(framebuffer), expected);
    }
};

class GLReattachTextureToFboTestES31 : public GLReattachTextureToFboTest
{};

// Test that allocating storage for a 2D array texture with glTexStorage3D while one of its layers
// is attached to a framebuffer detaches the texture in the native driver.
TEST_P(GLReattachTextureToFboTest, Texture2DArrayTexStorage3D)
{
    for (GLsizei levels : {1, 2})
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        const std::vector<GLColor> pixelsRed(kSize * kSize, GLColor::red);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kSize, kSize, 1, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixelsRed.data());
        ASSERT_GL_NO_ERROR();

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 0);
        EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
        expectNativelyAttached(fbo, texture);

        // Increase the layer count from 1 to 2.
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, levels, GL_RGBA8, kSize, kSize, 2);
        ASSERT_GL_NO_ERROR();
        expectNativelyDetachedIfWorkaroundEnabled(fbo, texture);

        // Using the framebuffer reattaches the texture and renders correctly.
        const std::vector<GLColor> pixelsGreen(kSize * kSize * 2, GLColor::green);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, kSize, kSize, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixelsGreen.data());
        EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::green);
        expectNativelyAttached(fbo, texture);
    }
}

// Test that allocating storage for a 3D texture with glTexStorage3D while one of its slices is
// attached to a framebuffer detaches the texture in the native driver.
TEST_P(GLReattachTextureToFboTest, Texture3DTexStorage3D)
{
    for (GLsizei levels : {1, 2})
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        const std::vector<GLColor> pixelsRed(kSize * kSize, GLColor::red);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kSize, kSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelsRed.data());
        ASSERT_GL_NO_ERROR();

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 0);
        EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
        expectNativelyAttached(fbo, texture);

        // Increase the depth from 1 to 2.
        glTexStorage3D(GL_TEXTURE_3D, levels, GL_RGBA8, kSize, kSize, 2);
        ASSERT_GL_NO_ERROR();
        expectNativelyDetachedIfWorkaroundEnabled(fbo, texture);

        const std::vector<GLColor> pixelsGreen(kSize * kSize * 2, GLColor::green);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kSize, kSize, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixelsGreen.data());
        EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::green);
        expectNativelyAttached(fbo, texture);
    }
}

// Test that allocating storage for a cube map texture with glTexStorage2D while one of its faces
// is attached to a framebuffer detaches the texture in the native driver.  The number of images
// grows when the storage has more levels than were previously defined.
TEST_P(GLReattachTextureToFboTest, TextureCubeMapTexStorage2D)
{
    for (GLsizei levels : {1, 3})
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        const std::vector<GLColor> pixelsRed(kSize * kSize, GLColor::red);
        for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
             ++face)
        {
            glTexImage2D(face, 0, GL_RGBA8, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixelsRed.data());
        }
        ASSERT_GL_NO_ERROR();

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                               texture, 0);
        EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
        expectNativelyAttached(fbo, texture);

        glTexStorage2D(GL_TEXTURE_CUBE_MAP, levels, GL_RGBA8, kSize, kSize);
        ASSERT_GL_NO_ERROR();
        expectNativelyDetachedIfWorkaroundEnabled(fbo, texture);

        const std::vector<GLColor> pixelsGreen(kSize * kSize, GLColor::green);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, kSize, kSize, GL_RGBA,
                        GL_UNSIGNED_BYTE, pixelsGreen.data());
        EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::green);
        expectNativelyAttached(fbo, texture);
    }
}

// Test that allocating storage for a cube map array texture with glTexStorage3D while one of its
// layer-faces is attached to a framebuffer detaches the texture in the native driver.
TEST_P(GLReattachTextureToFboTestES31, TextureCubeMapArrayTexStorage3D)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_cube_map_array"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // One cube, i.e. 6 layer-faces.
    const std::vector<GLColor> pixelsRed(kSize * kSize * 6, GLColor::red);
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA8, kSize, kSize, 6, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixelsRed.data());
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 0);
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
    expectNativelyAttached(fbo, texture);

    // Increase the number of cubes from 1 to 2.
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_RGBA8, kSize, kSize, 12);
    ASSERT_GL_NO_ERROR();
    expectNativelyDetachedIfWorkaroundEnabled(fbo, texture);

    const std::vector<GLColor> pixelsGreen(kSize * kSize * 12, GLColor::green);
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, kSize, kSize, 12, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixelsGreen.data());
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::green);
    expectNativelyAttached(fbo, texture);
}

// Test that increasing the layer count of a 2D array texture with glTexImage3D while one of its
// layers is attached to a framebuffer detaches the texture in the native driver.
TEST_P(GLReattachTextureToFboTest, Texture2DArrayTexImage3D)
{
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    const std::vector<GLColor> pixelsRed(kSize * kSize, GLColor::red);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kSize, kSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsRed.data());
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 0);
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::red);
    expectNativelyAttached(fbo, texture);

    const std::vector<GLColor> pixelsGreen(kSize * kSize * 2, GLColor::green);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kSize, kSize, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelsGreen.data());
    ASSERT_GL_NO_ERROR();
    expectNativelyDetachedIfWorkaroundEnabled(fbo, texture);

    EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::green);
    expectNativelyAttached(fbo, texture);
}

// Disable VirtualizeBaseLevelAndMipchain so that TexStorage does not recreate the native texture
// ID (which would implicitly detach it from the bound framebuffer even without
// ReattachTextureToFboAfterLayerIncrease).
ANGLE_INSTANTIATE_TEST(GLReattachTextureToFboTest,
                       ES3_OPENGL()
                           .enable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain),
                       ES3_OPENGL()
                           .disable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain),
                       ES3_OPENGLES()
                           .enable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain),
                       ES3_OPENGLES()
                           .disable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLReattachTextureToFboTest);

ANGLE_INSTANTIATE_TEST(GLReattachTextureToFboTestES31,
                       ES31_OPENGL()
                           .enable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain),
                       ES31_OPENGL()
                           .disable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain),
                       ES31_OPENGLES()
                           .enable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain),
                       ES31_OPENGLES()
                           .disable(Feature::ReattachTextureToFboAfterLayerIncrease)
                           .disable(Feature::VirtualizeBaseLevelAndMipchain));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLReattachTextureToFboTestES31);

}  // anonymous namespace
