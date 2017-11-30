//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Multiview draw tests:
// Test issuing multiview Draw* commands.
//

#include "platform/WorkaroundsD3D.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
GLuint CreateSimplePassthroughProgram(int numViews)
{
    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = " +
        ToString(numViews) +
        ") in;\n"
        "layout(location=0) in vec2 vPosition;\n"
        "void main()\n"
        "{\n"
        "   gl_PointSize = 1.;\n"
        "   gl_Position = vec4(vPosition.xy, 0.0, 1.0);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "   col = vec4(0,1,0,1);\n"
        "}\n";
    return CompileProgram(vsSource, fsSource);
}

std::vector<Vector2> ConvertPixelCoordinatesToClipSpace(const std::vector<Vector2I> &pixels,
                                                        int width,
                                                        int height)
{
    std::vector<Vector2> result(pixels.size());
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        const auto &pixel          = pixels[i];
        float pixelCenterRelativeX = (static_cast<float>(pixel.x()) + .5f) / width;
        float pixelCenterRelativeY = (static_cast<float>(pixel.y()) + .5f) / height;
        float xInClipSpace         = 2.f * pixelCenterRelativeX - 1.f;
        float yInClipSpace         = 2.f * pixelCenterRelativeY - 1.f;
        result[i]                  = Vector2(xInClipSpace, yInClipSpace);
    }
    return result;
}
}  // namespace

struct MultiviewImplementationParams : public PlatformParameters
{
    MultiviewImplementationParams(bool forceUseGeometryShaderOnD3D,
                                  const EGLPlatformParameters &eglPlatformParameters)
        : PlatformParameters(3, 0, eglPlatformParameters),
          mForceUseGeometryShaderOnD3D(forceUseGeometryShaderOnD3D)
    {
    }
    bool mForceUseGeometryShaderOnD3D;
};

std::ostream &operator<<(std::ostream &os, const MultiviewImplementationParams &params)
{
    const PlatformParameters &base = static_cast<const PlatformParameters &>(params);
    os << base;
    if (params.mForceUseGeometryShaderOnD3D)
    {
        os << "_force_geom_shader";
    }
    else
    {
        os << "_vertex_shader";
    }
    return os;
}

struct MultiviewTestParams final : public MultiviewImplementationParams
{
    MultiviewTestParams(GLenum multiviewLayout,
                        const MultiviewImplementationParams &implementationParams)
        : MultiviewImplementationParams(implementationParams), mMultiviewLayout(multiviewLayout)
    {
        ASSERT(multiviewLayout == GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE ||
               multiviewLayout == GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE);
    }
    GLenum mMultiviewLayout;
};

std::ostream &operator<<(std::ostream &os, const MultiviewTestParams &params)
{
    const MultiviewImplementationParams &base =
        static_cast<const MultiviewImplementationParams &>(params);
    os << base;
    switch (params.mMultiviewLayout)
    {
        case GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE:
            os << "_layered";
            break;
        case GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE:
            os << "_side_by_side";
            break;
        default:
            UNREACHABLE();
    }
    return os;
}

class MultiviewDrawTest : public ANGLETestBase
{
  protected:
    MultiviewDrawTest(const PlatformParameters &params) : ANGLETestBase(params)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setWebGLCompatibilityEnabled(true);
    }
    virtual ~MultiviewDrawTest() {}

    void DrawTestSetUp()
    {
        ANGLETestBase::ANGLETestSetUp();

        glRequestExtensionANGLE = reinterpret_cast<PFNGLREQUESTEXTENSIONANGLEPROC>(
            eglGetProcAddress("glRequestExtensionANGLE"));
    }

    // Requests the ANGLE_multiview extension and returns true if the operation succeeds.
    bool requestMultiviewExtension()
    {
        if (extensionRequestable("GL_ANGLE_multiview"))
        {
            glRequestExtensionANGLE("GL_ANGLE_multiview");
        }

        if (!extensionEnabled("GL_ANGLE_multiview"))
        {
            std::cout << "Test skipped due to missing GL_ANGLE_multiview." << std::endl;
            return false;
        }
        return true;
    }

    PFNGLREQUESTEXTENSIONANGLEPROC glRequestExtensionANGLE = nullptr;
};

class MultiviewDrawValidationTest : public MultiviewDrawTest,
                                    public ::testing::TestWithParam<PlatformParameters>
{
  protected:
    MultiviewDrawValidationTest() : MultiviewDrawTest(GetParam()) {}

    void SetUp() override
    {
        MultiviewDrawTest::DrawTestSetUp();

        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

        glBindTexture(GL_TEXTURE_2D, mTex2d);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindVertexArray(mVao);

        const float kVertexData[3] = {0.0f};
        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3u, &kVertexData[0], GL_STATIC_DRAW);

        const unsigned int kIndices[3] = {0u, 1u, 2u};
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 3, &kIndices[0],
                     GL_STATIC_DRAW);
        ASSERT_GL_NO_ERROR();
    }

    GLTexture mTex2d;
    GLVertexArray mVao;
    GLBuffer mVbo;
    GLBuffer mIbo;
    GLFramebuffer mFramebuffer;
};

class MultiviewRenderTestBase : public MultiviewDrawTest
{
  protected:
    MultiviewRenderTestBase(const PlatformParameters &params, GLenum multiviewLayout)
        : MultiviewDrawTest(params),
          mMultiviewLayout(multiviewLayout),
          mViewWidth(0),
          mViewHeight(0),
          mNumViews(0)
    {
    }
    void RenderTestSetUp() { MultiviewDrawTest::DrawTestSetUp(); }
    void createFBO(int viewWidth, int height, int numViews, int numLayers, int baseViewIndex)
    {
        ASSERT(numViews + baseViewIndex <= numLayers);

        mViewWidth  = viewWidth;
        mViewHeight = height;
        mNumViews   = numViews;

        mColorTexture.reset();
        mDepthTexture.reset();
        mDrawFramebuffer.reset();
        mReadFramebuffer.clear();

        // Create color and depth textures.
        switch (mMultiviewLayout)
        {
            case GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE:
            {
                int textureWidth = viewWidth * numViews;
                glBindTexture(GL_TEXTURE_2D, mColorTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                glBindTexture(GL_TEXTURE_2D, mDepthTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, textureWidth, height, 0,
                             GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                glBindTexture(GL_TEXTURE_2D, 0);
                break;
            }
            case GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE:
                glBindTexture(GL_TEXTURE_2D_ARRAY, mColorTexture);
                glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, viewWidth, height, numLayers, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                glBindTexture(GL_TEXTURE_2D_ARRAY, mDepthTexture);
                glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, viewWidth, height,
                             numLayers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
                break;
            default:
                UNREACHABLE();
        }
        ASSERT_GL_NO_ERROR();

        // Create draw framebuffer to be used for multiview rendering.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFramebuffer);
        switch (mMultiviewLayout)
        {
            case GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE:
            {
                std::vector<GLint> viewportOffsets(numViews * 2);
                for (int i = 0u; i < numViews; ++i)
                {
                    viewportOffsets[i * 2]     = i * viewWidth;
                    viewportOffsets[i * 2 + 1] = 0;
                }
                glFramebufferTextureMultiviewSideBySideANGLE(GL_DRAW_FRAMEBUFFER,
                                                             GL_COLOR_ATTACHMENT0, mColorTexture, 0,
                                                             numViews, &viewportOffsets[0]);
                glFramebufferTextureMultiviewSideBySideANGLE(GL_DRAW_FRAMEBUFFER,
                                                             GL_DEPTH_ATTACHMENT, mDepthTexture, 0,
                                                             numViews, &viewportOffsets[0]);
                break;
            }
            case GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE:
                glFramebufferTextureMultiviewLayeredANGLE(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                          mColorTexture, 0, baseViewIndex,
                                                          numViews);
                glFramebufferTextureMultiviewLayeredANGLE(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                          mDepthTexture, 0, baseViewIndex,
                                                          numViews);
                break;
            default:
                UNREACHABLE();
        }

        GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, DrawBuffers);
        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));

