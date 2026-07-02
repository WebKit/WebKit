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

#include "es3fFramebufferBlitTests.hpp"
#include "es3fFboTestCase.hpp"
#include "es3fFboTestUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluContextInfo.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "sglrContextUtil.hpp"
#include "glwEnums.hpp"
#include "deStringUtil.hpp"

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

class BlitRectCase : public FboTestCase
{
public:
    BlitRectCase(Context &context, const char *name, const char *desc, uint32_t filter, const IVec2 &srcSize,
                 const IVec4 &srcRect, const IVec2 &dstSize, const IVec4 &dstRect, int cellSize = 8)
        : FboTestCase(context, name, desc)
        , m_filter(filter)
        , m_srcSize(srcSize)
        , m_srcRect(srcRect)
        , m_dstSize(dstSize)
        , m_dstRect(dstRect)
        , m_cellSize(cellSize)
        , m_gridCellColorA(0.2f, 0.7f, 0.1f, 1.0f)
        , m_gridCellColorB(0.7f, 0.1f, 0.5f, 0.8f)
    {
    }

    void render(tcu::Surface &dst)
    {
        const uint32_t colorFormat = GL_RGBA8;

        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
        uint32_t gradShaderID = getCurrentContext()->createProgram(&gradShader);
        uint32_t texShaderID  = getCurrentContext()->createProgram(&texShader);

        uint32_t srcFbo = 0;
        uint32_t dstFbo = 0;
        uint32_t srcRbo = 0;
        uint32_t dstRbo = 0;

        // Setup shaders
        gradShader.setGradient(*getCurrentContext(), gradShaderID, Vec4(0.0f), Vec4(1.0f));
        texShader.setUniforms(*getCurrentContext(), texShaderID);

        // Create framebuffers.
        for (int ndx = 0; ndx < 2; ndx++)
        {
            uint32_t &fbo     = ndx ? dstFbo : srcFbo;
            uint32_t &rbo     = ndx ? dstRbo : srcRbo;
            const IVec2 &size = ndx ? m_dstSize : m_srcSize;

            glGenFramebuffers(1, &fbo);
            glGenRenderbuffers(1, &rbo);

            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, size.x(), size.y());

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);
        }

        // Fill destination with gradient.
        glBindFramebuffer(GL_FRAMEBUFFER, dstFbo);
        glViewport(0, 0, m_dstSize.x(), m_dstSize.y());

        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Fill source with grid pattern.
        {
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = m_srcSize.x();
            const int texH          = m_srcSize.y();
            uint32_t gridTex        = 0;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            tcu::fillWithGrid(data.getAccess(), m_cellSize, m_gridCellColorA, m_gridCellColorB);

            glGenTextures(1, &gridTex);
            glBindTexture(GL_TEXTURE_2D, gridTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            glBindFramebuffer(GL_FRAMEBUFFER, srcFbo);
            glViewport(0, 0, m_srcSize.x(), m_srcSize.y());
            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        // Perform copy.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
        glBlitFramebuffer(m_srcRect.x(), m_srcRect.y(), m_srcRect.z(), m_srcRect.w(), m_dstRect.x(), m_dstRect.y(),
                          m_dstRect.z(), m_dstRect.w(), GL_COLOR_BUFFER_BIT, m_filter);

        // Read back results.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo);
        readPixels(dst, 0, 0, m_dstSize.x(), m_dstSize.y(), glu::mapGLInternalFormat(colorFormat), Vec4(1.0f),
                   Vec4(0.0f));
    }

    virtual bool compare(const tcu::Surface &reference, const tcu::Surface &result)
    {
        // Use pixel-threshold compare for rect cases since 1px off will mean failure.
        tcu::RGBA threshold =
            TestCase::m_context.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(7, 7, 7, 7);
        return tcu::pixelThresholdCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference, result,
                                          threshold, tcu::COMPARE_LOG_RESULT);
    }

protected:
    const uint32_t m_filter;
    const IVec2 m_srcSize;
    const IVec4 m_srcRect;
    const IVec2 m_dstSize;
    const IVec4 m_dstRect;
    const int m_cellSize;
    const Vec4 m_gridCellColorA;
    const Vec4 m_gridCellColorB;
};

class BlitNearestFilterConsistencyCase : public BlitRectCase
{
public:
    BlitNearestFilterConsistencyCase(Context &context, const char *name, const char *desc, const IVec2 &srcSize,
                                     const IVec4 &srcRect, const IVec2 &dstSize, const IVec4 &dstRect);

    bool compare(const tcu::Surface &reference, const tcu::Surface &result);
};

BlitNearestFilterConsistencyCase::BlitNearestFilterConsistencyCase(Context &context, const char *name, const char *desc,
                                                                   const IVec2 &srcSize, const IVec4 &srcRect,
                                                                   const IVec2 &dstSize, const IVec4 &dstRect)
    : BlitRectCase(context, name, desc, GL_NEAREST, srcSize, srcRect, dstSize, dstRect, 1)
{
}

