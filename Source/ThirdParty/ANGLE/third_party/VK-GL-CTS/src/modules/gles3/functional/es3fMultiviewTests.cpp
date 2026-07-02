/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file
 * \brief Multiview tests.
 * Tests functionality provided by the three multiview extensions.
 */ /*--------------------------------------------------------------------*/

#include "es3fMultiviewTests.hpp"

#include "deString.h"
#include "deStringUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "glw.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"

using tcu::TestLog;
using tcu::Vec4;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const int NUM_CASE_ITERATIONS = 1;
static const float UNIT_SQUARE[16]   = {
    1.0f,  1.0f,  0.05f, 1.0f, // Vertex 0
    1.0f,  -1.0f, 0.05f, 1.0f, // Vertex 1
    -1.0f, 1.0f,  0.05f, 1.0f, // Vertex 2
    -1.0f, -1.0f, 0.05f, 1.0f  // Vertex 3
};
static const float COLOR_VALUES[] = {
    1, 0, 0, 1, // Red for level 0
    0, 1, 0, 1, // Green for level 1
};

class MultiviewCase : public TestCase
{
public:
    MultiviewCase(Context &context, const char *name, const char *description, int numSamples);
    ~MultiviewCase();
    void init();
    void deinit();
    IterateResult iterate();

private:
    MultiviewCase(const MultiviewCase &other);
    MultiviewCase &operator=(const MultiviewCase &other);
    void setupFramebufferObjects();
    void deleteFramebufferObjects();

    glu::ShaderProgram *m_multiviewProgram;
    uint32_t m_multiviewFbo;
    uint32_t m_arrayTexture;

    glu::ShaderProgram *m_finalProgram;

    int m_caseIndex;
    const int m_numSamples;
    const int m_width;
    const int m_height;
};

MultiviewCase::MultiviewCase(Context &context, const char *name, const char *description, int numSamples)
    : TestCase(context, name, description)
    , m_multiviewProgram(nullptr)
    , m_multiviewFbo(0)
    , m_arrayTexture(0)
    , m_finalProgram(nullptr)
    , m_caseIndex(0)
    , m_numSamples(numSamples)
    , m_width(512)
    , m_height(512)
{
}

MultiviewCase::~MultiviewCase()
{
    MultiviewCase::deinit();
}

void MultiviewCase::setupFramebufferObjects()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // First create the array texture and multiview FBO.

    gl.genTextures(1, &m_arrayTexture);
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_arrayTexture);
    gl.texStorage3D(GL_TEXTURE_2D_ARRAY, 1 /* num mipmaps */, GL_RGBA8, m_width / 2, m_height, 2 /* num levels */);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Create array texture");

    gl.genFramebuffers(1, &m_multiviewFbo);
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_multiviewFbo);
    if (m_numSamples == 1)
    {
        gl.framebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_arrayTexture, 0 /* mip level */,
                                          0 /* base view index */, 2 /* num views */);
    }
    else
    {
        gl.framebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_arrayTexture,
                                                     0 /* mip level */, m_numSamples /* samples */,
                                                     0 /* base view index */, 2 /* num views */);
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "Create multiview FBO");
    uint32_t fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus == GL_FRAMEBUFFER_UNSUPPORTED)
    {
        throw tcu::NotSupportedError("Framebuffer unsupported", "", __FILE__, __LINE__);
    }
    else if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        throw tcu::TestError("Failed to create framebuffer object", "", __FILE__, __LINE__);
    }
}

void MultiviewCase::deleteFramebufferObjects()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.deleteTextures(1, &m_arrayTexture);
    gl.deleteFramebuffers(1, &m_multiviewFbo);
}

