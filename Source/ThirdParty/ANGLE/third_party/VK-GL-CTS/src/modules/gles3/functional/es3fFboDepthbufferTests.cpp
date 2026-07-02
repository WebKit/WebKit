/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 *//*!
 * \file
 * \brief FBO depthbuffer tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFboDepthbufferTests.hpp"
#include "es3fFboTestCase.hpp"
#include "es3fFboTestUtil.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "sglrContextUtil.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace FboTestUtil;

class BasicFboDepthCase : public FboTestCase
{
public:
    BasicFboDepthCase(Context &context, const char *name, const char *desc, uint32_t format, int width, int height)
        : FboTestCase(context, name, desc)
        , m_format(format)
        , m_width(width)
        , m_height(height)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        const uint32_t colorFormat = GL_RGBA8;
        uint32_t fbo               = 0;
        uint32_t colorRbo          = 0;
        uint32_t depthRbo          = 0;
        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
        uint32_t gradShaderID = getCurrentContext()->createProgram(&gradShader);
        uint32_t texShaderID  = getCurrentContext()->createProgram(&texShader);
        float clearDepth      = 1.0f;

        // Setup shaders
        gradShader.setGradient(*getCurrentContext(), gradShaderID, tcu::Vec4(0.0f), tcu::Vec4(1.0f));
        texShader.setUniforms(*getCurrentContext(), texShaderID);

        // Setup FBO

        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(1, &colorRbo);
        glGenRenderbuffers(1, &depthRbo);

        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, m_width, m_height);

        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, m_format, m_width, m_height);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glViewport(0, 0, m_width, m_height);

        // Clear depth to 1
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        // Render gradient with depth = [-1..1]
        glEnable(GL_DEPTH_TEST);
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Render grid pattern with depth = 0
        {
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = 128;
            const int texH          = 128;
            uint32_t gridTex        = 0;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            tcu::fillWithGrid(data.getAccess(), 8, Vec4(0.2f, 0.7f, 0.1f, 1.0f), Vec4(0.7f, 0.1f, 0.5f, 0.8f));

            glGenTextures(1, &gridTex);
            glBindTexture(GL_TEXTURE_2D, gridTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        // Read results.
        readPixels(dst, 0, 0, m_width, m_height, glu::mapGLInternalFormat(colorFormat), Vec4(1.0f), Vec4(0.0f));
    }

private:
    uint32_t m_format;
    int m_width;
    int m_height;
};

class DepthWriteClampCase : public FboTestCase
{
public:
    DepthWriteClampCase(Context &context, const char *name, const char *desc, uint32_t format, int width, int height)
        : FboTestCase(context, name, desc)
        , m_format(format)
        , m_width(width)
        , m_height(height)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        const uint32_t colorFormat      = GL_RGBA8;
        uint32_t fbo                    = 0;
        uint32_t colorRbo               = 0;
        uint32_t depthTexture           = 0;
        glu::TransferFormat transferFmt = glu::getTransferFormat(glu::mapGLInternalFormat(m_format));

        DepthGradientShader depthGradShader(glu::TYPE_FLOAT_VEC4);
        const uint32_t depthGradShaderID = getCurrentContext()->createProgram(&depthGradShader);
        const float clearDepth           = 1.0f;
        const tcu::Vec4 red(1.0, 0.0, 0.0, 1.0);
        const tcu::Vec4 green(0.0, 1.0, 0.0, 1.0);

        // Setup FBO

        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(1, &colorRbo);
        glGenTextures(1, &depthTexture);

        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, m_width, m_height);

        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, m_format, m_width, m_height, 0, transferFmt.format, transferFmt.dataType,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glViewport(0, 0, m_width, m_height);

        // Clear depth to 1
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        // Test that invalid values are not written to the depth buffer

        // Render green quad, depth gradient = [-1..2]
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);

        depthGradShader.setUniforms(*getCurrentContext(), depthGradShaderID, -1.0f, 2.0f, green);
        sglr::drawQuad(*getCurrentContext(), depthGradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));
        glDepthMask(GL_FALSE);

        // Test if any fragment has greater depth than 1; there should be none
        glDepthFunc(GL_LESS); // (1 < depth) ?
        depthGradShader.setUniforms(*getCurrentContext(), depthGradShaderID, 1.0f, 1.0f, red);
        sglr::drawQuad(*getCurrentContext(), depthGradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Test if any fragment has smaller depth than 0; there should be none
        glDepthFunc(GL_GREATER); // (0 > depth) ?
        depthGradShader.setUniforms(*getCurrentContext(), depthGradShaderID, 0.0f, 0.0f, red);
        sglr::drawQuad(*getCurrentContext(), depthGradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Read results.
        readPixels(dst, 0, 0, m_width, m_height, glu::mapGLInternalFormat(colorFormat), Vec4(1.0f), Vec4(0.0f));
    }

private:
    const uint32_t m_format;
    const int m_width;
    const int m_height;
};

