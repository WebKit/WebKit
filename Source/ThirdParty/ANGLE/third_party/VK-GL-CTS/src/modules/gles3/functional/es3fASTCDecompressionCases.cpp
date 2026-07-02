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
 * \brief ASTC decompression tests
 *
 * \todo Parts of the block-generation code are same as in decompression
 *         code in tcuCompressedTexture.cpp ; could put them to some shared
 *         ASTC utility file.
 *
 * \todo Tests for void extents with nontrivial extent coordinates.
 *
 * \todo Better checking of the error color. Currently legitimate error
 *         pixels are just ignored in image comparison; however, spec says
 *         that error color is either magenta or all-NaNs. Can NaNs cause
 *         troubles, or can we assume that NaNs are well-supported in shader
 *         if the implementation chooses NaNs as error color?
 *//*--------------------------------------------------------------------*/

#include "es3fASTCDecompressionCases.hpp"
#include "gluTexture.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "glsTextureTestUtil.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deFloat16.h"
#include "deString.h"
#include "deMemory.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <vector>
#include <string>
#include <algorithm>

using std::string;
using std::vector;
using tcu::CompressedTexFormat;
using tcu::CompressedTexture;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Sampler;
using tcu::Surface;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec4;
using tcu::astc::BlockTestType;

namespace deqp
{

using gls::TextureTestUtil::RandomViewport;
using gls::TextureTestUtil::TextureRenderer;
using namespace glu::TextureTestUtil;

namespace gles3
{
namespace Functional
{

namespace ASTCDecompressionCaseInternal
{

// Get a string describing the data of an ASTC block. Currently contains just hex and bin dumps of the block.
static string astcBlockDataStr(const uint8_t *data)
{
    string result;
    result += "  Hexadecimal (big endian: upper left hex digit is block bits 127 to 124):";

    {
        static const char *const hexDigits = "0123456789ABCDEF";

        for (int i = tcu::astc::BLOCK_SIZE_BYTES - 1; i >= 0; i--)
        {
            if ((i + 1) % 2 == 0)
                result += "\n    ";
            else
                result += "  ";

            result += hexDigits[(data[i] & 0xf0) >> 4];
            result += " ";
            result += hexDigits[(data[i] & 0x0f) >> 0];
        }
    }

    result += "\n\n  Binary (big endian: upper left bit is block bit 127):";

    for (int i = tcu::astc::BLOCK_SIZE_BYTES - 1; i >= 0; i--)
    {
        if ((i + 1) % 2 == 0)
            result += "\n    ";
        else
            result += "  ";

        for (int j = 8 - 1; j >= 0; j--)
        {
            if (j == 3)
                result += " ";

            result += (data[i] >> j) & 1 ? "1" : "0";
        }
    }

    result += "\n";

    return result;
}

// Compare reference and result block images, reporting also the position of the first non-matching block.
static bool compareBlockImages(const Surface &reference, const Surface &result, const tcu::RGBA &thresholdRGBA,
                               const IVec2 &blockSize, int numUsedBlocks, IVec2 &firstFailedBlockCoordDst,
                               Surface &errorMaskDst, IVec4 &maxDiffDst)
{
    TCU_CHECK_INTERNAL(reference.getWidth() == result.getWidth() && reference.getHeight() == result.getHeight());

    const int width       = result.getWidth();
    const int height      = result.getHeight();
    const IVec4 threshold = thresholdRGBA.toIVec();
    const int numXBlocks  = width / blockSize.x();

    DE_ASSERT(width % blockSize.x() == 0 && height % blockSize.y() == 0);

    errorMaskDst.setSize(width, height);

    firstFailedBlockCoordDst = IVec2(-1, -1);
    maxDiffDst               = IVec4(0);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            const IVec2 blockCoord = IVec2(x, y) / blockSize;

            if (blockCoord.y() * numXBlocks + blockCoord.x() < numUsedBlocks)
            {
                const IVec4 refPix = reference.getPixel(x, y).toIVec();

                if (refPix == IVec4(255, 0, 255, 255))
                {
                    // ASTC error color - allow anything in result.
                    errorMaskDst.setPixel(x, y, tcu::RGBA(255, 0, 255, 255));
                    continue;
                }

                const IVec4 resPix = result.getPixel(x, y).toIVec();
                const IVec4 diff   = tcu::abs(refPix - resPix);
                const bool isOk    = tcu::boolAll(tcu::lessThanEqual(diff, threshold));

                maxDiffDst = tcu::max(maxDiffDst, diff);

                errorMaskDst.setPixel(x, y, isOk ? tcu::RGBA::green() : tcu::RGBA::red());

                if (!isOk && firstFailedBlockCoordDst.x() == -1)
                    firstFailedBlockCoordDst = blockCoord;
            }
        }