        // Create read framebuffer to be used to retrieve the pixel information for testing
        // purposes.
        switch (mMultiviewLayout)
        {
            case GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE:
                mReadFramebuffer.resize(1);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, mReadFramebuffer[0]);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       mColorTexture, 0);
                ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE,
                                 glCheckFramebufferStatus(GL_READ_FRAMEBUFFER));
                break;
            case GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE:
                mReadFramebuffer.resize(numLayers);
                for (int i = 0; i < numLayers; ++i)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, mReadFramebuffer[i]);
                    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mColorTexture,
                                              0, i);
                    glClearColor(0, 0, 0, 0);
                    glClear(GL_COLOR_BUFFER_BIT);
                    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE,
                                     glCheckFramebufferStatus(GL_READ_FRAMEBUFFER));
                }
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFramebuffer);
                break;
            default:
                UNREACHABLE();
        }

        // Clear the buffers.
        glViewport(0, 0, viewWidth, height);
        if (mMultiviewLayout == GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE)
        {
            // Enable the scissor test only for side-by-side framebuffers.
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, viewWidth, height);
        }
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void createFBO(int viewWidth, int height, int numViews)
    {
        createFBO(viewWidth, height, numViews, numViews, 0);
    }

    GLColor GetViewColor(int x, int y, int view)
    {
        switch (mMultiviewLayout)
        {
            case GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE:
                return ReadColor(view * mViewWidth + x, y);
            case GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE:
                ASSERT(static_cast<size_t>(view) < mReadFramebuffer.size());
                glBindFramebuffer(GL_READ_FRAMEBUFFER, mReadFramebuffer[view]);
                return ReadColor(x, y);
            default:
                UNREACHABLE();
        }
        return GLColor(0, 0, 0, 0);
    }

    GLTexture mColorTexture;
    GLTexture mDepthTexture;
    GLFramebuffer mDrawFramebuffer;
    std::vector<GLFramebuffer> mReadFramebuffer;
    GLenum mMultiviewLayout;
    int mViewWidth;
    int mViewHeight;
    int mNumViews;
};

class MultiviewRenderTest : public MultiviewRenderTestBase,
                            public ::testing::TestWithParam<MultiviewTestParams>
{
  protected:
    MultiviewRenderTest() : MultiviewRenderTestBase(GetParam(), GetParam().mMultiviewLayout) {}
    void SetUp() override { MultiviewRenderTestBase::RenderTestSetUp(); }

    void overrideWorkaroundsD3D(WorkaroundsD3D *workarounds) override
    {
        workarounds->selectViewInGeometryShader = GetParam().mForceUseGeometryShaderOnD3D;
    }
};

class MultiviewRenderDualViewTest : public MultiviewRenderTest
{
  protected:
    MultiviewRenderDualViewTest() : mProgram(0u) {}
    ~MultiviewRenderDualViewTest()
    {
        if (mProgram != 0u)
        {
            glDeleteProgram(mProgram);
        }
    }

    void SetUp() override
    {
        MultiviewRenderTest::SetUp();

        if (!requestMultiviewExtension())
        {
            return;
        }

        const std::string vsSource =
            "#version 300 es\n"
            "#extension GL_OVR_multiview : require\n"
            "layout(num_views = 2) in;\n"
            "in vec4 vPosition;\n"
            "void main()\n"
            "{\n"
            "   gl_Position.x = (gl_ViewID_OVR == 0u ? vPosition.x*0.5 + 0.5 : vPosition.x*0.5);\n"
            "   gl_Position.yzw = vPosition.yzw;\n"
            "}\n";

        const std::string fsSource =
            "#version 300 es\n"
            "#extension GL_OVR_multiview : require\n"
            "precision mediump float;\n"
            "out vec4 col;\n"
            "void main()\n"
            "{\n"
            "   col = vec4(0,1,0,1);\n"
            "}\n";

        createFBO(2, 1, 2);
        mProgram = CompileProgram(vsSource, fsSource);
        ASSERT_NE(mProgram, 0u);
        glUseProgram(mProgram);
        ASSERT_GL_NO_ERROR();
    }

    void checkOutput()
    {
        EXPECT_EQ(GLColor::black, GetViewColor(0, 0, 0));
        EXPECT_EQ(GLColor::green, GetViewColor(1, 0, 0));
        EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));
        EXPECT_EQ(GLColor::black, GetViewColor(1, 0, 1));
    }

    GLuint mProgram;
};

class MultiviewOcclusionQueryTest : public MultiviewRenderTest
{
  protected:
    MultiviewOcclusionQueryTest() {}

    bool requestOcclusionQueryExtension()
    {
        if (extensionRequestable("GL_EXT_occlusion_query_boolean"))
        {
            glRequestExtensionANGLE("GL_EXT_occlusion_query_boolean");
        }

        if (!extensionEnabled("GL_EXT_occlusion_query_boolean"))
        {
            std::cout << "Test skipped due to missing GL_EXT_occlusion_query_boolean." << std::endl;
            return false;
        }
        return true;
    }

    GLuint drawAndRetrieveOcclusionQueryResult(GLuint program)
    {
        GLQueryEXT query;
        glBeginQueryEXT(GL_ANY_SAMPLES_PASSED, query);
        drawQuad(program, "vPosition", 0.0f, 1.0f, true);
        glEndQueryEXT(GL_ANY_SAMPLES_PASSED);

        GLuint result = GL_TRUE;
        glGetQueryObjectuivEXT(query, GL_QUERY_RESULT, &result);
        return result;
    }
};

class MultiviewProgramGenerationTest : public MultiviewRenderTest
{
  protected:
    MultiviewProgramGenerationTest() {}
};

class MultiviewRenderPrimitiveTest : public MultiviewRenderTest
{
  protected:
    MultiviewRenderPrimitiveTest() {}

    void setupGeometry(const std::vector<Vector2> &vertexData)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(Vector2), vertexData.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }

    void checkGreenChannel(const GLubyte expectedGreenChannelData[])
    {
        for (int view = 0; view < mNumViews; ++view)
        {
            for (int w = 0; w < mViewWidth; ++w)
            {
                for (int h = 0; h < mViewHeight; ++h)
                {
                    size_t flatIndex =
                        static_cast<size_t>(view * mViewWidth * mViewHeight + mViewWidth * h + w);
                    EXPECT_EQ(GLColor(0, expectedGreenChannelData[flatIndex], 0, 255),
                              GetViewColor(w, h, view));
                }
            }
        }
    }
    GLBuffer mVBO;
};

class MultiviewSideBySideRenderTest : public MultiviewRenderTestBase,
                                      public ::testing::TestWithParam<MultiviewImplementationParams>
{
  protected:
    MultiviewSideBySideRenderTest()
        : MultiviewRenderTestBase(GetParam(), GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE)
    {
    }
    void SetUp() override { MultiviewRenderTestBase::RenderTestSetUp(); }
    void overrideWorkaroundsD3D(WorkaroundsD3D *workarounds) override
    {
        workarounds->selectViewInGeometryShader = GetParam().mForceUseGeometryShaderOnD3D;
    }
};