void MultiviewCase::init()
{
    const glu::ContextInfo &contextInfo = m_context.getContextInfo();
    bool mvsupported                    = contextInfo.isExtensionSupported("GL_OVR_multiview");
    if (!mvsupported)
    {
        TCU_THROW(NotSupportedError, "Multiview is not supported");
    }

    if (m_numSamples > 1)
    {
        bool msaasupported = contextInfo.isExtensionSupported("GL_OVR_multiview_multisampled_render_to_texture");
        if (!msaasupported)
        {
            TCU_THROW(NotSupportedError, "Implicit MSAA multiview is not supported");
        }
    }

    const char *multiviewVertexShader = "#version 300 es\n"
                                        "#extension GL_OVR_multiview : enable\n"
                                        "layout(num_views=2) in;\n"
                                        "layout(location = 0) in mediump vec4 a_position;\n"
                                        "uniform mediump vec4 uColor[2];\n"
                                        "out mediump vec4 vColor;\n"
                                        "void main() {\n"
                                        "  vColor = uColor[gl_ViewID_OVR];\n"
                                        "  gl_Position = a_position;\n"
                                        "}\n";

    const char *multiviewFragmentShader = "#version 300 es\n"
                                          "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                          "in mediump vec4 vColor;\n"
                                          "void main() {\n"
                                          "  dEQP_FragColor = vColor;\n"
                                          "}\n";

    m_multiviewProgram = new glu::ShaderProgram(
        m_context.getRenderContext(), glu::makeVtxFragSources(multiviewVertexShader, multiviewFragmentShader));
    DE_ASSERT(m_multiviewProgram);
    if (!m_multiviewProgram->isOk())
    {
        m_testCtx.getLog() << *m_multiviewProgram;
        TCU_FAIL("Failed to compile multiview shader");
    }

    // Draw the first layer on the left half of the screen and the second layer
    // on the right half.
    const char *finalVertexShader = "#version 300 es\n"
                                    "layout(location = 0) in mediump vec4 a_position;\n"
                                    "out highp vec3 vTexCoord;\n"
                                    "void main() {\n"
                                    "  vTexCoord.x = fract(a_position.x + 1.0);\n"
                                    "  vTexCoord.y = .5 * (a_position.y + 1.0);\n"
                                    "  vTexCoord.z = a_position.x;\n"
                                    "  gl_Position = a_position;\n"
                                    "}\n";

    const char *finalFragmentShader = "#version 300 es\n"
                                      "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                      "uniform lowp sampler2DArray uArrayTexture;\n"
                                      "in highp vec3 vTexCoord;\n"
                                      "void main() {\n"
                                      "  highp vec3 uvw = vTexCoord;\n"
                                      "  uvw.z = floor(vTexCoord.z + 1.0);\n"
                                      "  dEQP_FragColor = texture(uArrayTexture, uvw);\n"
                                      "}\n";

    m_finalProgram = new glu::ShaderProgram(m_context.getRenderContext(),
                                            glu::makeVtxFragSources(finalVertexShader, finalFragmentShader));
    DE_ASSERT(m_finalProgram);
    if (!m_finalProgram->isOk())
    {
        m_testCtx.getLog() << *m_finalProgram;
        TCU_FAIL("Failed to compile final shader");
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    GLU_CHECK_MSG("Case initialization finished");
}

void MultiviewCase::deinit()
{
    deleteFramebufferObjects();
    delete m_multiviewProgram;
    m_multiviewProgram = nullptr;
    delete m_finalProgram;
    m_finalProgram = nullptr;
}

MultiviewCase::IterateResult MultiviewCase::iterate()
{
    TestLog &log          = m_testCtx.getLog();
    uint32_t colorUniform = glGetUniformLocation(m_multiviewProgram->getProgram(), "uColor");
    std::string header = "Case iteration " + de::toString(m_caseIndex + 1) + " / " + de::toString(NUM_CASE_ITERATIONS);
    log << TestLog::Section(header, header);

    DE_ASSERT(m_multiviewProgram);

    // Create and bind the multiview FBO.

    try
    {
        setupFramebufferObjects();
    }
    catch (tcu::NotSupportedError &e)
    {
        log << TestLog::Message << "ERROR: " << e.what() << "." << TestLog::EndMessage << TestLog::EndSection;
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
        return STOP;
    }
    catch (tcu::InternalError &e)
    {
        log << TestLog::Message << "ERROR: " << e.what() << "." << TestLog::EndMessage << TestLog::EndSection;
        m_testCtx.setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, "Error");
        return STOP;
    }

    log << TestLog::EndSection;

    // Draw full screen quad into the multiview framebuffer.
    // The quad should be instanced into both layers of the array texture.

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_multiviewFbo);
    gl.viewport(0, 0, m_width / 2, m_height);
    gl.useProgram(m_multiviewProgram->getProgram());
    gl.uniform4fv(colorUniform, 2, COLOR_VALUES);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, &UNIT_SQUARE[0]);
    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Sample from the array texture to draw a quad into the backbuffer.

    const int backbufferWidth  = m_context.getRenderTarget().getWidth();
    const int backbufferHeight = m_context.getRenderTarget().getHeight();
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.viewport(0, 0, backbufferWidth, backbufferHeight);
    gl.useProgram(m_finalProgram->getProgram());
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_arrayTexture);
    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Read back the framebuffer, ensure that the left half is red and the
    // right half is green.

    tcu::Surface pixels(backbufferWidth, backbufferHeight);
    glu::readPixels(m_context.getRenderContext(), 0, 0, pixels.getAccess());
    bool failed = false;
    for (int y = 0; y < backbufferHeight; y++)
    {
        for (int x = 0; x < backbufferWidth; x++)
        {
            tcu::RGBA pixel = pixels.getPixel(x, y);
            if (x < backbufferWidth / 2)
            {
                if (pixel.getRed() != 255 || pixel.getGreen() != 0 || pixel.getBlue() != 0)
                {
                    failed = true;
                }
            }
            else if (x > backbufferWidth / 2)
            {
                if (pixel.getRed() != 0 || pixel.getGreen() != 255 || pixel.getBlue() != 0)
                {
                    failed = true;
                }
            }
            if (failed)
            {
                break;
            }
        }
    }

    deleteFramebufferObjects();

    if (failed)
    {
        log << TestLog::Image("Result image", "Result image", pixels);
    }

    log << TestLog::Message << "Test result: " << (failed ? "Failed!" : "Passed!") << TestLog::EndMessage;

    if (failed)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    return (++m_caseIndex < NUM_CASE_ITERATIONS) ? CONTINUE : STOP;
}

MultiviewTests::MultiviewTests(Context &context) : TestCaseGroup(context, "multiview", "Multiview Tests")
{
}

MultiviewTests::~MultiviewTests()
{
}

void MultiviewTests::init()
{
    addChild(new MultiviewCase(m_context, "samples_1", "Multiview test without multisampling", 1));
    addChild(new MultiviewCase(m_context, "samples_2", "Multiview test with MSAAx2", 2));
    addChild(new MultiviewCase(m_context, "samples_4", "Multiview test without MSAAx4", 4));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