    return boolAll(lessThanEqual(maxDiffDst, threshold));
}

enum ASTCSupportLevel
{
    // \note Ordered from smallest subset to full, for convenient comparison.
    ASTCSUPPORTLEVEL_NONE = 0,
    ASTCSUPPORTLEVEL_LDR,
    ASTCSUPPORTLEVEL_HDR,
    ASTCSUPPORTLEVEL_FULL
};

static inline ASTCSupportLevel getASTCSupportLevel(const glu::ContextInfo &contextInfo,
                                                   const glu::RenderContext &renderCtx)
{
    const bool isES32 = glu::contextSupports(renderCtx.getType(), glu::ApiType::es(3, 2));

    const vector<string> &extensions = contextInfo.getExtensions();

    ASTCSupportLevel maxLevel = ASTCSUPPORTLEVEL_NONE;

    for (int extNdx = 0; extNdx < (int)extensions.size(); extNdx++)
    {
        const string &ext = extensions[extNdx];
        if (isES32)
        {
            maxLevel = de::max(maxLevel, ext == "GL_KHR_texture_compression_astc_hdr" ? ASTCSUPPORTLEVEL_HDR :
                                         ext == "GL_OES_texture_compression_astc"     ? ASTCSUPPORTLEVEL_FULL :
                                                                                        ASTCSUPPORTLEVEL_LDR);
        }
        else
        {
            maxLevel = de::max(maxLevel, ext == "GL_KHR_texture_compression_astc_ldr" ? ASTCSUPPORTLEVEL_LDR :
                                         ext == "GL_KHR_texture_compression_astc_hdr" ? ASTCSUPPORTLEVEL_HDR :
                                         ext == "GL_OES_texture_compression_astc"     ? ASTCSUPPORTLEVEL_FULL :
                                                                                        ASTCSUPPORTLEVEL_NONE);
        }
    }

    return maxLevel;
}

// Class handling the common rendering stuff of ASTC cases.
class ASTCRenderer2D
{
public:
    ASTCRenderer2D(Context &context, CompressedTexFormat format, uint32_t randomSeed);

    ~ASTCRenderer2D(void);

    void initialize(int minRenderWidth, int minRenderHeight, const Vec4 &colorScale, const Vec4 &colorBias);
    void clear(void);

    void render(Surface &referenceDst, Surface &resultDst, const glu::Texture2D &texture,
                const tcu::TextureFormat &uncompressedFormat);

    CompressedTexFormat getFormat(void) const
    {
        return m_format;
    }
    IVec2 getBlockSize(void) const
    {
        return m_blockSize;
    }
    ASTCSupportLevel getASTCSupport(void) const
    {
        DE_ASSERT(m_initialized);
        return m_astcSupport;
    }

private:
    Context &m_context;
    TextureRenderer m_renderer;

    const CompressedTexFormat m_format;
    const IVec2 m_blockSize;
    ASTCSupportLevel m_astcSupport;
    Vec4 m_colorScale;
    Vec4 m_colorBias;

    de::Random m_rnd;