class MultiviewLayeredRenderTest : public MultiviewRenderTestBase,
                                   public ::testing::TestWithParam<MultiviewImplementationParams>
{
  protected:
    MultiviewLayeredRenderTest()
        : MultiviewRenderTestBase(GetParam(), GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE)
    {
    }
    void SetUp() override { MultiviewRenderTestBase::RenderTestSetUp(); }
    void overrideWorkaroundsD3D(WorkaroundsD3D *workarounds) override
    {
        workarounds->selectViewInGeometryShader = GetParam().mForceUseGeometryShaderOnD3D;
    }
};

// The test verifies that glDraw*Indirect:
// 1) generates an INVALID_OPERATION error if the number of views in the draw framebuffer is greater
// than 1.
// 2) does not generate any error if the draw framebuffer has exactly 1 view.
TEST_P(MultiviewDrawValidationTest, IndirectDraw)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const GLint viewportOffsets[4] = {0, 0, 2, 0};

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "void main()\n"
        "{}\n";

    GLBuffer commandBuffer;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer);
    const GLuint commandData[] = {1u, 1u, 0u, 0u, 0u};
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(GLuint) * 5u, &commandData[0], GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Check for a GL_INVALID_OPERATION error with the framebuffer having 2 views.
    {
        const std::string &vsSource =
            "#version 300 es\n"
            "#extension GL_OVR_multiview : require\n"
            "layout(num_views = 2) in;\n"
            "void main()\n"
            "{}\n";
        ANGLE_GL_PROGRAM(program, vsSource, fsSource);
        glUseProgram(program);

        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 2, &viewportOffsets[0]);

        glDrawArraysIndirect(GL_TRIANGLES, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    // Check that no errors are generated if the number of views is 1.
    {
        const std::string &vsSource =
            "#version 300 es\n"
            "#extension GL_OVR_multiview : require\n"
            "layout(num_views = 1) in;\n"
            "void main()\n"
            "{}\n";
        ANGLE_GL_PROGRAM(program, vsSource, fsSource);
        glUseProgram(program);

        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 1, &viewportOffsets[0]);

        glDrawArraysIndirect(GL_TRIANGLES, nullptr);
        EXPECT_GL_NO_ERROR();

        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// The test verifies that glDraw*:
// 1) generates an INVALID_OPERATION error if the number of views in the active draw framebuffer and
// program differs.
// 2) does not generate any error if the number of views is the same.
TEST_P(MultiviewDrawValidationTest, NumViewsMismatch)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const GLint viewportOffsets[4] = {0, 0, 2, 0};

    const std::string &vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{}\n";
    const std::string &fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "void main()\n"
        "{}\n";
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    // Check for a GL_INVALID_OPERATION error with the framebuffer and program having different
    // number of views.
    {
        // The framebuffer has only 1 view.
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 1, &viewportOffsets[0]);

        glDrawArrays(GL_TRIANGLES, 0, 3);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    // Check that no errors are generated if the number of views in both program and draw
    // framebuffer matches.
    {
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 2, &viewportOffsets[0]);

        glDrawArrays(GL_TRIANGLES, 0, 3);
        EXPECT_GL_NO_ERROR();

        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// The test verifies that glDraw* generates an INVALID_OPERATION error if the program does not use
// the multiview extension, but the active draw framebuffer has more than one view.
TEST_P(MultiviewDrawValidationTest, NumViewsMismatchForNonMultiviewProgram)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string &vsSourceNoMultiview =
        "#version 300 es\n"
        "void main()\n"
        "{}\n";
    const std::string &fsSourceNoMultiview =
        "#version 300 es\n"
        "precision mediump float;\n"
        "void main()\n"
        "{}\n";
    ANGLE_GL_PROGRAM(programNoMultiview, vsSourceNoMultiview, fsSourceNoMultiview);
    glUseProgram(programNoMultiview);

    const GLint viewportOffsets[4] = {0, 0, 2, 0};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d, 0, 2,
                                                 &viewportOffsets[0]);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// The test verifies that glDraw*:
// 1) generates an INVALID_OPERATION error if the number of views in the active draw framebuffer is
// greater than 1 and there is an active transform feedback object.
// 2) does not generate any error if the number of views in the draw framebuffer is 1.
TEST_P(MultiviewDrawValidationTest, ActiveTransformFeedback)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const GLint viewportOffsets[4] = {0, 0, 2, 0};

    const std::string &vsSource =
        R"(#version 300 es
        out float tfVarying;
        void main()
        {
            tfVarying = 1.0;
        })";
    const std::string &fsSource =
        R"(#version 300 es
        precision mediump float;
        void main()
        {})";
    std::vector<std::string> tfVaryings;
    tfVaryings.push_back(std::string("tfVarying"));
    ANGLE_GL_PROGRAM_TRANSFORM_FEEDBACK(program, vsSource, fsSource, tfVaryings,
                                        GL_SEPARATE_ATTRIBS);
    glUseProgram(program);

    GLBuffer tbo;
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, tbo);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(float) * 4u, nullptr, GL_STATIC_DRAW);

    GLTransformFeedback transformFeedback;
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedback);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbo);

    glBeginTransformFeedback(GL_TRIANGLES);
    ASSERT_GL_NO_ERROR();

    // Check that drawArrays generates an error when there is an active transform feedback object
    // and the number of views in the draw framebuffer is greater than 1.
    {
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 2, &viewportOffsets[0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    // Check that drawArrays does not generate an error when the number of views in the draw
    // framebuffer is 1.
    {
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 1, &viewportOffsets[0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        EXPECT_GL_NO_ERROR();
    }

    glEndTransformFeedback();
}

// The test verifies that glDraw*:
// 1) generates an INVALID_OPERATION error if the number of views in the active draw framebuffer is
// greater than 1 and there is an active query for target GL_TIME_ELAPSED_EXT.
// 2) does not generate any error if the number of views in the draw framebuffer is 1.
TEST_P(MultiviewDrawValidationTest, ActiveTimeElapsedQuery)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    if (!extensionEnabled("GL_EXT_disjoint_timer_query"))
    {
        std::cout << "Test skipped because GL_EXT_disjoint_timer_query is not available."
                  << std::endl;
        return;
    }

    const GLint viewportOffsets[4] = {0, 0, 2, 0};
    const std::string &vsSource =
        "#version 300 es\n"
        "void main()\n"
        "{}\n";
    const std::string &fsSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "void main()\n"
        "{}\n";
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    GLuint query = 0u;
    glGenQueriesEXT(1, &query);
    glBeginQueryEXT(GL_TIME_ELAPSED_EXT, query);

    // Check first case.
    {
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 2, &viewportOffsets[0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    // Check second case.
    {
        glFramebufferTextureMultiviewSideBySideANGLE(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTex2d,
                                                     0, 1, &viewportOffsets[0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        EXPECT_GL_NO_ERROR();
    }

    glEndQueryEXT(GL_TIME_ELAPSED_EXT);
    glDeleteQueries(1, &query);
}

// The test checks that glDrawArrays can be used to render into two views.
TEST_P(MultiviewRenderDualViewTest, DrawArrays)
{
    if (!requestMultiviewExtension())
    {
        return;
    }
    drawQuad(mProgram, "vPosition", 0.0f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    checkOutput();
}

// The test checks that glDrawElements can be used to render into two views.
TEST_P(MultiviewRenderDualViewTest, DrawElements)
{
    if (!requestMultiviewExtension())
    {
        return;
    }
    drawIndexedQuad(mProgram, "vPosition", 0.0f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    checkOutput();
}

// The test checks that glDrawRangeElements can be used to render into two views.
TEST_P(MultiviewRenderDualViewTest, DrawRangeElements)
{
    if (!requestMultiviewExtension())
    {
        return;
    }
    drawIndexedQuad(mProgram, "vPosition", 0.0f, 1.0f, true, true);
    ASSERT_GL_NO_ERROR();

    checkOutput();
}

// The test checks that glDrawArrays can be used to render into four views.
TEST_P(MultiviewRenderTest, DrawArraysFourViews)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 4) in;\n"
        "in vec4 vPosition;\n"
        "void main()\n"
        "{\n"
        "   if (gl_ViewID_OVR == 0u) {\n"
        "       gl_Position.x = vPosition.x*0.25 - 0.75;\n"
        "   } else if (gl_ViewID_OVR == 1u) {\n"
        "       gl_Position.x = vPosition.x*0.25 - 0.25;\n"
        "   } else if (gl_ViewID_OVR == 2u) {\n"
        "       gl_Position.x = vPosition.x*0.25 + 0.25;\n"
        "   } else {\n"
        "       gl_Position.x = vPosition.x*0.25 + 0.75;\n"
        "   }"
        "   gl_Position.yzw = vPosition.yzw;\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(0,1,0,1);\n"
        "}\n";

    createFBO(4, 1, 4);
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);

    drawQuad(program, "vPosition", 0.0f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            if (i == j)
            {
                EXPECT_EQ(GLColor::green, GetViewColor(j, 0, i));
            }
            else
            {
                EXPECT_EQ(GLColor::black, GetViewColor(j, 0, i));
            }
        }
    }
    EXPECT_GL_NO_ERROR();
}

// The test checks that glDrawArraysInstanced can be used to render into two views.
TEST_P(MultiviewRenderTest, DrawArraysInstanced)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "in vec4 vPosition;\n"
        "void main()\n"
        "{\n"
        "       vec4 p = vPosition;\n"
        "       if (gl_InstanceID == 1){\n"
        "               p.y = .5*p.y + .5;\n"
        "       } else {\n"
        "               p.y = p.y*.5;\n"
        "       }\n"
        "       gl_Position.x = (gl_ViewID_OVR == 0u ? p.x*0.5 + 0.5 : p.x*0.5);\n"
        "       gl_Position.yzw = p.yzw;\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(0,1,0,1);\n"
        "}\n";

    const int kViewWidth  = 2;
    const int kViewHeight = 2;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);

    drawQuadInstanced(program, "vPosition", 0.0f, 1.0f, true, 2u);
    ASSERT_GL_NO_ERROR();

    const GLubyte expectedGreenChannel[kNumViews][kViewHeight][kViewWidth] = {{{0, 255}, {0, 255}},
                                                                              {{255, 0}, {255, 0}}};

    for (int view = 0; view < 2; ++view)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                EXPECT_EQ(GLColor(0, expectedGreenChannel[view][y][x], 0, 255),
                          GetViewColor(x, y, view));
            }
        }
    }
}

