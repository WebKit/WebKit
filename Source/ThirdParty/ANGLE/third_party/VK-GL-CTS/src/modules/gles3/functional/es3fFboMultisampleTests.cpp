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
 * \brief FBO multisample tests.
 *//*--------------------------------------------------------------------*/

#include "es3fApiCase.hpp"
#include "es3fFboMultisampleTests.hpp"
#include "es3fFboTestCase.hpp"
#include "es3fFboTestUtil.hpp"
#include "gluTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
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
using tcu::TestLog;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace FboTestUtil;

class BasicFboMultisampleCase : public FboTestCase
{
public:
    BasicFboMultisampleCase(Context &context, const char *name, const char *desc, uint32_t colorFormat,
                            uint32_t depthStencilFormat, const IVec2 &size, int numSamples)
        : FboTestCase(context, name, desc)
        , m_colorFormat(colorFormat)
        , m_depthStencilFormat(depthStencilFormat)
        , m_size(size)
        , m_numSamples(numSamples)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_colorFormat);
        checkSampleCount(m_colorFormat, m_numSamples);

        if (m_depthStencilFormat != GL_NONE)
        {
            checkFormatSupport(m_depthStencilFormat);
            checkSampleCount(m_depthStencilFormat, m_numSamples);
        }
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat colorFmt = glu::mapGLInternalFormat(m_colorFormat);
        tcu::TextureFormat depthStencilFmt =
            m_depthStencilFormat != GL_NONE ? glu::mapGLInternalFormat(m_depthStencilFormat) : tcu::TextureFormat();
        tcu::TextureFormatInfo colorFmtInfo = tcu::getTextureFormatInfo(colorFmt);
        bool depth = depthStencilFmt.order == tcu::TextureFormat::D || depthStencilFmt.order == tcu::TextureFormat::DS;
        bool stencil =
            depthStencilFmt.order == tcu::TextureFormat::S || depthStencilFmt.order == tcu::TextureFormat::DS;
        GradientShader gradShader(getFragmentOutputType(colorFmt));
        FlatColorShader flatShader(getFragmentOutputType(colorFmt));
        uint32_t gradShaderID           = getCurrentContext()->createProgram(&gradShader);
        uint32_t flatShaderID           = getCurrentContext()->createProgram(&flatShader);
        uint32_t msaaFbo                = 0;
        uint32_t resolveFbo             = 0;
        uint32_t msaaColorRbo           = 0;
        uint32_t resolveColorRbo        = 0;
        uint32_t msaaDepthStencilRbo    = 0;
        uint32_t resolveDepthStencilRbo = 0;

        // Create framebuffers.
        for (int ndx = 0; ndx < 2; ndx++)
        {
            uint32_t &fbo             = ndx ? resolveFbo : msaaFbo;
            uint32_t &colorRbo        = ndx ? resolveColorRbo : msaaColorRbo;
            uint32_t &depthStencilRbo = ndx ? resolveDepthStencilRbo : msaaDepthStencilRbo;
            int samples               = ndx ? 0 : m_numSamples;

            glGenRenderbuffers(1, &colorRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, m_colorFormat, m_size.x(), m_size.y());

            if (depth || stencil)
            {
                glGenRenderbuffers(1, &depthStencilRbo);
                glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRbo);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, m_depthStencilFormat, m_size.x(),
                                                 m_size.y());
            }

            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
            if (depth)
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);
            if (stencil)
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRbo);

            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo);
        glViewport(0, 0, m_size.x(), m_size.y());

        // Clear depth and stencil buffers.
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

        // Fill MSAA fbo with gradient, depth = [-1..1]
        glEnable(GL_DEPTH_TEST);
        gradShader.setGradient(*getCurrentContext(), gradShaderID, colorFmtInfo.valueMin, colorFmtInfo.valueMax);
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Render random-colored quads.
        {
            const int numQuads = 8;
            de::Random rnd(9);

            glDepthFunc(GL_ALWAYS);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 0, 0xffu);
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

            for (int ndx = 0; ndx < numQuads; ndx++)
            {
                float r  = rnd.getFloat();
                float g  = rnd.getFloat();
                float b  = rnd.getFloat();
                float a  = rnd.getFloat();
                float x0 = rnd.getFloat(-1.0f, 1.0f);
                float y0 = rnd.getFloat(-1.0f, 1.0f);
                float z0 = rnd.getFloat(-1.0f, 1.0f);
                float x1 = rnd.getFloat(-1.0f, 1.0f);
                float y1 = rnd.getFloat(-1.0f, 1.0f);
                float z1 = rnd.getFloat(-1.0f, 1.0f);

                flatShader.setColor(*getCurrentContext(), flatShaderID,
                                    Vec4(r, g, b, a) * (colorFmtInfo.valueMax - colorFmtInfo.valueMin) +
                                        colorFmtInfo.valueMin);
                sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(x0, y0, z0), Vec3(x1, y1, z1));
            }
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        checkError();

        // Resolve using glBlitFramebuffer().
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo);
        glBlitFramebuffer(0, 0, m_size.x(), m_size.y(), 0, 0, m_size.x(), m_size.y(),
                          GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0) |
                              (stencil ? GL_STENCIL_BUFFER_BIT : 0),
                          GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFbo);

        if (depth)
        {
            // Visualize depth.
            const int numSteps = 8;
            const float step   = 2.0f / (float)numSteps;

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_FALSE);
            glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);

            for (int ndx = 0; ndx < numSteps; ndx++)
            {
                float d = -1.0f + step * (float)ndx;
                float c = (float)ndx / (float)(numSteps - 1);

                flatShader.setColor(*getCurrentContext(), flatShaderID,
                                    Vec4(0.0f, 0.0f, c, 1.0f) * (colorFmtInfo.valueMax - colorFmtInfo.valueMin) +
                                        colorFmtInfo.valueMin);
                sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, d), Vec3(1.0f, 1.0f, d));
            }

            glDisable(GL_DEPTH_TEST);
        }

        if (stencil)
        {
            // Visualize stencil.
            const int numSteps = 4;
            const int step     = 1;

            glEnable(GL_STENCIL_TEST);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);

            for (int ndx = 0; ndx < numSteps; ndx++)
            {
                int s   = step * ndx;
                float c = (float)ndx / (float)(numSteps - 1);

                glStencilFunc(GL_EQUAL, s, 0xffu);

                flatShader.setColor(*getCurrentContext(), flatShaderID,
                                    Vec4(0.0f, c, 0.0f, 1.0f) * (colorFmtInfo.valueMax - colorFmtInfo.valueMin) +
                                        colorFmtInfo.valueMin);
                sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            }

            glDisable(GL_STENCIL_TEST);
        }

        readPixels(dst, 0, 0, m_size.x(), m_size.y(), colorFmt, colorFmtInfo.lookupScale, colorFmtInfo.lookupBias);
    }

    bool colorCompare(const tcu::Surface &reference, const tcu::Surface &result)
    {
        const tcu::RGBA threshold(tcu::max(getFormatThreshold(m_colorFormat), tcu::RGBA(12, 12, 12, 12)));

        return tcu::bilinearCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference.getAccess(),
                                    result.getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
    }

    bool compare(const tcu::Surface &reference, const tcu::Surface &result)
    {
        if (m_depthStencilFormat != GL_NONE)
            return FboTestCase::compare(reference, result);
        else
            return colorCompare(reference, result);
    }

