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
 * \brief FBO invalidate tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFboInvalidateTests.hpp"
#include "es3fFboTestCase.hpp"
#include "es3fFboTestUtil.hpp"
#include "gluTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "sglrContextUtil.hpp"

#include "glwEnums.hpp"

#include <algorithm>

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace FboTestUtil;

static std::vector<uint32_t> getDefaultFBDiscardAttachments(uint32_t discardBufferBits)
{
    vector<uint32_t> attachments;

    if (discardBufferBits & GL_COLOR_BUFFER_BIT)
        attachments.push_back(GL_COLOR);

    if (discardBufferBits & GL_DEPTH_BUFFER_BIT)
        attachments.push_back(GL_DEPTH);

    if (discardBufferBits & GL_STENCIL_BUFFER_BIT)
        attachments.push_back(GL_STENCIL);

    return attachments;
}

static std::vector<uint32_t> getFBODiscardAttachments(uint32_t discardBufferBits)
{
    vector<uint32_t> attachments;

    if (discardBufferBits & GL_COLOR_BUFFER_BIT)
        attachments.push_back(GL_COLOR_ATTACHMENT0);

    // \note DEPTH_STENCIL_ATTACHMENT is allowed when discarding FBO, but not with default FB
    if ((discardBufferBits & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) ==
        (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
        attachments.push_back(GL_DEPTH_STENCIL_ATTACHMENT);
    else if (discardBufferBits & GL_DEPTH_BUFFER_BIT)
        attachments.push_back(GL_DEPTH_ATTACHMENT);
    else if (discardBufferBits & GL_STENCIL_BUFFER_BIT)
        attachments.push_back(GL_STENCIL_ATTACHMENT);

    return attachments;
}

static inline bool hasAttachment(const std::vector<uint32_t> &attachmentList, uint32_t attachment)
{
    return std::find(attachmentList.begin(), attachmentList.end(), attachment) != attachmentList.end();
}

static uint32_t getCompatibleColorFormat(const tcu::RenderTarget &renderTargetInfo)
{
    const tcu::PixelFormat &pxFmt = renderTargetInfo.getPixelFormat();
    DE_ASSERT(de::inBounds(pxFmt.redBits, 0, 0xff) && de::inBounds(pxFmt.greenBits, 0, 0xff) &&
              de::inBounds(pxFmt.blueBits, 0, 0xff) && de::inBounds(pxFmt.alphaBits, 0, 0xff));

#define PACK_FMT(R, G, B, A) (((R) << 24) | ((G) << 16) | ((B) << 8) | (A))

    // \note [pyry] This may not hold true on some implementations - best effort guess only.
    switch (PACK_FMT(pxFmt.redBits, pxFmt.greenBits, pxFmt.blueBits, pxFmt.alphaBits))
    {
    case PACK_FMT(8, 8, 8, 8):
        return GL_RGBA8;
    case PACK_FMT(8, 8, 8, 0):
        return GL_RGB8;
    case PACK_FMT(4, 4, 4, 4):
        return GL_RGBA4;
    case PACK_FMT(5, 5, 5, 1):
        return GL_RGB5_A1;
    case PACK_FMT(5, 6, 5, 0):
        return GL_RGB565;
    default:
        return GL_NONE;
    }

#undef PACK_FMT
}

static uint32_t getCompatibleDepthStencilFormat(const tcu::RenderTarget &renderTargetInfo)
{
    const int depthBits   = renderTargetInfo.getDepthBits();
    const int stencilBits = renderTargetInfo.getStencilBits();
    const bool hasDepth   = depthBits > 0;
    const bool hasStencil = stencilBits > 0;

    if (!hasDepth || !hasStencil || (stencilBits != 8))
        return GL_NONE;

    if (depthBits == 32)
        return GL_DEPTH32F_STENCIL8;
    else if (depthBits == 24)
        return GL_DEPTH24_STENCIL8;
    else
        return GL_NONE;
}

class InvalidateDefaultFramebufferRenderCase : public FboTestCase
{
public:
    InvalidateDefaultFramebufferRenderCase(Context &context, const char *name, const char *description,
                                           uint32_t buffers, uint32_t fboTarget = GL_FRAMEBUFFER)
        : FboTestCase(context, name, description)
        , m_buffers(buffers)
        , m_fboTarget(fboTarget)
    {
    }

    void render(tcu::Surface &dst)
    {
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getDefaultFBDiscardAttachments(m_buffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateFramebuffer(m_fboTarget, (int)attachments.size(), attachments.empty() ? nullptr : &attachments[0]);

        if ((m_buffers & GL_COLOR_BUFFER_BIT) != 0)
        {
            // Color was not preserved - fill with green.
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_STENCIL_TEST);

            flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));
            sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_STENCIL_TEST);
        }

        if ((m_buffers & GL_DEPTH_BUFFER_BIT) != 0)
        {
            // Depth was not preserved.
            glDepthFunc(GL_ALWAYS);
        }

        if ((m_buffers & GL_STENCIL_BUFFER_BIT) == 0)
        {
            // Stencil was preserved.
            glStencilFunc(GL_EQUAL, 1, 0xff);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    uint32_t m_buffers;
    uint32_t m_fboTarget;
};

class InvalidateDefaultFramebufferBindCase : public FboTestCase
{
public:
    InvalidateDefaultFramebufferBindCase(Context &context, const char *name, const char *description, uint32_t buffers)
        : FboTestCase(context, name, description)
        , m_buffers(buffers)
    {
    }

    void render(tcu::Surface &dst)
    {
        uint32_t fbo = 0;
        uint32_t tex = 0;
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getDefaultFBDiscardAttachments(m_buffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);
        uint32_t texShaderID         = getCurrentContext()->createProgram(&texShader);
        uint32_t gradShaderID        = getCurrentContext()->createProgram(&gradShader);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Create fbo.
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, getWidth(), getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(),
                                attachments.empty() ? nullptr : &attachments[0]);

        // Switch to fbo and render gradient into it.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        gradShader.setGradient(*getCurrentContext(), gradShaderID, Vec4(0.0f), Vec4(1.0f));
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Restore default fbo.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if ((m_buffers & GL_COLOR_BUFFER_BIT) != 0)
        {
            // Color was not preserved - fill with green.
            flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));
            sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        if ((m_buffers & GL_DEPTH_BUFFER_BIT) != 0)
        {
            // Depth was not preserved.
            glDepthFunc(GL_ALWAYS);
        }

        if ((m_buffers & GL_STENCIL_BUFFER_BIT) == 0)
        {
            // Stencil was preserved.
            glStencilFunc(GL_EQUAL, 1, 0xff);
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);
        glBindTexture(GL_TEXTURE_2D, tex);

        texShader.setUniforms(*getCurrentContext(), texShaderID);
        sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    uint32_t m_buffers;
};