// The test verifies that the attribute divisor is correctly adjusted when drawing with a multi-view
// program. The test draws 4 instances of a quad each of which covers a single pixel. The x and y
// offset of each quad are passed as separate attributes which are indexed based on the
// corresponding attribute divisors. A divisor of 1 is used for the y offset to have all quads
// drawn vertically next to each other. A divisor of 3 is used for the x offset to have the last
// quad offsetted by one pixel to the right. Note that the number of views is divisible by 1, but
// not by 3.
TEST_P(MultiviewRenderTest, AttribDivisor)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string &vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "in vec3 vPosition;\n"
        "in float offsetX;\n"
        "in float offsetY;\n"
        "void main()\n"
        "{\n"
        "       vec4 p = vec4(vPosition, 1.);\n"
        "       p.xy = p.xy * 0.25 - vec2(0.75) + vec2(offsetX, offsetY);\n"
        "       gl_Position.x = (gl_ViewID_OVR == 0u ? p.x : p.x + 1.0);\n"
        "       gl_Position.yzw = p.yzw;\n"
        "}\n";

    const std::string &fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(0,1,0,1);\n"
        "}\n";

    const int kViewWidth  = 4;
    const int kViewHeight = 4;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);

    GLBuffer xOffsetVBO;
    glBindBuffer(GL_ARRAY_BUFFER, xOffsetVBO);
    const GLfloat xOffsetData[4] = {0.0f, 0.5f, 1.0f, 1.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4, xOffsetData, GL_STATIC_DRAW);
    GLint xOffsetLoc = glGetAttribLocation(program, "offsetX");
    glVertexAttribPointer(xOffsetLoc, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribDivisor(xOffsetLoc, 3);
    glEnableVertexAttribArray(xOffsetLoc);

    GLBuffer yOffsetVBO;
    glBindBuffer(GL_ARRAY_BUFFER, yOffsetVBO);
    const GLfloat yOffsetData[4] = {0.0f, 0.5f, 1.0f, 1.5f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4, yOffsetData, GL_STATIC_DRAW);
    GLint yOffsetLoc = glGetAttribLocation(program, "offsetY");
    glVertexAttribDivisor(yOffsetLoc, 1);
    glVertexAttribPointer(yOffsetLoc, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(yOffsetLoc);

    drawQuadInstanced(program, "vPosition", 0.0f, 1.0f, true, 4u);
    ASSERT_GL_NO_ERROR();

    const GLubyte expectedGreenChannel[kNumViews][kViewHeight][kViewWidth] = {
        {{255, 0, 0, 0}, {255, 0, 0, 0}, {255, 0, 0, 0}, {0, 255, 0, 0}},
        {{0, 0, 255, 0}, {0, 0, 255, 0}, {0, 0, 255, 0}, {0, 0, 0, 255}}};
    for (int view = 0; view < 2; ++view)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                EXPECT_EQ(GLColor(0, expectedGreenChannel[view][row][col], 0, 255),
                          GetViewColor(col, row, view));
            }
        }
    }
}