bool BlitNearestFilterConsistencyCase::compare(const tcu::Surface &reference, const tcu::Surface &result)
{
    DE_ASSERT(reference.getWidth() == result.getWidth());
    DE_ASSERT(reference.getHeight() == result.getHeight());
    DE_UNREF(reference);

    // Image origin must be visible (for baseColor)
    DE_ASSERT(de::min(m_dstRect.x(), m_dstRect.z()) >= 0);
    DE_ASSERT(de::min(m_dstRect.y(), m_dstRect.w()) >= 0);

    const tcu::RGBA cellColorA(m_gridCellColorA);
    const tcu::RGBA cellColorB(m_gridCellColorB);
    const tcu::RGBA threshold =
        TestCase::m_context.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(7, 7, 7, 7);
    const tcu::IVec4 destinationArea =
        tcu::IVec4(de::clamp(de::min(m_dstRect.x(), m_dstRect.z()), 0, result.getWidth()),
                   de::clamp(de::min(m_dstRect.y(), m_dstRect.w()), 0, result.getHeight()),
                   de::clamp(de::max(m_dstRect.x(), m_dstRect.z()), 0, result.getWidth()),
                   de::clamp(de::max(m_dstRect.y(), m_dstRect.w()), 0, result.getHeight()));
    const tcu::RGBA baseColor = result.getPixel(destinationArea.x(), destinationArea.y());
    const bool signConfig     = tcu::compareThreshold(baseColor, cellColorA, threshold);

    bool error = false;
    tcu::Surface errorMask(result.getWidth(), result.getHeight());
    std::vector<bool> horisontalSign(destinationArea.z() - destinationArea.x());
    std::vector<bool> verticalSign(destinationArea.w() - destinationArea.y());

    tcu::clear(errorMask.getAccess(), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // Checking only area in our destination rect

    m_testCtx.getLog()
        << tcu::TestLog::Message << "Verifying consistency of NEAREST filtering. Verifying rect " << m_dstRect << ".\n"
        << "Rounding direction of the NEAREST filter at the horisontal texel edge (x = n + 0.5) should not depend on "
           "the y-coordinate.\n"
        << "Rounding direction of the NEAREST filter at the vertical texel edge (y = n + 0.5) should not depend on the "
           "x-coordinate.\n"
        << "Blitting a grid (with uniform sized cells) should result in a grid (with non-uniform sized cells)."
        << tcu::TestLog::EndMessage;

    // Verify that destination only contains valid colors

    for (int dy = 0; dy < destinationArea.w() - destinationArea.y(); ++dy)
        for (int dx = 0; dx < destinationArea.z() - destinationArea.x(); ++dx)
        {
            const tcu::RGBA color   = result.getPixel(destinationArea.x() + dx, destinationArea.y() + dy);
            const bool isValidColor = tcu::compareThreshold(color, cellColorA, threshold) ||
                                      tcu::compareThreshold(color, cellColorB, threshold);

            if (!isValidColor)
            {
                errorMask.setPixel(destinationArea.x() + dx, destinationArea.y() + dy, tcu::RGBA::red());
                error = true;
            }
        }
    if (error)
    {
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Image verification failed, destination rect contains unexpected values. "
                           << "Expected either " << cellColorA << " or " << cellColorB << "."
                           << tcu::TestLog::EndMessage << tcu::TestLog::ImageSet("Result", "Image verification result")
                           << tcu::TestLog::Image("Result", "Result", result)
                           << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
        return false;
    }

    // Detect result edges by reading the first row and first column of the blitted area.
    // Blitting a grid should result in a grid-like image. ("sign changes" should be consistent)

    for (int dx = 0; dx < destinationArea.z() - destinationArea.x(); ++dx)
    {
        const tcu::RGBA color = result.getPixel(destinationArea.x() + dx, destinationArea.y());

        if (tcu::compareThreshold(color, cellColorA, threshold))
            horisontalSign[dx] = true;
        else if (tcu::compareThreshold(color, cellColorB, threshold))
            horisontalSign[dx] = false;
        else
            DE_ASSERT(false);
    }
    for (int dy = 0; dy < destinationArea.w() - destinationArea.y(); ++dy)
    {
        const tcu::RGBA color = result.getPixel(destinationArea.x(), destinationArea.y() + dy);

        if (tcu::compareThreshold(color, cellColorA, threshold))
            verticalSign[dy] = true;
        else if (tcu::compareThreshold(color, cellColorB, threshold))
            verticalSign[dy] = false;
        else
            DE_ASSERT(false);
    }

    // Verify grid-like image

    for (int dy = 0; dy < destinationArea.w() - destinationArea.y(); ++dy)
        for (int dx = 0; dx < destinationArea.z() - destinationArea.x(); ++dx)
        {
            const tcu::RGBA color  = result.getPixel(destinationArea.x() + dx, destinationArea.y() + dy);
            const bool resultSign  = tcu::compareThreshold(cellColorA, color, threshold);
            const bool correctSign = (horisontalSign[dx] == verticalSign[dy]) == signConfig;

            if (resultSign != correctSign)
            {
                errorMask.setPixel(destinationArea.x() + dx, destinationArea.y() + dy, tcu::RGBA::red());
                error = true;
            }
        }

    // Report result

    if (error)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Image verification failed, nearest filter is not consistent."
                           << tcu::TestLog::EndMessage << tcu::TestLog::ImageSet("Result", "Image verification result")
                           << tcu::TestLog::Image("Result", "Result", result)
                           << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
    }
    else
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Image verification passed." << tcu::TestLog::EndMessage
                           << tcu::TestLog::ImageSet("Result", "Image verification result")
                           << tcu::TestLog::Image("Result", "Result", result) << tcu::TestLog::EndImageSet;
    }

    return !error;
}

static tcu::BVec4 getChannelMask(tcu::TextureFormat::ChannelOrder order)
{
    switch (order)
    {
    case tcu::TextureFormat::R:
        return tcu::BVec4(true, false, false, false);
    case tcu::TextureFormat::RG:
        return tcu::BVec4(true, true, false, false);
    case tcu::TextureFormat::RGB:
        return tcu::BVec4(true, true, true, false);
    case tcu::TextureFormat::RGBA:
        return tcu::BVec4(true, true, true, true);
    case tcu::TextureFormat::sRGB:
        return tcu::BVec4(true, true, true, false);
    case tcu::TextureFormat::sRGBA:
        return tcu::BVec4(true, true, true, true);
    default:
        DE_ASSERT(false);
        return tcu::BVec4(false);
    }
}

