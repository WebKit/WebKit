/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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
 * \brief GL_EXT_multisample_render_to_texture tests.
 */ /*--------------------------------------------------------------------*/

#include "es2fMultisampledRenderToTextureTests.hpp"

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
namespace gles2
{
namespace Functional
{

class MultisampledRenderToTextureReadPixelsCase : public TestCase
{
public:
    MultisampledRenderToTextureReadPixelsCase(Context &context, const char *name, const char *description);
    ~MultisampledRenderToTextureReadPixelsCase();
    void init();
    IterateResult iterate();

private:
    MultisampledRenderToTextureReadPixelsCase(const MultisampledRenderToTextureReadPixelsCase &other);
    MultisampledRenderToTextureReadPixelsCase &operator=(const MultisampledRenderToTextureReadPixelsCase &other);
};

MultisampledRenderToTextureReadPixelsCase::MultisampledRenderToTextureReadPixelsCase(Context &context, const char *name,
                                                                                     const char *description)
    : TestCase(context, name, description)
{
}

MultisampledRenderToTextureReadPixelsCase::~MultisampledRenderToTextureReadPixelsCase()
{
}

void MultisampledRenderToTextureReadPixelsCase::init()
{
    const glu::ContextInfo &contextInfo = m_context.getContextInfo();
    if (!contextInfo.isExtensionSupported("GL_EXT_multisampled_render_to_texture"))
    {
        TCU_THROW(NotSupportedError, "EXT_multisampled_render_to_texture is not supported");
    }
}

MultisampledRenderToTextureReadPixelsCase::IterateResult MultisampledRenderToTextureReadPixelsCase::iterate()
{
    // Test for a bug where ReadPixels fails on multisampled textures.
    // See http://crbug.com/890002
    // Note that this does not test whether multisampling is working properly,
    // only that ReadPixels is able to read from the texture.
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    // Create a framebuffer with a multisampled texture and a depth-stencil
    // renderbuffer.
    GLuint framebuffer = 0;
    GLuint texture     = 0;
    gl.genFramebuffers(1, &framebuffer);
    gl.genTextures(1, &texture);
    gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl.bindTexture(GL_TEXTURE_2D, texture);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GLint max_samples = 0;
    gl.getIntegerv(GL_MAX_SAMPLES_EXT, &max_samples);
    gl.framebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0, max_samples);
    GLuint depthStencil = 0;
    gl.genRenderbuffers(1, &depthStencil);
    gl.bindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    gl.renderbufferStorageMultisampleEXT(GL_RENDERBUFFER, max_samples, GL_DEPTH24_STENCIL8, 1, 1);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencil);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencil);
    if (gl.checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        TCU_THROW(NotSupportedError, "Framebuffer format not supported.");
    }
    gl.clearColor(1, 0, 1, 0);
    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "init");

    // ReadPixels should implicitly resolve the multisampled buffer.
    GLubyte pixel[4] = {0, 1, 0, 1};
    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
    GLU_EXPECT_NO_ERROR(gl.getError(), "ReadPixels");

    if (pixel[0] != 255 || pixel[1] != 0 || pixel[2] != 255 || pixel[3] != 0)
    {
        std::ostringstream msg;
        msg << "ReadPixels read incorrect values: [" << (int)pixel[0] << ", " << (int)pixel[1] << ", " << (int)pixel[2]
            << ", " << (int)pixel[3] << "]";
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, msg.str().c_str());
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }

    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.bindTexture(GL_TEXTURE_2D, 0);
    gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
    gl.deleteFramebuffers(1, &framebuffer);
    gl.deleteRenderbuffers(1, &depthStencil);
    gl.deleteTextures(1, &texture);
    return STOP;
}

MultisampledRenderToTextureTests::MultisampledRenderToTextureTests(Context &context)
    : TestCaseGroup(context, "multisampled_render_to_texture", "EXT_multisampled_render_to_texture tests")
{
}

MultisampledRenderToTextureTests::~MultisampledRenderToTextureTests()
{
}

void MultisampledRenderToTextureTests::init()
{
    addChild(new MultisampledRenderToTextureReadPixelsCase(m_context, "readpixels",
                                                           "Test ReadPixels with EXT_multisampled_render_to_texture"));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