class InvalidateDefaultSubFramebufferRenderCase : public FboTestCase
{
public:
    InvalidateDefaultSubFramebufferRenderCase(Context &context, const char *name, const char *description,
                                              uint32_t buffers)
        : FboTestCase(context, name, description)
        , m_buffers(buffers)
    {
    }

    void render(tcu::Surface &dst)
    {
        int invalidateX = getWidth() / 4;
        int invalidateY = getHeight() / 4;
        int invalidateW = getWidth() / 2;
        int invalidateH = getHeight() / 2;
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getDefaultFBDiscardAttachments(m_buffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateSubFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(), &attachments[0], invalidateX, invalidateY,
                                   invalidateW, invalidateH);

        // Clear invalidated buffers.
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClearStencil(1);
        glScissor(invalidateX, invalidateY, invalidateW, invalidateH);
        glEnable(GL_SCISSOR_TEST);
        glClear(m_buffers);
        glDisable(GL_SCISSOR_TEST);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    uint32_t m_buffers;
};

class InvalidateDefaultSubFramebufferBindCase : public FboTestCase
{
public:
    InvalidateDefaultSubFramebufferBindCase(Context &context, const char *name, const char *description,
                                            uint32_t buffers)
        : FboTestCase(context, name, description)
        , m_buffers(buffers)
    {
    }

    void render(tcu::Surface &dst)
    {
        uint32_t fbo = 0;
        uint32_t tex = 0;
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getDefaultFBDiscardAttachments(m_buffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);
        uint32_t texShaderID         = getCurrentContext()->createProgram(&texShader);
        uint32_t gradShaderID        = getCurrentContext()->createProgram(&gradShader);

        int invalidateX = getWidth() / 4;
        int invalidateY = getHeight() / 4;
        int invalidateW = getWidth() / 2;
        int invalidateH = getHeight() / 2;

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        texShader.setUniforms(*getCurrentContext(), texShaderID);
        gradShader.setGradient(*getCurrentContext(), gradShaderID, Vec4(0.0f), Vec4(1.0f));

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Create fbo.
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, getWidth(), getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateSubFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(), &attachments[0], invalidateX, invalidateY,
                                   invalidateW, invalidateH);

        // Switch to fbo and render gradient into it.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Restore default fbo.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Clear invalidated buffers.
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClearStencil(1);
        glScissor(invalidateX, invalidateY, invalidateW, invalidateH);
        glEnable(GL_SCISSOR_TEST);
        glClear(m_buffers);
        glDisable(GL_SCISSOR_TEST);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);
        glBindTexture(GL_TEXTURE_2D, tex);

        sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    uint32_t m_buffers;
};

class InvalidateFboRenderCase : public FboTestCase
{
public:
    InvalidateFboRenderCase(Context &context, const char *name, const char *description, uint32_t colorFmt,
                            uint32_t depthStencilFmt, uint32_t invalidateBuffers)
        : FboTestCase(context, name, description)
        , m_colorFmt(colorFmt)
        , m_depthStencilFmt(depthStencilFmt)
        , m_invalidateBuffers(invalidateBuffers)
    {
    }

protected:
    void preCheck(void)
    {
        if (m_colorFmt != GL_NONE)
            checkFormatSupport(m_colorFmt);
        if (m_depthStencilFmt != GL_NONE)
            checkFormatSupport(m_depthStencilFmt);
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat colorFmt = glu::mapGLInternalFormat(m_colorFmt);
        tcu::TextureFormat depthStencilFmt =
            m_depthStencilFmt != GL_NONE ? glu::mapGLInternalFormat(m_depthStencilFmt) : tcu::TextureFormat();
        tcu::TextureFormatInfo colorFmtInfo = tcu::getTextureFormatInfo(colorFmt);
        bool depth = depthStencilFmt.order == tcu::TextureFormat::D || depthStencilFmt.order == tcu::TextureFormat::DS;
        bool stencil =
            depthStencilFmt.order == tcu::TextureFormat::S || depthStencilFmt.order == tcu::TextureFormat::DS;
        const tcu::Vec4 &cBias   = colorFmtInfo.valueMin;
        tcu::Vec4 cScale         = colorFmtInfo.valueMax - colorFmtInfo.valueMin;
        uint32_t fbo             = 0;
        uint32_t colorRbo        = 0;
        uint32_t depthStencilRbo = 0;
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getFBODiscardAttachments(m_invalidateBuffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);

        // Create fbo.
        glGenRenderbuffers(1, &colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, m_colorFmt, getWidth(), getHeight());

        if (m_depthStencilFmt != GL_NONE)
        {
            glGenRenderbuffers(1, &depthStencilRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, m_depthStencilFmt, getWidth(), getHeight());
        }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);

        if (depth)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        if (stencil)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        checkFramebufferStatus(GL_FRAMEBUFFER);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias);
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(),
                                attachments.empty() ? nullptr : &attachments[0]);

        if ((m_invalidateBuffers & GL_COLOR_BUFFER_BIT) != 0)
        {
            // Color was not preserved - fill with green.
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_STENCIL_TEST);

            flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias);
            sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_STENCIL_TEST);
        }

        if ((m_invalidateBuffers & GL_DEPTH_BUFFER_BIT) != 0)
        {
            // Depth was not preserved.
            glDepthFunc(GL_ALWAYS);
        }

        if ((m_invalidateBuffers & GL_STENCIL_BUFFER_BIT) == 0)
        {
            // Stencil was preserved.
            glStencilFunc(GL_EQUAL, 1, 0xff);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f) * cScale + cBias);
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight(), colorFmt, colorFmtInfo.lookupScale, colorFmtInfo.lookupBias);
    }

private:
    uint32_t m_colorFmt;
    uint32_t m_depthStencilFmt;
    uint32_t m_invalidateBuffers;
};