    bool m_initialized;
};

} // namespace ASTCDecompressionCaseInternal

using namespace ASTCDecompressionCaseInternal;

ASTCRenderer2D::ASTCRenderer2D(Context &context, CompressedTexFormat format, uint32_t randomSeed)
    : m_context(context)
    , m_renderer(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES,
                 glu::PRECISION_HIGHP)
    , m_format(format)
    , m_blockSize(tcu::getBlockPixelSize(format).xy())
    , m_astcSupport(ASTCSUPPORTLEVEL_NONE)
    , m_colorScale(-1.0f)
    , m_colorBias(-1.0f)
    , m_rnd(randomSeed)
    , m_initialized(false)
{
    DE_ASSERT(tcu::getBlockPixelSize(format).z() == 1);
}

ASTCRenderer2D::~ASTCRenderer2D(void)
{
    clear();
}

void ASTCRenderer2D::initialize(int minRenderWidth, int minRenderHeight, const Vec4 &colorScale, const Vec4 &colorBias)
{
    DE_ASSERT(!m_initialized);

    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
    TestLog &log                          = m_context.getTestContext().getLog();

    m_astcSupport = getASTCSupportLevel(m_context.getContextInfo(), m_context.getRenderContext());
    m_colorScale  = colorScale;
    m_colorBias   = colorBias;

    switch (m_astcSupport)
    {
    case ASTCSUPPORTLEVEL_NONE:
        log << TestLog::Message << "No ASTC support detected" << TestLog::EndMessage;
        throw tcu::NotSupportedError("ASTC not supported");
    case ASTCSUPPORTLEVEL_LDR:
        log << TestLog::Message << "LDR ASTC support detected" << TestLog::EndMessage;
        break;
    case ASTCSUPPORTLEVEL_HDR:
        log << TestLog::Message << "HDR ASTC support detected" << TestLog::EndMessage;
        break;
    case ASTCSUPPORTLEVEL_FULL:
        log << TestLog::Message << "Full ASTC support detected" << TestLog::EndMessage;
        break;
    default:
        DE_ASSERT(false);
    }

    if (renderTarget.getWidth() < minRenderWidth || renderTarget.getHeight() < minRenderHeight)
        throw tcu::NotSupportedError("Render target must be at least " + de::toString(minRenderWidth) + "x" +
                                     de::toString(minRenderHeight));

    log << TestLog::Message << "Using color scale and bias: result = raw * " << colorScale << " + " << colorBias
        << TestLog::EndMessage;

    m_initialized = true;
}

void ASTCRenderer2D::clear(void)
{
    m_renderer.clear();
}

void ASTCRenderer2D::render(Surface &referenceDst, Surface &resultDst, const glu::Texture2D &texture,
                            const tcu::TextureFormat &uncompressedFormat)
{
    DE_ASSERT(m_initialized);

    const glw::Functions &gl            = m_context.getRenderContext().getFunctions();
    const glu::RenderContext &renderCtx = m_context.getRenderContext();
    const int textureWidth              = texture.getRefTexture().getWidth();
    const int textureHeight             = texture.getRefTexture().getHeight();
    const RandomViewport viewport(renderCtx.getRenderTarget(), textureWidth, textureHeight, m_rnd.getUint32());
    ReferenceParams renderParams(TEXTURETYPE_2D);
    vector<float> texCoord;
    computeQuadTexCoord2D(texCoord, Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f));

    renderParams.samplerType = getSamplerType(uncompressedFormat);
    renderParams.sampler     = Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE,
                                       Sampler::NEAREST, Sampler::NEAREST);
    renderParams.colorScale  = m_colorScale;
    renderParams.colorBias   = m_colorBias;

    // Setup base viewport.
    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, texture.getGLTexture());

    // Setup nearest neighbor filtering and clamp-to-edge.
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Issue GL draws.
    m_renderer.renderQuad(0, &texCoord[0], renderParams);
    gl.flush();

    // Compute reference.
    sampleTexture(tcu::SurfaceAccess(referenceDst, renderCtx.getRenderTarget().getPixelFormat()),
                  texture.getRefTexture(), &texCoord[0], renderParams);

    // Read GL-rendered image.
    glu::readPixels(renderCtx, viewport.x, viewport.y, resultDst.getAccess());
}

ASTCBlockCase2D::ASTCBlockCase2D(Context &context, const char *name, const char *description, BlockTestType testType,
                                 CompressedTexFormat format)
    : TestCase(context, name, description)
    , m_testType(testType)
    , m_format(format)
    , m_numBlocksTested(0)
    , m_currentIteration(0)
    , m_renderer(new ASTCRenderer2D(context, format, deStringHash(getName())))
{
    DE_ASSERT(!(tcu::isAstcSRGBFormat(m_format) &&
                tcu::astc::isBlockTestTypeHDROnly(
                    m_testType))); // \note There is no HDR sRGB mode, so these would be redundant.
}