// Test that different sequences of vertexAttribDivisor, useProgram and bindVertexArray in a
// multi-view context propagate the correct divisor to the driver.
TEST_P(MultiviewRenderTest, DivisorOrderOfOperation)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    createFBO(1, 1, 2);

    // Create multiview program.
    const std::string &vs =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "layout(location = 0) in vec2 vPosition;\n"
        "layout(location = 1) in float offsetX;\n"
        "void main()\n"
        "{\n"
        "       vec4 p = vec4(vPosition, 0.0, 1.0);\n"
        "       p.x += offsetX;\n"
        "       gl_Position = p;\n"
        "}\n";

    const std::string &fs =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(0,1,0,1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vs, fs);

    const std::string &dummyVS =
        "#version 300 es\n"
        "layout(location = 0) in vec2 vPosition;\n"
        "layout(location = 1) in float offsetX;\n"
        "void main()\n"
        "{\n"
        "       gl_Position = vec4(vPosition, 0.0, 1.0);\n"
        "}\n";

    const std::string &dummyFS =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(0,0,0,1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(dummyProgram, dummyVS, dummyFS);

    GLBuffer xOffsetVBO;
    glBindBuffer(GL_ARRAY_BUFFER, xOffsetVBO);
    const GLfloat xOffsetData[12] = {0.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f,
                                     4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, xOffsetData, GL_STATIC_DRAW);

    GLBuffer vertexVBO;
    glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
    Vector2 kQuadVertices[6] = {Vector2(-1.f, -1.f), Vector2(1.f, -1.f), Vector2(1.f, 1.f),
                                Vector2(-1.f, -1.f), Vector2(1.f, 1.f),  Vector2(-1.f, 1.f)};
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    GLVertexArray vao[2];
    for (size_t i = 0u; i < 2u; ++i)
    {
        glBindVertexArray(vao[i]);

        glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, xOffsetVBO);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);
    }
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, 1, 1);
    glScissor(0, 0, 1, 1);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);

    // Clear the buffers, propagate divisor to the driver, bind the vao and keep it active.
    // It is necessary to call draw, so that the divisor is propagated and to guarantee that dirty
    // bits are cleared.
    glUseProgram(dummyProgram);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(vao[0]);
    glVertexAttribDivisor(1, 0);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    glUseProgram(0);
    ASSERT_GL_NO_ERROR();

    // Check that vertexAttribDivisor uses the number of views to update the divisor.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glVertexAttribDivisor(1, 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 0));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));

    // Clear the buffers and propagate divisor to the driver.
    // We keep the vao active and propagate the divisor to guarantee that there are no unresolved
    // dirty bits when useProgram is called.
    glUseProgram(dummyProgram);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glVertexAttribDivisor(1, 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    glUseProgram(0);
    ASSERT_GL_NO_ERROR();

    // Check that useProgram uses the number of views to update the divisor.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 0));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));

    // We go through similar steps as before.
    glUseProgram(dummyProgram);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glVertexAttribDivisor(1, 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    glUseProgram(0);
    ASSERT_GL_NO_ERROR();

    // Check that bindVertexArray uses the number of views to update the divisor.
    {
        // Call useProgram with vao[1] being active to guarantee that useProgram will adjust the
        // divisor for vao[1] only.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFramebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(vao[1]);
        glUseProgram(program);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
        glBindVertexArray(0);
        ASSERT_GL_NO_ERROR();
    }
    // Bind vao[0] after useProgram is called to ensure that bindVertexArray is the call which
    // adjusts the divisor.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(vao[0]);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 0));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));
}

// Test that no fragments pass the occlusion query for a multi-view vertex shader which always
// transforms geometry to be outside of the clip region.
TEST_P(MultiviewOcclusionQueryTest, OcclusionQueryNothingVisible)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());
    ANGLE_SKIP_TEST_IF(!requestOcclusionQueryExtension());

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "in vec3 vPosition;\n"
        "void main()\n"
        "{\n"
        "       gl_Position.x = 2.0;\n"
        "       gl_Position.yzw = vec3(vPosition.yz, 1.);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(1,0,0,0);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    createFBO(1, 1, 2);

    GLuint result = drawAndRetrieveOcclusionQueryResult(program);
    ASSERT_GL_NO_ERROR();
    EXPECT_GL_FALSE(result);
}

// Test that there are fragments passing the occlusion query if only view 0 can produce
// output.
TEST_P(MultiviewOcclusionQueryTest, OcclusionQueryOnlyLeftVisible)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());
    ANGLE_SKIP_TEST_IF(!requestOcclusionQueryExtension());

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "in vec3 vPosition;\n"
        "void main()\n"
        "{\n"
        "       gl_Position.x = gl_ViewID_OVR == 0u ? vPosition.x : 2.0;\n"
        "       gl_Position.yzw = vec3(vPosition.yz, 1.);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(1,0,0,0);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    createFBO(1, 1, 2);

    GLuint result = drawAndRetrieveOcclusionQueryResult(program);
    ASSERT_GL_NO_ERROR();
    EXPECT_GL_TRUE(result);
}

// Test that there are fragments passing the occlusion query if only view 1 can produce
// output.
TEST_P(MultiviewOcclusionQueryTest, OcclusionQueryOnlyRightVisible)
{
    ANGLE_SKIP_TEST_IF(!requestMultiviewExtension());
    ANGLE_SKIP_TEST_IF(!requestOcclusionQueryExtension());

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "in vec3 vPosition;\n"
        "void main()\n"
        "{\n"
        "       gl_Position.x = gl_ViewID_OVR == 1u ? vPosition.x : 2.0;\n"
        "       gl_Position.yzw = vec3(vPosition.yz, 1.);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(1,0,0,0);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    createFBO(1, 1, 2);

    GLuint result = drawAndRetrieveOcclusionQueryResult(program);
    ASSERT_GL_NO_ERROR();
    EXPECT_GL_TRUE(result);
}

// Test that a simple multi-view program which doesn't use gl_ViewID_OVR in neither VS nor FS
// compiles and links without an error.
TEST_P(MultiviewProgramGenerationTest, SimpleProgram)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "void main()\n"
        "{\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    EXPECT_GL_NO_ERROR();
}

// Test that a simple multi-view program which uses gl_ViewID_OVR only in VS compiles and links
// without an error.
TEST_P(MultiviewProgramGenerationTest, UseViewIDInVertexShader)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "   if (gl_ViewID_OVR == 0u) {\n"
        "       gl_Position = vec4(1,0,0,1);\n"
        "   } else {\n"
        "       gl_Position = vec4(-1,0,0,1);\n"
        "   }\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "void main()\n"
        "{\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    EXPECT_GL_NO_ERROR();
}

// Test that a simple multi-view program which uses gl_ViewID_OVR only in FS compiles and links
// without an error.
TEST_P(MultiviewProgramGenerationTest, UseViewIDInFragmentShader)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "   if (gl_ViewID_OVR == 0u) {\n"
        "       col = vec4(1,0,0,1);\n"
        "   } else {\n"
        "       col = vec4(-1,0,0,1);\n"
        "   }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    EXPECT_GL_NO_ERROR();
}

// The test checks that GL_POINTS is correctly rendered.
TEST_P(MultiviewRenderPrimitiveTest, Points)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    // Test failing on P400 graphics card (anglebug.com/2228)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D11() && IsNVIDIA());

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "layout(location=0) in vec2 vPosition;\n"
        "void main()\n"
        "{\n"
        "   gl_PointSize = 1.0;\n"
        "   gl_Position = vec4(vPosition.xy, 0.0, 1.0);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "   col = vec4(0,1,0,1);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    const int kViewWidth  = 4;
    const int kViewHeight = 2;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);

    std::vector<Vector2I> windowCoordinates = {Vector2I(0, 0), Vector2I(3, 1)};
    std::vector<Vector2> vertexDataInClipSpace =
        ConvertPixelCoordinatesToClipSpace(windowCoordinates, 4, 2);
    setupGeometry(vertexDataInClipSpace);

    glDrawArrays(GL_POINTS, 0, 2);

    const GLubyte expectedGreenChannelData[kNumViews][kViewHeight][kViewWidth] = {
        {{255, 0, 0, 0}, {0, 0, 0, 255}}, {{255, 0, 0, 0}, {0, 0, 0, 255}}};
    checkGreenChannel(expectedGreenChannelData[0][0]);
}

