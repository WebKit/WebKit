//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WebGLCompatibilityTest.cpp : Tests of the GL_ANGLE_webgl_compatibility extension.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{

class WebGLCompatibilityTest : public ANGLETest
{
  protected:
    WebGLCompatibilityTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setWebGLCompatibilityEnabled(true);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();
        glEnableExtensionANGLE = reinterpret_cast<PFNGLENABLEEXTENSIONANGLEPROC>(
            eglGetProcAddress("glEnableExtensionANGLE"));
    }

    void TearDown() override { ANGLETest::TearDown(); }

    PFNGLENABLEEXTENSIONANGLEPROC glEnableExtensionANGLE = nullptr;
};

// Context creation would fail if EGL_ANGLE_create_context_webgl_compatibility was not available so
// the GL extension should always be present
TEST_P(WebGLCompatibilityTest, ExtensionStringExposed)
{
    EXPECT_TRUE(extensionEnabled("GL_ANGLE_webgl_compatibility"));
}

// Verify that all extension entry points are available
TEST_P(WebGLCompatibilityTest, EntryPoints)
{
    if (extensionEnabled("GL_ANGLE_webgl_compatibility"))
    {
        EXPECT_NE(nullptr, eglGetProcAddress("glEnableExtensionANGLE"));
    }
}

// WebGL 1 allows GL_DEPTH_STENCIL_ATTACHMENT as a valid binding point.  Make sure it is usable,
// even in ES2 contexts.
TEST_P(WebGLCompatibilityTest, DepthStencilBindingPoint)
{
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffer.get());

    EXPECT_GL_NO_ERROR();
}

// Test that attempting to enable an extension that doesn't exist generates GL_INVALID_OPERATION
TEST_P(WebGLCompatibilityTest, EnableExtensionValidation)
{
    EXPECT_EQ(GL_FALSE, glEnableExtensionANGLE("invalid_extension_string"));
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test enabling the GL_OES_element_index_uint extension
TEST_P(WebGLCompatibilityTest, EnableExtensionUintIndices)
{
    if (getClientMajorVersion() != 2)
    {
        // This test only works on ES2 where uint indices are not available by default
        return;
    }

    EXPECT_FALSE(extensionEnabled("GL_OES_element_index_uint"));

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());

    GLuint data[] = {0, 1, 2, 1, 3, 2};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

    ANGLE_GL_PROGRAM(program, "void main() { gl_Position = vec4(0, 0, 0, 1); }",
                     "void main() { gl_FragColor = vec4(0, 1, 0, 1); }")
    glUseProgram(program.get());

    glDrawElements(GL_TRIANGLES, 2, GL_UNSIGNED_INT, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (glEnableExtensionANGLE("GL_OES_element_index_uint"))
    {
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_OES_element_index_uint"));

        glDrawElements(GL_TRIANGLES, 2, GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(WebGLCompatibilityTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());

}  // namespace
