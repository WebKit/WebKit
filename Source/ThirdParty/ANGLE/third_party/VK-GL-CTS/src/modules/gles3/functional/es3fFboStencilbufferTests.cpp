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
 * \brief FBO stencilbuffer tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFboStencilbufferTests.hpp"
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

class BasicFboStencilCase : public FboTestCase
{
public:
    BasicFboStencilCase(Context &context, const char *name, const char *desc, uint32_t format, IVec2 size,
                        bool useDepth)
        : FboTestCase(context, name, desc)
        , m_format(format)
        , m_size(size)
        , m_useDepth(useDepth)
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

        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        uint32_t flatShaderID = getCurrentContext()->createProgram(&flatShader);
        uint32_t gradShaderID = getCurrentContext()->createProgram(&gradShader);

        uint32_t fbo             = 0;
        uint32_t colorRbo        = 0;
        uint32_t depthStencilRbo = 0;

        // Colorbuffer.
        glGenRenderbuffers(1, &colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, m_size.x(), m_size.y());

        // Stencil (and depth) buffer.
        glGenRenderbuffers(1, &depthStencilRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, m_format, m_size.x(), m_size.y());

        // Framebuffer.
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);
        if (m_useDepth)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glViewport(0, 0, m_size.x(), m_size.y());

        // Clear framebuffer.
        glClearBufferfv(GL_COLOR, 0, Vec4(0.0f).getPtr());
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

        // Render intersecting quads - increment stencil on depth pass
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 0, 0xffu);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

        gradShader.setGradient(*getCurrentContext(), gradShaderID, Vec4(0.0f), Vec4(1.0f));
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(+1.0f, +1.0f, +1.0f));

        glDisable(GL_DEPTH_TEST);

        // Draw quad with stencil test (stencil == 1 or 2 depending on depth) - decrement on stencil failure
        glStencilFunc(GL_EQUAL, m_useDepth ? 2 : 1, 0xffu);
        glStencilOp(GL_DECR, GL_KEEP, GL_KEEP);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-0.5f, -0.5f, 0.0f), Vec3(+0.5f, +0.5f, 0.0f));

        // Draw quad with stencil test where stencil > 1 or 2 depending on depth buffer
        glStencilFunc(GL_GREATER, m_useDepth ? 1 : 2, 0xffu);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

        readPixels(dst, 0, 0, m_size.x(), m_size.y(), glu::mapGLInternalFormat(colorFormat), Vec4(1.0f), Vec4(0.0f));
    }

private:
    uint32_t m_format;
    IVec2 m_size;
    bool m_useDepth;
};

class DepthStencilAttachCase : public FboTestCase
{
public:
    DepthStencilAttachCase(Context &context, const char *name, const char *desc, uint32_t attachDepth,
                           uint32_t attachStencil)
        : FboTestCase(context, name, desc)
        , m_attachDepth(attachDepth)
        , m_attachStencil(attachStencil)
    {
        DE_ASSERT(m_attachDepth == GL_DEPTH_ATTACHMENT || m_attachDepth == GL_DEPTH_STENCIL_ATTACHMENT ||
                  m_attachDepth == GL_NONE);
        DE_ASSERT(m_attachStencil == GL_STENCIL_ATTACHMENT || m_attachStencil == GL_NONE);
        DE_ASSERT(m_attachDepth != GL_DEPTH_STENCIL || m_attachStencil == GL_NONE);
    }

protected:
    void render(tcu::Surface &dst)
    {
        const uint32_t colorFormat        = GL_RGBA8;
        const uint32_t depthStencilFormat = GL_DEPTH24_STENCIL8;
        const int width                   = 128;
        const int height                  = 128;
        const bool hasDepth               = (m_attachDepth == GL_DEPTH_STENCIL || m_attachDepth == GL_DEPTH_ATTACHMENT);
        // const bool hasStencil = (m_attachDepth == GL_DEPTH_STENCIL || m_attachStencil == GL_DEPTH_STENCIL_ATTACHMENT);

        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        uint32_t flatShaderID = getCurrentContext()->createProgram(&flatShader);
        uint32_t gradShaderID = getCurrentContext()->createProgram(&gradShader);

        uint32_t fbo             = 0;
        uint32_t colorRbo        = 0;
        uint32_t depthStencilRbo = 0;

        // Colorbuffer.
        glGenRenderbuffers(1, &colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, width, height);

        // Depth-stencil buffer.
        glGenRenderbuffers(1, &depthStencilRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, depthStencilFormat, width, height);

        // Framebuffer.
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);