private:
    uint32_t m_colorFormat;
    uint32_t m_depthStencilFormat;
    IVec2 m_size;
    int m_numSamples;
};

// Ported from WebGL [1], originally written to test a Qualcomm driver bug [2].
// [1] https://github.com/KhronosGroup/WebGL/blob/main/sdk/tests/conformance2/renderbuffers/multisampled-renderbuffer-initialization.html
// [2] http://crbug.com/696126
class RenderbufferResizeCase : public ApiCase
{
public:
    RenderbufferResizeCase(Context &context, const char *name, const char *desc, bool multisampled1, bool multisampled2)
        : ApiCase(context, name, desc)
        , m_multisampled1(multisampled1)
        , m_multisampled2(multisampled2)
    {
    }

protected:
    void test()
    {
        glDisable(GL_DEPTH_TEST);

        int maxSamples = 0;
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES, 1, &maxSamples);
        uint32_t samp1 = m_multisampled1 ? maxSamples : 0;
        uint32_t samp2 = m_multisampled2 ? maxSamples : 0;

        static const uint32_t W1 = 10, H1 = 10;
        static const uint32_t W2 = 40, H2 = 40;

        // Set up non-multisampled buffer to blit to and read back from.
        uint32_t fboResolve = 0;
        uint32_t rboResolve = 0;
        {
            glGenFramebuffers(1, &fboResolve);
            glBindFramebuffer(GL_FRAMEBUFFER, fboResolve);
            glGenRenderbuffers(1, &rboResolve);
            glBindRenderbuffer(GL_RENDERBUFFER, rboResolve);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W2, H2);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboResolve);
            TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            glClearBufferfv(GL_COLOR, 0, Vec4(1.0f, 0.0f, 0.0f, 1.0f).getPtr());
        }
        expectError(GL_NO_ERROR);

        // Set up multisampled buffer to test.
        uint32_t fboMultisampled = 0;
        uint32_t rboMultisampled = 0;
        {
            glGenFramebuffers(1, &fboMultisampled);
            glBindFramebuffer(GL_FRAMEBUFFER, fboMultisampled);
            glGenRenderbuffers(1, &rboMultisampled);
            glBindRenderbuffer(GL_RENDERBUFFER, rboMultisampled);
            // Allocate,
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samp1, GL_RGBA8, W1, H1);
            // attach,
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboMultisampled);
            TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            glClearBufferfv(GL_COLOR, 0, Vec4(0.0f, 0.0f, 1.0f, 1.0f).getPtr());
            // and allocate again with different parameters.
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samp2, GL_RGBA8, W2, H2);
            TCU_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            glClearBufferfv(GL_COLOR, 0, Vec4(0.0f, 1.0f, 0.0f, 1.0f).getPtr());
        }
        expectError(GL_NO_ERROR);

        // This is a blit from the multisampled buffer to the non-multisampled buffer.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fboMultisampled);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboResolve);
        // Blit color from fboMultisampled (should be green) to fboResolve (should currently be red).
        glBlitFramebuffer(0, 0, W2, H2, 0, 0, W2, H2, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        expectError(GL_NO_ERROR);

        // fboResolve should now be green.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fboResolve);
        uint32_t pixels[W2 * H2] = {};
        glReadPixels(0, 0, W2, H2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        expectError(GL_NO_ERROR);

        const tcu::RGBA threshold(tcu::max(getFormatThreshold(GL_RGBA8), tcu::RGBA(12, 12, 12, 12)));
        for (uint32_t y = 0; y < H2; ++y)
        {
            for (uint32_t x = 0; x < W2; ++x)
            {
                tcu::RGBA color(pixels[y * W2 + x]);
                TCU_CHECK(compareThreshold(color, tcu::RGBA::green(), threshold));
            }
        }
    }

private:
    bool m_multisampled1;
    bool m_multisampled2;
};

