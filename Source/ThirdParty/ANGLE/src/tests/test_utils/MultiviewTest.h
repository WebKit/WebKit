//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MultiviewTest:
//   Implementation of helpers for multiview testing.
//

#ifndef ANGLE_TESTS_TESTUTILS_MULTIVIEWTEST_H_
#define ANGLE_TESTS_TESTUTILS_MULTIVIEWTEST_H_

#include "test_utils/ANGLETest.h"

namespace angle
{

// Creates a simple program that passes through two-dimensional vertices and renders green
// fragments.
GLuint CreateSimplePassthroughProgram(int numViews);

// Create a 2D texture array to use for multiview rendering. Texture ids should be
// created beforehand. If depthTexture or stencilTexture is 0, it will not be initialized.
// If samples is 0, then non-multisampled textures are created. Otherwise multisampled textures are
// created with the requested sample count.
void CreateMultiviewBackingTextures(int samples,
                                    int viewWidth,
                                    int height,
                                    int numLayers,
                                    std::vector<GLuint> colorTextures,
                                    GLuint depthTexture,
                                    GLuint depthStencilTexture);
void CreateMultiviewBackingTextures(int samples,
                                    int viewWidth,
                                    int height,
                                    int numLayers,
                                    GLuint colorTexture,
                                    GLuint depthTexture,
                                    GLuint depthStencilTexture);

// Attach multiview textures to the framebuffer denoted by target. If there are multiple color
// textures they get attached to different color attachments starting from 0.
void AttachMultiviewTextures(GLenum target,
                             int viewWidth,
                             int numViews,
                             int baseViewIndex,
                             std::vector<GLuint> colorTextures,
                             GLuint depthTexture,
                             GLuint depthStencilTexture);
void AttachMultiviewTextures(GLenum target,
                             int viewWidth,
                             int numViews,
                             int baseViewIndex,
                             GLuint colorTexture,
                             GLuint depthTexture,
                             GLuint depthStencilTexture);

struct MultiviewImplementationParams : public PlatformParameters
{
    MultiviewImplementationParams(GLint majorVersion,
                                  GLint minorVersion,
                                  bool forceUseGeometryShaderOnD3D,
                                  const EGLPlatformParameters &eglPlatformParameters)
        : PlatformParameters(majorVersion, minorVersion, eglPlatformParameters),
          mForceUseGeometryShaderOnD3D(forceUseGeometryShaderOnD3D)
    {}
    bool mForceUseGeometryShaderOnD3D;
};
std::ostream &operator<<(std::ostream &os, const MultiviewImplementationParams &params);

MultiviewImplementationParams VertexShaderOpenGL(GLint majorVersion, GLint minorVersion);
MultiviewImplementationParams VertexShaderD3D11(GLint majorVersion, GLint minorVersion);
MultiviewImplementationParams GeomShaderD3D11(GLint majorVersion, GLint minorVersion);

class MultiviewTestBase : public ANGLETestBase
{
  protected:
    MultiviewTestBase(const PlatformParameters &params) : ANGLETestBase(params)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setWebGLCompatibilityEnabled(true);
    }
    virtual ~MultiviewTestBase() {}

    void MultiviewTestBaseSetUp()
    {
        ANGLETestBase::ANGLETestSetUp();

        glRequestExtensionANGLE = reinterpret_cast<PFNGLREQUESTEXTENSIONANGLEPROC>(
            eglGetProcAddress("glRequestExtensionANGLE"));
    }

    void MultiviewTestBaseTearDown() { ANGLETestBase::ANGLETestTearDown(); }

    // Requests the OVR_multiview2 extension and returns true if the operation succeeds.
    bool requestMultiviewExtension(bool requireMultiviewMultisample)
    {
        if (IsGLExtensionRequestable("GL_OVR_multiview2"))
        {
            glRequestExtensionANGLE("GL_OVR_multiview2");
        }

        if (!IsGLExtensionEnabled("GL_OVR_multiview2"))
        {
            std::cout << "Test skipped due to missing GL_OVR_multiview2." << std::endl;
            return false;
        }

        if (requireMultiviewMultisample)
        {
            if (IsGLExtensionRequestable("GL_OES_texture_storage_multisample_2d_array"))
            {
                glRequestExtensionANGLE("GL_OES_texture_storage_multisample_2d_array");
            }
            if (IsGLExtensionRequestable("GL_ANGLE_multiview_multisample"))
            {
                glRequestExtensionANGLE("GL_ANGLE_multiview_multisample");
            }

            if (!IsGLExtensionEnabled("GL_ANGLE_multiview_multisample"))
            {
                std::cout << "Test skipped due to missing GL_ANGLE_multiview_multisample."
                          << std::endl;
                return false;
            }
        }
        return true;
    }

    bool requestMultiviewExtension() { return requestMultiviewExtension(false); }

    PFNGLREQUESTEXTENSIONANGLEPROC glRequestExtensionANGLE = nullptr;
};

// Base class for multiview tests that don't need specific helper functions.
class MultiviewTest : public MultiviewTestBase,
                      public ::testing::TestWithParam<MultiviewImplementationParams>
{
  protected:
    MultiviewTest() : MultiviewTestBase(GetParam()) {}
    void SetUp() override { MultiviewTestBase::MultiviewTestBaseSetUp(); }
    void TearDown() override { MultiviewTestBase::MultiviewTestBaseTearDown(); }

    void overrideWorkaroundsD3D(WorkaroundsD3D *workarounds) final;
};

}  // namespace angle

#endif  // ANGLE_TESTS_TESTUTILS_MULTIVIEWTEST_H_