class BlitColorConversionCase : public FboTestCase
{
public:
    BlitColorConversionCase(Context &context, const char *name, const char *desc, uint32_t srcFormat,
                            uint32_t dstFormat, const IVec2 &size)
        : FboTestCase(context, name, desc)
        , m_srcFormat(srcFormat)
        , m_dstFormat(dstFormat)
        , m_size(size)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_srcFormat);
        checkFormatSupport(m_dstFormat);
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat srcFormat = glu::mapGLInternalFormat(m_srcFormat);
        tcu::TextureFormat dstFormat = glu::mapGLInternalFormat(m_dstFormat);
        glu::DataType srcOutputType  = getFragmentOutputType(srcFormat);
        glu::DataType dstOutputType  = getFragmentOutputType(dstFormat);

        // Compute ranges \note Doesn't handle case where src or dest is not subset of the another!
        tcu::TextureFormatInfo srcFmtRangeInfo = tcu::getTextureFormatInfo(srcFormat);
        tcu::TextureFormatInfo dstFmtRangeInfo = tcu::getTextureFormatInfo(dstFormat);
        tcu::BVec4 copyMask     = tcu::logicalAnd(getChannelMask(srcFormat.order), getChannelMask(dstFormat.order));
        tcu::BVec4 srcIsGreater = tcu::greaterThan(srcFmtRangeInfo.valueMax - srcFmtRangeInfo.valueMin,
                                                   dstFmtRangeInfo.valueMax - dstFmtRangeInfo.valueMin);
        tcu::TextureFormatInfo srcRangeInfo(
            tcu::select(dstFmtRangeInfo.valueMin, srcFmtRangeInfo.valueMin, tcu::logicalAnd(copyMask, srcIsGreater)),
            tcu::select(dstFmtRangeInfo.valueMax, srcFmtRangeInfo.valueMax, tcu::logicalAnd(copyMask, srcIsGreater)),
            tcu::select(dstFmtRangeInfo.lookupScale, srcFmtRangeInfo.lookupScale,
                        tcu::logicalAnd(copyMask, srcIsGreater)),
            tcu::select(dstFmtRangeInfo.lookupBias, srcFmtRangeInfo.lookupBias,
                        tcu::logicalAnd(copyMask, srcIsGreater)));
        tcu::TextureFormatInfo dstRangeInfo(tcu::select(dstFmtRangeInfo.valueMin, srcFmtRangeInfo.valueMin,
                                                        tcu::logicalOr(tcu::logicalNot(copyMask), srcIsGreater)),
                                            tcu::select(dstFmtRangeInfo.valueMax, srcFmtRangeInfo.valueMax,
                                                        tcu::logicalOr(tcu::logicalNot(copyMask), srcIsGreater)),
                                            tcu::select(dstFmtRangeInfo.lookupScale, srcFmtRangeInfo.lookupScale,
                                                        tcu::logicalOr(tcu::logicalNot(copyMask), srcIsGreater)),
                                            tcu::select(dstFmtRangeInfo.lookupBias, srcFmtRangeInfo.lookupBias,
                                                        tcu::logicalOr(tcu::logicalNot(copyMask), srcIsGreater)));

        // Shaders.
        GradientShader gradientToSrcShader(srcOutputType);
        GradientShader gradientToDstShader(dstOutputType);

        uint32_t gradShaderSrcID = getCurrentContext()->createProgram(&gradientToSrcShader);
        uint32_t gradShaderDstID = getCurrentContext()->createProgram(&gradientToDstShader);

        uint32_t srcFbo, dstFbo;
        uint32_t srcRbo, dstRbo;

        // Create framebuffers.
        for (int ndx = 0; ndx < 2; ndx++)
        {
            uint32_t &fbo   = ndx ? dstFbo : srcFbo;
            uint32_t &rbo   = ndx ? dstRbo : srcRbo;
            uint32_t format = ndx ? m_dstFormat : m_srcFormat;

            glGenFramebuffers(1, &fbo);
            glGenRenderbuffers(1, &rbo);

            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, format, m_size.x(), m_size.y());

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);
        }

        glViewport(0, 0, m_size.x(), m_size.y());

        // Render gradients.
        for (int ndx = 0; ndx < 2; ndx++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, ndx ? dstFbo : srcFbo);

            if (ndx)
            {
                gradientToDstShader.setGradient(*getCurrentContext(), gradShaderDstID, dstRangeInfo.valueMax,
                                                dstRangeInfo.valueMin);
                sglr::drawQuad(*getCurrentContext(), gradShaderDstID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            }
            else
            {
                gradientToSrcShader.setGradient(*getCurrentContext(), gradShaderSrcID, srcRangeInfo.valueMin,
                                                dstRangeInfo.valueMax);
                sglr::drawQuad(*getCurrentContext(), gradShaderSrcID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            }
        }

        // Execute copy.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
        glBlitFramebuffer(0, 0, m_size.x(), m_size.y(), 0, 0, m_size.x(), m_size.y(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        checkError();

        // Read results.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo);
        readPixels(dst, 0, 0, m_size.x(), m_size.y(), dstFormat, dstRangeInfo.lookupScale, dstRangeInfo.lookupBias);
    }

    bool compare(const tcu::Surface &reference, const tcu::Surface &result)
    {
        const tcu::TextureFormat srcFormat = glu::mapGLInternalFormat(m_srcFormat);
        const tcu::TextureFormat dstFormat = glu::mapGLInternalFormat(m_dstFormat);
        const bool srcIsSRGB               = tcu::isSRGB(srcFormat);
        const bool dstIsSRGB               = tcu::isSRGB(dstFormat);

        tcu::RGBA threshold;

        if (dstIsSRGB)
        {
            threshold = getToSRGBConversionThreshold(srcFormat, dstFormat);
        }
        else
        {
            const tcu::RGBA srcMaxDiff = getFormatThreshold(srcFormat) * (srcIsSRGB ? 2 : 1);
            const tcu::RGBA dstMaxDiff = getFormatThreshold(dstFormat);

            threshold = tcu::max(srcMaxDiff, dstMaxDiff);
        }

        m_testCtx.getLog() << tcu::TestLog::Message << "threshold = " << threshold << tcu::TestLog::EndMessage;
        return tcu::pixelThresholdCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference, result,
                                          threshold, tcu::COMPARE_LOG_RESULT);
    }

private:
    uint32_t m_srcFormat;
    uint32_t m_dstFormat;
    IVec2 m_size;
};