FboMultisampleTests::FboMultisampleTests(Context &context) : TestCaseGroup(context, "msaa", "Multisample FBO tests")
{
}

FboMultisampleTests::~FboMultisampleTests(void)
{
}

void FboMultisampleTests::init(void)
{
    static const uint32_t colorFormats[] = {// RGBA formats
                                            GL_RGBA8, GL_SRGB8_ALPHA8, GL_RGB10_A2, GL_RGBA4, GL_RGB5_A1,

                                            // RGB formats
                                            GL_RGB8, GL_RGB565,

                                            // RG formats
                                            GL_RG8,

                                            // R formats
                                            GL_R8,

                                            // GL_EXT_color_buffer_float
                                            GL_RGBA32F, GL_RGBA16F, GL_R11F_G11F_B10F, GL_RG32F, GL_RG16F, GL_R32F,
                                            GL_R16F};

    static const uint32_t depthStencilFormats[] = {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16,
                                                   GL_DEPTH32F_STENCIL8,  GL_DEPTH24_STENCIL8,  GL_STENCIL_INDEX8};

    static const int sampleCounts[] = {2, 4, 8};

    for (int sampleCntNdx = 0; sampleCntNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCntNdx++)
    {
        int samples = sampleCounts[sampleCntNdx];
        tcu::TestCaseGroup *sampleCountGroup =
            new tcu::TestCaseGroup(m_testCtx, (de::toString(samples) + "_samples").c_str(), "");
        addChild(sampleCountGroup);

        // Color formats.
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
            sampleCountGroup->addChild(new BasicFboMultisampleCase(m_context, getFormatName(colorFormats[fmtNdx]), "",
                                                                   colorFormats[fmtNdx], GL_NONE, IVec2(119, 131),
                                                                   samples));

        // Depth/stencil formats.
        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats); fmtNdx++)
            sampleCountGroup->addChild(
                new BasicFboMultisampleCase(m_context, getFormatName(depthStencilFormats[fmtNdx]), "", GL_RGBA8,
                                            depthStencilFormats[fmtNdx], IVec2(119, 131), samples));
    }

    // .renderbuffer_resize
    {
        tcu::TestCaseGroup *group =
            new tcu::TestCaseGroup(m_testCtx, "renderbuffer_resize", "Multisample renderbuffer resize");
        addChild(group);

        {
            group->addChild(new RenderbufferResizeCase(m_context, "nonms_to_nonms", "", false, false));
            group->addChild(new RenderbufferResizeCase(m_context, "nonms_to_ms", "", false, true));
            group->addChild(new RenderbufferResizeCase(m_context, "ms_to_nonms", "", true, false));
            group->addChild(new RenderbufferResizeCase(m_context, "ms_to_ms", "", true, true));
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