// The test checks that GL_LINES is correctly rendered.
// The behavior of this test is not guaranteed by the spec:
// OpenGL ES 3.0.5 (November 3, 2016), Section 3.5.1 Basic Line Segment Rasterization:
// "The coordinates of a fragment produced by the algorithm may not deviate by more than one unit in
// either x or y window coordinates from a corresponding fragment produced by the diamond-exit
// rule."
TEST_P(MultiviewRenderPrimitiveTest, Lines)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    GLuint program = CreateSimplePassthroughProgram(2);
    ASSERT_NE(program, 0u);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    const int kViewWidth  = 4;
    const int kViewHeight = 2;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);

    std::vector<Vector2I> windowCoordinates = {Vector2I(0, 0), Vector2I(4, 0)};
    std::vector<Vector2> vertexDataInClipSpace =
        ConvertPixelCoordinatesToClipSpace(windowCoordinates, 4, 2);
    setupGeometry(vertexDataInClipSpace);

    glDrawArrays(GL_LINES, 0, 2);

    const GLubyte expectedGreenChannelData[kNumViews][kViewHeight][kViewWidth] = {
        {{255, 255, 255, 255}, {0, 0, 0, 0}}, {{255, 255, 255, 255}, {0, 0, 0, 0}}};
    checkGreenChannel(expectedGreenChannelData[0][0]);

    glDeleteProgram(program);
}

// The test checks that GL_LINE_STRIP is correctly rendered.
// The behavior of this test is not guaranteed by the spec:
// OpenGL ES 3.0.5 (November 3, 2016), Section 3.5.1 Basic Line Segment Rasterization:
// "The coordinates of a fragment produced by the algorithm may not deviate by more than one unit in
// either x or y window coordinates from a corresponding fragment produced by the diamond-exit
// rule."
TEST_P(MultiviewRenderPrimitiveTest, LineStrip)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    GLuint program = CreateSimplePassthroughProgram(2);
    ASSERT_NE(program, 0u);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    const int kViewWidth  = 4;
    const int kViewHeight = 2;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);

    std::vector<Vector2I> windowCoordinates = {Vector2I(0, 0), Vector2I(3, 0), Vector2I(3, 2)};
    std::vector<Vector2> vertexDataInClipSpace =
        ConvertPixelCoordinatesToClipSpace(windowCoordinates, 4, 2);
    setupGeometry(vertexDataInClipSpace);

    glDrawArrays(GL_LINE_STRIP, 0, 3);

    const GLubyte expectedGreenChannelData[kNumViews][kViewHeight][kViewWidth] = {
        {{255, 255, 255, 255}, {0, 0, 0, 255}}, {{255, 255, 255, 255}, {0, 0, 0, 255}}};
    checkGreenChannel(expectedGreenChannelData[0][0]);

    glDeleteProgram(program);
}

// The test checks that GL_LINE_LOOP is correctly rendered.
// The behavior of this test is not guaranteed by the spec:
// OpenGL ES 3.0.5 (November 3, 2016), Section 3.5.1 Basic Line Segment Rasterization:
// "The coordinates of a fragment produced by the algorithm may not deviate by more than one unit in
// either x or y window coordinates from a corresponding fragment produced by the diamond-exit
// rule."
TEST_P(MultiviewRenderPrimitiveTest, LineLoop)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    GLuint program = CreateSimplePassthroughProgram(2);
    ASSERT_NE(program, 0u);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    const int kViewWidth  = 4;
    const int kViewHeight = 4;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);

    std::vector<Vector2I> windowCoordinates = {Vector2I(0, 0), Vector2I(3, 0), Vector2I(3, 3),
                                               Vector2I(0, 3)};
    std::vector<Vector2> vertexDataInClipSpace =
        ConvertPixelCoordinatesToClipSpace(windowCoordinates, 4, 4);
    setupGeometry(vertexDataInClipSpace);

    glDrawArrays(GL_LINE_LOOP, 0, 4);

    const GLubyte expectedGreenChannelData[kNumViews][kViewHeight][kViewWidth] = {
        {{255, 255, 255, 255}, {255, 0, 0, 255}, {255, 0, 0, 255}, {255, 255, 255, 255}},
        {{255, 255, 255, 255}, {255, 0, 0, 255}, {255, 0, 0, 255}, {255, 255, 255, 255}}};
    checkGreenChannel(expectedGreenChannelData[0][0]);

    glDeleteProgram(program);
}

// The test checks that GL_TRIANGLE_STRIP is correctly rendered.
TEST_P(MultiviewRenderPrimitiveTest, TriangleStrip)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    GLuint program = CreateSimplePassthroughProgram(2);
    ASSERT_NE(program, 0u);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    std::vector<Vector2> vertexDataInClipSpace = {Vector2(1.0f, 0.0f), Vector2(0.0f, 0.0f),
                                                  Vector2(1.0f, 1.0f), Vector2(0.0f, 1.0f)};
    setupGeometry(vertexDataInClipSpace);

    const int kViewWidth  = 2;
    const int kViewHeight = 2;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    const GLubyte expectedGreenChannelData[kNumViews][kViewHeight][kViewWidth] = {
        {{0, 0}, {0, 255}}, {{0, 0}, {0, 255}}};
    checkGreenChannel(expectedGreenChannelData[0][0]);

    glDeleteProgram(program);
}

// The test checks that GL_TRIANGLE_FAN is correctly rendered.
TEST_P(MultiviewRenderPrimitiveTest, TriangleFan)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    GLuint program = CreateSimplePassthroughProgram(2);
    ASSERT_NE(program, 0u);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    std::vector<Vector2> vertexDataInClipSpace = {Vector2(0.0f, 0.0f), Vector2(0.0f, 1.0f),
                                                  Vector2(1.0f, 1.0f), Vector2(1.0f, 0.0f)};
    setupGeometry(vertexDataInClipSpace);

    const int kViewWidth  = 2;
    const int kViewHeight = 2;
    const int kNumViews   = 2;
    createFBO(kViewWidth, kViewHeight, kNumViews);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    const GLubyte expectedGreenChannelData[kNumViews][kViewHeight][kViewWidth] = {
        {{0, 0}, {0, 255}}, {{0, 0}, {0, 255}}};
    checkGreenChannel(expectedGreenChannelData[0][0]);

    glDeleteProgram(program);
}

// Test that rendering enlarged points and lines does not leak fragments outside of the views'
// bounds. The test does not rely on the actual line width being greater than 1.0.
TEST_P(MultiviewSideBySideRenderTest, NoLeakingFragments)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    createFBO(2, 1, 2);

    GLint viewportOffsets[4] = {1, 0, 3, 0};
    glFramebufferTextureMultiviewSideBySideANGLE(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                 mColorTexture, 0, 2, &viewportOffsets[0]);
    glFramebufferTextureMultiviewSideBySideANGLE(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                 mDepthTexture, 0, 2, &viewportOffsets[0]);

    glViewport(0, 0, 1, 1);
    glScissor(0, 0, 1, 1);
    glEnable(GL_SCISSOR_TEST);

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "layout(location=0) in vec2 vPosition;\n"
        "void main()\n"
        "{\n"
        "   gl_PointSize = 10.0;\n"
        "   gl_Position = vec4(vPosition.xy, 0.0, 1.0);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "   if (gl_ViewID_OVR == 0u) {\n"
        "       col = vec4(1,0,0,1);\n"
        "   } else {\n"
        "       col = vec4(0,1,0,1);\n"
        "   }\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    const std::vector<Vector2I> &windowCoordinates = {Vector2I(0, 0), Vector2I(2, 0)};
    const std::vector<Vector2> &vertexDataInClipSpace =
        ConvertPixelCoordinatesToClipSpace(windowCoordinates, 1, 1);

    GLBuffer vbo;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexDataInClipSpace.size() * sizeof(Vector2),
                 vertexDataInClipSpace.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    // Test rendering points.
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_POINTS, 0, 2);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
        EXPECT_PIXEL_COLOR_EQ(1, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(2, 0, GLColor::black);
        EXPECT_PIXEL_COLOR_EQ(3, 0, GLColor::green);
    }

    // Test rendering lines.
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLineWidth(10.f);
        glDrawArrays(GL_LINES, 0, 2);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
        EXPECT_PIXEL_COLOR_EQ(1, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(2, 0, GLColor::black);
        EXPECT_PIXEL_COLOR_EQ(3, 0, GLColor::green);
    }
}