class InvalidateFboUnbindReadCase : public FboTestCase
{
public:
    InvalidateFboUnbindReadCase(Context &context, const char *name, const char *description, uint32_t colorFmt,
                                uint32_t depthStencilFmt, uint32_t invalidateBuffers)
        : FboTestCase(context, name, description)
        , m_colorFmt(colorFmt)
        , m_depthStencilFmt(depthStencilFmt)
        , m_invalidateBuffers(invalidateBuffers)
    {
        DE_ASSERT((m_invalidateBuffers & (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) !=
                  (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    }

protected:
    void preCheck(void)
    {
        if (m_colorFmt != GL_NONE)
            checkFormatSupport(m_colorFmt);
        if (m_depthStencilFmt != GL_NONE)
            checkFormatSupport(m_depthStencilFmt);
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat colorFmt = glu::mapGLInternalFormat(m_colorFmt);
        tcu::TextureFormat depthStencilFmt =
            m_depthStencilFmt != GL_NONE ? glu::mapGLInternalFormat(m_depthStencilFmt) : tcu::TextureFormat();
        tcu::TextureFormatInfo colorFmtInfo = tcu::getTextureFormatInfo(colorFmt);
        bool depth = depthStencilFmt.order == tcu::TextureFormat::D || depthStencilFmt.order == tcu::TextureFormat::DS;
        bool stencil =
            depthStencilFmt.order == tcu::TextureFormat::S || depthStencilFmt.order == tcu::TextureFormat::DS;
        uint32_t fbo             = 0;
        uint32_t colorTex        = 0;
        uint32_t depthStencilTex = 0;
        GradientShader gradShader(getFragmentOutputType(colorFmt));
        vector<uint32_t> attachments = getFBODiscardAttachments(m_invalidateBuffers);
        uint32_t gradShaderID        = getCurrentContext()->createProgram(&gradShader);

        // Create fbo.
        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(colorFmt);

            glGenTextures(1, &colorTex);
            glBindTexture(GL_TEXTURE_2D, colorTex);
            glTexImage2D(GL_TEXTURE_2D, 0, m_colorFmt, getWidth(), getHeight(), 0, transferFmt.format,
                         transferFmt.dataType, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }

        if (m_depthStencilFmt != GL_NONE)
        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(depthStencilFmt);

            glGenTextures(1, &depthStencilTex);
            glBindTexture(GL_TEXTURE_2D, depthStencilTex);
            glTexImage2D(GL_TEXTURE_2D, 0, m_depthStencilFmt, getWidth(), getHeight(), 0, transferFmt.format,
                         transferFmt.dataType, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        if (depth)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthStencilTex, 0);

        if (stencil)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthStencilTex, 0);

        checkFramebufferStatus(GL_FRAMEBUFFER);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        gradShader.setGradient(*getCurrentContext(), gradShaderID, colorFmtInfo.valueMin, colorFmtInfo.valueMax);
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(), &attachments[0]);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);

        if ((m_invalidateBuffers & GL_DEPTH_BUFFER_BIT) != 0)
        {
            // Render color.
            Texture2DShader texShader(DataTypes() << glu::getSampler2DType(colorFmt), glu::TYPE_FLOAT_VEC4);
            uint32_t texShaderID = getCurrentContext()->createProgram(&texShader);

            texShader.setTexScaleBias(0, colorFmtInfo.lookupScale, colorFmtInfo.lookupBias);
            texShader.setUniforms(*getCurrentContext(), texShaderID);

            glBindTexture(GL_TEXTURE_2D, colorTex);
            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }
        else
        {
            // Render depth.
            Texture2DShader texShader(DataTypes() << glu::getSampler2DType(depthStencilFmt), glu::TYPE_FLOAT_VEC4);
            uint32_t texShaderID = getCurrentContext()->createProgram(&texShader);

            texShader.setUniforms(*getCurrentContext(), texShaderID);

            glBindTexture(GL_TEXTURE_2D, depthStencilTex);
            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    uint32_t m_colorFmt;
    uint32_t m_depthStencilFmt;
    uint32_t m_invalidateBuffers;
};

class InvalidateFboUnbindBlitCase : public FboTestCase
{
public:
    InvalidateFboUnbindBlitCase(Context &context, const char *name, const char *description, int numSamples,
                                uint32_t invalidateBuffers)
        : FboTestCase(context, name, description,
                      numSamples >
                          0) // \note Use fullscreen viewport when multisampling - we can't allow GLES3Context do its
        //         behing-the-scenes viewport position randomization, because with glBlitFramebuffer,
        //         source and destination rectangles must match when multisampling.
        , m_colorFmt(0)
        , m_depthStencilFmt(0)
        , m_numSamples(numSamples)
        , m_invalidateBuffers(invalidateBuffers)
    {
        // Figure out formats that are compatible with default framebuffer.
        m_colorFmt        = getCompatibleColorFormat(m_context.getRenderTarget());
        m_depthStencilFmt = getCompatibleDepthStencilFormat(m_context.getRenderTarget());
    }

protected:
    void preCheck(void)
    {
        if (m_context.getRenderTarget().getNumSamples() > 0)
            throw tcu::NotSupportedError("Not supported in MSAA config");

        if (m_colorFmt == GL_NONE)
            throw tcu::NotSupportedError("Unsupported color format");

        if (m_depthStencilFmt == GL_NONE)
            throw tcu::NotSupportedError("Unsupported depth/stencil format");

        checkFormatSupport(m_colorFmt);
        checkFormatSupport(m_depthStencilFmt);
    }

    void render(tcu::Surface &dst)
    {
        // \note When using fullscreen viewport (when m_numSamples > 0), still only use a 128x128 pixel quad at most.
        IVec2 quadSizePixels(m_numSamples == 0 ? getWidth() : de::min(128, getWidth()),
                             m_numSamples == 0 ? getHeight() : de::min(128, getHeight()));
        Vec2 quadNDCLeftBottomXY(-1.0f, -1.0f);
        Vec2 quadNDCSize(2.0f * (float)quadSizePixels.x() / (float)getWidth(),
                         2.0f * (float)quadSizePixels.y() / (float)getHeight());
        Vec2 quadNDCRightTopXY = quadNDCLeftBottomXY + quadNDCSize;
        tcu::TextureFormat depthStencilFmt =
            m_depthStencilFmt != GL_NONE ? glu::mapGLInternalFormat(m_depthStencilFmt) : tcu::TextureFormat();
        bool depth = depthStencilFmt.order == tcu::TextureFormat::D || depthStencilFmt.order == tcu::TextureFormat::DS;
        bool stencil =
            depthStencilFmt.order == tcu::TextureFormat::S || depthStencilFmt.order == tcu::TextureFormat::DS;
        uint32_t fbo             = 0;
        uint32_t colorRbo        = 0;
        uint32_t depthStencilRbo = 0;
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getFBODiscardAttachments(m_invalidateBuffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Create fbo.
        glGenRenderbuffers(1, &colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_numSamples, m_colorFmt, quadSizePixels.x(),
                                         quadSizePixels.y());

        if (m_depthStencilFmt != GL_NONE)
        {
            glGenRenderbuffers(1, &depthStencilRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_numSamples, m_depthStencilFmt, quadSizePixels.x(),
                                             quadSizePixels.y());
        }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);

        if (depth)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        if (stencil)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        checkFramebufferStatus(GL_FRAMEBUFFER);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID,
                       Vec3(quadNDCLeftBottomXY.x(), quadNDCLeftBottomXY.y(), -1.0f),
                       Vec3(quadNDCRightTopXY.x(), quadNDCRightTopXY.y(), 1.0f));

        glInvalidateFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(), &attachments[0]);

        // Set default framebuffer as draw framebuffer and blit preserved buffers.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, quadSizePixels.x(), quadSizePixels.y(), 0, 0, quadSizePixels.x(), quadSizePixels.y(),
                          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) & ~m_invalidateBuffers,
                          GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        if ((m_invalidateBuffers & GL_COLOR_BUFFER_BIT) != 0)
        {
            // Color was not preserved - fill with green.
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_STENCIL_TEST);

            flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));
            sglr::drawQuad(*getCurrentContext(), flatShaderID,
                           Vec3(quadNDCLeftBottomXY.x(), quadNDCLeftBottomXY.y(), 0.0f),
                           Vec3(quadNDCRightTopXY.x(), quadNDCRightTopXY.y(), 0.0f));

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_STENCIL_TEST);
        }

        if ((m_invalidateBuffers & GL_DEPTH_BUFFER_BIT) != 0)
        {
            // Depth was not preserved.
            glDepthFunc(GL_ALWAYS);
        }

        if ((m_invalidateBuffers & GL_STENCIL_BUFFER_BIT) == 0)
        {
            // Stencil was preserved.
            glStencilFunc(GL_EQUAL, 1, 0xff);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(quadNDCLeftBottomXY.x(), quadNDCLeftBottomXY.y(), 0.0f),
                       Vec3(quadNDCRightTopXY.x(), quadNDCRightTopXY.y(), 0.0f));

        readPixels(dst, 0, 0, quadSizePixels.x(), quadSizePixels.y());
    }

private:
    uint32_t m_colorFmt;
    uint32_t m_depthStencilFmt;
    int m_numSamples;
    uint32_t m_invalidateBuffers;
};

class InvalidateSubFboRenderCase : public FboTestCase
{
public:
    InvalidateSubFboRenderCase(Context &context, const char *name, const char *description, uint32_t colorFmt,
                               uint32_t depthStencilFmt, uint32_t invalidateBuffers)
        : FboTestCase(context, name, description)
        , m_colorFmt(colorFmt)
        , m_depthStencilFmt(depthStencilFmt)
        , m_invalidateBuffers(invalidateBuffers)
    {
    }

protected:
    void preCheck(void)
    {
        if (m_colorFmt != GL_NONE)
            checkFormatSupport(m_colorFmt);
        if (m_depthStencilFmt != GL_NONE)
            checkFormatSupport(m_depthStencilFmt);
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat colorFmt = glu::mapGLInternalFormat(m_colorFmt);
        tcu::TextureFormat depthStencilFmt =
            m_depthStencilFmt != GL_NONE ? glu::mapGLInternalFormat(m_depthStencilFmt) : tcu::TextureFormat();
        tcu::TextureFormatInfo colorFmtInfo = tcu::getTextureFormatInfo(colorFmt);
        bool depth = depthStencilFmt.order == tcu::TextureFormat::D || depthStencilFmt.order == tcu::TextureFormat::DS;
        bool stencil =
            depthStencilFmt.order == tcu::TextureFormat::S || depthStencilFmt.order == tcu::TextureFormat::DS;
        const tcu::Vec4 &cBias   = colorFmtInfo.valueMin;
        tcu::Vec4 cScale         = colorFmtInfo.valueMax - colorFmtInfo.valueMin;
        uint32_t fbo             = 0;
        uint32_t colorRbo        = 0;
        uint32_t depthStencilRbo = 0;
        int invalidateX          = getWidth() / 4;
        int invalidateY          = getHeight() / 4;
        int invalidateW          = getWidth() / 2;
        int invalidateH          = getHeight() / 2;
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getFBODiscardAttachments(m_invalidateBuffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);

        // Create fbo.
        glGenRenderbuffers(1, &colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, m_colorFmt, getWidth(), getHeight());

        if (m_depthStencilFmt != GL_NONE)
        {
            glGenRenderbuffers(1, &depthStencilRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, m_depthStencilFmt, getWidth(), getHeight());
        }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);

        if (depth)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        if (stencil)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        checkFramebufferStatus(GL_FRAMEBUFFER);

        glClearBufferfv(GL_COLOR, 0, (Vec4(0.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias).getPtr());
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias);
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateSubFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(),
                                   attachments.empty() ? nullptr : &attachments[0], invalidateX, invalidateY,
                                   invalidateW, invalidateH);

        // Clear invalidated buffers.
        glScissor(invalidateX, invalidateY, invalidateW, invalidateH);
        glEnable(GL_SCISSOR_TEST);

        if (m_invalidateBuffers & GL_COLOR_BUFFER_BIT)
            glClearBufferfv(GL_COLOR, 0, (Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias).getPtr());

        glClear(m_invalidateBuffers & ~GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f) * cScale + cBias);
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight(), colorFmt, colorFmtInfo.lookupScale, colorFmtInfo.lookupBias);
    }

private:
    uint32_t m_colorFmt;
    uint32_t m_depthStencilFmt;
    uint32_t m_invalidateBuffers;
};