        if (m_attachDepth != GL_NONE)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, m_attachDepth, GL_RENDERBUFFER, depthStencilRbo);
        if (m_attachStencil != GL_NONE)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, m_attachStencil, GL_RENDERBUFFER, depthStencilRbo);

        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glViewport(0, 0, width, height);

        // Clear framebuffer.
        glClearBufferfv(GL_COLOR, 0, Vec4(0.0f).getPtr());
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

        // Render intersecting quads - increment stencil on depth pass
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 0, 0xffu);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

        gradShader.setGradient(*getCurrentContext(), gradShaderID, Vec4(0.0f), Vec4(1.0f));
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(+1.0f, +1.0f, +1.0f));

        glDisable(GL_DEPTH_TEST);

        // Draw quad with stencil test (stencil == 1 or 2 depending on depth) - decrement on stencil failure
        glStencilFunc(GL_EQUAL, hasDepth ? 2 : 1, 0xffu);
        glStencilOp(GL_DECR, GL_KEEP, GL_KEEP);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-0.5f, -0.5f, 0.0f), Vec3(+0.5f, +0.5f, 0.0f));

        // Draw quad with stencil test where stencil > 1 or 2 depending on depth buffer
        glStencilFunc(GL_GREATER, hasDepth ? 1 : 2, 0xffu);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

        readPixels(dst, 0, 0, width, height, glu::mapGLInternalFormat(colorFormat), Vec4(1.0f), Vec4(0.0f));
    }

private:
    uint32_t m_attachDepth;
    uint32_t m_attachStencil;
};

FboStencilTests::FboStencilTests(Context &context) : TestCaseGroup(context, "stencil", "FBO Stencilbuffer tests")
{
}

FboStencilTests::~FboStencilTests(void)
{
}

void FboStencilTests::init(void)
{
    static const uint32_t stencilFormats[] = {GL_DEPTH32F_STENCIL8, GL_DEPTH24_STENCIL8, GL_STENCIL_INDEX8};

    // .basic
    {
        tcu::TestCaseGroup *basicGroup = new tcu::TestCaseGroup(m_testCtx, "basic", "Basic stencil tests");
        addChild(basicGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(stencilFormats); fmtNdx++)
        {
            uint32_t format           = stencilFormats[fmtNdx];
            tcu::TextureFormat texFmt = glu::mapGLInternalFormat(format);

            basicGroup->addChild(
                new BasicFboStencilCase(m_context, getFormatName(format), "", format, IVec2(111, 132), false));

            if (texFmt.order == tcu::TextureFormat::DS)
                basicGroup->addChild(new BasicFboStencilCase(
                    m_context, (string(getFormatName(format)) + "_depth").c_str(), "", format, IVec2(111, 132), true));
        }
    }

    // .attach
    {
        tcu::TestCaseGroup *attachGroup = new tcu::TestCaseGroup(m_testCtx, "attach", "Attaching depth stencil");
        addChild(attachGroup);

        attachGroup->addChild(new DepthStencilAttachCase(
            m_context, "depth_only", "Only depth part of depth-stencil RBO attached", GL_DEPTH_ATTACHMENT, GL_NONE));
        attachGroup->addChild(new DepthStencilAttachCase(m_context, "stencil_only",
                                                         "Only stencil part of depth-stencil RBO attached", GL_NONE,
                                                         GL_STENCIL_ATTACHMENT));
        attachGroup->addChild(new DepthStencilAttachCase(m_context, "depth_stencil_separate",
                                                         "Depth and stencil attached separately", GL_DEPTH_ATTACHMENT,
                                                         GL_STENCIL_ATTACHMENT));
        attachGroup->addChild(new DepthStencilAttachCase(m_context, "depth_stencil_attachment",
                                                         "Depth and stencil attached with DEPTH_STENCIL_ATTACHMENT",
                                                         GL_DEPTH_STENCIL_ATTACHMENT, GL_NONE));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