// Verify that re-linking a program adjusts the attribute divisor.
// The test uses instacing to draw for each view a strips of two red quads and two blue quads next
// to each other. The quads' position and color depend on the corresponding attribute divisors.
TEST_P(MultiviewRenderTest, ProgramRelinkUpdatesAttribDivisor)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const int kViewWidth  = 4;
    const int kViewHeight = 1;
    const int kNumViews   = 2;

    const std::string &fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "in vec4 oColor;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = oColor;\n"
        "}\n";

    auto generateVertexShaderSource = [](int numViews) -> std::string {
        std::string source =
            "#version 300 es\n"
            "#extension GL_OVR_multiview : require\n"
            "layout(num_views = " +
            ToString(numViews) +
            ") in;\n"
            "in vec3 vPosition;\n"
            "in float vOffsetX;\n"
            "in vec4 vColor;\n"
            "out vec4 oColor;\n"
            "void main()\n"
            "{\n"
            "       vec4 p = vec4(vPosition, 1.);\n"
            "       p.x = p.x * 0.25 - 0.75 + vOffsetX;\n"
            "       oColor = vColor;\n"
            "       gl_Position = p;\n"
            "}\n";
        return source;
    };

    std::string vsSource = generateVertexShaderSource(kNumViews);
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);
    glUseProgram(program);

    GLint positionLoc;
    GLBuffer xOffsetVBO;
    GLint xOffsetLoc;
    GLBuffer colorVBO;
    GLint colorLoc;

    {
        // Initialize buffers and setup attributes.
        glBindBuffer(GL_ARRAY_BUFFER, xOffsetVBO);
        const GLfloat kXOffsetData[4] = {0.0f, 0.5f, 1.0f, 1.5f};
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4, kXOffsetData, GL_STATIC_DRAW);
        xOffsetLoc = glGetAttribLocation(program, "vOffsetX");
        glVertexAttribPointer(xOffsetLoc, 1, GL_FLOAT, GL_FALSE, 0, 0);
        glVertexAttribDivisor(xOffsetLoc, 1);
        glEnableVertexAttribArray(xOffsetLoc);

        glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
        const GLColor kColors[2] = {GLColor::red, GLColor::blue};
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLColor) * 2, kColors, GL_STATIC_DRAW);
        colorLoc = glGetAttribLocation(program, "vColor");
        glVertexAttribDivisor(colorLoc, 2);
        glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(colorLoc);

        positionLoc = glGetAttribLocation(program, "vPosition");
    }

    {
        createFBO(kViewWidth, kViewHeight, kNumViews);

        drawQuadInstanced(program, "vPosition", 0.0f, 1.0f, true, 4u);
        ASSERT_GL_NO_ERROR();

        EXPECT_EQ(GLColor::red, GetViewColor(0, 0, 0));
        EXPECT_EQ(GLColor::red, GetViewColor(1, 0, 0));
        EXPECT_EQ(GLColor::blue, GetViewColor(2, 0, 0));
        EXPECT_EQ(GLColor::blue, GetViewColor(3, 0, 0));
    }

    {
        const int kNewNumViews = 3;
        vsSource               = generateVertexShaderSource(kNewNumViews);
        createFBO(kViewWidth, kViewHeight, kNewNumViews);

        GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
        ASSERT_NE(0u, vs);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
        ASSERT_NE(0u, fs);

        GLint numAttachedShaders = 0;
        glGetProgramiv(program, GL_ATTACHED_SHADERS, &numAttachedShaders);

        GLuint attachedShaders[2] = {0u};
        glGetAttachedShaders(program, numAttachedShaders, nullptr, attachedShaders);
        for (int i = 0; i < 2; ++i)
        {
            glDetachShader(program, attachedShaders[i]);
        }

        glAttachShader(program, vs);
        glDeleteShader(vs);

        glAttachShader(program, fs);
        glDeleteShader(fs);

        glBindAttribLocation(program, positionLoc, "vPosition");
        glBindAttribLocation(program, xOffsetLoc, "vOffsetX");
        glBindAttribLocation(program, colorLoc, "vColor");

        glLinkProgram(program);

        drawQuadInstanced(program, "vPosition", 0.0f, 1.0f, true, 4u);
        ASSERT_GL_NO_ERROR();

        for (int i = 0; i < kNewNumViews; ++i)
        {
            EXPECT_EQ(GLColor::red, GetViewColor(0, 0, i));
            EXPECT_EQ(GLColor::red, GetViewColor(1, 0, i));
            EXPECT_EQ(GLColor::blue, GetViewColor(2, 0, i));
            EXPECT_EQ(GLColor::blue, GetViewColor(3, 0, i));
        }
    }
}

// Test that useProgram applies the number of views in computing the final value of the attribute
// divisor.
TEST_P(MultiviewRenderTest, DivisorUpdatedOnProgramChange)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    // Test failing on P400 graphics card (anglebug.com/2228)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D11() && IsNVIDIA());

    GLVertexArray vao;
    glBindVertexArray(vao);
    GLBuffer vbo;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    std::vector<Vector2I> windowCoordinates = {Vector2I(0, 0), Vector2I(1, 0), Vector2I(2, 0),
                                               Vector2I(3, 0)};
    std::vector<Vector2> vertexDataInClipSpace =
        ConvertPixelCoordinatesToClipSpace(windowCoordinates, 4, 1);
    // Fill with x positions so that the resulting clip space coordinate fails the clip test.
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vector2) * vertexDataInClipSpace.size(),
                 vertexDataInClipSpace.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, nullptr);
    glVertexAttribDivisor(0, 1);
    ASSERT_GL_NO_ERROR();

    // Create a program and fbo with N views and draw N instances of a point horizontally.
    for (int numViews = 2; numViews <= 4; ++numViews)
    {
        createFBO(4, 1, numViews);
        ASSERT_GL_NO_ERROR();

        GLuint program = CreateSimplePassthroughProgram(numViews);
        ASSERT_NE(program, 0u);
        glUseProgram(program);
        ASSERT_GL_NO_ERROR();

        glDrawArraysInstanced(GL_POINTS, 0, 1, numViews);

        for (int view = 0; view < numViews; ++view)
        {
            for (int j = 0; j < numViews; ++j)
            {
                EXPECT_EQ(GLColor::green, GetViewColor(j, 0, view));
            }
            for (int j = numViews; j < 4; ++j)
            {
                EXPECT_EQ(GLColor::black, GetViewColor(j, 0, view));
            }
        }

        glDeleteProgram(program);
    }
}