class InvalidateSubFboUnbindReadCase : public FboTestCase
{
public:
    InvalidateSubFboUnbindReadCase(Context &context, const char *name, const char *description, uint32_t colorFmt,
                                   uint32_t depthStencilFmt, uint32_t invalidateBuffers)
        : FboTestCase(context, name, description)
        , m_colorFmt(colorFmt)
        , m_depthStencilFmt(depthStencilFmt)
        , m_invalidateBuffers(invalidateBuffers)
    {
        DE_ASSERT((m_invalidateBuffers & (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) !=
                  (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    }

protected:
    void preCheck(void)
    {
        if (m_colorFmt != GL_NONE)
            checkFormatSupport(m_colorFmt);
        if (m_depthStencilFmt != GL_NONE)
            checkFormatSupport(m_depthStencilFmt);
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat colorFmt = glu::mapGLInternalFormat(m_colorFmt);
        tcu::TextureFormat depthStencilFmt =
            m_depthStencilFmt != GL_NONE ? glu::mapGLInternalFormat(m_depthStencilFmt) : tcu::TextureFormat();
        tcu::TextureFormatInfo colorFmtInfo = tcu::getTextureFormatInfo(colorFmt);
        bool depth = depthStencilFmt.order == tcu::TextureFormat::D || depthStencilFmt.order == tcu::TextureFormat::DS;
        bool stencil =
            depthStencilFmt.order == tcu::TextureFormat::S || depthStencilFmt.order == tcu::TextureFormat::DS;
        uint32_t fbo             = 0;
        uint32_t colorTex        = 0;
        uint32_t depthStencilTex = 0;
        int invalidateX          = 0;
        int invalidateY          = 0;
        int invalidateW          = getWidth() / 2;
        int invalidateH          = getHeight();
        int readX                = invalidateW;
        int readY                = 0;
        int readW                = getWidth() / 2;
        int readH                = getHeight();
        GradientShader gradShader(getFragmentOutputType(colorFmt));
        vector<uint32_t> attachments = getFBODiscardAttachments(m_invalidateBuffers);
        uint32_t gradShaderID        = getCurrentContext()->createProgram(&gradShader);

        // Create fbo.
        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(colorFmt);

            glGenTextures(1, &colorTex);
            glBindTexture(GL_TEXTURE_2D, colorTex);
            glTexImage2D(GL_TEXTURE_2D, 0, m_colorFmt, getWidth(), getHeight(), 0, transferFmt.format,
                         transferFmt.dataType, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        if (m_depthStencilFmt != GL_NONE)
        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(depthStencilFmt);

            glGenTextures(1, &depthStencilTex);
            glBindTexture(GL_TEXTURE_2D, depthStencilTex);
            glTexImage2D(GL_TEXTURE_2D, 0, m_depthStencilFmt, getWidth(), getHeight(), 0, transferFmt.format,
                         transferFmt.dataType, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        if (depth)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthStencilTex, 0);

        if (stencil)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthStencilTex, 0);

        checkFramebufferStatus(GL_FRAMEBUFFER);

        clearColorBuffer(colorFmt, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        gradShader.setGradient(*getCurrentContext(), gradShaderID, colorFmtInfo.valueMin, colorFmtInfo.valueMax);
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        glInvalidateSubFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(), &attachments[0], invalidateX, invalidateY,
                                   invalidateW, invalidateH);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);

        glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Limit read area using scissor.
        glScissor(readX, readY, readW, readH);
        glEnable(GL_SCISSOR_TEST);

        if ((m_invalidateBuffers & GL_COLOR_BUFFER_BIT) != 0)
        {
            // Render color.
            Texture2DShader texShader(DataTypes() << glu::getSampler2DType(colorFmt), glu::TYPE_FLOAT_VEC4);
            uint32_t texShaderID = getCurrentContext()->createProgram(&texShader);

            texShader.setTexScaleBias(0, colorFmtInfo.lookupScale, colorFmtInfo.lookupBias);
            texShader.setUniforms(*getCurrentContext(), texShaderID);

            glBindTexture(GL_TEXTURE_2D, colorTex);
            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }
        else
        {
            // Render depth.
            Texture2DShader texShader(DataTypes() << glu::getSampler2DType(depthStencilFmt), glu::TYPE_FLOAT_VEC4);
            uint32_t texShaderID = getCurrentContext()->createProgram(&texShader);

            texShader.setUniforms(*getCurrentContext(), texShaderID);

            glBindTexture(GL_TEXTURE_2D, depthStencilTex);
            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

    bool compare(const tcu::Surface &reference, const tcu::Surface &result)
    {
        const tcu::RGBA threshold(tcu::max(getFormatThreshold(m_colorFmt), tcu::RGBA(12, 12, 12, 12)));

        return tcu::bilinearCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference.getAccess(),
                                    result.getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
    }

private:
    uint32_t m_colorFmt;
    uint32_t m_depthStencilFmt;
    uint32_t m_invalidateBuffers;
};

class InvalidateSubFboUnbindBlitCase : public FboTestCase
{
public:
    InvalidateSubFboUnbindBlitCase(Context &context, const char *name, const char *description, int numSamples,
                                   uint32_t invalidateBuffers)
        : FboTestCase(context, name, description,
                      numSamples >
                          0) // \note Use fullscreen viewport when multisampling - we can't allow GLES3Context do its
        //         behing-the-scenes viewport position randomization, because with glBlitFramebuffer,
        //         source and destination rectangles must match when multisampling.
        , m_colorFmt(0)
        , m_depthStencilFmt(0)
        , m_numSamples(numSamples)
        , m_invalidateBuffers(invalidateBuffers)
    {
        // Figure out formats that are compatible with default framebuffer.
        m_colorFmt        = getCompatibleColorFormat(m_context.getRenderTarget());
        m_depthStencilFmt = getCompatibleDepthStencilFormat(m_context.getRenderTarget());
    }

protected:
    void preCheck(void)
    {
        if (m_context.getRenderTarget().getNumSamples() > 0)
            throw tcu::NotSupportedError("Not supported in MSAA config");

        if (m_colorFmt == GL_NONE)
            throw tcu::NotSupportedError("Unsupported color format");

        if (m_depthStencilFmt == GL_NONE)
            throw tcu::NotSupportedError("Unsupported depth/stencil format");

        checkFormatSupport(m_colorFmt);
        checkFormatSupport(m_depthStencilFmt);
    }

    void render(tcu::Surface &dst)
    {
        // \note When using fullscreen viewport (when m_numSamples > 0), still only use a 128x128 pixel quad at most.
        IVec2 quadSizePixels(m_numSamples == 0 ? getWidth() : de::min(128, getWidth()),
                             m_numSamples == 0 ? getHeight() : de::min(128, getHeight()));
        Vec2 quadNDCLeftBottomXY(-1.0f, -1.0f);
        Vec2 quadNDCSize(2.0f * (float)quadSizePixels.x() / (float)getWidth(),
                         2.0f * (float)quadSizePixels.y() / (float)getHeight());
        Vec2 quadNDCRightTopXY = quadNDCLeftBottomXY + quadNDCSize;
        tcu::TextureFormat depthStencilFmt =
            m_depthStencilFmt != GL_NONE ? glu::mapGLInternalFormat(m_depthStencilFmt) : tcu::TextureFormat();
        bool depth = depthStencilFmt.order == tcu::TextureFormat::D || depthStencilFmt.order == tcu::TextureFormat::DS;
        bool stencil =
            depthStencilFmt.order == tcu::TextureFormat::S || depthStencilFmt.order == tcu::TextureFormat::DS;
        uint32_t fbo             = 0;
        uint32_t colorRbo        = 0;
        uint32_t depthStencilRbo = 0;
        int invalidateX          = 0;
        int invalidateY          = 0;
        int invalidateW          = quadSizePixels.x() / 2;
        int invalidateH          = quadSizePixels.y();
        int blitX0               = invalidateW;
        int blitY0               = 0;
        int blitX1               = blitX0 + quadSizePixels.x() / 2;
        int blitY1               = blitY0 + quadSizePixels.y();
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        vector<uint32_t> attachments = getFBODiscardAttachments(m_invalidateBuffers);
        uint32_t flatShaderID        = getCurrentContext()->createProgram(&flatShader);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Create fbo.
        glGenRenderbuffers(1, &colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_numSamples, m_colorFmt, quadSizePixels.x(),
                                         quadSizePixels.y());

        if (m_depthStencilFmt != GL_NONE)
        {
            glGenRenderbuffers(1, &depthStencilRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_numSamples, m_depthStencilFmt, quadSizePixels.x(),
                                             quadSizePixels.y());
        }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);

        if (depth)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        if (stencil)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        checkFramebufferStatus(GL_FRAMEBUFFER);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID,
                       Vec3(quadNDCLeftBottomXY.x(), quadNDCLeftBottomXY.y(), -1.0f),
                       Vec3(quadNDCRightTopXY.x(), quadNDCRightTopXY.y(), 1.0f));

        glInvalidateSubFramebuffer(GL_FRAMEBUFFER, (int)attachments.size(), &attachments[0], invalidateX, invalidateY,
                                   invalidateW, invalidateH);

        // Set default framebuffer as draw framebuffer and blit preserved buffers.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(blitX0, blitY0, blitX1, blitY1, blitX0, blitY0, blitX1, blitY1,
                          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) & ~m_invalidateBuffers,
                          GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        if ((m_invalidateBuffers & GL_COLOR_BUFFER_BIT) != 0)
        {
            // Color was not preserved - fill with green.
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_STENCIL_TEST);

            flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));
            sglr::drawQuad(*getCurrentContext(), flatShaderID,
                           Vec3(quadNDCLeftBottomXY.x(), quadNDCLeftBottomXY.y(), 0.0f),
                           Vec3(quadNDCRightTopXY.x(), quadNDCRightTopXY.y(), 0.0f));

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_STENCIL_TEST);
        }

        if ((m_invalidateBuffers & GL_DEPTH_BUFFER_BIT) != 0)
        {
            // Depth was not preserved.
            glDepthFunc(GL_ALWAYS);
        }

        if ((m_invalidateBuffers & GL_STENCIL_BUFFER_BIT) == 0)
        {
            // Stencil was preserved.
            glStencilFunc(GL_EQUAL, 1, 0xff);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(quadNDCLeftBottomXY.x(), quadNDCLeftBottomXY.y(), 0.0f),
                       Vec3(quadNDCRightTopXY.x(), quadNDCRightTopXY.y(), 0.0f));

        readPixels(dst, 0, 0, quadSizePixels.x(), quadSizePixels.y());
    }