class BlitDepthStencilCase : public FboTestCase
{
public:
    BlitDepthStencilCase(Context &context, const char *name, const char *desc, uint32_t depthFormat,
                         uint32_t stencilFormat, uint32_t srcBuffers, const IVec2 &srcSize, const IVec4 &srcRect,
                         uint32_t dstBuffers, const IVec2 &dstSize, const IVec4 &dstRect, uint32_t copyBuffers)
        : FboTestCase(context, name, desc)
        , m_depthFormat(depthFormat)
        , m_stencilFormat(stencilFormat)
        , m_srcBuffers(srcBuffers)
        , m_srcSize(srcSize)
        , m_srcRect(srcRect)
        , m_dstBuffers(dstBuffers)
        , m_dstSize(dstSize)
        , m_dstRect(dstRect)
        , m_copyBuffers(copyBuffers)
    {
    }

protected:
    void preCheck(void)
    {
        if (m_depthFormat != GL_NONE)
            checkFormatSupport(m_depthFormat);
        if (m_stencilFormat != GL_NONE)
            checkFormatSupport(m_stencilFormat);
        if (!m_context.getContextInfo().isExtensionSupported("GL_EXT_separate_depth_stencil") &&
            m_depthFormat != GL_NONE && m_stencilFormat != GL_NONE)
            throw tcu::NotSupportedError("Separate depth and stencil buffers not supported");
    }

    void render(tcu::Surface &dst)
    {
        const uint32_t colorFormat = GL_RGBA8;

        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
        FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);

        uint32_t flatShaderID = getCurrentContext()->createProgram(&flatShader);
        uint32_t texShaderID  = getCurrentContext()->createProgram(&texShader);
        uint32_t gradShaderID = getCurrentContext()->createProgram(&gradShader);

        uint32_t srcFbo        = 0;
        uint32_t dstFbo        = 0;
        uint32_t srcColorRbo   = 0;
        uint32_t dstColorRbo   = 0;
        uint32_t srcDepthRbo   = 0;
        uint32_t srcStencilRbo = 0;
        uint32_t dstDepthRbo   = 0;
        uint32_t dstStencilRbo = 0;

        // setup shaders
        gradShader.setGradient(*getCurrentContext(), gradShaderID, Vec4(0.0f), Vec4(1.0f));
        texShader.setUniforms(*getCurrentContext(), texShaderID);

        // Create framebuffers.
        for (int ndx = 0; ndx < 2; ndx++)
        {
            uint32_t &fbo        = ndx ? dstFbo : srcFbo;
            uint32_t &colorRbo   = ndx ? dstColorRbo : srcColorRbo;
            uint32_t &depthRbo   = ndx ? dstDepthRbo : srcDepthRbo;
            uint32_t &stencilRbo = ndx ? dstStencilRbo : srcStencilRbo;
            uint32_t bufs        = ndx ? m_dstBuffers : m_srcBuffers;
            const IVec2 &size    = ndx ? m_dstSize : m_srcSize;

            glGenFramebuffers(1, &fbo);
            glGenRenderbuffers(1, &colorRbo);

            glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, colorFormat, size.x(), size.y());

            if (m_depthFormat != GL_NONE)
            {
                glGenRenderbuffers(1, &depthRbo);
                glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
                glRenderbufferStorage(GL_RENDERBUFFER, m_depthFormat, size.x(), size.y());
            }

            if (m_stencilFormat != GL_NONE)
            {
                glGenRenderbuffers(1, &stencilRbo);
                glBindRenderbuffer(GL_RENDERBUFFER, stencilRbo);
                glRenderbufferStorage(GL_RENDERBUFFER, m_stencilFormat, size.x(), size.y());
            }

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);

            if (bufs & GL_DEPTH_BUFFER_BIT)
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
            if (bufs & GL_STENCIL_BUFFER_BIT)
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                          m_stencilFormat == GL_NONE ? depthRbo : stencilRbo);

            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);

            // Clear depth to 1 and stencil to 0.
            glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
        }

        // Fill source with gradient, depth = [-1..1], stencil = 7
        glBindFramebuffer(GL_FRAMEBUFFER, srcFbo);
        glViewport(0, 0, m_srcSize.x(), m_srcSize.y());
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 7, 0xffu);

        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

        // Fill destination with grid pattern, depth = 0 and stencil = 1
        {
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = m_srcSize.x();
            const int texH          = m_srcSize.y();
            uint32_t gridTex        = 0;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            tcu::fillWithGrid(data.getAccess(), 8, Vec4(0.2f, 0.7f, 0.1f, 1.0f), Vec4(0.7f, 0.1f, 0.5f, 0.8f));

            glGenTextures(1, &gridTex);
            glBindTexture(GL_TEXTURE_2D, gridTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            glBindFramebuffer(GL_FRAMEBUFFER, dstFbo);
            glViewport(0, 0, m_dstSize.x(), m_dstSize.y());
            glStencilFunc(GL_ALWAYS, 1, 0xffu);
            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        // Perform copy.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
        glBlitFramebuffer(m_srcRect.x(), m_srcRect.y(), m_srcRect.z(), m_srcRect.w(), m_dstRect.x(), m_dstRect.y(),
                          m_dstRect.z(), m_dstRect.w(), m_copyBuffers, GL_NEAREST);

        // Render blue color where depth < 0, decrement on depth failure.
        glBindFramebuffer(GL_FRAMEBUFFER, dstFbo);
        glViewport(0, 0, m_dstSize.x(), m_dstSize.y());
        glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
        glStencilFunc(GL_ALWAYS, 0, 0xffu);

        flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        if (m_dstBuffers & GL_STENCIL_BUFFER_BIT)
        {
            // Render green color where stencil == 6.
            glDisable(GL_DEPTH_TEST);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            glStencilFunc(GL_EQUAL, 6, 0xffu);

            flatShader.setColor(*getCurrentContext(), flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));
            sglr::drawQuad(*getCurrentContext(), flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        readPixels(dst, 0, 0, m_dstSize.x(), m_dstSize.y(), glu::mapGLInternalFormat(colorFormat), Vec4(1.0f),
                   Vec4(0.0f));
    }

private:
    uint32_t m_depthFormat;
    uint32_t m_stencilFormat;
    uint32_t m_srcBuffers;
    IVec2 m_srcSize;
    IVec4 m_srcRect;
    uint32_t m_dstBuffers;
    IVec2 m_dstSize;
    IVec4 m_dstRect;
    uint32_t m_copyBuffers;
};

class BlitDefaultFramebufferCase : public FboTestCase
{
public:
    BlitDefaultFramebufferCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t filter)
        : FboTestCase(context, name, desc)
        , m_format(format)
        , m_filter(filter)
    {
    }

protected:
    void preCheck(void)
    {
        if (m_context.getRenderTarget().getNumSamples() > 0)
            throw tcu::NotSupportedError("Not supported in MSAA config");

        checkFormatSupport(m_format);
    }

    virtual void render(tcu::Surface &dst)
    {
        tcu::TextureFormat colorFormat  = glu::mapGLInternalFormat(m_format);
        glu::TransferFormat transferFmt = glu::getTransferFormat(colorFormat);
        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        Texture2DShader texShader(DataTypes() << glu::getSampler2DType(colorFormat), glu::TYPE_FLOAT_VEC4);
        uint32_t gradShaderID = getCurrentContext()->createProgram(&gradShader);
        uint32_t texShaderID  = getCurrentContext()->createProgram(&texShader);
        uint32_t fbo          = 0;
        uint32_t tex          = 0;
        const int texW        = 128;
        const int texH        = 128;

        // Setup shaders
        gradShader.setGradient(*getCurrentContext(), gradShaderID, Vec4(0.0f), Vec4(1.0f));
        texShader.setUniforms(*getCurrentContext(), texShaderID);

        // FBO
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter);
        glTexImage2D(GL_TEXTURE_2D, 0, m_format, texW, texH, 0, transferFmt.format, transferFmt.dataType, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        // Render gradient to screen.
        glBindFramebuffer(GL_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());

        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Blit gradient from screen to fbo.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, getWidth(), getHeight(), 0, 0, texW, texH, GL_COLOR_BUFFER_BIT, m_filter);

        // Fill left half of viewport with quad that uses texture.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());
        glClearBufferfv(GL_COLOR, 0, Vec4(1.0f, 0.0f, 0.0f, 1.0f).getPtr());
        sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

        // Blit fbo to right half.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, texW, texH, getWidth() / 2, 0, getWidth(), getHeight(), GL_COLOR_BUFFER_BIT, m_filter);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_context.getRenderContext().getDefaultFramebuffer());
        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

    bool compare(const tcu::Surface &reference, const tcu::Surface &result)
    {
        const tcu::RGBA threshold(tcu::max(getFormatThreshold(m_format), tcu::RGBA(12, 12, 12, 12)));

        m_testCtx.getLog() << TestLog::Message << "Comparing images, threshold: " << threshold << TestLog::EndMessage;

        return tcu::bilinearCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference.getAccess(),
                                    result.getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
    }

protected:
    const uint32_t m_format;
    const uint32_t m_filter;
};