// The test checks that gl_ViewID_OVR is correctly propagated to the fragment shader.
TEST_P(MultiviewRenderTest, SelectColorBasedOnViewIDOVR)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 3) in;\n"
        "in vec3 vPosition;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(vPosition, 1.);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    if (gl_ViewID_OVR == 0u) {\n"
        "       col = vec4(1,0,0,1);\n"
        "    } else if (gl_ViewID_OVR == 1u) {\n"
        "       col = vec4(0,1,0,1);\n"
        "    } else if (gl_ViewID_OVR == 2u) {\n"
        "       col = vec4(0,0,1,1);\n"
        "    } else {\n"
        "       col = vec4(0,0,0,0);\n"
        "    }\n"
        "}\n";

    createFBO(1, 1, 3);
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);

    drawQuad(program, "vPosition", 0.0f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    EXPECT_EQ(GLColor::red, GetViewColor(0, 0, 0));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));
    EXPECT_EQ(GLColor::blue, GetViewColor(0, 0, 2));
}

// The test checks that the inactive layers of a 2D texture array are not written to by a
// multi-view program.
TEST_P(MultiviewLayeredRenderTest, RenderToSubrageOfLayers)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "in vec3 vPosition;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(vPosition, 1.);\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "     col = vec4(0,1,0,1);\n"
        "}\n";

    createFBO(1, 1, 2, 4, 1);
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);

    drawQuad(program, "vPosition", 0.0f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    EXPECT_EQ(GLColor::transparentBlack, GetViewColor(0, 0, 0));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 2));
    EXPECT_EQ(GLColor::transparentBlack, GetViewColor(0, 0, 3));
}

// The D3D11 renderer uses a GS whenever the varyings are flat interpolated which can cause
// potential bugs if the view is selected in the VS. The test contains a program in which the
// gl_InstanceID is passed as a flat varying to the fragment shader where it is used to discard the
// fragment if its value is negative. The gl_InstanceID should never be negative and that branch is
// never taken. One quad is drawn and the color is selected based on the ViewID - red for view 0 and
// green for view 1.
TEST_P(MultiviewRenderTest, FlatInterpolation)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    // Test failing on P400 graphics card (anglebug.com/2228)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsD3D11() && IsNVIDIA());

    // TODO(mradev): Find out why this fails on Win10 Intel HD 630 D3D11
    // (http://anglebug.com/2062)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsIntel() && IsD3D11());

    const std::string vsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "in vec3 vPosition;\n"
        "flat out int oInstanceID;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(vPosition, 1.);\n"
        "   oInstanceID = gl_InstanceID;\n"
        "}\n";

    const std::string fsSource =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "flat in int oInstanceID;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    if (oInstanceID < 0) {\n"
        "       discard;\n"
        "    }\n"
        "    if (gl_ViewID_OVR == 0u) {\n"
        "       col = vec4(1,0,0,1);\n"
        "    } else {\n"
        "       col = vec4(0,1,0,1);\n"
        "    }\n"
        "}\n";

    createFBO(1, 1, 2);
    ANGLE_GL_PROGRAM(program, vsSource, fsSource);

    drawQuad(program, "vPosition", 0.0f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    EXPECT_EQ(GLColor::red, GetViewColor(0, 0, 0));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));
}

// The test is added to cover a bug which resulted in the viewport/scissor and viewport offsets not
// being correctly applied.
TEST_P(MultiviewSideBySideRenderTest, ViewportOffsetsAppliedBugCoverage)
{
    if (!requestMultiviewExtension())
    {
        return;
    }

    createFBO(1, 1, 2);

    // Create multiview program.
    const std::string &vs =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "layout(num_views = 2) in;\n"
        "layout(location = 0) in vec3 vPosition;\n"
        "void main()\n"
        "{\n"
        "       gl_Position = vec4(vPosition, 1.0);\n"
        "}\n";

    const std::string &fs =
        "#version 300 es\n"
        "#extension GL_OVR_multiview : require\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(0,1,0,1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vs, fs);

    glViewport(0, 0, 1, 1);
    glScissor(0, 0, 1, 1);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);

    // Bind the default FBO and make sure that the state is synchronized.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Draw and check that both views are rendered to.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawQuad(program, "vPosition", 0.0f, 1.0f, true);
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 0));
    EXPECT_EQ(GLColor::green, GetViewColor(0, 0, 1));
}

MultiviewImplementationParams VertexShaderOpenGL()
{
    return MultiviewImplementationParams(false, egl_platform::OPENGL());
}

MultiviewImplementationParams VertexShaderD3D11()
{
    return MultiviewImplementationParams(false, egl_platform::D3D11());
}

MultiviewImplementationParams GeomShaderD3D11()
{
    return MultiviewImplementationParams(true, egl_platform::D3D11());
}

MultiviewTestParams SideBySideVertexShaderOpenGL()
{
    return MultiviewTestParams(GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE, VertexShaderOpenGL());
}

MultiviewTestParams LayeredVertexShaderOpenGL()
{
    return MultiviewTestParams(GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE, VertexShaderOpenGL());
}

MultiviewTestParams SideBySideGeomShaderD3D11()
{
    return MultiviewTestParams(GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE, GeomShaderD3D11());
}

MultiviewTestParams LayeredGeomShaderD3D11()
{
    return MultiviewTestParams(GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE, GeomShaderD3D11());
}

MultiviewTestParams SideBySideVertexShaderD3D11()
{
    return MultiviewTestParams(GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE, VertexShaderD3D11());
}

MultiviewTestParams LayeredVertexShaderD3D11()
{
    return MultiviewTestParams(GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE, VertexShaderD3D11());
}

ANGLE_INSTANTIATE_TEST(MultiviewDrawValidationTest, ES31_OPENGL());
ANGLE_INSTANTIATE_TEST(MultiviewRenderDualViewTest,
                       SideBySideVertexShaderOpenGL(),
                       LayeredVertexShaderOpenGL(),
                       SideBySideGeomShaderD3D11(),
                       SideBySideVertexShaderD3D11(),
                       LayeredGeomShaderD3D11(),
                       LayeredVertexShaderD3D11());
ANGLE_INSTANTIATE_TEST(MultiviewRenderTest,
                       SideBySideVertexShaderOpenGL(),
                       LayeredVertexShaderOpenGL(),
                       SideBySideGeomShaderD3D11(),
                       SideBySideVertexShaderD3D11(),
                       LayeredGeomShaderD3D11(),
                       LayeredVertexShaderD3D11());
ANGLE_INSTANTIATE_TEST(MultiviewOcclusionQueryTest,
                       SideBySideVertexShaderOpenGL(),
                       LayeredVertexShaderOpenGL(),
                       SideBySideGeomShaderD3D11(),
                       SideBySideVertexShaderD3D11(),
                       LayeredGeomShaderD3D11(),
                       LayeredVertexShaderD3D11());
ANGLE_INSTANTIATE_TEST(MultiviewProgramGenerationTest,
                       SideBySideVertexShaderOpenGL(),
                       LayeredVertexShaderOpenGL(),
                       SideBySideGeomShaderD3D11(),
                       SideBySideVertexShaderD3D11(),
                       LayeredGeomShaderD3D11(),
                       LayeredVertexShaderD3D11());
ANGLE_INSTANTIATE_TEST(MultiviewRenderPrimitiveTest,
                       SideBySideVertexShaderOpenGL(),
                       LayeredVertexShaderOpenGL(),
                       SideBySideGeomShaderD3D11(),
                       SideBySideVertexShaderD3D11(),
                       LayeredGeomShaderD3D11(),
                       LayeredVertexShaderD3D11());
ANGLE_INSTANTIATE_TEST(MultiviewSideBySideRenderTest, VertexShaderOpenGL(), GeomShaderD3D11());
ANGLE_INSTANTIATE_TEST(MultiviewLayeredRenderTest, VertexShaderOpenGL(), GeomShaderD3D11());