private:
    uint32_t m_colorFmt;
    uint32_t m_depthStencilFmt;
    int m_numSamples;
    uint32_t m_invalidateBuffers;
};

class InvalidateFboTargetCase : public FboTestCase
{
public:
    InvalidateFboTargetCase(Context &context, const char *name, const char *description, uint32_t boundTarget,
                            uint32_t invalidateTarget, const uint32_t *invalidateAttachments, int numAttachments)
        : FboTestCase(context, name, description)
        , m_boundTarget(boundTarget)
        , m_invalidateTarget(invalidateTarget)
        , m_invalidateAttachments(invalidateAttachments, invalidateAttachments + numAttachments)
    {
    }

protected:
    void render(tcu::Surface &dst)
    {
        const uint32_t colorFormat                = GL_RGBA8;
        const uint32_t depthStencilFormat         = GL_DEPTH24_STENCIL8;
        const tcu::TextureFormat colorFmt         = glu::mapGLInternalFormat(colorFormat);
        const tcu::TextureFormatInfo colorFmtInfo = tcu::getTextureFormatInfo(colorFmt);
        const tcu::Vec4 &cBias                    = colorFmtInfo.valueMin;
        const tcu::Vec4 cScale                    = colorFmtInfo.valueMax - colorFmtInfo.valueMin;
        const bool isDiscarded                    = (m_boundTarget == GL_FRAMEBUFFER) ||
                                 (m_invalidateTarget == GL_FRAMEBUFFER && m_boundTarget == GL_DRAW_FRAMEBUFFER) ||
                                 (m_invalidateTarget == m_boundTarget);
        const bool isColorDiscarded = isDiscarded && hasAttachment(m_invalidateAttachments, GL_COLOR_ATTACHMENT0);
        const bool isDepthDiscarded =
            isDiscarded && (hasAttachment(m_invalidateAttachments, GL_DEPTH_ATTACHMENT) ||
                            hasAttachment(m_invalidateAttachments, GL_DEPTH_STENCIL_ATTACHMENT));
        const bool isStencilDiscarded =
            isDiscarded && (hasAttachment(m_invalidateAttachments, GL_STENCIL_ATTACHMENT) ||
                            hasAttachment(m_invalidateAttachments, GL_DEPTH_STENCIL_ATTACHMENT));

        uint32_t fbo             = 0;
        uint32_t colorRbo        = 0;
        uint32_t depthStencilRbo = 0;
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
        uint32_t flatShaderID = getCurrentContext()->createProgram(&flatShader);

        // Create fbo.
        glGenRenderbuffers(1, &colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, getWidth(), getHeight());

        glGenRenderbuffers(1, &depthStencilRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, depthStencilFormat, getWidth(), getHeight());

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

        checkFramebufferStatus(GL_FRAMEBUFFER);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xff);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias);
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Bound FBO to test target and default to other
        if (m_boundTarget != GL_FRAMEBUFFER)
        {
            // Unused fbo is used as complemeting target (read when discarding draw for example).
            // \note Framework takes care of deleting objects at the end of test case.
            const uint32_t unusedTarget =
                m_boundTarget == GL_DRAW_FRAMEBUFFER ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;
            uint32_t unusedFbo      = 0;
            uint32_t unusedColorRbo = 0;

            glGenRenderbuffers(1, &unusedColorRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, unusedColorRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
            glGenFramebuffers(1, &unusedFbo);
            glBindFramebuffer(unusedTarget, unusedFbo);
            glFramebufferRenderbuffer(unusedTarget, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, unusedColorRbo);

            glBindFramebuffer(m_boundTarget, fbo);
        }

        glInvalidateFramebuffer(m_invalidateTarget, (int)m_invalidateAttachments.size(),
                                m_invalidateAttachments.empty() ? nullptr : &m_invalidateAttachments[0]);

        if (m_boundTarget != GL_FRAMEBUFFER)
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        if (isColorDiscarded)
        {
            // Color was not preserved - fill with green.
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_STENCIL_TEST);

            flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias);
            sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_STENCIL_TEST);
        }

        if (isDepthDiscarded)
        {
            // Depth was not preserved.
            glDepthFunc(GL_ALWAYS);
        }

        if (!isStencilDiscarded)
        {
            // Stencil was preserved.
            glStencilFunc(GL_EQUAL, 1, 0xff);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f) * cScale + cBias);
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight(), colorFmt, colorFmtInfo.lookupScale, colorFmtInfo.lookupBias);
    }

private:
    uint32_t m_boundTarget;
    uint32_t m_invalidateTarget;
    std::vector<uint32_t> m_invalidateAttachments;
};

FboInvalidateTests::FboInvalidateTests(Context &context)
    : TestCaseGroup(context, "invalidate", "Framebuffer invalidate tests")
{
}

FboInvalidateTests::~FboInvalidateTests(void)
{
}

