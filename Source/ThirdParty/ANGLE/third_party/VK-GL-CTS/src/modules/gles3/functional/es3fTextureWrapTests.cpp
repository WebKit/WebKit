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
 * \brief Texture wrap mode tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureWrapTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::CompressedTexFormat;
using tcu::CompressedTexture;
using tcu::Sampler;
using tcu::TestLog;
using namespace glu;
using namespace gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

//! Checks whether any ASTC version (LDR, HDR, full) is supported.
static inline bool isASTCSupported(const glu::ContextInfo &contextInfo)
{
    const vector<string> &extensions = contextInfo.getExtensions();

    for (int extNdx = 0; extNdx < (int)extensions.size(); extNdx++)
    {
        const string &ext = extensions[extNdx];

        if (ext == "GL_KHR_texture_compression_astc_ldr" || ext == "GL_KHR_texture_compression_astc_hdr" ||
            ext == "GL_OES_texture_compression_astc")
            return true;
    }

    return false;
}

enum
{
    VIEWPORT_WIDTH  = 256,
    VIEWPORT_HEIGHT = 256
};

class TextureWrapCase : public tcu::TestCase
{
public:
    TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                    const char *name, const char *description, uint32_t format, uint32_t dataType, uint32_t wrapS,
                    uint32_t wrapT, uint32_t minFilter, uint32_t magFilter, int width, int height,
                    bool enableRelaxedRef = false);
    TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                    const char *name, const char *description, uint32_t wrapS, uint32_t wrapT, uint32_t minFilter,
                    uint32_t magFilter, const std::vector<std::string> &filenames, bool enableRelaxedRef = false);
    TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                    const char *name, const char *description, CompressedTexFormat compressedFormat, uint32_t wrapS,
                    uint32_t wrapT, uint32_t minFilter, uint32_t magFilter, int width, int height,
                    bool enableRelaxedRef = false);
    ~TextureWrapCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureWrapCase(const TextureWrapCase &other);
    TextureWrapCase &operator=(const TextureWrapCase &other);

    struct Case
    {
        tcu::Vec2 bottomLeft;
        tcu::Vec2 topRight;

        Case(void)
        {
        }
        Case(const tcu::Vec2 &bl, const tcu::Vec2 &tr) : bottomLeft(bl), topRight(tr)
        {
        }
    };

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    const uint32_t m_format;
    const uint32_t m_dataType;
    const CompressedTexFormat m_compressedFormat;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;
    const uint32_t m_minFilter;
    const uint32_t m_magFilter;

    int m_width;
    int m_height;
    const std::vector<std::string> m_filenames;

    vector<Case> m_cases;
    int m_caseNdx;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;

    bool m_enableRelaxedRef;
};

TextureWrapCase::TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                 const glu::ContextInfo &ctxInfo, const char *name, const char *description,
                                 uint32_t format, uint32_t dataType, uint32_t wrapS, uint32_t wrapT, uint32_t minFilter,
                                 uint32_t magFilter, int width, int height, bool enableRelaxedRef)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_format(format)
    , m_dataType(dataType)
    , m_compressedFormat(tcu::COMPRESSEDTEXFORMAT_LAST)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_width(width)
    , m_height(height)
    , m_caseNdx(0)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_MEDIUMP)
    , m_enableRelaxedRef(enableRelaxedRef)
{
}

TextureWrapCase::TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                 const glu::ContextInfo &ctxInfo, const char *name, const char *description,
                                 uint32_t wrapS, uint32_t wrapT, uint32_t minFilter, uint32_t magFilter,
                                 const std::vector<std::string> &filenames, bool enableRelaxedRef)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_format(GL_NONE)
    , m_dataType(GL_NONE)
    , m_compressedFormat(tcu::COMPRESSEDTEXFORMAT_LAST)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_width(0)
    , m_height(0)
    , m_filenames(filenames)
    , m_caseNdx(0)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_MEDIUMP)
    , m_enableRelaxedRef(enableRelaxedRef)
{
}