class DepthTestClampCase : public FboTestCase
{
public:
    DepthTestClampCase(Context &context, const char *name, const char *desc, uint32_t format, int width, int height)
        : FboTestCase(context, name, desc)
        , m_format(format)
        , m_width(width)
        , m_height(height)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        const uint32_t colorFormat      = GL_RGBA8;
        uint32_t fbo                    = 0;
        uint32_t colorRbo               = 0;
        uint32_t depthTexture           = 0;
        glu::TransferFormat transferFmt = glu::getTransferFormat(glu::mapGLInternalFormat(m_format));

        DepthGradientShader depthGradShader(glu::TYPE_FLOAT_VEC4);
        const uint32_t depthGradShaderID = getCurrentContext()->createProgram(&depthGradShader);
        const float clearDepth           = 1.0f;
        const tcu::Vec4 yellow(1.0, 1.0, 0.0, 1.0);
        const tcu::Vec4 green(0.0, 1.0, 0.0, 1.0);

        // Setup FBO

        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(1, &colorRbo);
        glGenTextures(1, &depthTexture);

        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, m_width, m_height);

        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, m_format, m_width, m_height, 0, transferFmt.format, transferFmt.dataType,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glViewport(0, 0, m_width, m_height);

        // Clear depth to 1
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        // Test values used in depth test are clamped

        // Render green quad, depth gradient = [-1..2]
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);

        depthGradShader.setUniforms(*getCurrentContext(), depthGradShaderID, -1.0f, 2.0f, green);
        sglr::drawQuad(*getCurrentContext(), depthGradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Render yellow quad, depth gradient = [-0.5..3]. Gradients have equal values only outside [0, 1] range due to clamping
        glDepthFunc(GL_EQUAL);
        depthGradShader.setUniforms(*getCurrentContext(), depthGradShaderID, -0.5f, 3.0f, yellow);
        sglr::drawQuad(*getCurrentContext(), depthGradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Read results.
        readPixels(dst, 0, 0, m_width, m_height, glu::mapGLInternalFormat(colorFormat), Vec4(1.0f), Vec4(0.0f));
    }

private:
    const uint32_t m_format;
    const int m_width;
    const int m_height;
};

FboDepthTests::FboDepthTests(Context &context) : TestCaseGroup(context, "depth", "Depth tests")
{
}

FboDepthTests::~FboDepthTests(void)
{
}

void FboDepthTests::init(void)
{
    static const uint32_t depthFormats[] = {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16,
                                            GL_DEPTH32F_STENCIL8, GL_DEPTH24_STENCIL8};

    // .basic
    {
        tcu::TestCaseGroup *basicGroup = new tcu::TestCaseGroup(m_testCtx, "basic", "Basic depth tests");
        addChild(basicGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(depthFormats); fmtNdx++)
            basicGroup->addChild(new BasicFboDepthCase(m_context, getFormatName(depthFormats[fmtNdx]), "",
                                                       depthFormats[fmtNdx], 119, 127));
    }

    // .depth_write_clamp
    {
        tcu::TestCaseGroup *depthClampGroup =
            new tcu::TestCaseGroup(m_testCtx, "depth_write_clamp", "Depth write clamping tests");
        addChild(depthClampGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(depthFormats); fmtNdx++)
            depthClampGroup->addChild(new DepthWriteClampCase(m_context, getFormatName(depthFormats[fmtNdx]), "",
                                                              depthFormats[fmtNdx], 119, 127));
    }

    // .depth_test_clamp
    {
        tcu::TestCaseGroup *depthClampGroup =
            new tcu::TestCaseGroup(m_testCtx, "depth_test_clamp", "Depth test value clamping tests");
        addChild(depthClampGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(depthFormats); fmtNdx++)
            depthClampGroup->addChild(new DepthTestClampCase(m_context, getFormatName(depthFormats[fmtNdx]), "",
                                                             depthFormats[fmtNdx], 119, 127));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