void FboInvalidateTests::init(void)
{
    // invalidate.default.
    {
        tcu::TestCaseGroup *defaultFbGroup =
            new tcu::TestCaseGroup(m_testCtx, "default", "Default framebuffer invalidate tests");
        addChild(defaultFbGroup);

        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(m_context, "render_none",
                                                                            "Invalidating no framebuffers (ref)", 0));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "render_color", "Rendering after invalidating colorbuffer", GL_COLOR_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "render_depth", "Rendering after invalidating depthbuffer", GL_DEPTH_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "render_stencil", "Rendering after invalidating stencilbuffer", GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "render_depth_stencil", "Rendering after invalidating depth- and stencilbuffers",
            GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "render_all", "Rendering after invalidating all buffers",
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        defaultFbGroup->addChild(new InvalidateDefaultFramebufferBindCase(
            m_context, "bind_color", "Binding fbo after invalidating colorbuffer", GL_COLOR_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferBindCase(
            m_context, "bind_depth", "Binding fbo after invalidating depthbuffer", GL_DEPTH_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferBindCase(
            m_context, "bind_stencil", "Binding fbo after invalidating stencilbuffer", GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferBindCase(
            m_context, "bind_depth_stencil", "Binding fbo after invalidating depth- and stencilbuffers",
            GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferBindCase(
            m_context, "bind_all", "Binding fbo after invalidating all buffers",
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferRenderCase(
            m_context, "sub_render_color", "Rendering after invalidating colorbuffer", GL_COLOR_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferRenderCase(
            m_context, "sub_render_depth", "Rendering after invalidating depthbuffer", GL_DEPTH_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferRenderCase(
            m_context, "sub_render_stencil", "Rendering after invalidating stencilbuffer", GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferRenderCase(
            m_context, "sub_render_depth_stencil", "Rendering after invalidating depth- and stencilbuffers",
            GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferRenderCase(
            m_context, "sub_render_all", "Rendering after invalidating all buffers",
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferBindCase(
            m_context, "sub_bind_color", "Binding fbo after invalidating colorbuffer", GL_COLOR_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferBindCase(
            m_context, "sub_bind_depth", "Binding fbo after invalidating depthbuffer", GL_DEPTH_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferBindCase(
            m_context, "sub_bind_stencil", "Binding fbo after invalidating stencilbuffer", GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferBindCase(
            m_context, "sub_bind_depth_stencil", "Binding fbo after invalidating depth- and stencilbuffers",
            GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        defaultFbGroup->addChild(new InvalidateDefaultSubFramebufferBindCase(
            m_context, "sub_bind_all", "Binding fbo after invalidating all buffers",
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "draw_framebuffer_color", "Invalidating GL_COLOR in GL_DRAW_FRAMEBUFFER", GL_COLOR_BUFFER_BIT,
            GL_DRAW_FRAMEBUFFER));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "draw_framebuffer_all", "Invalidating all in GL_DRAW_FRAMEBUFFER",
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_DRAW_FRAMEBUFFER));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "read_framebuffer_color", "Invalidating GL_COLOR in GL_READ_FRAMEBUFFER", GL_COLOR_BUFFER_BIT,
            GL_READ_FRAMEBUFFER));
        defaultFbGroup->addChild(new InvalidateDefaultFramebufferRenderCase(
            m_context, "read_framebuffer_all", "Invalidating all in GL_READ_FRAMEBUFFER",
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_READ_FRAMEBUFFER));
    }

    // invalidate.whole.
    {
        tcu::TestCaseGroup *wholeFboGroup =
            new tcu::TestCaseGroup(m_testCtx, "whole", "Invalidating whole framebuffer object");
        addChild(wholeFboGroup);

        wholeFboGroup->addChild(
            new InvalidateFboRenderCase(m_context, "render_none", "", GL_RGBA8, GL_DEPTH24_STENCIL8, 0));
        wholeFboGroup->addChild(new InvalidateFboRenderCase(m_context, "render_color", "", GL_RGBA8,
                                                            GL_DEPTH24_STENCIL8, GL_COLOR_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboRenderCase(m_context, "render_depth", "", GL_RGBA8,
                                                            GL_DEPTH24_STENCIL8, GL_DEPTH_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboRenderCase(m_context, "render_stencil", "", GL_RGBA8,
                                                            GL_DEPTH24_STENCIL8, GL_STENCIL_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboRenderCase(m_context, "render_depth_stencil", "", GL_RGBA8,
                                                            GL_DEPTH24_STENCIL8,
                                                            GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        wholeFboGroup->addChild(
            new InvalidateFboRenderCase(m_context, "render_all", "", GL_RGBA8, GL_DEPTH24_STENCIL8,
                                        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        wholeFboGroup->addChild(new InvalidateFboUnbindReadCase(m_context, "unbind_read_color", "", GL_RGBA8,
                                                                GL_DEPTH24_STENCIL8, GL_COLOR_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboUnbindReadCase(m_context, "unbind_read_depth", "", GL_RGBA8,
                                                                GL_DEPTH24_STENCIL8, GL_DEPTH_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboUnbindReadCase(m_context, "unbind_read_stencil", "", GL_RGBA8,
                                                                GL_DEPTH24_STENCIL8, GL_STENCIL_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboUnbindReadCase(m_context, "unbind_read_depth_stencil", "", GL_RGBA8,
                                                                GL_DEPTH24_STENCIL8,
                                                                GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboUnbindReadCase(m_context, "unbind_read_color_stencil", "", GL_RGBA8,
                                                                GL_DEPTH24_STENCIL8,
                                                                GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        wholeFboGroup->addChild(
            new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_color", "", 0, GL_COLOR_BUFFER_BIT));
        wholeFboGroup->addChild(
            new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_depth", "", 0, GL_DEPTH_BUFFER_BIT));
        wholeFboGroup->addChild(
            new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_stencil", "", 0, GL_STENCIL_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_depth_stencil", "", 0,
                                                                GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        wholeFboGroup->addChild(
            new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_msaa_color", "", 4, GL_COLOR_BUFFER_BIT));
        wholeFboGroup->addChild(
            new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_msaa_depth", "", 4, GL_DEPTH_BUFFER_BIT));
        wholeFboGroup->addChild(
            new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_msaa_stencil", "", 4, GL_STENCIL_BUFFER_BIT));
        wholeFboGroup->addChild(new InvalidateFboUnbindBlitCase(m_context, "unbind_blit_msaa_depth_stencil", "", 4,
                                                                GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }

    // invalidate.sub.
    {
        tcu::TestCaseGroup *subFboGroup =
            new tcu::TestCaseGroup(m_testCtx, "sub", "Invalidating subsection of framebuffer object");
        addChild(subFboGroup);

        subFboGroup->addChild(
            new InvalidateSubFboRenderCase(m_context, "render_none", "", GL_RGBA8, GL_DEPTH24_STENCIL8, 0));
        subFboGroup->addChild(new InvalidateSubFboRenderCase(m_context, "render_color", "", GL_RGBA8,
                                                             GL_DEPTH24_STENCIL8, GL_COLOR_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboRenderCase(m_context, "render_depth", "", GL_RGBA8,
                                                             GL_DEPTH24_STENCIL8, GL_DEPTH_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboRenderCase(m_context, "render_stencil", "", GL_RGBA8,
                                                             GL_DEPTH24_STENCIL8, GL_STENCIL_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboRenderCase(m_context, "render_depth_stencil", "", GL_RGBA8,
                                                             GL_DEPTH24_STENCIL8,
                                                             GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        subFboGroup->addChild(
            new InvalidateSubFboRenderCase(m_context, "render_all", "", GL_RGBA8, GL_DEPTH24_STENCIL8,
                                           GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        subFboGroup->addChild(new InvalidateSubFboUnbindReadCase(m_context, "unbind_read_color", "", GL_RGBA8,
                                                                 GL_DEPTH24_STENCIL8, GL_COLOR_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboUnbindReadCase(m_context, "unbind_read_depth", "", GL_RGBA8,
                                                                 GL_DEPTH24_STENCIL8, GL_DEPTH_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboUnbindReadCase(m_context, "unbind_read_stencil", "", GL_RGBA8,
                                                                 GL_DEPTH24_STENCIL8, GL_STENCIL_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboUnbindReadCase(m_context, "unbind_read_depth_stencil", "", GL_RGBA8,
                                                                 GL_DEPTH24_STENCIL8,
                                                                 GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboUnbindReadCase(m_context, "unbind_read_color_stencil", "", GL_RGBA8,
                                                                 GL_DEPTH24_STENCIL8,
                                                                 GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        subFboGroup->addChild(
            new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_color", "", 0, GL_COLOR_BUFFER_BIT));
        subFboGroup->addChild(
            new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_depth", "", 0, GL_DEPTH_BUFFER_BIT));
        subFboGroup->addChild(
            new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_stencil", "", 0, GL_STENCIL_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_depth_stencil", "", 0,
                                                                 GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        subFboGroup->addChild(
            new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_msaa_color", "", 4, GL_COLOR_BUFFER_BIT));
        subFboGroup->addChild(
            new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_msaa_depth", "", 4, GL_DEPTH_BUFFER_BIT));
        subFboGroup->addChild(
            new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_msaa_stencil", "", 4, GL_STENCIL_BUFFER_BIT));
        subFboGroup->addChild(new InvalidateSubFboUnbindBlitCase(m_context, "unbind_blit_msaa_depth_stencil", "", 4,
                                                                 GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }

    // invalidate.format.
    {
        tcu::TestCaseGroup *formatGroup =
            new tcu::TestCaseGroup(m_testCtx, "format", "Invalidating framebuffers with selected formats");
        addChild(formatGroup);

        // Color buffer formats.
        static const uint32_t colorFormats[] = {
            // RGBA formats
            GL_RGBA32I, GL_RGBA32UI, GL_RGBA16I, GL_RGBA16UI, GL_RGBA8, GL_RGBA8I, GL_RGBA8UI, GL_SRGB8_ALPHA8,
            GL_RGB10_A2, GL_RGB10_A2UI, GL_RGBA4, GL_RGB5_A1,

            // RGB formats
            GL_RGB8, GL_RGB565,

            // RG formats
            GL_RG32I, GL_RG32UI, GL_RG16I, GL_RG16UI, GL_RG8, GL_RG8I, GL_RG8UI,

            // R formats
            GL_R32I, GL_R32UI, GL_R16I, GL_R16UI, GL_R8, GL_R8I, GL_R8UI,

            // GL_EXT_color_buffer_float
            GL_RGBA32F, GL_RGBA16F, GL_R11F_G11F_B10F, GL_RG32F, GL_RG16F, GL_R32F, GL_R16F};

        // Depth/stencilbuffer formats.
        static const uint32_t depthStencilFormats[] = {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT24,
                                                       GL_DEPTH_COMPONENT16,  GL_DEPTH32F_STENCIL8,
                                                       GL_DEPTH24_STENCIL8,   GL_STENCIL_INDEX8};

        // Colorbuffer tests use invalidate, unbind, read test.
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(colorFormats); ndx++)
            formatGroup->addChild(new InvalidateSubFboUnbindReadCase(m_context, getFormatName(colorFormats[ndx]), "",
                                                                     colorFormats[ndx], GL_NONE, GL_COLOR_BUFFER_BIT));

        // Depth/stencilbuffer tests use invalidate, render test.
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilFormats); ndx++)
            formatGroup->addChild(new InvalidateSubFboRenderCase(m_context, getFormatName(depthStencilFormats[ndx]), "",
                                                                 GL_RGBA8, depthStencilFormats[ndx],
                                                                 GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }

    // invalidate.target
    {
        tcu::TestCaseGroup *targetGroup = new tcu::TestCaseGroup(m_testCtx, "target", "Invalidate target");
        addChild(targetGroup);

        static const struct
        {
            const char *name;
            uint32_t invalidateTarget;
            uint32_t boundTarget;
        } s_targetCases[] = {
            {"framebuffer_framebuffer", GL_FRAMEBUFFER, GL_FRAMEBUFFER},
            {"framebuffer_read_framebuffer", GL_FRAMEBUFFER, GL_READ_FRAMEBUFFER},
            {"framebuffer_draw_framebuffer", GL_FRAMEBUFFER, GL_DRAW_FRAMEBUFFER},
            {"read_framebuffer_framebuffer", GL_READ_FRAMEBUFFER, GL_FRAMEBUFFER},
            {"read_framebuffer_read_framebuffer", GL_READ_FRAMEBUFFER, GL_READ_FRAMEBUFFER},
            {"read_framebuffer_draw_framebuffer", GL_READ_FRAMEBUFFER, GL_DRAW_FRAMEBUFFER},
            {"draw_framebuffer_framebuffer", GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER},
            {"draw_framebuffer_read_framebuffer", GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER},
            {"draw_framebuffer_draw_framebuffer", GL_DRAW_FRAMEBUFFER, GL_DRAW_FRAMEBUFFER},
        };

        static const uint32_t colorAttachment[]        = {GL_COLOR_ATTACHMENT0};
        static const uint32_t depthStencilAttachment[] = {GL_DEPTH_STENCIL_ATTACHMENT};
        static const uint32_t allAttachments[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};

        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(s_targetCases); caseNdx++)
        {
            const std::string baseName = s_targetCases[caseNdx].name;
            const uint32_t invalidateT = s_targetCases[caseNdx].invalidateTarget;
            const uint32_t boundT      = s_targetCases[caseNdx].boundTarget;

            targetGroup->addChild(new InvalidateFboTargetCase(m_context, (baseName + "_color").c_str(), "", boundT,
                                                              invalidateT, &colorAttachment[0],
                                                              DE_LENGTH_OF_ARRAY(colorAttachment)));
            targetGroup->addChild(new InvalidateFboTargetCase(m_context, (baseName + "_depth_stencil").c_str(), "",
                                                              boundT, invalidateT, &depthStencilAttachment[0],
                                                              DE_LENGTH_OF_ARRAY(depthStencilAttachment)));
            targetGroup->addChild(new InvalidateFboTargetCase(m_context, (baseName + "_all").c_str(), "", boundT,
                                                              invalidateT, &allAttachments[0],
                                                              DE_LENGTH_OF_ARRAY(allAttachments)));
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