ASTCBlockCase2D::~ASTCBlockCase2D(void)
{
    ASTCBlockCase2D::deinit();
}

void ASTCBlockCase2D::init(void)
{
    m_renderer->initialize(64, 64, tcu::astc::getBlockTestTypeColorScale(m_testType),
                           tcu::astc::getBlockTestTypeColorBias(m_testType));

    generateBlockCaseTestData(m_blockData, m_format, m_testType);
    DE_ASSERT(!m_blockData.empty());
    DE_ASSERT(m_blockData.size() % tcu::astc::BLOCK_SIZE_BYTES == 0);

    m_testCtx.getLog() << TestLog::Message << "Total " << m_blockData.size() / tcu::astc::BLOCK_SIZE_BYTES
                       << " blocks to test" << TestLog::EndMessage << TestLog::Message
                       << "Note: Legitimate ASTC error pixels will be ignored when comparing to reference"
                       << TestLog::EndMessage;
}

void ASTCBlockCase2D::deinit(void)
{
    m_renderer->clear();
    m_blockData.clear();
}

ASTCBlockCase2D::IterateResult ASTCBlockCase2D::iterate(void)
{
    TestLog &log = m_testCtx.getLog();

    if (m_renderer->getASTCSupport() == ASTCSUPPORTLEVEL_LDR && tcu::astc::isBlockTestTypeHDROnly(m_testType))
    {
        log << TestLog::Message
            << "Passing the case immediately, since only LDR support was detected and test only contains HDR blocks"
            << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }

    const IVec2 blockSize               = m_renderer->getBlockSize();
    const int totalNumBlocks            = (int)m_blockData.size() / tcu::astc::BLOCK_SIZE_BYTES;
    const int numXBlocksPerImage        = de::min(m_context.getRenderTarget().getWidth(), 512) / blockSize.x();
    const int numYBlocksPerImage        = de::min(m_context.getRenderTarget().getHeight(), 512) / blockSize.y();
    const int numBlocksPerImage         = numXBlocksPerImage * numYBlocksPerImage;
    const int imageWidth                = numXBlocksPerImage * blockSize.x();
    const int imageHeight               = numYBlocksPerImage * blockSize.y();
    const int numBlocksRemaining        = totalNumBlocks - m_numBlocksTested;
    const int curNumUsedBlocks          = de::min(numBlocksPerImage, numBlocksRemaining);
    const int curNumUnusedBlocks        = numBlocksPerImage - curNumUsedBlocks;
    const glu::RenderContext &renderCtx = m_context.getRenderContext();
    const tcu::RGBA threshold           = renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() +
                                (tcu::isAstcSRGBFormat(m_format) ? tcu::RGBA(2, 2, 2, 2) : tcu::RGBA(1, 1, 1, 1));
    tcu::CompressedTexture compressed(m_format, imageWidth, imageHeight);

    if (m_currentIteration == 0)
    {
        log << TestLog::Message << "Using texture of size " << imageWidth << "x" << imageHeight << ", with "
            << numXBlocksPerImage << " block columns and " << numYBlocksPerImage << " block rows "
            << ", with block size " << blockSize.x() << "x" << blockSize.y() << TestLog::EndMessage;
    }

    DE_ASSERT(compressed.getDataSize() == numBlocksPerImage * tcu::astc::BLOCK_SIZE_BYTES);
    deMemcpy(compressed.getData(), &m_blockData[m_numBlocksTested * tcu::astc::BLOCK_SIZE_BYTES],
             curNumUsedBlocks * tcu::astc::BLOCK_SIZE_BYTES);
    if (curNumUsedBlocks > 1)
        tcu::astc::generateDefaultVoidExtentBlocks(
            (uint8_t *)compressed.getData() + curNumUsedBlocks * tcu::astc::BLOCK_SIZE_BYTES, curNumUnusedBlocks);

    // Create texture and render.

    const tcu::TexDecompressionParams::AstcMode decompressionMode =
        (m_renderer->getASTCSupport() == ASTCSUPPORTLEVEL_LDR || tcu::isAstcSRGBFormat(m_format)) ?
            tcu::TexDecompressionParams::ASTCMODE_LDR :
            tcu::TexDecompressionParams::ASTCMODE_HDR;
    glu::Texture2D texture(renderCtx, m_context.getContextInfo(), 1, &compressed,
                           tcu::TexDecompressionParams(decompressionMode));
    Surface renderedFrame(imageWidth, imageHeight);
    Surface referenceFrame(imageWidth, imageHeight);

    m_renderer->render(referenceFrame, renderedFrame, texture, getUncompressedFormat(compressed.getFormat()));

    // Compare and log.
    // \note Since a case can draw quite many images, only log the first iteration and failures.

    {
        Surface errorMask;
        IVec2 firstFailedBlockCoord;
        IVec4 maxDiff;
        const bool compareOk = compareBlockImages(referenceFrame, renderedFrame, threshold, blockSize, curNumUsedBlocks,
                                                  firstFailedBlockCoord, errorMask, maxDiff);

        if (m_currentIteration == 0 || !compareOk)
        {
            const char *const imageSetName = "ComparisonResult";
            const char *const imageSetDesc = "Comparison Result";

            {
                tcu::ScopedLogSection section(log, "Iteration " + de::toString(m_currentIteration),
                                              "Blocks " + de::toString(m_numBlocksTested) + " to " +
                                                  de::toString(m_numBlocksTested + curNumUsedBlocks - 1));

                if (curNumUsedBlocks > 0)
                    log << TestLog::Message << "Note: Only the first " << curNumUsedBlocks
                        << " blocks in the image are relevant; rest " << curNumUnusedBlocks
                        << " are dummies and not checked" << TestLog::EndMessage;

                if (!compareOk)
                {
                    log << TestLog::Message << "Image comparison failed: max difference = " << maxDiff
                        << ", threshold = " << threshold << TestLog::EndMessage
                        << TestLog::ImageSet(imageSetName, imageSetDesc)
                        << TestLog::Image("Result", "Result", renderedFrame)
                        << TestLog::Image("Reference", "Reference", referenceFrame)
                        << TestLog::Image("ErrorMask", "Error mask", errorMask) << TestLog::EndImageSet;

                    const int blockNdx =
                        m_numBlocksTested + firstFailedBlockCoord.y() * numXBlocksPerImage + firstFailedBlockCoord.x();
                    DE_ASSERT(blockNdx < totalNumBlocks);

                    log << TestLog::Message << "First failed block at column " << firstFailedBlockCoord.x()
                        << " and row " << firstFailedBlockCoord.y() << TestLog::EndMessage << TestLog::Message
                        << "Data of first failed block:\n"
                        << astcBlockDataStr(&m_blockData[blockNdx * tcu::astc::BLOCK_SIZE_BYTES])
                        << TestLog::EndMessage;

                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
                    return STOP;
                }
                else
                {
                    log << TestLog::ImageSet(imageSetName, imageSetDesc)
                        << TestLog::Image("Result", "Result", renderedFrame) << TestLog::EndImageSet;
                }
            }

            if (m_numBlocksTested + curNumUsedBlocks < totalNumBlocks)
                log << TestLog::Message << "Note: not logging further images unless reference comparison fails"
                    << TestLog::EndMessage;
        }
    }

    m_currentIteration++;
    m_numBlocksTested += curNumUsedBlocks;

    if (m_numBlocksTested >= totalNumBlocks)
    {
        DE_ASSERT(m_numBlocksTested == totalNumBlocks);
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }

    return CONTINUE;
}