class DefaultFramebufferBlitCase : public BlitDefaultFramebufferCase
{
public:
    enum BlitDirection
    {
        BLIT_DEFAULT_TO_TARGET,
        BLIT_TO_DEFAULT_FROM_TARGET,

        BLIT_LAST
    };
    enum BlitArea
    {
        AREA_SCALE,
        AREA_OUT_OF_BOUNDS,

        AREA_LAST
    };

    DefaultFramebufferBlitCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t filter,
                               BlitDirection dir, BlitArea area)
        : BlitDefaultFramebufferCase(context, name, desc, format, filter)
        , m_blitDir(dir)
        , m_blitArea(area)
        , m_srcRect(-1, -1, -1, -1)
        , m_dstRect(-1, -1, -1, -1)
        , m_interestingArea(-1, -1, -1, -1)
    {
        DE_ASSERT(dir < BLIT_LAST);
        DE_ASSERT(area < AREA_LAST);
    }

    void init(void)
    {
        // requirements
        const int minViewportSize = 128;
        if (m_context.getRenderTarget().getWidth() < minViewportSize ||
            m_context.getRenderTarget().getHeight() < minViewportSize)
            throw tcu::NotSupportedError("Viewport size " + de::toString(minViewportSize) + "x" +
                                         de::toString(minViewportSize) + " required");

        // prevent viewport randoming
        m_viewportWidth  = m_context.getRenderTarget().getWidth();
        m_viewportHeight = m_context.getRenderTarget().getHeight();

        // set proper areas
        if (m_blitArea == AREA_SCALE)
        {
            m_srcRect         = IVec4(10, 20, 65, 100);
            m_dstRect         = IVec4(25, 30, 125, 94);
            m_interestingArea = IVec4(0, 0, 128, 128);
        }
        else if (m_blitArea == AREA_OUT_OF_BOUNDS)
        {
            const tcu::IVec2 ubound =
                (m_blitDir == BLIT_DEFAULT_TO_TARGET) ?
                    (tcu::IVec2(128, 128)) :
                    (tcu::IVec2(m_context.getRenderTarget().getWidth(), m_context.getRenderTarget().getHeight()));

            m_srcRect         = IVec4(-10, -15, 100, 63);
            m_dstRect         = ubound.swizzle(0, 1, 0, 1) + IVec4(-75, -99, 8, 16);
            m_interestingArea = IVec4(ubound.x() - 128, ubound.y() - 128, ubound.x(), ubound.y());
        }
        else
            DE_ASSERT(false);
    }

    void render(tcu::Surface &dst)
    {
        const tcu::TextureFormat colorFormat       = glu::mapGLInternalFormat(m_format);
        const glu::TransferFormat transferFmt      = glu::getTransferFormat(colorFormat);
        const tcu::TextureChannelClass targetClass = (m_blitDir == BLIT_DEFAULT_TO_TARGET) ?
                                                         (tcu::getTextureChannelClass(colorFormat.type)) :
                                                         (tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT);
        uint32_t fbo                               = 0;
        uint32_t fboTex                            = 0;
        const int fboTexW                          = 128;
        const int fboTexH                          = 128;
        const int sourceWidth                      = (m_blitDir == BLIT_DEFAULT_TO_TARGET) ? (getWidth()) : (fboTexW);
        const int sourceHeight                     = (m_blitDir == BLIT_DEFAULT_TO_TARGET) ? (getHeight()) : (fboTexH);
        const int gridRenderWidth                  = de::min(256, sourceWidth);
        const int gridRenderHeight                 = de::min(256, sourceHeight);

        int targetFbo = -1;
        int sourceFbo = -1;

        // FBO
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &fboTex);

        glBindTexture(GL_TEXTURE_2D, fboTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter);
        glTexImage2D(GL_TEXTURE_2D, 0, m_format, fboTexW, fboTexH, 0, transferFmt.format, transferFmt.dataType,
                     nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        targetFbo =
            (m_blitDir == BLIT_DEFAULT_TO_TARGET) ? (fbo) : (m_context.getRenderContext().getDefaultFramebuffer());
        sourceFbo =
            (m_blitDir == BLIT_DEFAULT_TO_TARGET) ? (m_context.getRenderContext().getDefaultFramebuffer()) : (fbo);

        // Render grid to source framebuffer
        {
            Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
            const uint32_t texShaderID    = getCurrentContext()->createProgram(&texShader);
            const uint32_t internalFormat = GL_RGBA8;
            const uint32_t format         = GL_RGBA;
            const uint32_t dataType       = GL_UNSIGNED_BYTE;
            const int gridTexW            = 128;
            const int gridTexH            = 128;
            uint32_t gridTex              = 0;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), gridTexW, gridTexH, 1);

            tcu::fillWithGrid(data.getAccess(), 9, tcu::Vec4(0.9f, 0.5f, 0.1f, 0.9f),
                              tcu::Vec4(0.2f, 0.8f, 0.2f, 0.7f));

            glGenTextures(1, &gridTex);
            glBindTexture(GL_TEXTURE_2D, gridTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, gridTexW, gridTexH, 0, format, dataType,
                         data.getAccess().getDataPtr());

            glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
            glViewport(0, 0, gridRenderWidth, gridRenderHeight);
            glClearBufferfv(GL_COLOR, 0, Vec4(1.0f, 0.0f, 0.0f, 1.0f).getPtr());

            texShader.setUniforms(*getCurrentContext(), texShaderID);
            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            glUseProgram(0);
        }

        // Blit source framebuffer to destination

        glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFbo);
        checkError();

        if (targetClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT ||
            targetClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
            targetClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
            glClearBufferfv(GL_COLOR, 0, Vec4(1.0f, 1.0f, 0.0f, 1.0f).getPtr());
        else if (targetClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
            glClearBufferiv(GL_COLOR, 0, IVec4(0, 0, 0, 0).getPtr());
        else if (targetClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
            glClearBufferuiv(GL_COLOR, 0, UVec4(0, 0, 0, 0).getPtr());
        else
            DE_ASSERT(false);

        glBlitFramebuffer(m_srcRect.x(), m_srcRect.y(), m_srcRect.z(), m_srcRect.w(), m_dstRect.x(), m_dstRect.y(),
                          m_dstRect.z(), m_dstRect.w(), GL_COLOR_BUFFER_BIT, m_filter);
        checkError();

        // Read target

        glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);

        if (m_blitDir == BLIT_TO_DEFAULT_FROM_TARGET)
            readPixels(dst, m_interestingArea.x(), m_interestingArea.y(), m_interestingArea.z() - m_interestingArea.x(),
                       m_interestingArea.w() - m_interestingArea.y());
        else
            readPixels(dst, m_interestingArea.x(), m_interestingArea.y(), m_interestingArea.z() - m_interestingArea.x(),
                       m_interestingArea.w() - m_interestingArea.y(), colorFormat, tcu::Vec4(1.0f), tcu::Vec4(0.0f));

        checkError();
    }

private:
    const BlitDirection m_blitDir;
    const BlitArea m_blitArea;
    tcu::IVec4 m_srcRect;
    tcu::IVec4 m_dstRect;
    tcu::IVec4 m_interestingArea;
};

