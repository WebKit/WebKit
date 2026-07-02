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
 * \brief Base class for FBO tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFboTestCase.hpp"
#include "es3fFboTestUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "gluStrUtil.hpp"
#include "gluContextInfo.hpp"
#include "deRandom.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <algorithm>

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using tcu::TestLog;

FboTestCase::FboTestCase(Context &context, const char *name, const char *description, bool useScreenSizedViewport)
    : TestCase(context, name, description)
    , m_viewportWidth(useScreenSizedViewport ? context.getRenderTarget().getWidth() : 128)
    , m_viewportHeight(useScreenSizedViewport ? context.getRenderTarget().getHeight() : 128)
{
}

FboTestCase::~FboTestCase(void)
{
}

FboTestCase::IterateResult FboTestCase::iterate(void)
{
    glu::RenderContext &renderCtx         = TestCase::m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();
    TestLog &log                          = m_testCtx.getLog();

    // Viewport.
    de::Random rnd(deStringHash(getName()));
    int width  = deMin32(renderTarget.getWidth(), m_viewportWidth);
    int height = deMin32(renderTarget.getHeight(), m_viewportHeight);
    int x      = rnd.getInt(0, renderTarget.getWidth() - width);
    int y      = rnd.getInt(0, renderTarget.getHeight() - height);

    // Surface format and storage is choosen by render().
    tcu::Surface reference;
    tcu::Surface result;

    // Call preCheck() that can throw exception if some requirement is not met.
    preCheck();

    // Render using GLES3.
    try
    {
        sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(x, y, width, height));
        setContext(&context);
        render(result);

        // Check error.
        uint32_t err = glGetError();
        if (err != GL_NO_ERROR)
            throw glu::Error(err, glu::getErrorStr(err).toString().c_str(), nullptr, __FILE__, __LINE__);

        setContext(nullptr);
    }
    catch (const FboTestUtil::FboIncompleteException &e)
    {
        if (e.getReason() == GL_FRAMEBUFFER_UNSUPPORTED)
        {
            log << e;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
            return STOP;
        }
        else
            throw;
    }

    // Render reference.
    {
        sglr::ReferenceContextBuffers buffers(
            tcu::PixelFormat(8, 8, 8, renderTarget.getPixelFormat().alphaBits ? 8 : 0), renderTarget.getDepthBits(),
            renderTarget.getStencilBits(), width, height);
        sglr::ReferenceContext context(sglr::ReferenceContextLimits(renderCtx), buffers.getColorbuffer(),
                                       buffers.getDepthbuffer(), buffers.getStencilbuffer());

        setContext(&context);
        render(reference);
        setContext(nullptr);
    }

    bool isOk = compare(reference, result);
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");
    return STOP;
}

bool FboTestCase::compare(const tcu::Surface &reference, const tcu::Surface &result)
{
    return tcu::fuzzyCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference, result, 0.05f,
                             tcu::COMPARE_LOG_RESULT);
}

void FboTestCase::readPixels(tcu::Surface &dst, int x, int y, int width, int height, const tcu::TextureFormat &format,
                             const tcu::Vec4 &scale, const tcu::Vec4 &bias)
{
    FboTestUtil::readPixels(*getCurrentContext(), dst, x, y, width, height, format, scale, bias);
}

void FboTestCase::readPixels(tcu::Surface &dst, int x, int y, int width, int height)
{
    getCurrentContext()->readPixels(dst, x, y, width, height);
}

void FboTestCase::checkFramebufferStatus(uint32_t target)
{
    uint32_t status = glCheckFramebufferStatus(target);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw FboTestUtil::FboIncompleteException(status, __FILE__, __LINE__);
}

void FboTestCase::checkError(void)
{
    uint32_t err = glGetError();
    if (err != GL_NO_ERROR)
        throw glu::Error((int)err, (string("Got ") + glu::getErrorStr(err).toString()).c_str(), nullptr, __FILE__,
                         __LINE__);
}

static bool isRequiredFormat(uint32_t format, glu::RenderContext &renderContext)
{
    switch (format)
    {
    // Color-renderable formats
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA8:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_SRGB8_ALPHA8:
    case GL_RGB10_A2:
    case GL_RGB10_A2UI:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB8:
    case GL_RGB565:
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG8:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_R32I:
    case GL_R32UI:
    case GL_R16I:
    case GL_R16UI:
    case GL_R8:
    case GL_R8I:
    case GL_R8UI:
        return true;

    // Depth formats
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT16:
        return true;

    // Depth+stencil formats
    case GL_DEPTH32F_STENCIL8:
    case GL_DEPTH24_STENCIL8:
        return true;

    // Stencil formats
    case GL_STENCIL_INDEX8:
        return true;

    // Float formats
    case GL_RGBA32F:
    case GL_RGB32F:
    case GL_R11F_G11F_B10F:
    case GL_RG32F:
    case GL_R32F:
        return glu::contextSupports(renderContext.getType(), glu::ApiType::es(3, 2));

    default:
        return false;
    }
}