ASTCBlockSizeRemainderCase2D::ASTCBlockSizeRemainderCase2D(Context &context, const char *name, const char *description,
                                                           CompressedTexFormat format)
    : TestCase(context, name, description)
    , m_format(format)
    , m_currentIteration(0)
    , m_renderer(new ASTCRenderer2D(context, format, deStringHash(getName())))
{
}

ASTCBlockSizeRemainderCase2D::~ASTCBlockSizeRemainderCase2D(void)
{
    ASTCBlockSizeRemainderCase2D::deinit();
}

void ASTCBlockSizeRemainderCase2D::init(void)
{
    const IVec2 blockSize = m_renderer->getBlockSize();
    m_renderer->initialize(MAX_NUM_BLOCKS_X * blockSize.x(), MAX_NUM_BLOCKS_Y * blockSize.y(), Vec4(1.0f), Vec4(0.0f));
}

void ASTCBlockSizeRemainderCase2D::deinit(void)
{
    m_renderer->clear();
}

ASTCBlockSizeRemainderCase2D::IterateResult ASTCBlockSizeRemainderCase2D::iterate(void)
{
    TestLog &log                        = m_testCtx.getLog();
    const IVec2 blockSize               = m_renderer->getBlockSize();
    const int curRemainderX             = m_currentIteration % blockSize.x();
    const int curRemainderY             = m_currentIteration / blockSize.x();
    const int imageWidth                = (MAX_NUM_BLOCKS_X - 1) * blockSize.x() + curRemainderX;
    const int imageHeight               = (MAX_NUM_BLOCKS_Y - 1) * blockSize.y() + curRemainderY;
    const int numBlocksX                = deDivRoundUp32(imageWidth, blockSize.x());
    const int numBlocksY                = deDivRoundUp32(imageHeight, blockSize.y());
    const int totalNumBlocks            = numBlocksX * numBlocksY;
    const glu::RenderContext &renderCtx = m_context.getRenderContext();
    const tcu::RGBA threshold           = renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() +
                                (tcu::isAstcSRGBFormat(m_format) ? tcu::RGBA(2, 2, 2, 2) : tcu::RGBA(1, 1, 1, 1));
    tcu::CompressedTexture compressed(m_format, imageWidth, imageHeight);

    DE_ASSERT(compressed.getDataSize() == totalNumBlocks * tcu::astc::BLOCK_SIZE_BYTES);
    tcu::astc::generateDefaultNormalBlocks((uint8_t *)compressed.getData(), totalNumBlocks, blockSize.x(),
                                           blockSize.y());

    // Create texture and render.

    const tcu::TexDecompressionParams::AstcMode decompressionMode =
        (m_renderer->getASTCSupport() == ASTCSUPPORTLEVEL_LDR || tcu::isAstcSRGBFormat(m_format)) ?
            tcu::TexDecompressionParams::ASTCMODE_LDR :
            tcu::TexDecompressionParams::ASTCMODE_HDR;
    Surface renderedFrame(imageWidth, imageHeight);
    Surface referenceFrame(imageWidth, imageHeight);
    glu::Texture2D texture(renderCtx, m_context.getContextInfo(), 1, &compressed,
                           tcu::TexDecompressionParams(decompressionMode));

    m_renderer->render(referenceFrame, renderedFrame, texture, getUncompressedFormat(compressed.getFormat()));

    {
        // Compare and log.

        tcu::ScopedLogSection section(log, "Iteration " + de::toString(m_currentIteration),
                                      "Remainder " + de::toString(curRemainderX) + "x" + de::toString(curRemainderY));

        log << TestLog::Message << "Using texture of size " << imageWidth << "x" << imageHeight << " and block size "
            << blockSize.x() << "x" << blockSize.y() << "; the x and y remainders are " << curRemainderX << " and "
            << curRemainderY << " respectively" << TestLog::EndMessage;

        const bool compareOk = tcu::pixelThresholdCompare(
            m_testCtx.getLog(), "ComparisonResult", "Comparison Result", referenceFrame, renderedFrame, threshold,
            m_currentIteration == 0 ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

        if (!compareOk)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
            return STOP;
        }
    }

    if (m_currentIteration == 0 && m_currentIteration + 1 < blockSize.x() * blockSize.y())
        log << TestLog::Message << "Note: not logging further images unless reference comparison fails"
            << TestLog::EndMessage;

    m_currentIteration++;

    if (m_currentIteration >= blockSize.x() * blockSize.y())
    {
        DE_ASSERT(m_currentIteration == blockSize.x() * blockSize.y());
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
    return CONTINUE;
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