FramebufferBlitTests::FramebufferBlitTests(Context &context) : TestCaseGroup(context, "blit", "Framebuffer blit tests")
{
}

FramebufferBlitTests::~FramebufferBlitTests(void)
{
}

void FramebufferBlitTests::init(void)
{
    static const uint32_t colorFormats[] = {
        // RGBA formats
        GL_RGBA32I, GL_RGBA32UI, GL_RGBA16I, GL_RGBA16UI, GL_RGBA8, GL_RGBA8I, GL_RGBA8UI, GL_SRGB8_ALPHA8, GL_RGB10_A2,
        GL_RGB10_A2UI, GL_RGBA4, GL_RGB5_A1,

        // RGB formats
        GL_RGB8, GL_RGB565,

        // RG formats
        GL_RG32I, GL_RG32UI, GL_RG16I, GL_RG16UI, GL_RG8, GL_RG8I, GL_RG8UI,

        // R formats
        GL_R32I, GL_R32UI, GL_R16I, GL_R16UI, GL_R8, GL_R8I, GL_R8UI,

        // GL_EXT_color_buffer_float
        GL_RGBA32F, GL_RGBA16F, GL_R11F_G11F_B10F, GL_RG32F, GL_RG16F, GL_R32F, GL_R16F};

    static const uint32_t depthStencilFormats[] = {GL_NONE,
                                                   GL_DEPTH_COMPONENT32F,
                                                   GL_DEPTH_COMPONENT24,
                                                   GL_DEPTH_COMPONENT16,
                                                   GL_DEPTH32F_STENCIL8,
                                                   GL_DEPTH24_STENCIL8};

    static const uint32_t stencilFormats[] = {GL_NONE, GL_STENCIL_INDEX8};

    // .rect
    {
        static const struct
        {
            const char *name;
            IVec4 srcRect;
            IVec4 dstRect;
        } copyRects[] = {
            {"basic", IVec4(10, 20, 65, 100), IVec4(45, 5, 100, 85)},
            {"scale", IVec4(10, 20, 65, 100), IVec4(25, 30, 125, 94)},
            {"out_of_bounds", IVec4(-10, -15, 100, 63), IVec4(50, 30, 136, 144)},
        };

        static const struct
        {
            const char *name;
            IVec4 srcRect;
            IVec4 dstRect;
        } filterConsistencyRects[] = {
            {"mag", IVec4(20, 10, 74, 88), IVec4(10, 10, 91, 101)},
            {"min", IVec4(10, 20, 78, 100), IVec4(20, 20, 71, 80)},
            {"out_of_bounds_mag", IVec4(21, 10, 73, 82), IVec4(11, 43, 141, 151)},
            {"out_of_bounds_min", IVec4(11, 21, 77, 97), IVec4(80, 82, 135, 139)},
        };

        static const struct
        {
            const char *name;
            IVec4 srcSwizzle;
            IVec4 dstSwizzle;
        } swizzles[] = {{nullptr, IVec4(0, 1, 2, 3), IVec4(0, 1, 2, 3)},
                        {"reverse_src_x", IVec4(2, 1, 0, 3), IVec4(0, 1, 2, 3)},
                        {"reverse_src_y", IVec4(0, 3, 2, 1), IVec4(0, 1, 2, 3)},
                        {"reverse_dst_x", IVec4(0, 1, 2, 3), IVec4(2, 1, 0, 3)},
                        {"reverse_dst_y", IVec4(0, 1, 2, 3), IVec4(0, 3, 2, 1)},
                        {"reverse_src_dst_x", IVec4(2, 1, 0, 3), IVec4(2, 1, 0, 3)},
                        {"reverse_src_dst_y", IVec4(0, 3, 2, 1), IVec4(0, 3, 2, 1)}};

        const IVec2 srcSize(127, 119);
        const IVec2 dstSize(132, 128);

        // Blit rectangle tests.
        tcu::TestCaseGroup *rectGroup = new tcu::TestCaseGroup(m_testCtx, "rect", "Blit rectangle tests");
        addChild(rectGroup);
        for (int rectNdx = 0; rectNdx < DE_LENGTH_OF_ARRAY(copyRects); rectNdx++)
        {
            for (int swzNdx = 0; swzNdx < DE_LENGTH_OF_ARRAY(swizzles); swzNdx++)
            {
                string name = string(copyRects[rectNdx].name) +
                              (swizzles[swzNdx].name ? (string("_") + swizzles[swzNdx].name) : string());
                IVec4 srcSwz  = swizzles[swzNdx].srcSwizzle;
                IVec4 dstSwz  = swizzles[swzNdx].dstSwizzle;
                IVec4 srcRect = copyRects[rectNdx].srcRect.swizzle(srcSwz[0], srcSwz[1], srcSwz[2], srcSwz[3]);
                IVec4 dstRect = copyRects[rectNdx].dstRect.swizzle(dstSwz[0], dstSwz[1], dstSwz[2], dstSwz[3]);

                rectGroup->addChild(new BlitRectCase(m_context, (name + "_nearest").c_str(), "", GL_NEAREST, srcSize,
                                                     srcRect, dstSize, dstRect));
                rectGroup->addChild(new BlitRectCase(m_context, (name + "_linear").c_str(), "", GL_LINEAR, srcSize,
                                                     srcRect, dstSize, dstRect));
            }
        }

        // Nearest filter tests
        for (int rectNdx = 0; rectNdx < DE_LENGTH_OF_ARRAY(filterConsistencyRects); rectNdx++)
        {
            for (int swzNdx = 0; swzNdx < DE_LENGTH_OF_ARRAY(swizzles); swzNdx++)
            {
                string name = string("nearest_consistency_") + filterConsistencyRects[rectNdx].name +
                              (swizzles[swzNdx].name ? (string("_") + swizzles[swzNdx].name) : string());
                IVec4 srcSwz = swizzles[swzNdx].srcSwizzle;
                IVec4 dstSwz = swizzles[swzNdx].dstSwizzle;
                IVec4 srcRect =
                    filterConsistencyRects[rectNdx].srcRect.swizzle(srcSwz[0], srcSwz[1], srcSwz[2], srcSwz[3]);
                IVec4 dstRect =
                    filterConsistencyRects[rectNdx].dstRect.swizzle(dstSwz[0], dstSwz[1], dstSwz[2], dstSwz[3]);

                rectGroup->addChild(new BlitNearestFilterConsistencyCase(m_context, name.c_str(),
                                                                         "Test consistency of the nearest filter",
                                                                         srcSize, srcRect, dstSize, dstRect));
            }
        }
    }

    // .conversion
    {
        tcu::TestCaseGroup *conversionGroup = new tcu::TestCaseGroup(m_testCtx, "conversion", "Color conversion tests");
        addChild(conversionGroup);

        for (int srcFmtNdx = 0; srcFmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); srcFmtNdx++)
        {
            for (int dstFmtNdx = 0; dstFmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); dstFmtNdx++)
            {
                uint32_t srcFormat               = colorFormats[srcFmtNdx];
                tcu::TextureFormat srcTexFmt     = glu::mapGLInternalFormat(srcFormat);
                tcu::TextureChannelClass srcType = tcu::getTextureChannelClass(srcTexFmt.type);
                uint32_t dstFormat               = colorFormats[dstFmtNdx];
                tcu::TextureFormat dstTexFmt     = glu::mapGLInternalFormat(dstFormat);
                tcu::TextureChannelClass dstType = tcu::getTextureChannelClass(dstTexFmt.type);

                if (((srcType == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ||
                      srcType == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT) !=
                     (dstType == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ||
                      dstType == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)) ||
                    ((srcType == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER) !=
                     (dstType == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)) ||
                    ((srcType == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER) !=
                     (dstType == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)))
                    continue; // Conversion not supported.

                string name = string(getFormatName(srcFormat)) + "_to_" + getFormatName(dstFormat);

                conversionGroup->addChild(
                    new BlitColorConversionCase(m_context, name.c_str(), "", srcFormat, dstFormat, IVec2(127, 113)));
            }
        }
    }

    // .depth_stencil
    {
        tcu::TestCaseGroup *depthStencilGroup =
            new tcu::TestCaseGroup(m_testCtx, "depth_stencil", "Depth and stencil blits");
        addChild(depthStencilGroup);

        for (int dFmtNdx = 0; dFmtNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats); dFmtNdx++)
        {
            for (int sFmtNdx = 0; sFmtNdx < DE_LENGTH_OF_ARRAY(stencilFormats); sFmtNdx++)
            {
                uint32_t depthFormat   = depthStencilFormats[dFmtNdx];
                uint32_t stencilFormat = stencilFormats[sFmtNdx];
                bool depth             = false;
                bool stencil           = false;
                string fmtName;

                if (depthFormat != GL_NONE)
                {
                    tcu::TextureFormat info = glu::mapGLInternalFormat(depthFormat);

                    fmtName += getFormatName(depthFormat);
                    if (info.order == tcu::TextureFormat::D || info.order == tcu::TextureFormat::DS)
                        depth = true;
                    if (info.order == tcu::TextureFormat::DS)
                        stencil = true;
                }

                if (stencilFormat != GL_NONE)
                {
                    // Do not try separate stencil along with a combined depth/stencil
                    if (stencil)
                        continue;

                    if (depthFormat != GL_NONE)
                        fmtName += "_";
                    fmtName += getFormatName(stencilFormat);
                    stencil = true;
                }

                if (!stencil && !depth)
                    continue;

                uint32_t buffers = (depth ? GL_DEPTH_BUFFER_BIT : 0) | (stencil ? GL_STENCIL_BUFFER_BIT : 0);

                depthStencilGroup->addChild(new BlitDepthStencilCase(
                    m_context, (fmtName + "_basic").c_str(), "", depthFormat, stencilFormat, buffers, IVec2(128, 128),
                    IVec4(0, 0, 128, 128), buffers, IVec2(128, 128), IVec4(0, 0, 128, 128), buffers));
                depthStencilGroup->addChild(new BlitDepthStencilCase(
                    m_context, (fmtName + "_scale").c_str(), "", depthFormat, stencilFormat, buffers, IVec2(127, 119),
                    IVec4(10, 30, 100, 70), buffers, IVec2(111, 130), IVec4(20, 5, 80, 130), buffers));

                if (depth && stencil)
                {
                    depthStencilGroup->addChild(
                        new BlitDepthStencilCase(m_context, (fmtName + "_depth_only").c_str(), "", depthFormat,
                                                 stencilFormat, buffers, IVec2(128, 128), IVec4(0, 0, 128, 128),
                                                 buffers, IVec2(128, 128), IVec4(0, 0, 128, 128), GL_DEPTH_BUFFER_BIT));
                    depthStencilGroup->addChild(new BlitDepthStencilCase(
                        m_context, (fmtName + "_stencil_only").c_str(), "", depthFormat, stencilFormat, buffers,
                        IVec2(128, 128), IVec4(0, 0, 128, 128), buffers, IVec2(128, 128), IVec4(0, 0, 128, 128),
                        GL_STENCIL_BUFFER_BIT));
                }
            }
        }
    }

    // .default_framebuffer
    {
        static const struct
        {
            const char *name;
            DefaultFramebufferBlitCase::BlitArea area;
        } areas[] = {
            {"scale", DefaultFramebufferBlitCase::AREA_SCALE},
            {"out_of_bounds", DefaultFramebufferBlitCase::AREA_OUT_OF_BOUNDS},
        };

        tcu::TestCaseGroup *defaultFbGroup =
            new tcu::TestCaseGroup(m_testCtx, "default_framebuffer", "Blits with default framebuffer");
        addChild(defaultFbGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
        {
            const uint32_t format                   = colorFormats[fmtNdx];
            const tcu::TextureFormat texFmt         = glu::mapGLInternalFormat(format);
            const tcu::TextureChannelClass fmtClass = tcu::getTextureChannelClass(texFmt.type);
            const uint32_t filter = glu::isGLInternalColorFormatFilterable(format) ? GL_LINEAR : GL_NEAREST;
            const bool filterable = glu::isGLInternalColorFormatFilterable(format);

            if (fmtClass != tcu::TEXTURECHANNELCLASS_FLOATING_POINT &&
                fmtClass != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT &&
                fmtClass != tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
                continue; // Conversion not supported.

            defaultFbGroup->addChild(
                new BlitDefaultFramebufferCase(m_context, getFormatName(format), "", format, filter));

            for (int areaNdx = 0; areaNdx < DE_LENGTH_OF_ARRAY(areas); areaNdx++)
            {
                const string name    = string(areas[areaNdx].name);
                const bool addLinear = filterable;
                const bool addNearest =
                    !addLinear || (areas[areaNdx].area !=
                                   DefaultFramebufferBlitCase::
                                       AREA_OUT_OF_BOUNDS); // No need to check out-of-bounds with different filtering

                if (addNearest)
                {
                    defaultFbGroup->addChild(new DefaultFramebufferBlitCase(
                        m_context,
                        (std::string(getFormatName(format)) + "_nearest_" + name + "_blit_from_default").c_str(), "",
                        format, GL_NEAREST, DefaultFramebufferBlitCase::BLIT_DEFAULT_TO_TARGET, areas[areaNdx].area));
                    defaultFbGroup->addChild(new DefaultFramebufferBlitCase(
                        m_context,
                        (std::string(getFormatName(format)) + "_nearest_" + name + "_blit_to_default").c_str(), "",
                        format, GL_NEAREST, DefaultFramebufferBlitCase::BLIT_TO_DEFAULT_FROM_TARGET,
                        areas[areaNdx].area));
                }

                if (addLinear)
                {
                    defaultFbGroup->addChild(new DefaultFramebufferBlitCase(
                        m_context,
                        (std::string(getFormatName(format)) + "_linear_" + name + "_blit_from_default").c_str(), "",
                        format, GL_LINEAR, DefaultFramebufferBlitCase::BLIT_DEFAULT_TO_TARGET, areas[areaNdx].area));
                    defaultFbGroup->addChild(new DefaultFramebufferBlitCase(
                        m_context,
                        (std::string(getFormatName(format)) + "_linear_" + name + "_blit_to_default").c_str(), "",
                        format, GL_LINEAR, DefaultFramebufferBlitCase::BLIT_TO_DEFAULT_FROM_TARGET,
                        areas[areaNdx].area));
                }
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