static std::vector<std::string> getEnablingExtensions(uint32_t format, glu::RenderContext &renderContext)
{
    const bool isES32 = glu::contextSupports(renderContext.getType(), glu::ApiType::es(3, 2));
    std::vector<std::string> out;

    DE_ASSERT(!isRequiredFormat(format, renderContext));

    switch (format)
    {
    case GL_RGB16F:
        out.push_back("GL_EXT_color_buffer_half_float");
        break;

    case GL_RGBA16F:
    case GL_RG16F:
    case GL_R16F:
        out.push_back("GL_EXT_color_buffer_half_float");
        // Fallthrough

    case GL_RGBA32F:
    case GL_RGB32F:
    case GL_R11F_G11F_B10F:
    case GL_RG32F:
    case GL_R32F:
        if (!isES32)
            out.push_back("GL_EXT_color_buffer_float");
        break;

    default:
        break;
    }

    return out;
}

static bool isAnyExtensionSupported(Context &context, const std::vector<std::string> &requiredExts)
{
    for (std::vector<std::string>::const_iterator iter = requiredExts.begin(); iter != requiredExts.end(); iter++)
    {
        const std::string &extension = *iter;

        if (context.getContextInfo().isExtensionSupported(extension.c_str()))
            return true;
    }

    return false;
}

void FboTestCase::checkFormatSupport(uint32_t sizedFormat)
{
    const bool isCoreFormat = isRequiredFormat(sizedFormat, m_context.getRenderContext());
    const std::vector<std::string> requiredExts =
        (!isCoreFormat) ? getEnablingExtensions(sizedFormat, m_context.getRenderContext()) : std::vector<std::string>();

    // Check that we don't try to use invalid formats.
    DE_ASSERT(isCoreFormat || !requiredExts.empty());

    if (!requiredExts.empty() && !isAnyExtensionSupported(m_context, requiredExts))
        throw tcu::NotSupportedError("Format not supported");
}

static int getMinimumSampleCount(uint32_t format)
{
    switch (format)
    {
    // Core formats
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA8:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_SRGB8_ALPHA8:
    case GL_RGB10_A2:
    case GL_RGB10_A2UI:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB8:
    case GL_RGB565:
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG8:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_R32I:
    case GL_R32UI:
    case GL_R16I:
    case GL_R16UI:
    case GL_R8:
    case GL_R8I:
    case GL_R8UI:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH32F_STENCIL8:
    case GL_DEPTH24_STENCIL8:
    case GL_STENCIL_INDEX8:
        return 4;

    // GL_EXT_color_buffer_float
    case GL_R11F_G11F_B10F:
    case GL_RG16F:
    case GL_R16F:
        return 4;

    case GL_RGBA32F:
    case GL_RGBA16F:
    case GL_RG32F:
    case GL_R32F:
        return 0;

    // GL_EXT_color_buffer_half_float
    case GL_RGB16F:
        return 0;

    default:
        DE_FATAL("Unknown format");
        return 0;
    }
}

static std::vector<int> querySampleCounts(const glw::Functions &gl, uint32_t format)
{
    int numSampleCounts = 0;
    std::vector<int> sampleCounts;

    gl.getInternalformativ(GL_RENDERBUFFER, format, GL_NUM_SAMPLE_COUNTS, 1, &numSampleCounts);

    if (numSampleCounts > 0)
    {
        sampleCounts.resize(numSampleCounts);
        gl.getInternalformativ(GL_RENDERBUFFER, format, GL_SAMPLES, (glw::GLsizei)sampleCounts.size(),
                               &sampleCounts[0]);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to query sample counts for format");

    return sampleCounts;
}

void FboTestCase::checkSampleCount(uint32_t sizedFormat, int numSamples)
{
    const int minSampleCount = getMinimumSampleCount(sizedFormat);

    if (numSamples > minSampleCount)
    {
        // Exceeds spec-mandated minimum - need to check.
        const std::vector<int> supportedSampleCounts =
            querySampleCounts(m_context.getRenderContext().getFunctions(), sizedFormat);

        if (std::find(supportedSampleCounts.begin(), supportedSampleCounts.end(), numSamples) ==
            supportedSampleCounts.end())
            throw tcu::NotSupportedError("Sample count not supported");
    }
}

void FboTestCase::clearColorBuffer(const tcu::TextureFormat &format, const tcu::Vec4 &value)
{
    FboTestUtil::clearColorBuffer(*getCurrentContext(), format, value);
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