TextureWrapCase::TextureWrapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                 const glu::ContextInfo &ctxInfo, const char *name, const char *description,
                                 CompressedTexFormat compressedFormat, uint32_t wrapS, uint32_t wrapT,
                                 uint32_t minFilter, uint32_t magFilter, int width, int height, bool enableRelaxedRef)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(ctxInfo)
    , m_format(GL_NONE)
    , m_dataType(GL_NONE)
    , m_compressedFormat(compressedFormat)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_width(width)
    , m_height(height)
    , m_caseNdx(0)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_MEDIUMP)
    , m_enableRelaxedRef(enableRelaxedRef)
{
}

TextureWrapCase::~TextureWrapCase(void)
{
    deinit();
}

void TextureWrapCase::init(void)
{
    // Load or generate texture.

    if (!m_filenames.empty())
    {
        // Load compressed texture from file.

        DE_ASSERT(m_width == 0 && m_height == 0 && m_format == GL_NONE && m_dataType == GL_NONE);

        m_texture = glu::Texture2D::create(m_renderCtx, m_renderCtxInfo, m_testCtx.getArchive(),
                                           (int)m_filenames.size(), m_filenames);
        m_width   = m_texture->getRefTexture().getWidth();
        m_height  = m_texture->getRefTexture().getHeight();
    }
    else if (m_compressedFormat != tcu::COMPRESSEDTEXFORMAT_LAST)
    {
        // Generate compressed texture.

        DE_ASSERT(m_format == GL_NONE && m_dataType == GL_NONE);

        if (tcu::isEtcFormat(m_compressedFormat))
        {
            // Create ETC texture. Any content is valid.

            tcu::CompressedTexture compressedTexture(m_compressedFormat, m_width, m_height);
            const int dataSize  = compressedTexture.getDataSize();
            uint8_t *const data = (uint8_t *)compressedTexture.getData();
            de::Random rnd(deStringHash(getName()));

            for (int i = 0; i < dataSize; i++)
                data[i] = rnd.getUint32() & 0xff;

            m_texture = new glu::Texture2D(m_renderCtx, m_renderCtxInfo, 1, &compressedTexture);
        }
        else if (tcu::isAstcFormat(m_compressedFormat))
        {
            // Create ASTC texture by picking from a set of pre-generated blocks.

            static const int BLOCK_SIZE               = 16;
            static const uint8_t blocks[][BLOCK_SIZE] = {
                // \note All of the following blocks are valid in LDR mode.
                {
                    252,
                    253,
                    255,
                    255,
                    255,
                    255,
                    255,
                    255,
                    8,
                    71,
                    90,
                    78,
                    22,
                    17,
                    26,
                    66,
                },
                {252, 253, 255, 255, 255, 255, 255, 255, 220, 74, 139, 235, 249, 6, 145, 125},
                {252, 253, 255, 255, 255, 255, 255, 255, 223, 251, 28, 206, 54, 251, 160, 174},
                {252, 253, 255, 255, 255, 255, 255, 255, 39, 4, 153, 219, 180, 61, 51, 37},
                {67, 2, 0, 254, 1, 0, 64, 215, 83, 211, 159, 105, 41, 140, 50, 2},
                {67, 130, 0, 170, 84, 255, 65, 215, 83, 211, 159, 105, 41, 140, 50, 2},
                {67, 2, 129, 38, 51, 229, 95, 215, 83, 211, 159, 105, 41, 140, 50, 2},
                {67, 130, 193, 56, 213, 144, 95, 215, 83, 211, 159, 105, 41, 140, 50, 2}};

            if (!isASTCSupported(
                    m_renderCtxInfo)) // \note Any level of ASTC support is enough, since we're only using LDR blocks.
                throw tcu::NotSupportedError("ASTC not supported");

            tcu::CompressedTexture compressedTexture(m_compressedFormat, m_width, m_height);
            const int dataSize  = compressedTexture.getDataSize();
            uint8_t *const data = (uint8_t *)compressedTexture.getData();
            de::Random rnd(deStringHash(getName()));
            DE_ASSERT(dataSize % BLOCK_SIZE == 0);

            for (int i = 0; i < dataSize / BLOCK_SIZE; i++)
                deMemcpy(&data[i * BLOCK_SIZE], &blocks[rnd.getInt(0, DE_LENGTH_OF_ARRAY(blocks) - 1)][0], BLOCK_SIZE);

            // \note All blocks are valid LDR blocks so ASTCMODE_* doesn't change anything
            m_texture = new glu::Texture2D(m_renderCtx, m_renderCtxInfo, 1, &compressedTexture,
                                           tcu::TexDecompressionParams(tcu::TexDecompressionParams::ASTCMODE_LDR));
        }
        else
            DE_ASSERT(false);
    }
    else
    {
        m_texture = new Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height);

        // Fill level 0.
        m_texture->getRefTexture().allocLevel(0);
        if (m_wrapS == GL_REPEAT || m_wrapT == GL_REPEAT)
        {
            // If run in repeat mode, use conical style texture to avoid edge sample result have a huge difference when coordinate offset in allow range.
            tcu::fillWithComponentGradients3(m_texture->getRefTexture().getLevel(0),
                                             tcu::Vec4(-0.5f, -0.5f, -0.5f, 1.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
        }
        else
        {
            tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(0),
                                            tcu::Vec4(-0.5f, -0.5f, -0.5f, 1.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
        }

        m_texture->upload();
    }

    // Sub-cases.

    m_cases.push_back(Case(tcu::Vec2(-1.5f, -3.0f), tcu::Vec2(1.5f, 2.5f)));
    m_cases.push_back(Case(tcu::Vec2(-0.5f, 0.75f), tcu::Vec2(0.25f, 1.25f)));
    DE_ASSERT(m_caseNdx == 0);

    // Initialize to success, set to failure later if needed.

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void TextureWrapCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

TextureWrapCase::IterateResult TextureWrapCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    const RandomViewport viewport(m_renderCtx.getRenderTarget(), VIEWPORT_WIDTH, VIEWPORT_HEIGHT,
                                  deStringHash(getName()) + m_caseNdx);
    tcu::Surface renderedFrame(viewport.width, viewport.height);
    ReferenceParams refParams(TEXTURETYPE_2D);
    const tcu::TextureFormat texFormat = m_texture->getRefTexture().getFormat();
    vector<float> texCoord;
    const tcu::TextureFormatInfo texFormatInfo = tcu::getTextureFormatInfo(texFormat);
    // \note For non-sRGB ASTC formats, the values are fp16 in range [0..1], not the range assumed given by tcu::getTextureFormatInfo().
    const bool useDefaultColorScaleAndBias =
        !tcu::isAstcFormat(m_compressedFormat) || tcu::isAstcSRGBFormat(m_compressedFormat);

    // Bind to unit 0.
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());

    // Setup filtering and wrap modes.
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

    // Parameters for reference images.
    refParams.sampler     = mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    refParams.lodMode     = LODMODE_EXACT;
    refParams.samplerType = getSamplerType(m_texture->getRefTexture().getFormat());
    refParams.colorScale  = useDefaultColorScaleAndBias ? texFormatInfo.lookupScale : tcu::Vec4(1.0f);
    refParams.colorBias   = useDefaultColorScaleAndBias ? texFormatInfo.lookupBias : tcu::Vec4(0.0f);

    gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    computeQuadTexCoord2D(texCoord, m_cases[m_caseNdx].bottomLeft, m_cases[m_caseNdx].topRight);
    m_renderer.renderQuad(0, &texCoord[0], refParams);
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    {
        const tcu::ScopedLogSection section(log, string("Test") + de::toString(m_caseNdx),
                                            string("Test ") + de::toString(m_caseNdx));
        const bool isNearestOnly           = m_minFilter == GL_NEAREST && m_magFilter == GL_NEAREST;
        const bool isSRGB                  = tcu::isSRGB(texFormat);
        const tcu::PixelFormat pixelFormat = m_renderCtx.getRenderTarget().getPixelFormat();
        const tcu::IVec4 colorBits =
            tcu::max(getBitsVec(pixelFormat) - (isNearestOnly && !isSRGB ? 1 : 2), tcu::IVec4(0));
        tcu::LodPrecision lodPrecision;
        tcu::LookupPrecision lookupPrecision;

        lodPrecision.derivateBits = 18;
        lodPrecision.lodBits      = 5;
        lookupPrecision.colorThreshold =
            tcu::computeColorBitsThreshold(getBitsVec(pixelFormat), colorBits) / refParams.colorScale;
        lookupPrecision.coordBits = tcu::IVec3(20, 20, 0);
        lookupPrecision.uvwBits   = tcu::IVec3(5, 5, 0);
        lookupPrecision.colorMask = getCompareMask(pixelFormat);

        log << TestLog::Message << "Note: lookup coordinates: bottom-left " << m_cases[m_caseNdx].bottomLeft
            << ", top-right " << m_cases[m_caseNdx].topRight << TestLog::EndMessage;

        bool isOk = verifyTextureResult(m_testCtx, renderedFrame.getAccess(), m_texture->getRefTexture(), &texCoord[0],
                                        refParams, lookupPrecision, lodPrecision, pixelFormat);

        if ((isOk == false) && m_enableRelaxedRef && m_renderer.getTexCoordPrecision() != PRECISION_HIGHP)
        {
            refParams.float16TexCoord = true;
            isOk |= verifyTextureResult(m_testCtx, renderedFrame.getAccess(), m_texture->getRefTexture(), &texCoord[0],
                                        refParams, lookupPrecision, lodPrecision, pixelFormat);
        }

        if (!isOk)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image verification failed");
    }

    m_caseNdx++;
    return m_caseNdx < (int)m_cases.size() ? CONTINUE : STOP;
}

TextureWrapTests::TextureWrapTests(Context &context) : TestCaseGroup(context, "wrap", "Wrap Mode Tests")
{
}

TextureWrapTests::~TextureWrapTests(void)
{
}

void TextureWrapTests::init(void)
{
    static const struct
    {
        const char *name;
        uint32_t mode;
    } wrapModes[] = {{"clamp", GL_CLAMP_TO_EDGE}, {"repeat", GL_REPEAT}, {"mirror", GL_MIRRORED_REPEAT}};

    static const struct
    {
        const char *name;
        uint32_t mode;
    } filteringModes[] = {{"nearest", GL_NEAREST}, {"linear", GL_LINEAR}};

#define FOR_EACH(ITERATOR, ARRAY, BODY)                                      \
    for (int ITERATOR = 0; ITERATOR < DE_LENGTH_OF_ARRAY(ARRAY); ITERATOR++) \
    BODY

    // RGBA8 cases.
    {
        static const struct
        {
            const char *name;
            int width;
            int height;
        } rgba8Sizes[] = {{"pot", 64, 128}, {"npot", 63, 112}};

        {
            TestCaseGroup *const rgba8Group = new TestCaseGroup(m_context, "rgba8", "");
            addChild(rgba8Group);

            FOR_EACH(size, rgba8Sizes,
                     FOR_EACH(wrapS, wrapModes,
                              FOR_EACH(wrapT, wrapModes, FOR_EACH(filter, filteringModes, {
                                           const string name =
                                               string("") + wrapModes[wrapS].name + "_" + wrapModes[wrapT].name + "_" +
                                               filteringModes[filter].name + "_" + rgba8Sizes[size].name;
                                           rgba8Group->addChild(new TextureWrapCase(
                                               m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                               name.c_str(), "", GL_RGBA, GL_UNSIGNED_BYTE, wrapModes[wrapS].mode,
                                               wrapModes[wrapT].mode, filteringModes[filter].mode,
                                               filteringModes[filter].mode, rgba8Sizes[size].width,
                                               rgba8Sizes[size].height));
                                       }))))
        }
    }

    // ETC1 cases.
    {
        TestCaseGroup *const etc1Group = new TestCaseGroup(m_context, "etc1", "");
        addChild(etc1Group);

        // Power-of-two ETC1 texture
        std::vector<std::string> potFilenames;
        potFilenames.push_back("data/etc1/photo_helsinki_mip_0.pkm");

        FOR_EACH(wrapS, wrapModes,
                 FOR_EACH(wrapT, wrapModes, FOR_EACH(filter, filteringModes, {
                              const string name = string("") + wrapModes[wrapS].name + "_" + wrapModes[wrapT].name +
                                                  "_" + filteringModes[filter].name + "_pot";

                              bool enableRelaxedPrecisionRef = wrapModes[wrapS].mode == GL_REPEAT ||
                                                               wrapModes[wrapT].mode == GL_REPEAT ||
                                                               wrapModes[wrapS].mode == GL_MIRRORED_REPEAT ||
                                                               wrapModes[wrapT].mode == GL_MIRRORED_REPEAT;

                              etc1Group->addChild(new TextureWrapCase(
                                  m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "",
                                  wrapModes[wrapS].mode, wrapModes[wrapT].mode, filteringModes[filter].mode,
                                  filteringModes[filter].mode, potFilenames, enableRelaxedPrecisionRef));
                          })))

        std::vector<std::string> npotFilenames;
        npotFilenames.push_back("data/etc1/photo_helsinki_113x89.pkm");

        // NPOT ETC1 texture
        FOR_EACH(wrapS, wrapModes,
                 FOR_EACH(wrapT, wrapModes, FOR_EACH(filter, filteringModes, {
                              const string name = string("") + wrapModes[wrapS].name + "_" + wrapModes[wrapT].name +
                                                  "_" + filteringModes[filter].name + "_npot";

                              bool enableRelaxedPrecisionRef = wrapModes[wrapS].mode == GL_REPEAT ||
                                                               wrapModes[wrapT].mode == GL_REPEAT ||
                                                               wrapModes[wrapS].mode == GL_MIRRORED_REPEAT ||
                                                               wrapModes[wrapT].mode == GL_MIRRORED_REPEAT;

                              etc1Group->addChild(new TextureWrapCase(
                                  m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.c_str(), "",
                                  wrapModes[wrapS].mode, wrapModes[wrapT].mode, filteringModes[filter].mode,
                                  filteringModes[filter].mode, npotFilenames, enableRelaxedPrecisionRef));
                          })))
    }

    // ETC-2 (and EAC) cases.
    {
        static const struct
        {
            const char *name;
            CompressedTexFormat format;
        } etc2Formats[] = {{
                               "eac_r11",
                               tcu::COMPRESSEDTEXFORMAT_EAC_R11,
                           },
                           {
                               "eac_signed_r11",
                               tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11,
                           },
                           {
                               "eac_rg11",
                               tcu::COMPRESSEDTEXFORMAT_EAC_RG11,
                           },
                           {
                               "eac_signed_rg11",
                               tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11,
                           },
                           {
                               "etc2_rgb8",
                               tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8,
                           },
                           {
                               "etc2_srgb8",
                               tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8,
                           },
                           {
                               "etc2_rgb8_punchthrough_alpha1",
                               tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1,
                           },
                           {
                               "etc2_srgb8_punchthrough_alpha1",
                               tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1,
                           },
                           {
                               "etc2_eac_rgba8",
                               tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8,
                           },
                           {
                               "etc2_eac_srgb8_alpha8",
                               tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8,
                           }};

        static const struct
        {
            const char *name;
            int width;
            int height;
        } etc2Sizes[] = {{"pot", 64, 128}, {"npot", 123, 107}};

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(etc2Formats); formatNdx++)
        {
            TestCaseGroup *const formatGroup = new TestCaseGroup(m_context, etc2Formats[formatNdx].name, "");
            addChild(formatGroup);

            FOR_EACH(size, etc2Sizes,
                     FOR_EACH(wrapS, wrapModes,
                              FOR_EACH(wrapT, wrapModes, FOR_EACH(filter, filteringModes, {
                                           const string name = string("") + wrapModes[wrapS].name + "_" +
                                                               wrapModes[wrapT].name + "_" +
                                                               filteringModes[filter].name + "_" + etc2Sizes[size].name;

                                           bool enableRelaxedPrecisionRef =
                                               wrapModes[wrapS].mode == GL_REPEAT ||
                                               wrapModes[wrapT].mode == GL_REPEAT ||
                                               wrapModes[wrapS].mode == GL_MIRRORED_REPEAT ||
                                               wrapModes[wrapT].mode == GL_MIRRORED_REPEAT;

                                           formatGroup->addChild(new TextureWrapCase(
                                               m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                               name.c_str(), "", etc2Formats[formatNdx].format, wrapModes[wrapS].mode,
                                               wrapModes[wrapT].mode, filteringModes[filter].mode,
                                               filteringModes[filter].mode, etc2Sizes[size].width,
                                               etc2Sizes[size].height, enableRelaxedPrecisionRef));
                                       }))))
        }
    }

    // ASTC cases.
    {
        for (int formatI = 0; formatI < tcu::COMPRESSEDTEXFORMAT_LAST; formatI++)
        {
            const CompressedTexFormat format = (CompressedTexFormat)formatI;

            if (!tcu::isAstcFormat(format))
                continue;

            {
                const tcu::IVec3 blockSize = tcu::getBlockPixelSize(format);
                const string formatName    = "astc_" + de::toString(blockSize.x()) + "x" + de::toString(blockSize.y()) +
                                          (tcu::isAstcSRGBFormat(format) ? "_srgb" : "");
                TestCaseGroup *const formatGroup = new TestCaseGroup(m_context, formatName.c_str(), "");
                addChild(formatGroup);

                DE_ASSERT(blockSize.z() == 1);

                // \note This array is NOT static.
                const struct
                {
                    const char *name;
                    int width;
                    int height;
                } formatSizes[] = {
                    {"divisible", blockSize.x() * 10, blockSize.y() * 10},
                    {"not_divisible", blockSize.x() * 10 + 1, blockSize.y() * 10 + 1},
                };

                FOR_EACH(size, formatSizes,
                         FOR_EACH(wrapS, wrapModes,
                                  FOR_EACH(wrapT, wrapModes, FOR_EACH(filter, filteringModes, {
                                               string name = string("") + wrapModes[wrapS].name + "_" +
                                                             wrapModes[wrapT].name + "_" + filteringModes[filter].name +
                                                             "_" + formatSizes[size].name;

                                               bool enableRelaxedPrecisionRef =
                                                   wrapModes[wrapS].mode == GL_REPEAT ||
                                                   wrapModes[wrapT].mode == GL_REPEAT ||
                                                   wrapModes[wrapS].mode == GL_MIRRORED_REPEAT ||
                                                   wrapModes[wrapT].mode == GL_MIRRORED_REPEAT;

                                               formatGroup->addChild(new TextureWrapCase(
                                                   m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(),
                                                   name.c_str(), "", format, wrapModes[wrapS].mode,
                                                   wrapModes[wrapT].mode, filteringModes[filter].mode,
                                                   filteringModes[filter].mode, formatSizes[size].width,
                                                   formatSizes[size].height, enableRelaxedPrecisionRef));
                                           }))))
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
