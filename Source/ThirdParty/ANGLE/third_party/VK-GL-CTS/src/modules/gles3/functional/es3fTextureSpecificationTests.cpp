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
 * \brief Texture specification tests.
 *
 * \todo [pyry] Following tests are missing:
 *  - Specify mipmap incomplete texture, use without mipmaps, re-specify
 *    as complete and render.
 *  - Randomly re-specify levels to eventually reach mipmap-complete texture.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureSpecificationTests.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "gluStrUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "sglrContextUtil.hpp"
#include "sglrContextWrapper.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "glsTextureTestUtil.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

// \todo [2012-04-29 pyry] Should be named SglrUtil
#include "es3fFboTestUtil.hpp"

#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::pair;
using std::string;
using std::vector;
using tcu::IVec4;
using tcu::TestLog;
using tcu::UVec4;
using tcu::Vec4;
using namespace FboTestUtil;

tcu::TextureFormat mapGLUnsizedInternalFormat(uint32_t internalFormat)
{
    using tcu::TextureFormat;
    switch (internalFormat)
    {
    case GL_ALPHA:
        return TextureFormat(TextureFormat::A, TextureFormat::UNORM_INT8);
    case GL_LUMINANCE:
        return TextureFormat(TextureFormat::L, TextureFormat::UNORM_INT8);
    case GL_LUMINANCE_ALPHA:
        return TextureFormat(TextureFormat::LA, TextureFormat::UNORM_INT8);
    case GL_RGB:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8);
    case GL_RGBA:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);
    default:
        throw tcu::InternalError(string("Can't map GL unsized internal format (") +
                                 tcu::toHex(internalFormat).toString() + ") to texture format");
    }
}

enum
{
    VIEWPORT_WIDTH  = 256,
    VIEWPORT_HEIGHT = 256
};

static inline int maxLevelCount(int width, int height)
{
    return (int)deLog2Floor32(de::max(width, height)) + 1;
}

static inline int maxLevelCount(int width, int height, int depth)
{
    return (int)deLog2Floor32(de::max(width, de::max(height, depth))) + 1;
}

template <int Size>
static tcu::Vector<float, Size> randomVector(de::Random &rnd,
                                             const tcu::Vector<float, Size> &minVal = tcu::Vector<float, Size>(0.0f),
                                             const tcu::Vector<float, Size> &maxVal = tcu::Vector<float, Size>(1.0f))
{
    tcu::Vector<float, Size> res;
    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = rnd.getFloat(minVal[ndx], maxVal[ndx]);
    return res;
}

static const uint32_t s_cubeMapFaces[] = {
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
};

static tcu::IVec4 getPixelFormatCompareDepth(const tcu::PixelFormat &pixelFormat, tcu::TextureFormat textureFormat)
{
    switch (textureFormat.order)
    {
    case tcu::TextureFormat::L:
    case tcu::TextureFormat::LA:
        return tcu::IVec4(pixelFormat.redBits, pixelFormat.redBits, pixelFormat.redBits, pixelFormat.alphaBits);
    default:
        return tcu::IVec4(pixelFormat.redBits, pixelFormat.greenBits, pixelFormat.blueBits, pixelFormat.alphaBits);
    }
}

static IVec4 getEffectiveTextureFormatBitDepth(tcu::TextureFormat textureFormat)
{
    if (textureFormat.order == tcu::TextureFormat::DS)
    {
        // When sampling depth-stencil texture, we actually sample just
        // the depth component.
        return tcu::getTextureFormatBitDepth(
            tcu::getEffectiveDepthStencilTextureFormat(textureFormat, tcu::Sampler::MODE_DEPTH));
    }
    else
        return tcu::getTextureFormatBitDepth(textureFormat);
}

static tcu::UVec4 computeCompareThreshold(const tcu::PixelFormat &pixelFormat, tcu::TextureFormat textureFormat)
{
    const IVec4 texFormatBits   = getEffectiveTextureFormatBitDepth(textureFormat);
    const IVec4 pixelFormatBits = getPixelFormatCompareDepth(pixelFormat, textureFormat);
    const IVec4 accurateFmtBits = min(pixelFormatBits, texFormatBits);
    const IVec4 compareBits     = select(accurateFmtBits, IVec4(8), greaterThan(accurateFmtBits, IVec4(0))) - 1;

    return (IVec4(1) << (8 - compareBits)).asUint();
}

class TextureSpecCase : public TestCase, public sglr::ContextWrapper
{
public:
    TextureSpecCase(Context &context, const char *name, const char *desc);
    ~TextureSpecCase(void);

    IterateResult iterate(void);

protected:
    virtual void createTexture(void)                                                              = 0;
    virtual void verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext) = 0;

    // Utilities.
    void renderTex(tcu::Surface &dst, uint32_t program, int width, int height);
    void readPixels(tcu::Surface &dst, int x, int y, int width, int height);

private:
    TextureSpecCase(const TextureSpecCase &other);
    TextureSpecCase &operator=(const TextureSpecCase &other);
};

TextureSpecCase::TextureSpecCase(Context &context, const char *name, const char *desc) : TestCase(context, name, desc)
{
}

TextureSpecCase::~TextureSpecCase(void)
{
}

TextureSpecCase::IterateResult TextureSpecCase::iterate(void)
{
    glu::RenderContext &renderCtx         = TestCase::m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();
    tcu::TestLog &log                     = m_testCtx.getLog();

    if (renderTarget.getWidth() < VIEWPORT_WIDTH || renderTarget.getHeight() < VIEWPORT_HEIGHT)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Context size, and viewport for GLES3
    de::Random rnd(deStringHash(getName()));
    int width  = deMin32(renderTarget.getWidth(), VIEWPORT_WIDTH);
    int height = deMin32(renderTarget.getHeight(), VIEWPORT_HEIGHT);
    int x      = rnd.getInt(0, renderTarget.getWidth() - width);
    int y      = rnd.getInt(0, renderTarget.getHeight() - height);

    // Contexts.
    sglr::GLContext gles3Context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(x, y, width, height));
    sglr::ReferenceContextBuffers refBuffers(tcu::PixelFormat(8, 8, 8, renderTarget.getPixelFormat().alphaBits ? 8 : 0),
                                             0 /* depth */, 0 /* stencil */, width, height);
    sglr::ReferenceContext refContext(sglr::ReferenceContextLimits(renderCtx), refBuffers.getColorbuffer(),
                                      refBuffers.getDepthbuffer(), refBuffers.getStencilbuffer());

    // Clear color buffer.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        setContext(ndx ? (sglr::Context *)&refContext : (sglr::Context *)&gles3Context);
        glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    // Construct texture using both GLES3 and reference contexts.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        setContext(ndx ? (sglr::Context *)&refContext : (sglr::Context *)&gles3Context);
        createTexture();
        TCU_CHECK(glGetError() == GL_NO_ERROR);
    }

    // Initialize case result to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    // Disable logging.
    gles3Context.enableLogging(0);

    // Verify results.
    verifyTexture(gles3Context, refContext);

    return STOP;
}

void TextureSpecCase::renderTex(tcu::Surface &dst, uint32_t program, int width, int height)
{
    int targetW = getWidth();
    int targetH = getHeight();

    float w = (float)width / (float)targetW;
    float h = (float)height / (float)targetH;

    sglr::drawQuad(*getCurrentContext(), program, tcu::Vec3(-1.0f, -1.0f, 0.0f),
                   tcu::Vec3(-1.0f + w * 2.0f, -1.0f + h * 2.0f, 0.0f));

    // Read pixels back.
    readPixels(dst, 0, 0, width, height);
}

void TextureSpecCase::readPixels(tcu::Surface &dst, int x, int y, int width, int height)
{
    dst.setSize(width, height);
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dst.getAccess().getDataPtr());
}

class Texture2DSpecCase : public TextureSpecCase
{
public:
    Texture2DSpecCase(Context &context, const char *name, const char *desc, const tcu::TextureFormat &format, int width,
                      int height, int numLevels);
    ~Texture2DSpecCase(void);

protected:
    virtual void verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext);

    tcu::TextureFormat m_texFormat;
    tcu::TextureFormatInfo m_texFormatInfo;
    int m_width;
    int m_height;
    int m_numLevels;
};

Texture2DSpecCase::Texture2DSpecCase(Context &context, const char *name, const char *desc,
                                     const tcu::TextureFormat &format, int width, int height, int numLevels)
    : TextureSpecCase(context, name, desc)
    , m_texFormat(format)
    , m_texFormatInfo(tcu::getTextureFormatInfo(format))
    , m_width(width)
    , m_height(height)
    , m_numLevels(numLevels)
{
}

Texture2DSpecCase::~Texture2DSpecCase(void)
{
}

void Texture2DSpecCase::verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext)
{
    Texture2DShader shader(DataTypes() << glu::getSampler2DType(m_texFormat), glu::TYPE_FLOAT_VEC4);
    uint32_t shaderIDgles = gles3Context.createProgram(&shader);
    uint32_t shaderIDRef  = refContext.createProgram(&shader);

    shader.setTexScaleBias(0, m_texFormatInfo.lookupScale, m_texFormatInfo.lookupBias);

    // Set state.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        sglr::Context *ctx =
            ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);

        setContext(ctx);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_numLevels - 1);
    }

    for (int levelNdx = 0; levelNdx < m_numLevels; levelNdx++)
    {
        int levelW = de::max(1, m_width >> levelNdx);
        int levelH = de::max(1, m_height >> levelNdx);
        tcu::Surface reference;
        tcu::Surface result;

        for (int ndx = 0; ndx < 2; ndx++)
        {
            tcu::Surface &dst = ndx ? reference : result;
            sglr::Context *ctx =
                ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);
            uint32_t shaderID = ndx ? shaderIDRef : shaderIDgles;

            setContext(ctx);
            shader.setUniforms(*ctx, shaderID);
            renderTex(dst, shaderID, levelW, levelH);
        }

        UVec4 threshold = computeCompareThreshold(m_context.getRenderTarget().getPixelFormat(), m_texFormat);
        string levelStr = de::toString(levelNdx);
        string name     = string("Level") + levelStr;
        string desc     = string("Level ") + levelStr;
        bool isOk = tcu::intThresholdCompare(m_testCtx.getLog(), name.c_str(), desc.c_str(), reference.getAccess(),
                                             result.getAccess(), threshold,
                                             levelNdx == 0 ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

        if (!isOk)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
            break;
        }
    }
}

class TextureCubeSpecCase : public TextureSpecCase
{
public:
    TextureCubeSpecCase(Context &context, const char *name, const char *desc, const tcu::TextureFormat &format,
                        int size, int numLevels);
    ~TextureCubeSpecCase(void);

protected:
    virtual void verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext);

    tcu::TextureFormat m_texFormat;
    tcu::TextureFormatInfo m_texFormatInfo;
    int m_size;
    int m_numLevels;
};

TextureCubeSpecCase::TextureCubeSpecCase(Context &context, const char *name, const char *desc,
                                         const tcu::TextureFormat &format, int size, int numLevels)
    : TextureSpecCase(context, name, desc)
    , m_texFormat(format)
    , m_texFormatInfo(tcu::getTextureFormatInfo(format))
    , m_size(size)
    , m_numLevels(numLevels)
{
}

TextureCubeSpecCase::~TextureCubeSpecCase(void)
{
}

void TextureCubeSpecCase::verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext)
{
    TextureCubeShader shader(glu::getSamplerCubeType(m_texFormat), glu::TYPE_FLOAT_VEC4);
    uint32_t shaderIDgles = gles3Context.createProgram(&shader);
    uint32_t shaderIDRef  = refContext.createProgram(&shader);

    shader.setTexScaleBias(m_texFormatInfo.lookupScale, m_texFormatInfo.lookupBias);

    // Set state.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        sglr::Context *ctx =
            ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);

        setContext(ctx);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, m_numLevels - 1);
    }

    for (int levelNdx = 0; levelNdx < m_numLevels; levelNdx++)
    {
        int levelSize = de::max(1, m_size >> levelNdx);
        bool isOk     = true;

        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            tcu::Surface reference;
            tcu::Surface result;

            if (levelSize <= 2)
                continue; // Fuzzy compare doesn't work for images this small.

            shader.setFace((tcu::CubeFace)face);

            for (int ndx = 0; ndx < 2; ndx++)
            {
                tcu::Surface &dst = ndx ? reference : result;
                sglr::Context *ctx =
                    ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);
                uint32_t shaderID = ndx ? shaderIDRef : shaderIDgles;

                setContext(ctx);
                shader.setUniforms(*ctx, shaderID);
                renderTex(dst, shaderID, levelSize, levelSize);
            }

            const float threshold = 0.02f;
            string faceStr        = de::toString((tcu::CubeFace)face);
            string levelStr       = de::toString(levelNdx);
            string name           = string("Level") + levelStr;
            string desc           = string("Level ") + levelStr + ", face " + faceStr;
            bool isFaceOk =
                tcu::fuzzyCompare(m_testCtx.getLog(), name.c_str(), desc.c_str(), reference, result, threshold,
                                  levelNdx == 0 ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

            if (!isFaceOk)
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
                isOk = false;
                break;
            }
        }

        if (!isOk)
            break;
    }
}

class Texture2DArraySpecCase : public TextureSpecCase
{
public:
    Texture2DArraySpecCase(Context &context, const char *name, const char *desc, const tcu::TextureFormat &format,
                           int width, int height, int numLayers, int numLevels);
    ~Texture2DArraySpecCase(void);

protected:
    virtual void verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext);

    tcu::TextureFormat m_texFormat;
    tcu::TextureFormatInfo m_texFormatInfo;
    int m_width;
    int m_height;
    int m_numLayers;
    int m_numLevels;
};

Texture2DArraySpecCase::Texture2DArraySpecCase(Context &context, const char *name, const char *desc,
                                               const tcu::TextureFormat &format, int width, int height, int numLayers,
                                               int numLevels)
    : TextureSpecCase(context, name, desc)
    , m_texFormat(format)
    , m_texFormatInfo(tcu::getTextureFormatInfo(format))
    , m_width(width)
    , m_height(height)
    , m_numLayers(numLayers)
    , m_numLevels(numLevels)
{
}

Texture2DArraySpecCase::~Texture2DArraySpecCase(void)
{
}

void Texture2DArraySpecCase::verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext)
{
    Texture2DArrayShader shader(glu::getSampler2DArrayType(m_texFormat), glu::TYPE_FLOAT_VEC4);
    uint32_t shaderIDgles = gles3Context.createProgram(&shader);
    uint32_t shaderIDRef  = refContext.createProgram(&shader);

    shader.setTexScaleBias(m_texFormatInfo.lookupScale, m_texFormatInfo.lookupBias);

    // Set state.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        sglr::Context *ctx =
            ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);

        setContext(ctx);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, m_numLevels - 1);
    }

    for (int layerNdx = 0; layerNdx < m_numLayers; layerNdx++)
    {
        bool layerOk = true;

        shader.setLayer(layerNdx);

        for (int levelNdx = 0; levelNdx < m_numLevels; levelNdx++)
        {
            int levelW = de::max(1, m_width >> levelNdx);
            int levelH = de::max(1, m_height >> levelNdx);
            tcu::Surface reference;
            tcu::Surface result;

            for (int ndx = 0; ndx < 2; ndx++)
            {
                tcu::Surface &dst = ndx ? reference : result;
                sglr::Context *ctx =
                    ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);
                uint32_t shaderID = ndx ? shaderIDRef : shaderIDgles;

                setContext(ctx);
                shader.setUniforms(*ctx, shaderID);
                renderTex(dst, shaderID, levelW, levelH);
            }

            UVec4 threshold = computeCompareThreshold(m_context.getRenderTarget().getPixelFormat(), m_texFormat);
            string levelStr = de::toString(levelNdx);
            string layerStr = de::toString(layerNdx);
            string name     = string("Layer") + layerStr + "Level" + levelStr;
            string desc     = string("Layer ") + layerStr + ", Level " + levelStr;
            bool depthOk    = tcu::intThresholdCompare(
                m_testCtx.getLog(), name.c_str(), desc.c_str(), reference.getAccess(), result.getAccess(), threshold,
                (levelNdx == 0 && layerNdx == 0) ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

            if (!depthOk)
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
                layerOk = false;
                break;
            }
        }

        if (!layerOk)
            break;
    }
}

class Texture3DSpecCase : public TextureSpecCase
{
public:
    Texture3DSpecCase(Context &context, const char *name, const char *desc, const tcu::TextureFormat &format, int width,
                      int height, int depth, int numLevels);
    ~Texture3DSpecCase(void);

protected:
    virtual void verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext);

    tcu::TextureFormat m_texFormat;
    tcu::TextureFormatInfo m_texFormatInfo;
    int m_width;
    int m_height;
    int m_depth;
    int m_numLevels;
};

Texture3DSpecCase::Texture3DSpecCase(Context &context, const char *name, const char *desc,
                                     const tcu::TextureFormat &format, int width, int height, int depth, int numLevels)
    : TextureSpecCase(context, name, desc)
    , m_texFormat(format)
    , m_texFormatInfo(tcu::getTextureFormatInfo(format))
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_numLevels(numLevels)
{
}

Texture3DSpecCase::~Texture3DSpecCase(void)
{
}

void Texture3DSpecCase::verifyTexture(sglr::GLContext &gles3Context, sglr::ReferenceContext &refContext)
{
    Texture3DShader shader(glu::getSampler3DType(m_texFormat), glu::TYPE_FLOAT_VEC4);
    uint32_t shaderIDgles = gles3Context.createProgram(&shader);
    uint32_t shaderIDRef  = refContext.createProgram(&shader);

    shader.setTexScaleBias(m_texFormatInfo.lookupScale, m_texFormatInfo.lookupBias);

    // Set state.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        sglr::Context *ctx =
            ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);

        setContext(ctx);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, m_numLevels - 1);
    }

    for (int levelNdx = 0; levelNdx < m_numLevels; levelNdx++)
    {
        int levelW   = de::max(1, m_width >> levelNdx);
        int levelH   = de::max(1, m_height >> levelNdx);
        int levelD   = de::max(1, m_depth >> levelNdx);
        bool levelOk = true;

        for (int depth = 0; depth < levelD; depth++)
        {
            tcu::Surface reference;
            tcu::Surface result;

            shader.setDepth(((float)depth + 0.5f) / (float)levelD);

            for (int ndx = 0; ndx < 2; ndx++)
            {
                tcu::Surface &dst = ndx ? reference : result;
                sglr::Context *ctx =
                    ndx ? static_cast<sglr::Context *>(&refContext) : static_cast<sglr::Context *>(&gles3Context);
                uint32_t shaderID = ndx ? shaderIDRef : shaderIDgles;

                setContext(ctx);
                shader.setUniforms(*ctx, shaderID);
                renderTex(dst, shaderID, levelW, levelH);
            }

            UVec4 threshold = computeCompareThreshold(m_context.getRenderTarget().getPixelFormat(), m_texFormat);
            string levelStr = de::toString(levelNdx);
            string sliceStr = de::toString(depth);
            string name     = string("Level") + levelStr + "Slice" + sliceStr;
            string desc     = string("Level ") + levelStr + ", Slice " + sliceStr;
            bool depthOk    = tcu::intThresholdCompare(
                m_testCtx.getLog(), name.c_str(), desc.c_str(), reference.getAccess(), result.getAccess(), threshold,
                (levelNdx == 0 && depth == 0) ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

            if (!depthOk)
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
                levelOk = false;
                break;
            }
        }

        if (!levelOk)
            break;
    }
}

// Basic TexImage2D() with 2D texture usage
class BasicTexImage2DCase : public Texture2DSpecCase
{
public:
    // Unsized internal format.
    BasicTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                        int width, int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    // Sized internal format.
    BasicTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                        int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(m_format, m_dataType));
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, levelW, levelH, 0, m_format, m_dataType,
                         levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic TexImage2D() with cubemap usage
class BasicTexImageCubeCase : public TextureCubeSpecCase
{
public:
    // Unsized formats.
    BasicTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                          int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    // Sized internal formats.
    BasicTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(m_format, m_dataType));
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            levelData.setSize(levelSize, levelSize);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 gMin = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
                Vec4 gMax = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

                tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

                glTexImage2D(s_cubeMapFaces[face], ndx, m_internalFormat, levelSize, levelSize, 0, m_format, m_dataType,
                             levelData.getAccess().getDataPtr());
            }
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic TexImage3D() with 2D array texture usage
class BasicTexImage2DArrayCase : public Texture2DArraySpecCase
{
public:
    BasicTexImage2DArrayCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                             int height, int numLayers)
        : Texture2DArraySpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height,
                                 numLayers, maxLevelCount(width, height))
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        de::Random rnd(deStringHash(getName()));
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(transferFmt.format, transferFmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelW, levelH, m_numLayers);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage3D(GL_TEXTURE_2D_ARRAY, ndx, m_internalFormat, levelW, levelH, m_numLayers, 0, transferFmt.format,
                         transferFmt.dataType, levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
};

// Basic TexImage3D() with 3D texture usage
class BasicTexImage3DCase : public Texture3DSpecCase
{
public:
    BasicTexImage3DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                        int height, int depth)
        : Texture3DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth,
                            maxLevelCount(width, height, depth))
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        de::Random rnd(deStringHash(getName()));
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(transferFmt.format, transferFmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            int levelD = de::max(1, m_depth >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelW, levelH, levelD);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage3D(GL_TEXTURE_3D, ndx, m_internalFormat, levelW, levelH, levelD, 0, transferFmt.format,
                         transferFmt.dataType, levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
};

// Randomized 2D texture specification using TexImage2D
class RandomOrderTexImage2DCase : public Texture2DSpecCase
{
public:
    RandomOrderTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                              int width, int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    RandomOrderTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                              int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(m_format, m_dataType));
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        vector<int> levels(m_numLevels);

        for (int i = 0; i < m_numLevels; i++)
            levels[i] = i;
        rnd.shuffle(levels.begin(), levels.end());

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelNdx = levels[ndx];
            int levelW   = de::max(1, m_width >> levelNdx);
            int levelH   = de::max(1, m_height >> levelNdx);
            Vec4 gMin    = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax    = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, levelNdx, m_internalFormat, levelW, levelH, 0, m_format, m_dataType,
                         levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// Randomized cubemap texture specification using TexImage2D
class RandomOrderTexImageCubeCase : public TextureCubeSpecCase
{
public:
    RandomOrderTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format,
                                uint32_t dataType, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(GL_NONE)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    RandomOrderTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(m_format, m_dataType));
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Level-face pairs.
        vector<pair<int, tcu::CubeFace>> images(m_numLevels * 6);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                images[ndx * 6 + face] = std::make_pair(ndx, (tcu::CubeFace)face);

        rnd.shuffle(images.begin(), images.end());

        for (int ndx = 0; ndx < (int)images.size(); ndx++)
        {
            int levelNdx       = images[ndx].first;
            tcu::CubeFace face = images[ndx].second;
            int levelSize      = de::max(1, m_size >> levelNdx);
            Vec4 gMin          = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax          = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelSize, levelSize);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(s_cubeMapFaces[face], levelNdx, m_internalFormat, levelSize, levelSize, 0, m_format,
                         m_dataType, levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// TexImage2D() unpack alignment case.
class TexImage2DAlignCase : public Texture2DSpecCase
{
public:
    TexImage2DAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                        int width, int height, int numLevels, int alignment)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), width, height, numLevels)
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
        , m_alignment(alignment)
    {
    }

    TexImage2DAlignCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                        int height, int numLevels, int alignment)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, numLevels)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
        , m_alignment(alignment)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        vector<uint8_t> data;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW  = de::max(1, m_width >> ndx);
            int levelH  = de::max(1, m_height >> ndx);
            Vec4 colorA = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * (m_texFormatInfo.valueMax - m_texFormatInfo.valueMin) +
                          m_texFormatInfo.valueMin;
            Vec4 colorB = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * (m_texFormatInfo.valueMax - m_texFormatInfo.valueMin) +
                          m_texFormatInfo.valueMin;
            int rowPitch = deAlign32(levelW * m_texFormat.getPixelSize(), m_alignment);
            int cellSize = de::max(1, de::min(levelW >> 2, levelH >> 2));

            data.resize(rowPitch * levelH);
            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, levelW, levelH, 1, rowPitch, 0, &data[0]), cellSize,
                              colorA, colorB);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, levelW, levelH, 0, m_format, m_dataType, &data[0]);
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
    int m_alignment;
};

// TexImage2D() unpack alignment case.
class TexImageCubeAlignCase : public TextureCubeSpecCase
{
public:
    TexImageCubeAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                          int size, int numLevels, int alignment)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), size, numLevels)
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
        , m_alignment(alignment)
    {
    }

    TexImageCubeAlignCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size,
                          int numLevels, int alignment)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size, numLevels)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
        , m_alignment(alignment)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        vector<uint8_t> data;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);
            int rowPitch  = deAlign32(m_texFormat.getPixelSize() * levelSize, m_alignment);
            Vec4 colorA   = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * (m_texFormatInfo.valueMax - m_texFormatInfo.valueMin) +
                          m_texFormatInfo.valueMin;
            Vec4 colorB = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * (m_texFormatInfo.valueMax - m_texFormatInfo.valueMin) +
                          m_texFormatInfo.valueMin;
            int cellSize = de::max(1, levelSize >> 2);

            data.resize(rowPitch * levelSize);
            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, levelSize, levelSize, 1, rowPitch, 0, &data[0]),
                              cellSize, colorA, colorB);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
                glTexImage2D(s_cubeMapFaces[face], ndx, m_internalFormat, levelSize, levelSize, 0, m_format, m_dataType,
                             &data[0]);
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
    int m_alignment;
};

// TexImage2D() unpack parameters case.
class TexImage2DParamsCase : public Texture2DSpecCase
{
public:
    TexImage2DParamsCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                         int height, int rowLength, int skipRows, int skipPixels, int alignment)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, 1)
        , m_internalFormat(internalFormat)
        , m_rowLength(rowLength)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        int rowLength                   = m_rowLength > 0 ? m_rowLength : m_width;
        int rowPitch                    = deAlign32(rowLength * pixelSize, m_alignment);
        uint32_t tex                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        // Fill data with grid.
        data.resize(pixelSize * m_skipPixels + rowPitch * (m_height + m_skipRows));
        {
            Vec4 cScale = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias  = m_texFormatInfo.valueMin;
            Vec4 colorA = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_width, m_height, 1, rowPitch, 0,
                                                     &data[0] + m_skipRows * rowPitch + m_skipPixels * pixelSize),
                              4, colorA, colorB);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, transferFmt.format, transferFmt.dataType,
                     &data[0]);
    }

    uint32_t m_internalFormat;
    int m_rowLength;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
};

// TexImage3D() unpack parameters case.
class TexImage3DParamsCase : public Texture3DSpecCase
{
public:
    TexImage3DParamsCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                         int height, int depth, int imageHeight, int rowLength, int skipImages, int skipRows,
                         int skipPixels, int alignment)
        : Texture3DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth, 1)
        , m_internalFormat(internalFormat)
        , m_imageHeight(imageHeight)
        , m_rowLength(rowLength)
        , m_skipImages(skipImages)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        int rowLength                   = m_rowLength > 0 ? m_rowLength : m_width;
        int rowPitch                    = deAlign32(rowLength * pixelSize, m_alignment);
        int imageHeight                 = m_imageHeight > 0 ? m_imageHeight : m_height;
        int slicePitch                  = imageHeight * rowPitch;
        uint32_t tex                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        // Fill data with grid.
        data.resize(pixelSize * m_skipPixels + rowPitch * m_skipRows + slicePitch * (m_skipImages + m_depth));
        {
            Vec4 cScale = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias  = m_texFormatInfo.valueMin;
            Vec4 colorA = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_width, m_height, m_depth, rowPitch, slicePitch,
                                                     &data[0] + m_skipImages * slicePitch + m_skipRows * rowPitch +
                                                         m_skipPixels * pixelSize),
                              4, colorA, colorB);
        }

        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_imageHeight);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, m_skipImages);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexImage3D(GL_TEXTURE_3D, 0, m_internalFormat, m_width, m_height, m_depth, 0, transferFmt.format,
                     transferFmt.dataType, &data[0]);
    }

    uint32_t m_internalFormat;
    int m_imageHeight;
    int m_rowLength;
    int m_skipImages;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
};

// Basic TexSubImage2D() with 2D texture usage
class BasicTexSubImage2DCase : public Texture2DSpecCase
{
public:
    BasicTexSubImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                           int width, int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    BasicTexSubImage2DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                           int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel data(m_texFormat);
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // First specify full texture.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            data.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, levelW, levelH, 0, m_format, m_dataType,
                         data.getAccess().getDataPtr());
        }

        // Re-specify parts of each level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            int w = rnd.getInt(1, levelW);
            int h = rnd.getInt(1, levelH);
            int x = rnd.getInt(0, levelW - w);
            int y = rnd.getInt(0, levelH - h);

            Vec4 colorA  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 colorB  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            int cellSize = rnd.getInt(2, 16);

            data.setSize(w, h);
            tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);

            glTexSubImage2D(GL_TEXTURE_2D, ndx, x, y, w, h, m_format, m_dataType, data.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic TexSubImage2D() with cubemap usage
class BasicTexSubImageCubeCase : public TextureCubeSpecCase
{
public:
    BasicTexSubImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                             int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    BasicTexSubImageCubeCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel data(m_texFormat);
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            data.setSize(levelSize, levelSize);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 gMin = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
                Vec4 gMax = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

                tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

                glTexImage2D(s_cubeMapFaces[face], ndx, m_internalFormat, levelSize, levelSize, 0, m_format, m_dataType,
                             data.getAccess().getDataPtr());
            }
        }

        // Re-specify parts of each face and level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                int w = rnd.getInt(1, levelSize);
                int h = rnd.getInt(1, levelSize);
                int x = rnd.getInt(0, levelSize - w);
                int y = rnd.getInt(0, levelSize - h);

                Vec4 colorA  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
                Vec4 colorB  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
                int cellSize = rnd.getInt(2, 16);

                data.setSize(w, h);
                tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);

                glTexSubImage2D(s_cubeMapFaces[face], ndx, x, y, w, h, m_format, m_dataType,
                                data.getAccess().getDataPtr());
            }
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// TexSubImage2D() unpack parameters case.
class TexSubImage2DParamsCase : public Texture2DSpecCase
{
public:
    TexSubImage2DParamsCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                            int height, int subX, int subY, int subW, int subH, int rowLength, int skipRows,
                            int skipPixels, int alignment)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, 1)
        , m_internalFormat(internalFormat)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_rowLength(rowLength)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        uint32_t tex                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        // First fill texture with gradient.
        data.resize(deAlign32(m_width * pixelSize, 4) * m_height);
        tcu::fillWithComponentGradients(
            tcu::PixelBufferAccess(m_texFormat, m_width, m_height, 1, deAlign32(m_width * pixelSize, 4), 0, &data[0]),
            m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, transferFmt.format, transferFmt.dataType,
                     &data[0]);

        // Fill data with grid.
        {
            int rowLength = m_rowLength > 0 ? m_rowLength : m_subW;
            int rowPitch  = deAlign32(rowLength * pixelSize, m_alignment);
            int height    = m_subH + m_skipRows;
            Vec4 cScale   = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias    = m_texFormatInfo.valueMin;
            Vec4 colorA   = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB   = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            data.resize(rowPitch * height);
            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, 1, rowPitch, 0,
                                                     &data[0] + m_skipRows * rowPitch + m_skipPixels * pixelSize),
                              4, colorA, colorB);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glTexSubImage2D(GL_TEXTURE_2D, 0, m_subX, m_subY, m_subW, m_subH, transferFmt.format, transferFmt.dataType,
                        &data[0]);
    }

    uint32_t m_internalFormat;
    int m_subX;
    int m_subY;
    int m_subW;
    int m_subH;
    int m_rowLength;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
};

// Basic TexSubImage3D() with 3D texture usage
class BasicTexSubImage3DCase : public Texture3DSpecCase
{
public:
    BasicTexSubImage3DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                           int height, int depth)
        : Texture3DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth,
                            maxLevelCount(width, height, depth))
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel data(m_texFormat);
        de::Random rnd(deStringHash(getName()));
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // First specify full texture.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            int levelD = de::max(1, m_depth >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            data.setSize(levelW, levelH, levelD);
            tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

            glTexImage3D(GL_TEXTURE_3D, ndx, m_internalFormat, levelW, levelH, levelD, 0, transferFmt.format,
                         transferFmt.dataType, data.getAccess().getDataPtr());
        }

        // Re-specify parts of each level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            int levelD = de::max(1, m_depth >> ndx);

            int w = rnd.getInt(1, levelW);
            int h = rnd.getInt(1, levelH);
            int d = rnd.getInt(1, levelD);
            int x = rnd.getInt(0, levelW - w);
            int y = rnd.getInt(0, levelH - h);
            int z = rnd.getInt(0, levelD - d);

            Vec4 colorA  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 colorB  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            int cellSize = rnd.getInt(2, 16);

            data.setSize(w, h, d);
            tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);

            glTexSubImage3D(GL_TEXTURE_3D, ndx, x, y, z, w, h, d, transferFmt.format, transferFmt.dataType,
                            data.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
};

// TexSubImage2D() to texture initialized with empty data
class TexSubImage2DEmptyTexCase : public Texture2DSpecCase
{
public:
    TexSubImage2DEmptyTexCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                              int width, int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    TexSubImage2DEmptyTexCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                              int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height,
                            maxLevelCount(width, height))
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel data(m_texFormat);
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // First allocate storage for each level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, levelW, levelH, 0, m_format, m_dataType, nullptr);
        }

        // Specify pixel data to all levels using glTexSubImage2D()
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            data.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

            glTexSubImage2D(GL_TEXTURE_2D, ndx, 0, 0, levelW, levelH, m_format, m_dataType,
                            data.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// TexSubImage2D() to empty cubemap texture
class TexSubImageCubeEmptyTexCase : public TextureCubeSpecCase
{
public:
    TexSubImageCubeEmptyTexCase(Context &context, const char *name, const char *desc, uint32_t format,
                                uint32_t dataType, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

    TexSubImageCubeEmptyTexCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        tcu::TextureLevel data(m_texFormat);
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Specify storage for each level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
                glTexImage2D(s_cubeMapFaces[face], ndx, m_internalFormat, levelSize, levelSize, 0, m_format, m_dataType,
                             nullptr);
        }

        // Specify data using glTexSubImage2D()
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            data.setSize(levelSize, levelSize);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 gMin = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
                Vec4 gMax = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

                tcu::fillWithComponentGradients(data.getAccess(), gMin, gMax);

                glTexSubImage2D(s_cubeMapFaces[face], ndx, 0, 0, levelSize, levelSize, m_format, m_dataType,
                                data.getAccess().getDataPtr());
            }
        }
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
};

// TexSubImage2D() unpack alignment with 2D texture
class TexSubImage2DAlignCase : public Texture2DSpecCase
{
public:
    TexSubImage2DAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                           int width, int height, int subX, int subY, int subW, int subH, int alignment)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), width, height, 1)
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_alignment(alignment)
    {
    }

    TexSubImage2DAlignCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                           int height, int subX, int subY, int subW, int subH, int alignment)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, 1)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_alignment(alignment)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        vector<uint8_t> data;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        // Specify base level.
        data.resize(m_texFormat.getPixelSize() * m_width * m_height);
        tcu::fillWithComponentGradients(tcu::PixelBufferAccess(m_texFormat, m_width, m_height, 1, &data[0]), Vec4(0.0f),
                                        Vec4(1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, m_format, m_dataType, &data[0]);

        // Re-specify subrectangle.
        int rowPitch = deAlign32(m_texFormat.getPixelSize() * m_subW, m_alignment);
        data.resize(rowPitch * m_subH);
        tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, 1, rowPitch, 0, &data[0]), 4,
                          Vec4(1.0f, 0.0f, 0.0f, 1.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glTexSubImage2D(GL_TEXTURE_2D, 0, m_subX, m_subY, m_subW, m_subH, m_format, m_dataType, &data[0]);
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
    int m_subX;
    int m_subY;
    int m_subW;
    int m_subH;
    int m_alignment;
};

// TexSubImage2D() unpack alignment with cubemap texture
class TexSubImageCubeAlignCase : public TextureCubeSpecCase
{
public:
    TexSubImageCubeAlignCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                             int size, int subX, int subY, int subW, int subH, int alignment)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), size, 1)
        , m_internalFormat(format)
        , m_format(format)
        , m_dataType(dataType)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_alignment(alignment)
    {
    }

    TexSubImageCubeAlignCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size,
                             int subX, int subY, int subW, int subH, int alignment)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size, 1)
        , m_internalFormat(internalFormat)
        , m_format(GL_NONE)
        , m_dataType(GL_NONE)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_alignment(alignment)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        m_format                = fmt.format;
        m_dataType              = fmt.dataType;
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        vector<uint8_t> data;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

        // Specify base level.
        data.resize(m_texFormat.getPixelSize() * m_size * m_size);
        tcu::fillWithComponentGradients(tcu::PixelBufferAccess(m_texFormat, m_size, m_size, 1, &data[0]), Vec4(0.0f),
                                        Vec4(1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            glTexImage2D(s_cubeMapFaces[face], 0, m_internalFormat, m_size, m_size, 0, m_format, m_dataType, &data[0]);

        // Re-specify subrectangle.
        int rowPitch = deAlign32(m_texFormat.getPixelSize() * m_subW, m_alignment);
        data.resize(rowPitch * m_subH);
        tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, 1, rowPitch, 0, &data[0]), 4,
                          Vec4(1.0f, 0.0f, 0.0f, 1.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f));

        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            glTexSubImage2D(s_cubeMapFaces[face], 0, m_subX, m_subY, m_subW, m_subH, m_format, m_dataType, &data[0]);
    }

    uint32_t m_internalFormat;
    uint32_t m_format;
    uint32_t m_dataType;
    int m_subX;
    int m_subY;
    int m_subW;
    int m_subH;
    int m_alignment;
};

// TexSubImage3D() unpack parameters case.
class TexSubImage3DParamsCase : public Texture3DSpecCase
{
public:
    TexSubImage3DParamsCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                            int height, int depth, int subX, int subY, int subZ, int subW, int subH, int subD,
                            int imageHeight, int rowLength, int skipImages, int skipRows, int skipPixels, int alignment)
        : Texture3DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth, 1)
        , m_internalFormat(internalFormat)
        , m_subX(subX)
        , m_subY(subY)
        , m_subZ(subZ)
        , m_subW(subW)
        , m_subH(subH)
        , m_subD(subD)
        , m_imageHeight(imageHeight)
        , m_rowLength(rowLength)
        , m_skipImages(skipImages)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        uint32_t tex                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);

        // Fill with gradient.
        {
            int rowPitch   = deAlign32(pixelSize * m_width, 4);
            int slicePitch = rowPitch * m_height;

            data.resize(slicePitch * m_depth);
            tcu::fillWithComponentGradients(
                tcu::PixelBufferAccess(m_texFormat, m_width, m_height, m_depth, rowPitch, slicePitch, &data[0]),
                m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
        }

        glTexImage3D(GL_TEXTURE_3D, 0, m_internalFormat, m_width, m_height, m_depth, 0, transferFmt.format,
                     transferFmt.dataType, &data[0]);

        // Fill data with grid.
        {
            int rowLength   = m_rowLength > 0 ? m_rowLength : m_subW;
            int rowPitch    = deAlign32(rowLength * pixelSize, m_alignment);
            int imageHeight = m_imageHeight > 0 ? m_imageHeight : m_subH;
            int slicePitch  = imageHeight * rowPitch;
            Vec4 cScale     = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias      = m_texFormatInfo.valueMin;
            Vec4 colorA     = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB     = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            data.resize(slicePitch * (m_depth + m_skipImages));
            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, m_subD, rowPitch, slicePitch,
                                                     &data[0] + m_skipImages * slicePitch + m_skipRows * rowPitch +
                                                         m_skipPixels * pixelSize),
                              4, colorA, colorB);
        }

        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_imageHeight);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, m_skipImages);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glTexSubImage3D(GL_TEXTURE_3D, 0, m_subX, m_subY, m_subZ, m_subW, m_subH, m_subD, transferFmt.format,
                        transferFmt.dataType, &data[0]);
    }

    uint32_t m_internalFormat;
    int m_subX;
    int m_subY;
    int m_subZ;
    int m_subW;
    int m_subH;
    int m_subD;
    int m_imageHeight;
    int m_rowLength;
    int m_skipImages;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
};

// Basic CopyTexImage2D() with 2D texture usage
class BasicCopyTexImage2DCase : public Texture2DSpecCase
{
public:
    BasicCopyTexImage2DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                            int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(internalFormat, GL_UNSIGNED_BYTE), width,
                            height, maxLevelCount(width, height))
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = mapGLUnsizedInternalFormat(m_internalFormat);
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        uint32_t tex = 0;
        de::Random rnd(deStringHash(getName()));
        GradientShader shader(glu::TYPE_FLOAT_VEC4);
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        // Fill render target with gradient.
        shader.setGradient(*getCurrentContext(), shaderID, Vec4(0.0f), Vec4(1.0f));
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            int x      = rnd.getInt(0, getWidth() - levelW);
            int y      = rnd.getInt(0, getHeight() - levelH);

            glCopyTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, x, y, levelW, levelH, 0);
        }
    }

    uint32_t m_internalFormat;
};

// Basic CopyTexImage2D() with cubemap usage
class BasicCopyTexImageCubeCase : public TextureCubeSpecCase
{
public:
    BasicCopyTexImageCubeCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(internalFormat, GL_UNSIGNED_BYTE), size,
                              deLog2Floor32(size) + 1)
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = mapGLUnsizedInternalFormat(m_internalFormat);
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        uint32_t tex = 0;
        de::Random rnd(deStringHash(getName()));
        GradientShader shader(glu::TYPE_FLOAT_VEC4);
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        // Fill render target with gradient.
        shader.setGradient(*getCurrentContext(), shaderID, Vec4(0.0f), Vec4(1.0f));
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                int x = rnd.getInt(0, getWidth() - levelSize);
                int y = rnd.getInt(0, getHeight() - levelSize);

                glCopyTexImage2D(s_cubeMapFaces[face], ndx, m_internalFormat, x, y, levelSize, levelSize, 0);
            }
        }
    }

    uint32_t m_internalFormat;
};

// Basic CopyTexSubImage2D() with 2D texture usage
class BasicCopyTexSubImage2DCase : public Texture2DSpecCase
{
public:
    BasicCopyTexSubImage2DCase(Context &context, const char *name, const char *desc, uint32_t format, uint32_t dataType,
                               int width, int height)
        : Texture2DSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), width, height,
                            maxLevelCount(width, height))
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = glu::mapGLTransferFormat(m_format, m_dataType);
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        uint32_t tex = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));
        GradientShader shader(glu::TYPE_FLOAT_VEC4);
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // First specify full texture.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            Vec4 colorA  = randomVector<4>(rnd);
            Vec4 colorB  = randomVector<4>(rnd);
            int cellSize = rnd.getInt(2, 16);

            data.setSize(levelW, levelH);
            tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_format, levelW, levelH, 0, m_format, m_dataType,
                         data.getAccess().getDataPtr());
        }

        // Fill render target with gradient.
        shader.setGradient(*getCurrentContext(), shaderID, Vec4(0.0f), Vec4(1.0f));
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        // Re-specify parts of each level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);

            int w  = rnd.getInt(1, levelW);
            int h  = rnd.getInt(1, levelH);
            int xo = rnd.getInt(0, levelW - w);
            int yo = rnd.getInt(0, levelH - h);

            int x = rnd.getInt(0, getWidth() - w);
            int y = rnd.getInt(0, getHeight() - h);

            glCopyTexSubImage2D(GL_TEXTURE_2D, ndx, xo, yo, x, y, w, h);
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic CopyTexSubImage2D() with cubemap usage
class BasicCopyTexSubImageCubeCase : public TextureCubeSpecCase
{
public:
    BasicCopyTexSubImageCubeCase(Context &context, const char *name, const char *desc, uint32_t format,
                                 uint32_t dataType, int size)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLTransferFormat(format, dataType), size,
                              deLog2Floor32(size) + 1)
        , m_format(format)
        , m_dataType(dataType)
    {
    }

protected:
    void createTexture(void)
    {
        const tcu::RenderTarget &renderTarget = TestCase::m_context.getRenderContext().getRenderTarget();
        bool targetHasRGB = renderTarget.getPixelFormat().redBits > 0 && renderTarget.getPixelFormat().greenBits > 0 &&
                            renderTarget.getPixelFormat().blueBits > 0;
        bool targetHasAlpha    = renderTarget.getPixelFormat().alphaBits > 0;
        tcu::TextureFormat fmt = glu::mapGLTransferFormat(m_format, m_dataType);
        bool texHasRGB         = fmt.order != tcu::TextureFormat::A;
        bool texHasAlpha       = fmt.order == tcu::TextureFormat::RGBA || fmt.order == tcu::TextureFormat::LA ||
                           fmt.order == tcu::TextureFormat::A;
        uint32_t tex = 0;
        tcu::TextureLevel data(fmt);
        de::Random rnd(deStringHash(getName()));
        GradientShader shader(glu::TYPE_FLOAT_VEC4);
        uint32_t shaderID = getCurrentContext()->createProgram(&shader);

        if ((texHasRGB && !targetHasRGB) || (texHasAlpha && !targetHasAlpha))
            throw tcu::NotSupportedError("Copying from current framebuffer is not supported", "", __FILE__, __LINE__);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            data.setSize(levelSize, levelSize);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 colorA  = randomVector<4>(rnd);
                Vec4 colorB  = randomVector<4>(rnd);
                int cellSize = rnd.getInt(2, 16);

                tcu::fillWithGrid(data.getAccess(), cellSize, colorA, colorB);
                glTexImage2D(s_cubeMapFaces[face], ndx, m_format, levelSize, levelSize, 0, m_format, m_dataType,
                             data.getAccess().getDataPtr());
            }
        }

        // Fill render target with gradient.
        shader.setGradient(*getCurrentContext(), shaderID, Vec4(0.0f), Vec4(1.0f));
        sglr::drawQuad(*getCurrentContext(), shaderID, tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 0.0f));

        // Re-specify parts of each face and level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                int w  = rnd.getInt(1, levelSize);
                int h  = rnd.getInt(1, levelSize);
                int xo = rnd.getInt(0, levelSize - w);
                int yo = rnd.getInt(0, levelSize - h);

                int x = rnd.getInt(0, getWidth() - w);
                int y = rnd.getInt(0, getHeight() - h);

                glCopyTexSubImage2D(s_cubeMapFaces[face], ndx, xo, yo, x, y, w, h);
            }
        }
    }

    uint32_t m_format;
    uint32_t m_dataType;
};

// Basic glTexStorage2D() with 2D texture usage
class BasicTexStorage2DCase : public Texture2DSpecCase
{
public:
    BasicTexStorage2DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                          int height, int numLevels)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, numLevels)
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt          = glu::mapGLInternalFormat(m_internalFormat);
        glu::TransferFormat transferFmt = glu::getTransferFormat(fmt);
        uint32_t tex                    = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(transferFmt.format, transferFmt.dataType));
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexStorage2D(GL_TEXTURE_2D, m_numLevels, m_internalFormat, m_width, m_height);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexSubImage2D(GL_TEXTURE_2D, ndx, 0, 0, levelW, levelH, transferFmt.format, transferFmt.dataType,
                            levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
};

// Basic glTexStorage2D() with cubemap usage
class BasicTexStorageCubeCase : public TextureCubeSpecCase
{
public:
    BasicTexStorageCubeCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size,
                            int numLevels)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size, numLevels)
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        tcu::TextureFormat fmt          = glu::mapGLInternalFormat(m_internalFormat);
        glu::TransferFormat transferFmt = glu::getTransferFormat(fmt);
        uint32_t tex                    = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(transferFmt.format, transferFmt.dataType));
        de::Random rnd(deStringHash(getName()));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, m_numLevels, m_internalFormat, m_size, m_size);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelSize = de::max(1, m_size >> ndx);

            levelData.setSize(levelSize, levelSize);

            for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
            {
                Vec4 gMin = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
                Vec4 gMax = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

                tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

                glTexSubImage2D(s_cubeMapFaces[face], ndx, 0, 0, levelSize, levelSize, transferFmt.format,
                                transferFmt.dataType, levelData.getAccess().getDataPtr());
            }
        }
    }

    uint32_t m_internalFormat;
};

// Basic glTexStorage3D() with 2D array texture usage
class BasicTexStorage2DArrayCase : public Texture2DArraySpecCase
{
public:
    BasicTexStorage2DArrayCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                               int height, int numLayers, int numLevels)
        : Texture2DArraySpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height,
                                 numLayers, numLevels)
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        de::Random rnd(deStringHash(getName()));
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(transferFmt.format, transferFmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, m_numLevels, m_internalFormat, m_width, m_height, m_numLayers);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelW, levelH, m_numLayers);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, ndx, 0, 0, 0, levelW, levelH, m_numLayers, transferFmt.format,
                            transferFmt.dataType, levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
};

// Basic TexStorage3D() with 3D texture usage
class BasicTexStorage3DCase : public Texture3DSpecCase
{
public:
    BasicTexStorage3DCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                          int height, int depth, int numLevels)
        : Texture3DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth,
                            numLevels)
        , m_internalFormat(internalFormat)
    {
    }

protected:
    void createTexture(void)
    {
        uint32_t tex = 0;
        de::Random rnd(deStringHash(getName()));
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(transferFmt.format, transferFmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexStorage3D(GL_TEXTURE_3D, m_numLevels, m_internalFormat, m_width, m_height, m_depth);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            int levelW = de::max(1, m_width >> ndx);
            int levelH = de::max(1, m_height >> ndx);
            int levelD = de::max(1, m_depth >> ndx);
            Vec4 gMin  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            Vec4 gMax  = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            levelData.setSize(levelW, levelH, levelD);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexSubImage3D(GL_TEXTURE_3D, ndx, 0, 0, 0, levelW, levelH, levelD, transferFmt.format,
                            transferFmt.dataType, levelData.getAccess().getDataPtr());
        }
    }

    uint32_t m_internalFormat;
};

// Pixel buffer object cases.

// TexImage2D() from pixel buffer object.
class TexImage2DBufferCase : public Texture2DSpecCase
{
public:
    TexImage2DBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                         int height, int rowLength, int skipRows, int skipPixels, int alignment, int offset)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, 1)
        , m_internalFormat(internalFormat)
        , m_rowLength(rowLength)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        int rowLength                   = m_rowLength > 0 ? m_rowLength : m_width + m_skipPixels;
        int rowPitch                    = deAlign32(rowLength * pixelSize, m_alignment);
        int height                      = m_height + m_skipRows;
        uint32_t buf                    = 0;
        uint32_t tex                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        // Fill data with grid.
        data.resize(rowPitch * height + m_offset);
        {
            Vec4 cScale = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias  = m_texFormatInfo.valueMin;
            Vec4 colorA = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            tcu::fillWithGrid(
                tcu::PixelBufferAccess(m_texFormat, m_width, m_height, 1, rowPitch, 0,
                                       &data[0] + m_skipRows * rowPitch + m_skipPixels * pixelSize + m_offset),
                4, colorA, colorB);
        }

        // Create buffer and upload.
        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, transferFmt.format, transferFmt.dataType,
                     (const void *)(uintptr_t)m_offset);
    }

    uint32_t m_internalFormat;
    int m_rowLength;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexImage2D() cubemap from pixel buffer object case
class TexImageCubeBufferCase : public TextureCubeSpecCase
{
public:
    TexImageCubeBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size,
                           int rowLength, int skipRows, int skipPixels, int alignment, int offset)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size, 1)
        , m_internalFormat(internalFormat)
        , m_rowLength(rowLength)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        de::Random rnd(deStringHash(getName()));
        uint32_t tex            = 0;
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        const int pixelSize     = m_texFormat.getPixelSize();
        const int rowLength     = m_rowLength > 0 ? m_rowLength : m_size + m_skipPixels;
        const int rowPitch      = deAlign32(rowLength * pixelSize, m_alignment);
        const int height        = m_size + m_skipRows;
        vector<vector<uint8_t>> data(DE_LENGTH_OF_ARRAY(s_cubeMapFaces));

        DE_ASSERT(m_numLevels == 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
        {
            uint32_t buf = 0;

            {
                const Vec4 gMin = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
                const Vec4 gMax = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

                data[face].resize(rowPitch * height + m_offset);
                tcu::fillWithComponentGradients(tcu::PixelBufferAccess(m_texFormat, m_size, m_size, 1, rowPitch, 0,
                                                                       &data[face][0] + m_skipRows * rowPitch +
                                                                           m_skipPixels * pixelSize + m_offset),
                                                gMin, gMax);
            }

            // Create buffer and upload.
            glGenBuffers(1, &buf);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data[face].size(), &data[face][0], GL_STATIC_DRAW);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
            glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

            glTexImage2D(s_cubeMapFaces[face], 0, m_internalFormat, m_size, m_size, 0, fmt.format, fmt.dataType,
                         (const void *)(uintptr_t)m_offset);
        }
    }

    uint32_t m_internalFormat;
    int m_rowLength;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexImage3D() 2D array from pixel buffer object.
class TexImage2DArrayBufferCase : public Texture2DArraySpecCase
{
public:
    TexImage2DArrayBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                              int height, int depth, int imageHeight, int rowLength, int skipImages, int skipRows,
                              int skipPixels, int alignment, int offset)
        : Texture2DArraySpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth, 1)
        , m_internalFormat(internalFormat)
        , m_imageHeight(imageHeight)
        , m_rowLength(rowLength)
        , m_skipImages(skipImages)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        int rowLength                   = m_rowLength > 0 ? m_rowLength : m_width;
        int rowPitch                    = deAlign32(rowLength * pixelSize, m_alignment);
        int imageHeight                 = m_imageHeight > 0 ? m_imageHeight : m_height;
        int slicePitch                  = imageHeight * rowPitch;
        uint32_t tex                    = 0;
        uint32_t buf                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        // Fill data with grid.
        data.resize(slicePitch * (m_numLayers + m_skipImages) + m_offset);
        {
            Vec4 cScale = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias  = m_texFormatInfo.valueMin;
            Vec4 colorA = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_width, m_height, m_numLayers, rowPitch, slicePitch,
                                                     &data[0] + m_skipImages * slicePitch + m_skipRows * rowPitch +
                                                         m_skipPixels * pixelSize + m_offset),
                              4, colorA, colorB);
        }

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_imageHeight);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, m_skipImages);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, m_internalFormat, m_width, m_height, m_numLayers, 0, transferFmt.format,
                     transferFmt.dataType, (const void *)(uintptr_t)m_offset);
    }

    uint32_t m_internalFormat;
    int m_imageHeight;
    int m_rowLength;
    int m_skipImages;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexImage3D() from pixel buffer object.
class TexImage3DBufferCase : public Texture3DSpecCase
{
public:
    TexImage3DBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                         int height, int depth, int imageHeight, int rowLength, int skipImages, int skipRows,
                         int skipPixels, int alignment, int offset)
        : Texture3DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth, 1)
        , m_internalFormat(internalFormat)
        , m_imageHeight(imageHeight)
        , m_rowLength(rowLength)
        , m_skipImages(skipImages)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        int rowLength                   = m_rowLength > 0 ? m_rowLength : m_width;
        int rowPitch                    = deAlign32(rowLength * pixelSize, m_alignment);
        int imageHeight                 = m_imageHeight > 0 ? m_imageHeight : m_height;
        int slicePitch                  = imageHeight * rowPitch;
        uint32_t tex                    = 0;
        uint32_t buf                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        // Fill data with grid.
        data.resize(slicePitch * (m_depth + m_skipImages) + m_offset);
        {
            Vec4 cScale = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias  = m_texFormatInfo.valueMin;
            Vec4 colorA = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_width, m_height, m_depth, rowPitch, slicePitch,
                                                     &data[0] + m_skipImages * slicePitch + m_skipRows * rowPitch +
                                                         m_skipPixels * pixelSize + m_offset),
                              4, colorA, colorB);
        }

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_imageHeight);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, m_skipImages);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexImage3D(GL_TEXTURE_3D, 0, m_internalFormat, m_width, m_height, m_depth, 0, transferFmt.format,
                     transferFmt.dataType, (const void *)(uintptr_t)m_offset);
    }

    uint32_t m_internalFormat;
    int m_imageHeight;
    int m_rowLength;
    int m_skipImages;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexSubImage2D() PBO case.
class TexSubImage2DBufferCase : public Texture2DSpecCase
{
public:
    TexSubImage2DBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                            int height, int subX, int subY, int subW, int subH, int rowLength, int skipRows,
                            int skipPixels, int alignment, int offset)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, 1)
        , m_internalFormat(internalFormat)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_rowLength(rowLength)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        uint32_t tex                    = 0;
        uint32_t buf                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        // First fill texture with gradient.
        data.resize(deAlign32(m_width * pixelSize, 4) * m_height);
        tcu::fillWithComponentGradients(
            tcu::PixelBufferAccess(m_texFormat, m_width, m_height, 1, deAlign32(m_width * pixelSize, 4), 0, &data[0]),
            m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, transferFmt.format, transferFmt.dataType,
                     &data[0]);

        // Fill data with grid.
        {
            int rowLength = m_rowLength > 0 ? m_rowLength : m_subW;
            int rowPitch  = deAlign32(rowLength * pixelSize, m_alignment);
            int height    = m_subH + m_skipRows;
            Vec4 cScale   = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias    = m_texFormatInfo.valueMin;
            Vec4 colorA   = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB   = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            data.resize(rowPitch * height + m_offset);
            tcu::fillWithGrid(
                tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, 1, rowPitch, 0,
                                       &data[0] + m_skipRows * rowPitch + m_skipPixels * pixelSize + m_offset),
                4, colorA, colorB);
        }

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glTexSubImage2D(GL_TEXTURE_2D, 0, m_subX, m_subY, m_subW, m_subH, transferFmt.format, transferFmt.dataType,
                        (const void *)(uintptr_t)m_offset);
    }

    uint32_t m_internalFormat;
    int m_subX;
    int m_subY;
    int m_subW;
    int m_subH;
    int m_rowLength;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexSubImage2D() cubemap PBO case.
class TexSubImageCubeBufferCase : public TextureCubeSpecCase
{
public:
    TexSubImageCubeBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int size,
                              int subX, int subY, int subW, int subH, int rowLength, int skipRows, int skipPixels,
                              int alignment, int offset)
        : TextureCubeSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), size, 1)
        , m_internalFormat(internalFormat)
        , m_subX(subX)
        , m_subY(subY)
        , m_subW(subW)
        , m_subH(subH)
        , m_rowLength(rowLength)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        de::Random rnd(deStringHash(getName()));
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        uint32_t tex                    = 0;
        uint32_t buf                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

        // Fill faces with different gradients.

        data.resize(deAlign32(m_size * pixelSize, 4) * m_size);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int face = 0; face < DE_LENGTH_OF_ARRAY(s_cubeMapFaces); face++)
        {
            const Vec4 gMin = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
            const Vec4 gMax = randomVector<4>(rnd, m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);

            tcu::fillWithComponentGradients(
                tcu::PixelBufferAccess(m_texFormat, m_size, m_size, 1, deAlign32(m_size * pixelSize, 4), 0, &data[0]),
                gMin, gMax);

            glTexImage2D(s_cubeMapFaces[face], 0, m_internalFormat, m_size, m_size, 0, transferFmt.format,
                         transferFmt.dataType, &data[0]);
        }

        // Fill data with grid.
        {
            int rowLength = m_rowLength > 0 ? m_rowLength : m_subW;
            int rowPitch  = deAlign32(rowLength * pixelSize, m_alignment);
            int height    = m_subH + m_skipRows;
            Vec4 cScale   = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias    = m_texFormatInfo.valueMin;
            Vec4 colorA   = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB   = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            data.resize(rowPitch * height + m_offset);
            tcu::fillWithGrid(
                tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, 1, rowPitch, 0,
                                       &data[0] + m_skipRows * rowPitch + m_skipPixels * pixelSize + m_offset),
                4, colorA, colorB);
        }

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);

        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            glTexSubImage2D(s_cubeMapFaces[face], 0, m_subX, m_subY, m_subW, m_subH, transferFmt.format,
                            transferFmt.dataType, (const void *)(uintptr_t)m_offset);
    }

    uint32_t m_internalFormat;
    int m_subX;
    int m_subY;
    int m_subW;
    int m_subH;
    int m_rowLength;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexSubImage3D() 2D array PBO case.
class TexSubImage2DArrayBufferCase : public Texture2DArraySpecCase
{
public:
    TexSubImage2DArrayBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                                 int width, int height, int depth, int subX, int subY, int subZ, int subW, int subH,
                                 int subD, int imageHeight, int rowLength, int skipImages, int skipRows, int skipPixels,
                                 int alignment, int offset)
        : Texture2DArraySpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth, 1)
        , m_internalFormat(internalFormat)
        , m_subX(subX)
        , m_subY(subY)
        , m_subZ(subZ)
        , m_subW(subW)
        , m_subH(subH)
        , m_subD(subD)
        , m_imageHeight(imageHeight)
        , m_rowLength(rowLength)
        , m_skipImages(skipImages)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        uint32_t tex                    = 0;
        uint32_t buf                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);

        // Fill with gradient.
        {
            int rowPitch   = deAlign32(pixelSize * m_width, 4);
            int slicePitch = rowPitch * m_height;

            data.resize(slicePitch * m_numLayers);
            tcu::fillWithComponentGradients(
                tcu::PixelBufferAccess(m_texFormat, m_width, m_height, m_numLayers, rowPitch, slicePitch, &data[0]),
                m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
        }

        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, m_internalFormat, m_width, m_height, m_numLayers, 0, transferFmt.format,
                     transferFmt.dataType, &data[0]);

        // Fill data with grid.
        {
            int rowLength   = m_rowLength > 0 ? m_rowLength : m_subW;
            int rowPitch    = deAlign32(rowLength * pixelSize, m_alignment);
            int imageHeight = m_imageHeight > 0 ? m_imageHeight : m_subH;
            int slicePitch  = imageHeight * rowPitch;
            Vec4 cScale     = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias      = m_texFormatInfo.valueMin;
            Vec4 colorA     = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB     = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            data.resize(slicePitch * (m_numLayers + m_skipImages) + m_offset);
            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, m_subD, rowPitch, slicePitch,
                                                     &data[0] + m_skipImages * slicePitch + m_skipRows * rowPitch +
                                                         m_skipPixels * pixelSize + m_offset),
                              4, colorA, colorB);
        }

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_imageHeight);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, m_skipImages);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, m_subX, m_subY, m_subZ, m_subW, m_subH, m_subD, transferFmt.format,
                        transferFmt.dataType, (const void *)(intptr_t)m_offset);
    }

    uint32_t m_internalFormat;
    int m_subX;
    int m_subY;
    int m_subZ;
    int m_subW;
    int m_subH;
    int m_subD;
    int m_imageHeight;
    int m_rowLength;
    int m_skipImages;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexSubImage2D() test case for PBO bounds.
class CopyTexFromPBOCase : public Texture2DSpecCase
{
public:
    CopyTexFromPBOCase(Context &context, const char *name, const char *desc)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(GL_RGBA8), 4, 4, 1)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);

        const glw::GLuint red   = 0xff0000ff; // alpha, blue, green, red
        const glw::GLuint green = 0xff00ff00;
        const glw::GLuint blue  = 0xffff0000;
        const glw::GLuint black = 0xff000000;

        glw::GLuint texId = 0;
        glw::GLuint fboId = 0;
        glw::GLuint pboId = 0;

        const uint32_t texWidth      = 4;
        const uint32_t texHeight     = 4;
        const uint32_t texSubWidth   = 2;
        const uint32_t texSubHeight  = 4;
        const uint32_t texSubOffsetX = 2;
        const uint32_t texSubOffsetY = 0;

        const uint32_t pboRowLength      = 4;
        const glw::GLuint pboOffset      = 2;
        const glw::GLintptr pboOffsetPtr = pboOffset * sizeof(glw::GLuint);
        const uint32_t halfWidth         = pboRowLength / 2;

        bool imageOk = true;

        glw::GLuint tex_data[texHeight][texWidth];
        glw::GLuint pixel_data[texHeight][texWidth];
        glw::GLuint color_data[texHeight][texWidth];

        // Fill pixel data.
        for (uint32_t row = 0; row < texHeight; row++)
        {
            for (uint32_t column = 0; column < texWidth; column++)
            {
                tex_data[row][column] = red;

                if (column < halfWidth)
                    pixel_data[row][column] = blue;
                else
                    pixel_data[row][column] = green;

                color_data[row][column] = black;
            }
        }

        // Create main texture.
        glGenTextures(1, &texId);
        GLU_EXPECT_NO_ERROR(glGetError(), "glGenTextures() failed");
        glBindTexture(GL_TEXTURE_2D, texId);
        GLU_EXPECT_NO_ERROR(glGetError(), "glBindTexture() failed");
        glTexImage2D(GL_TEXTURE_2D, 0, transferFmt.format, texWidth, texHeight, 0, transferFmt.format,
                     transferFmt.dataType, (void *)tex_data[0]);
        GLU_EXPECT_NO_ERROR(glGetError(), "glTexImage2D() failed");

        // Create pixel buffer object.
        glGenBuffers(1, &pboId);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboId);
        GLU_EXPECT_NO_ERROR(glGetError(), "glBindBuffer() failed");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pboRowLength);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLU_EXPECT_NO_ERROR(glGetError(), "glPixelStorei() failed");
        glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(pixel_data), (void *)pixel_data, GL_STREAM_DRAW);

        // The very last pixel of the PBO should be available for TexSubImage.
        glTexSubImage2D(GL_TEXTURE_2D, 0, texSubOffsetX, texSubOffsetY, texSubWidth, texSubHeight, transferFmt.format,
                        transferFmt.dataType, reinterpret_cast<void *>(pboOffsetPtr));
        GLU_EXPECT_NO_ERROR(glGetError(), "glTexSubImage2D() failed");

        // Create a framebuffer.
        glGenFramebuffers(1, &fboId);
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
        GLU_EXPECT_NO_ERROR(glGetError(), "glFramebufferTexture2D() failed");

        // Read pixels for pixel comparison.
        glReadPixels(0, 0, texWidth, texHeight, transferFmt.format, transferFmt.dataType, &color_data);
        GLU_EXPECT_NO_ERROR(glGetError(), "glReadPixels() failed");

        // Run pixel to pixel comparison tests to confirm all the stored data is there and correctly aligned.
        for (uint32_t row = 0; row < texSubHeight; row++)
        {
            for (uint32_t column = 0; column < pboOffset; column++)
            {
                if (color_data[row][column] != tex_data[row][column])
                    imageOk = false;
            }
        }

        if (!imageOk)
            TCU_FAIL("Color data versus texture data comparison failed.");

        for (uint32_t row = texSubOffsetY; row < texSubHeight; row++)
        {
            for (uint32_t column = 0; column < texSubWidth; column++)
            {
                if (color_data[row][column + texSubWidth] != pixel_data[row][column + pboOffset])
                    imageOk = false;
            }
        }

        if (!imageOk)
            TCU_FAIL("Color data versus pixel data comparison failed.");

        // Cleanup
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glDeleteBuffers(1, &pboId);
    }
};

// TexSubImage3D() PBO case.
class TexSubImage3DBufferCase : public Texture3DSpecCase
{
public:
    TexSubImage3DBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int width,
                            int height, int depth, int subX, int subY, int subZ, int subW, int subH, int subD,
                            int imageHeight, int rowLength, int skipImages, int skipRows, int skipPixels, int alignment,
                            int offset)
        : Texture3DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), width, height, depth, 1)
        , m_internalFormat(internalFormat)
        , m_subX(subX)
        , m_subY(subY)
        , m_subZ(subZ)
        , m_subW(subW)
        , m_subH(subH)
        , m_subD(subD)
        , m_imageHeight(imageHeight)
        , m_rowLength(rowLength)
        , m_skipImages(skipImages)
        , m_skipRows(skipRows)
        , m_skipPixels(skipPixels)
        , m_alignment(alignment)
        , m_offset(offset)
    {
    }

protected:
    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        uint32_t tex                    = 0;
        uint32_t buf                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);

        // Fill with gradient.
        {
            int rowPitch   = deAlign32(pixelSize * m_width, 4);
            int slicePitch = rowPitch * m_height;

            data.resize(slicePitch * m_depth);
            tcu::fillWithComponentGradients(
                tcu::PixelBufferAccess(m_texFormat, m_width, m_height, m_depth, rowPitch, slicePitch, &data[0]),
                m_texFormatInfo.valueMin, m_texFormatInfo.valueMax);
        }

        glTexImage3D(GL_TEXTURE_3D, 0, m_internalFormat, m_width, m_height, m_depth, 0, transferFmt.format,
                     transferFmt.dataType, &data[0]);

        // Fill data with grid.
        {
            int rowLength   = m_rowLength > 0 ? m_rowLength : m_subW;
            int rowPitch    = deAlign32(rowLength * pixelSize, m_alignment);
            int imageHeight = m_imageHeight > 0 ? m_imageHeight : m_subH;
            int slicePitch  = imageHeight * rowPitch;
            Vec4 cScale     = m_texFormatInfo.valueMax - m_texFormatInfo.valueMin;
            Vec4 cBias      = m_texFormatInfo.valueMin;
            Vec4 colorA     = Vec4(1.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            Vec4 colorB     = Vec4(0.0f, 1.0f, 0.0f, 1.0f) * cScale + cBias;

            data.resize(slicePitch * (m_depth + m_skipImages) + m_offset);
            tcu::fillWithGrid(tcu::PixelBufferAccess(m_texFormat, m_subW, m_subH, m_subD, rowPitch, slicePitch,
                                                     &data[0] + m_skipImages * slicePitch + m_skipRows * rowPitch +
                                                         m_skipPixels * pixelSize + m_offset),
                              4, colorA, colorB);
        }

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_imageHeight);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, m_skipImages);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glTexSubImage3D(GL_TEXTURE_3D, 0, m_subX, m_subY, m_subZ, m_subW, m_subH, m_subD, transferFmt.format,
                        transferFmt.dataType, (const void *)(intptr_t)m_offset);
    }

    uint32_t m_internalFormat;
    int m_subX;
    int m_subY;
    int m_subZ;
    int m_subW;
    int m_subH;
    int m_subD;
    int m_imageHeight;
    int m_rowLength;
    int m_skipImages;
    int m_skipRows;
    int m_skipPixels;
    int m_alignment;
    int m_offset;
};

// TexImage2D() depth case.
class TexImage2DDepthCase : public Texture2DSpecCase
{
public:
    TexImage2DDepthCase(Context &context, const char *name, const char *desc, uint32_t internalFormat, int imageWidth,
                        int imageHeight)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), imageWidth, imageHeight,
                            maxLevelCount(imageWidth, imageHeight))
        , m_internalFormat(internalFormat)
    {
        // we are interested in the behavior near [-2, 2], map it to visible range [0, 1]
        m_texFormatInfo.lookupBias  = Vec4(0.25f, 0.0f, 0.0f, 1.0f);
        m_texFormatInfo.lookupScale = Vec4(0.5f, 1.0f, 1.0f, 0.0f);
    }

    void createTexture(void)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        uint32_t tex            = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(fmt.format, fmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLU_CHECK();

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            const int levelW = de::max(1, m_width >> ndx);
            const int levelH = de::max(1, m_height >> ndx);
            const Vec4 gMin  = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const Vec4 gMax  = Vec4(2.0f, 1.5f, -1.0f, 2.0f);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, levelW, levelH, 0, fmt.format, fmt.dataType,
                         levelData.getAccess().getDataPtr());
        }
    }

    const uint32_t m_internalFormat;
};

// TexImage3D() depth case.
class TexImage2DArrayDepthCase : public Texture2DArraySpecCase
{
public:
    TexImage2DArrayDepthCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                             int imageWidth, int imageHeight, int numLayers)
        : Texture2DArraySpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), imageWidth, imageHeight,
                                 numLayers, maxLevelCount(imageWidth, imageHeight))
        , m_internalFormat(internalFormat)
    {
        // we are interested in the behavior near [-2, 2], map it to visible range [0, 1]
        m_texFormatInfo.lookupBias  = Vec4(0.25f, 0.0f, 0.0f, 1.0f);
        m_texFormatInfo.lookupScale = Vec4(0.5f, 1.0f, 1.0f, 0.0f);
    }

    void createTexture(void)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        uint32_t tex            = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(fmt.format, fmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLU_CHECK();

        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            const int levelW = de::max(1, m_width >> ndx);
            const int levelH = de::max(1, m_height >> ndx);
            const Vec4 gMin  = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const Vec4 gMax  = Vec4(2.0f, 1.5f, -1.0f, 2.0f);

            levelData.setSize(levelW, levelH, m_numLayers);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage3D(GL_TEXTURE_2D_ARRAY, ndx, m_internalFormat, levelW, levelH, m_numLayers, 0, fmt.format,
                         fmt.dataType, levelData.getAccess().getDataPtr());
        }
    }

    const uint32_t m_internalFormat;
};

// TexSubImage2D() depth case.
class TexSubImage2DDepthCase : public Texture2DSpecCase
{
public:
    TexSubImage2DDepthCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                           int imageWidth, int imageHeight)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), imageWidth, imageHeight,
                            maxLevelCount(imageWidth, imageHeight))
        , m_internalFormat(internalFormat)
    {
        // we are interested in the behavior near [-2, 2], map it to visible range [0, 1]
        m_texFormatInfo.lookupBias  = Vec4(0.25f, 0.0f, 0.0f, 1.0f);
        m_texFormatInfo.lookupScale = Vec4(0.5f, 1.0f, 1.0f, 0.0f);
    }

    void createTexture(void)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        de::Random rnd(deStringHash(getName()));
        uint32_t tex = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(fmt.format, fmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLU_CHECK();

        // First specify full texture.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            const int levelW = de::max(1, m_width >> ndx);
            const int levelH = de::max(1, m_height >> ndx);
            const Vec4 gMin  = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const Vec4 gMax  = Vec4(2.0f, 1.5f, -1.0f, 2.0f);

            levelData.setSize(levelW, levelH);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage2D(GL_TEXTURE_2D, ndx, m_internalFormat, levelW, levelH, 0, fmt.format, fmt.dataType,
                         levelData.getAccess().getDataPtr());
        }

        // Re-specify parts of each level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            const int levelW = de::max(1, m_width >> ndx);
            const int levelH = de::max(1, m_height >> ndx);

            const int w = rnd.getInt(1, levelW);
            const int h = rnd.getInt(1, levelH);
            const int x = rnd.getInt(0, levelW - w);
            const int y = rnd.getInt(0, levelH - h);

            const Vec4 colorA  = Vec4(2.0f, 1.5f, -1.0f, 2.0f);
            const Vec4 colorB  = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const int cellSize = rnd.getInt(2, 16);

            levelData.setSize(w, h);
            tcu::fillWithGrid(levelData.getAccess(), cellSize, colorA, colorB);

            glTexSubImage2D(GL_TEXTURE_2D, ndx, x, y, w, h, fmt.format, fmt.dataType,
                            levelData.getAccess().getDataPtr());
        }
    }

    const uint32_t m_internalFormat;
};

// TexSubImage3D() depth case.
class TexSubImage2DArrayDepthCase : public Texture2DArraySpecCase
{
public:
    TexSubImage2DArrayDepthCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                                int imageWidth, int imageHeight, int numLayers)
        : Texture2DArraySpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), imageWidth, imageHeight,
                                 numLayers, maxLevelCount(imageWidth, imageHeight))
        , m_internalFormat(internalFormat)
    {
        // we are interested in the behavior near [-2, 2], map it to visible range [0, 1]
        m_texFormatInfo.lookupBias  = Vec4(0.25f, 0.0f, 0.0f, 1.0f);
        m_texFormatInfo.lookupScale = Vec4(0.5f, 1.0f, 1.0f, 0.0f);
    }

    void createTexture(void)
    {
        glu::TransferFormat fmt = glu::getTransferFormat(m_texFormat);
        de::Random rnd(deStringHash(getName()));
        uint32_t tex = 0;
        tcu::TextureLevel levelData(glu::mapGLTransferFormat(fmt.format, fmt.dataType));

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLU_CHECK();

        // First specify full texture.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            const int levelW = de::max(1, m_width >> ndx);
            const int levelH = de::max(1, m_height >> ndx);
            const Vec4 gMin  = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const Vec4 gMax  = Vec4(2.0f, 1.5f, -1.0f, 2.0f);

            levelData.setSize(levelW, levelH, m_numLayers);
            tcu::fillWithComponentGradients(levelData.getAccess(), gMin, gMax);

            glTexImage3D(GL_TEXTURE_2D_ARRAY, ndx, m_internalFormat, levelW, levelH, m_numLayers, 0, fmt.format,
                         fmt.dataType, levelData.getAccess().getDataPtr());
        }

        // Re-specify parts of each level.
        for (int ndx = 0; ndx < m_numLevels; ndx++)
        {
            const int levelW = de::max(1, m_width >> ndx);
            const int levelH = de::max(1, m_height >> ndx);

            const int w = rnd.getInt(1, levelW);
            const int h = rnd.getInt(1, levelH);
            const int d = rnd.getInt(1, m_numLayers);
            const int x = rnd.getInt(0, levelW - w);
            const int y = rnd.getInt(0, levelH - h);
            const int z = rnd.getInt(0, m_numLayers - d);

            const Vec4 colorA  = Vec4(2.0f, 1.5f, -1.0f, 2.0f);
            const Vec4 colorB  = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const int cellSize = rnd.getInt(2, 16);

            levelData.setSize(w, h, d);
            tcu::fillWithGrid(levelData.getAccess(), cellSize, colorA, colorB);

            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, ndx, x, y, z, w, h, d, fmt.format, fmt.dataType,
                            levelData.getAccess().getDataPtr());
        }
    }

    const uint32_t m_internalFormat;
};

// TexImage2D() depth case with pbo.
class TexImage2DDepthBufferCase : public Texture2DSpecCase
{
public:
    TexImage2DDepthBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                              int imageWidth, int imageHeight)
        : Texture2DSpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), imageWidth, imageHeight, 1)
        , m_internalFormat(internalFormat)
    {
        // we are interested in the behavior near [-2, 2], map it to visible range [0, 1]
        m_texFormatInfo.lookupBias  = Vec4(0.25f, 0.0f, 0.0f, 1.0f);
        m_texFormatInfo.lookupScale = Vec4(0.5f, 1.0f, 1.0f, 0.0f);
    }

    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        int rowLength                   = m_width;
        int alignment                   = 4;
        int rowPitch                    = deAlign32(rowLength * pixelSize, alignment);
        int height                      = m_height;
        uint32_t buf                    = 0;
        uint32_t tex                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        // Fill data with gradient
        data.resize(rowPitch * height);
        {
            const Vec4 gMin = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const Vec4 gMax = Vec4(2.0f, 1.5f, -1.0f, 2.0f);

            tcu::fillWithComponentGradients(
                tcu::PixelBufferAccess(m_texFormat, m_width, m_height, 1, rowPitch, 0, &data[0]), gMin, gMax);
        }

        // Create buffer and upload.
        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, transferFmt.format, transferFmt.dataType,
                     nullptr);
        glDeleteBuffers(1, &buf);
    }

    const uint32_t m_internalFormat;
};

// TexImage3D() depth case with pbo.
class TexImage2DArrayDepthBufferCase : public Texture2DArraySpecCase
{
public:
    TexImage2DArrayDepthBufferCase(Context &context, const char *name, const char *desc, uint32_t internalFormat,
                                   int imageWidth, int imageHeight, int numLayers)
        : Texture2DArraySpecCase(context, name, desc, glu::mapGLInternalFormat(internalFormat), imageWidth, imageHeight,
                                 numLayers, 1)
        , m_internalFormat(internalFormat)
    {
        // we are interested in the behavior near [-2, 2], map it to visible range [0, 1]
        m_texFormatInfo.lookupBias  = Vec4(0.25f, 0.0f, 0.0f, 1.0f);
        m_texFormatInfo.lookupScale = Vec4(0.5f, 1.0f, 1.0f, 0.0f);
    }

    void createTexture(void)
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(m_texFormat);
        int pixelSize                   = m_texFormat.getPixelSize();
        int rowLength                   = m_width;
        int alignment                   = 4;
        int rowPitch                    = deAlign32(rowLength * pixelSize, alignment);
        int imageHeight                 = m_height;
        int slicePitch                  = imageHeight * rowPitch;
        uint32_t tex                    = 0;
        uint32_t buf                    = 0;
        vector<uint8_t> data;

        DE_ASSERT(m_numLevels == 1);

        // Fill data with grid.
        data.resize(slicePitch * m_numLayers);
        {
            const Vec4 gMin = Vec4(-1.5f, -2.0f, 1.7f, -1.5f);
            const Vec4 gMax = Vec4(2.0f, 1.5f, -1.0f, 2.0f);

            tcu::fillWithComponentGradients(
                tcu::PixelBufferAccess(m_texFormat, m_width, m_height, m_numLayers, rowPitch, slicePitch, &data[0]),
                gMin, gMax);
        }

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (int)data.size(), &data[0], GL_STATIC_DRAW);

        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, imageHeight);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, m_internalFormat, m_width, m_height, m_numLayers, 0, transferFmt.format,
                     transferFmt.dataType, nullptr);
        glDeleteBuffers(1, &buf);
    }

    const uint32_t m_internalFormat;
};

TextureSpecificationTests::TextureSpecificationTests(Context &context)
    : TestCaseGroup(context, "specification", "Texture Specification Tests")
{
}

TextureSpecificationTests::~TextureSpecificationTests(void)
{
}

void TextureSpecificationTests::init(void)
{
    struct
    {
        const char *name;
        uint32_t format;
        uint32_t dataType;
    } unsizedFormats[] = {{"alpha_unsigned_byte", GL_ALPHA, GL_UNSIGNED_BYTE},
                          {"luminance_unsigned_byte", GL_LUMINANCE, GL_UNSIGNED_BYTE},
                          {"luminance_alpha_unsigned_byte", GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
                          {"rgb_unsigned_short_5_6_5", GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
                          {"rgb_unsigned_byte", GL_RGB, GL_UNSIGNED_BYTE},
                          {"rgba_unsigned_short_4_4_4_4", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
                          {"rgba_unsigned_short_5_5_5_1", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
                          {"rgba_unsigned_byte", GL_RGBA, GL_UNSIGNED_BYTE}};

    struct
    {
        const char *name;
        uint32_t internalFormat;
    } colorFormats[] = {{
                            "rgba32f",
                            GL_RGBA32F,
                        },
                        {
                            "rgba32i",
                            GL_RGBA32I,
                        },
                        {
                            "rgba32ui",
                            GL_RGBA32UI,
                        },
                        {
                            "rgba16f",
                            GL_RGBA16F,
                        },
                        {
                            "rgba16i",
                            GL_RGBA16I,
                        },
                        {
                            "rgba16ui",
                            GL_RGBA16UI,
                        },
                        {
                            "rgba8",
                            GL_RGBA8,
                        },
                        {
                            "rgba8i",
                            GL_RGBA8I,
                        },
                        {
                            "rgba8ui",
                            GL_RGBA8UI,
                        },
                        {
                            "srgb8_alpha8",
                            GL_SRGB8_ALPHA8,
                        },
                        {
                            "rgb10_a2",
                            GL_RGB10_A2,
                        },
                        {
                            "rgb10_a2ui",
                            GL_RGB10_A2UI,
                        },
                        {
                            "rgba4",
                            GL_RGBA4,
                        },
                        {
                            "rgb5_a1",
                            GL_RGB5_A1,
                        },
                        {
                            "rgba8_snorm",
                            GL_RGBA8_SNORM,
                        },
                        {
                            "rgb8",
                            GL_RGB8,
                        },
                        {
                            "rgb565",
                            GL_RGB565,
                        },
                        {
                            "r11f_g11f_b10f",
                            GL_R11F_G11F_B10F,
                        },
                        {
                            "rgb32f",
                            GL_RGB32F,
                        },
                        {
                            "rgb32i",
                            GL_RGB32I,
                        },
                        {
                            "rgb32ui",
                            GL_RGB32UI,
                        },
                        {
                            "rgb16f",
                            GL_RGB16F,
                        },
                        {
                            "rgb16i",
                            GL_RGB16I,
                        },
                        {
                            "rgb16ui",
                            GL_RGB16UI,
                        },
                        {
                            "rgb8_snorm",
                            GL_RGB8_SNORM,
                        },
                        {
                            "rgb8i",
                            GL_RGB8I,
                        },
                        {
                            "rgb8ui",
                            GL_RGB8UI,
                        },
                        {
                            "srgb8",
                            GL_SRGB8,
                        },
                        {
                            "rgb9_e5",
                            GL_RGB9_E5,
                        },
                        {
                            "rg32f",
                            GL_RG32F,
                        },
                        {
                            "rg32i",
                            GL_RG32I,
                        },
                        {
                            "rg32ui",
                            GL_RG32UI,
                        },
                        {
                            "rg16f",
                            GL_RG16F,
                        },
                        {
                            "rg16i",
                            GL_RG16I,
                        },
                        {
                            "rg16ui",
                            GL_RG16UI,
                        },
                        {
                            "rg8",
                            GL_RG8,
                        },
                        {
                            "rg8i",
                            GL_RG8I,
                        },
                        {
                            "rg8ui",
                            GL_RG8UI,
                        },
                        {
                            "rg8_snorm",
                            GL_RG8_SNORM,
                        },
                        {
                            "r32f",
                            GL_R32F,
                        },
                        {
                            "r32i",
                            GL_R32I,
                        },
                        {
                            "r32ui",
                            GL_R32UI,
                        },
                        {
                            "r16f",
                            GL_R16F,
                        },
                        {
                            "r16i",
                            GL_R16I,
                        },
                        {
                            "r16ui",
                            GL_R16UI,
                        },
                        {
                            "r8",
                            GL_R8,
                        },
                        {
                            "r8i",
                            GL_R8I,
                        },
                        {
                            "r8ui",
                            GL_R8UI,
                        },
                        {
                            "r8_snorm",
                            GL_R8_SNORM,
                        }};

    static const struct
    {
        const char *name;
        uint32_t internalFormat;
    } depthStencilFormats[] = {// Depth and stencil formats
                               {"depth_component32f", GL_DEPTH_COMPONENT32F},
                               {"depth_component24", GL_DEPTH_COMPONENT24},
                               {"depth_component16", GL_DEPTH_COMPONENT16},
                               {"depth32f_stencil8", GL_DEPTH32F_STENCIL8},
                               {"depth24_stencil8", GL_DEPTH24_STENCIL8}};

    // Basic TexImage2D usage.
    {
        tcu::TestCaseGroup *basicTexImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_teximage2d", "Basic glTexImage2D() usage");
        addChild(basicTexImageGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const char *fmtName   = colorFormats[formatNdx].name;
            uint32_t format       = colorFormats[formatNdx].internalFormat;
            const int tex2DWidth  = 64;
            const int tex2DHeight = 128;
            const int texCubeSize = 64;

            basicTexImageGroup->addChild(new BasicTexImage2DCase(m_context, (string(fmtName) + "_2d").c_str(), "",
                                                                 format, tex2DWidth, tex2DHeight));
            basicTexImageGroup->addChild(
                new BasicTexImageCubeCase(m_context, (string(fmtName) + "_cube").c_str(), "", format, texCubeSize));
        }
    }

    // Randomized TexImage2D order.
    {
        tcu::TestCaseGroup *randomTexImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "random_teximage2d", "Randomized glTexImage2D() usage");
        addChild(randomTexImageGroup);

        de::Random rnd(9);

        // 2D cases.
        for (int ndx = 0; ndx < 10; ndx++)
        {
            int formatNdx = rnd.getInt(0, DE_LENGTH_OF_ARRAY(colorFormats) - 1);
            int width     = 1 << rnd.getInt(2, 8);
            int height    = 1 << rnd.getInt(2, 8);

            randomTexImageGroup->addChild(
                new RandomOrderTexImage2DCase(m_context, (string("2d_") + de::toString(ndx)).c_str(), "",
                                              colorFormats[formatNdx].internalFormat, width, height));
        }

        // Cubemap cases.
        for (int ndx = 0; ndx < 10; ndx++)
        {
            int formatNdx = rnd.getInt(0, DE_LENGTH_OF_ARRAY(colorFormats) - 1);
            int size      = 1 << rnd.getInt(2, 8);

            randomTexImageGroup->addChild(
                new RandomOrderTexImageCubeCase(m_context, (string("cube_") + de::toString(ndx)).c_str(), "",
                                                colorFormats[formatNdx].internalFormat, size));
        }
    }

    // TexImage2D unpack alignment.
    {
        tcu::TestCaseGroup *alignGroup =
            new tcu::TestCaseGroup(m_testCtx, "teximage2d_align", "glTexImage2D() unpack alignment tests");
        addChild(alignGroup);

        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_r8_4_8", "", GL_R8, 4, 8, 4, 8));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_r8_63_1", "", GL_R8, 63, 30, 1, 1));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_r8_63_2", "", GL_R8, 63, 30, 1, 2));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_r8_63_4", "", GL_R8, 63, 30, 1, 4));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_r8_63_8", "", GL_R8, 63, 30, 1, 8));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4_51_1", "", GL_RGBA4, 51, 30, 1, 1));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4_51_2", "", GL_RGBA4, 51, 30, 1, 2));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4_51_4", "", GL_RGBA4, 51, 30, 1, 4));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba4_51_8", "", GL_RGBA4, 51, 30, 1, 8));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgb8_39_1", "", GL_RGB8, 39, 43, 1, 1));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgb8_39_2", "", GL_RGB8, 39, 43, 1, 2));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgb8_39_4", "", GL_RGB8, 39, 43, 1, 4));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgb8_39_8", "", GL_RGB8, 39, 43, 1, 8));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba8_47_1", "", GL_RGBA8, 47, 27, 1, 1));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba8_47_2", "", GL_RGBA8, 47, 27, 1, 2));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba8_47_4", "", GL_RGBA8, 47, 27, 1, 4));
        alignGroup->addChild(new TexImage2DAlignCase(m_context, "2d_rgba8_47_8", "", GL_RGBA8, 47, 27, 1, 8));

        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_r8_4_8", "", GL_R8, 4, 3, 8));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_r8_63_1", "", GL_R8, 63, 1, 1));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_r8_63_2", "", GL_R8, 63, 1, 2));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_r8_63_4", "", GL_R8, 63, 1, 4));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_r8_63_8", "", GL_R8, 63, 1, 8));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4_51_1", "", GL_RGBA4, 51, 1, 1));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4_51_2", "", GL_RGBA4, 51, 1, 2));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4_51_4", "", GL_RGBA4, 51, 1, 4));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba4_51_8", "", GL_RGBA4, 51, 1, 8));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgb8_39_1", "", GL_RGB8, 39, 1, 1));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgb8_39_2", "", GL_RGB8, 39, 1, 2));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgb8_39_4", "", GL_RGB8, 39, 1, 4));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgb8_39_8", "", GL_RGB8, 39, 1, 8));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba8_47_1", "", GL_RGBA8, 47, 1, 1));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba8_47_2", "", GL_RGBA8, 47, 1, 2));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba8_47_4", "", GL_RGBA8, 47, 1, 4));
        alignGroup->addChild(new TexImageCubeAlignCase(m_context, "cube_rgba8_47_8", "", GL_RGBA8, 47, 1, 8));
    }

    // glTexImage2D() unpack parameter cases.
    {
        tcu::TestCaseGroup *paramGroup =
            new tcu::TestCaseGroup(m_testCtx, "teximage2d_unpack_params", "glTexImage2D() pixel transfer mode cases");
        addChild(paramGroup);

        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int rowLength;
            int skipRows;
            int skipPixels;
            int alignment;
        } cases[] = {
            {"rgb8_alignment", GL_RGB8, 31, 30, 0, 0, 0, 2},     {"rgb8_row_length", GL_RGB8, 31, 30, 50, 0, 0, 4},
            {"rgb8_skip_rows", GL_RGB8, 31, 30, 0, 3, 0, 4},     {"rgb8_skip_pixels", GL_RGB8, 31, 30, 36, 0, 5, 4},
            {"r8_complex1", GL_R8, 31, 30, 64, 1, 3, 1},         {"r8_complex2", GL_R8, 31, 30, 64, 1, 3, 2},
            {"r8_complex3", GL_R8, 31, 30, 64, 1, 3, 4},         {"r8_complex4", GL_R8, 31, 30, 64, 1, 3, 8},
            {"rgba8_complex1", GL_RGBA8, 56, 61, 69, 0, 0, 8},   {"rgba8_complex2", GL_RGBA8, 56, 61, 69, 0, 7, 8},
            {"rgba8_complex3", GL_RGBA8, 56, 61, 69, 3, 0, 8},   {"rgba8_complex4", GL_RGBA8, 56, 61, 69, 3, 7, 8},
            {"rgba32f_complex", GL_RGBA32F, 19, 10, 27, 1, 7, 8}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cases); ndx++)
            paramGroup->addChild(new TexImage2DParamsCase(
                m_context, cases[ndx].name, "", cases[ndx].format, cases[ndx].width, cases[ndx].height,
                cases[ndx].rowLength, cases[ndx].skipRows, cases[ndx].skipPixels, cases[ndx].alignment));
    }

    // glTexImage2D() pbo cases.
    {
        tcu::TestCaseGroup *pboGroup = new tcu::TestCaseGroup(m_testCtx, "teximage2d_pbo", "glTexImage2D() from PBO");
        addChild(pboGroup);

        // Parameter cases
        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int rowLength;
            int skipRows;
            int skipPixels;
            int alignment;
            int offset;
        } parameterCases[] = {{"rgb8_offset", GL_RGB8, 31, 30, 0, 0, 0, 4, 67},
                              {"rgb8_alignment", GL_RGB8, 31, 30, 0, 0, 0, 2, 0},
                              {"rgb8_row_length", GL_RGB8, 31, 30, 50, 0, 0, 4, 0},
                              {"rgb8_skip_rows", GL_RGB8, 31, 30, 0, 3, 0, 4, 0},
                              {"rgb8_skip_pixels", GL_RGB8, 31, 30, 36, 0, 5, 4, 0}};

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const string fmtName  = colorFormats[formatNdx].name;
            const uint32_t format = colorFormats[formatNdx].internalFormat;
            const int tex2DWidth  = 65;
            const int tex2DHeight = 37;
            const int texCubeSize = 64;

            pboGroup->addChild(new TexImage2DBufferCase(m_context, (fmtName + "_2d").c_str(), "", format, tex2DWidth,
                                                        tex2DHeight, 0, 0, 0, 4, 0));
            pboGroup->addChild(new TexImageCubeBufferCase(m_context, (fmtName + "_cube").c_str(), "", format,
                                                          texCubeSize, 0, 0, 0, 4, 0));
        }

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(parameterCases); ndx++)
        {
            pboGroup->addChild(new TexImage2DBufferCase(m_context, (string(parameterCases[ndx].name) + "_2d").c_str(),
                                                        "", parameterCases[ndx].format, parameterCases[ndx].width,
                                                        parameterCases[ndx].height, parameterCases[ndx].rowLength,
                                                        parameterCases[ndx].skipRows, parameterCases[ndx].skipPixels,
                                                        parameterCases[ndx].alignment, parameterCases[ndx].offset));
            pboGroup->addChild(new TexImageCubeBufferCase(
                m_context, (string(parameterCases[ndx].name) + "_cube").c_str(), "", parameterCases[ndx].format,
                parameterCases[ndx].width, parameterCases[ndx].rowLength, parameterCases[ndx].skipRows,
                parameterCases[ndx].skipPixels, parameterCases[ndx].alignment, parameterCases[ndx].offset));
        }
    }

    // glTexImage2D() depth cases.
    {
        tcu::TestCaseGroup *shadow2dGroup =
            new tcu::TestCaseGroup(m_testCtx, "teximage2d_depth", "glTexImage2D() with depth or depth/stencil format");
        addChild(shadow2dGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilFormats); ndx++)
        {
            const int tex2DWidth  = 64;
            const int tex2DHeight = 128;

            shadow2dGroup->addChild(new TexImage2DDepthCase(m_context, depthStencilFormats[ndx].name, "",
                                                            depthStencilFormats[ndx].internalFormat, tex2DWidth,
                                                            tex2DHeight));
        }
    }

    // glTexImage2D() depth cases with pbo.
    {
        tcu::TestCaseGroup *shadow2dGroup = new tcu::TestCaseGroup(
            m_testCtx, "teximage2d_depth_pbo", "glTexImage2D() with depth or depth/stencil format with pbo");
        addChild(shadow2dGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilFormats); ndx++)
        {
            const int tex2DWidth  = 64;
            const int tex2DHeight = 128;

            shadow2dGroup->addChild(new TexImage2DDepthBufferCase(m_context, depthStencilFormats[ndx].name, "",
                                                                  depthStencilFormats[ndx].internalFormat, tex2DWidth,
                                                                  tex2DHeight));
        }
    }

    // Basic TexSubImage2D usage.
    {
        tcu::TestCaseGroup *basicTexSubImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_texsubimage2d", "Basic glTexSubImage2D() usage");
        addChild(basicTexSubImageGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const char *fmtName   = colorFormats[formatNdx].name;
            uint32_t format       = colorFormats[formatNdx].internalFormat;
            const int tex2DWidth  = 64;
            const int tex2DHeight = 128;
            const int texCubeSize = 64;

            basicTexSubImageGroup->addChild(new BasicTexSubImage2DCase(m_context, (string(fmtName) + "_2d").c_str(), "",
                                                                       format, tex2DWidth, tex2DHeight));
            basicTexSubImageGroup->addChild(
                new BasicTexSubImageCubeCase(m_context, (string(fmtName) + "_cube").c_str(), "", format, texCubeSize));
        }
    }

    // TexSubImage2D to empty texture.
    {
        tcu::TestCaseGroup *texSubImageEmptyTexGroup = new tcu::TestCaseGroup(
            m_testCtx, "texsubimage2d_empty_tex", "glTexSubImage2D() to texture that has storage but no data");
        addChild(texSubImageEmptyTexGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(unsizedFormats); formatNdx++)
        {
            const char *fmtName   = unsizedFormats[formatNdx].name;
            uint32_t format       = unsizedFormats[formatNdx].format;
            uint32_t dataType     = unsizedFormats[formatNdx].dataType;
            const int tex2DWidth  = 64;
            const int tex2DHeight = 32;
            const int texCubeSize = 32;

            texSubImageEmptyTexGroup->addChild(new TexSubImage2DEmptyTexCase(
                m_context, (string(fmtName) + "_2d").c_str(), "", format, dataType, tex2DWidth, tex2DHeight));
            texSubImageEmptyTexGroup->addChild(new TexSubImageCubeEmptyTexCase(
                m_context, (string(fmtName) + "_cube").c_str(), "", format, dataType, texCubeSize));
        }
    }

    // TexSubImage2D alignment cases.
    {
        tcu::TestCaseGroup *alignGroup =
            new tcu::TestCaseGroup(m_testCtx, "texsubimage2d_align", "glTexSubImage2D() unpack alignment tests");
        addChild(alignGroup);

        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_1_1", "", GL_R8, 64, 64, 13, 17, 1, 6, 1));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_1_2", "", GL_R8, 64, 64, 13, 17, 1, 6, 2));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_1_4", "", GL_R8, 64, 64, 13, 17, 1, 6, 4));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_1_8", "", GL_R8, 64, 64, 13, 17, 1, 6, 8));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_63_1", "", GL_R8, 64, 64, 1, 9, 63, 30, 1));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_63_2", "", GL_R8, 64, 64, 1, 9, 63, 30, 2));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_63_4", "", GL_R8, 64, 64, 1, 9, 63, 30, 4));
        alignGroup->addChild(new TexSubImage2DAlignCase(m_context, "2d_r8_63_8", "", GL_R8, 64, 64, 1, 9, 63, 30, 8));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba4_51_1", "", GL_RGBA4, 64, 64, 7, 29, 51, 30, 1));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba4_51_2", "", GL_RGBA4, 64, 64, 7, 29, 51, 30, 2));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba4_51_4", "", GL_RGBA4, 64, 64, 7, 29, 51, 30, 4));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba4_51_8", "", GL_RGBA4, 64, 64, 7, 29, 51, 30, 8));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgb8_39_1", "", GL_RGB8, 64, 64, 11, 8, 39, 43, 1));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgb8_39_2", "", GL_RGB8, 64, 64, 11, 8, 39, 43, 2));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgb8_39_4", "", GL_RGB8, 64, 64, 11, 8, 39, 43, 4));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgb8_39_8", "", GL_RGB8, 64, 64, 11, 8, 39, 43, 8));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba8_47_1", "", GL_RGBA8, 64, 64, 10, 1, 47, 27, 1));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba8_47_2", "", GL_RGBA8, 64, 64, 10, 1, 47, 27, 2));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba8_47_4", "", GL_RGBA8, 64, 64, 10, 1, 47, 27, 4));
        alignGroup->addChild(
            new TexSubImage2DAlignCase(m_context, "2d_rgba8_47_8", "", GL_RGBA8, 64, 64, 10, 1, 47, 27, 8));

        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_1_1", "", GL_R8, 64, 13, 17, 1, 6, 1));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_1_2", "", GL_R8, 64, 13, 17, 1, 6, 2));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_1_4", "", GL_R8, 64, 13, 17, 1, 6, 4));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_1_8", "", GL_R8, 64, 13, 17, 1, 6, 8));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_63_1", "", GL_R8, 64, 1, 9, 63, 30, 1));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_63_2", "", GL_R8, 64, 1, 9, 63, 30, 2));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_63_4", "", GL_R8, 64, 1, 9, 63, 30, 4));
        alignGroup->addChild(new TexSubImageCubeAlignCase(m_context, "cube_r8_63_8", "", GL_R8, 64, 1, 9, 63, 30, 8));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba4_51_1", "", GL_RGBA4, 64, 7, 29, 51, 30, 1));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba4_51_2", "", GL_RGBA4, 64, 7, 29, 51, 30, 2));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba4_51_4", "", GL_RGBA4, 64, 7, 29, 51, 30, 4));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba4_51_8", "", GL_RGBA4, 64, 7, 29, 51, 30, 8));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgb8_39_1", "", GL_RGB8, 64, 11, 8, 39, 43, 1));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgb8_39_2", "", GL_RGB8, 64, 11, 8, 39, 43, 2));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgb8_39_4", "", GL_RGB8, 64, 11, 8, 39, 43, 4));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgb8_39_8", "", GL_RGB8, 64, 11, 8, 39, 43, 8));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba8_47_1", "", GL_RGBA8, 64, 10, 1, 47, 27, 1));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba8_47_2", "", GL_RGBA8, 64, 10, 1, 47, 27, 2));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba8_47_4", "", GL_RGBA8, 64, 10, 1, 47, 27, 4));
        alignGroup->addChild(
            new TexSubImageCubeAlignCase(m_context, "cube_rgba8_47_8", "", GL_RGBA8, 64, 10, 1, 47, 27, 8));
    }

    // glTexSubImage2D() pixel transfer mode cases.
    {
        tcu::TestCaseGroup *paramGroup = new tcu::TestCaseGroup(m_testCtx, "texsubimage2d_unpack_params",
                                                                "glTexSubImage2D() pixel transfer mode cases");
        addChild(paramGroup);

        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int subX;
            int subY;
            int subW;
            int subH;
            int rowLength;
            int skipRows;
            int skipPixels;
            int alignment;
        } cases[] = {{"rgb8_alignment", GL_RGB8, 54, 60, 11, 7, 31, 30, 0, 0, 0, 2},
                     {"rgb8_row_length", GL_RGB8, 54, 60, 11, 7, 31, 30, 50, 0, 0, 4},
                     {"rgb8_skip_rows", GL_RGB8, 54, 60, 11, 7, 31, 30, 0, 3, 0, 4},
                     {"rgb8_skip_pixels", GL_RGB8, 54, 60, 11, 7, 31, 30, 36, 0, 5, 4},
                     {"r8_complex1", GL_R8, 54, 60, 11, 7, 31, 30, 64, 1, 3, 1},
                     {"r8_complex2", GL_R8, 54, 60, 11, 7, 31, 30, 64, 1, 3, 2},
                     {"r8_complex3", GL_R8, 54, 60, 11, 7, 31, 30, 64, 1, 3, 4},
                     {"r8_complex4", GL_R8, 54, 60, 11, 7, 31, 30, 64, 1, 3, 8},
                     {"rgba8_complex1", GL_RGBA8, 92, 84, 13, 19, 56, 61, 69, 0, 0, 8},
                     {"rgba8_complex2", GL_RGBA8, 92, 84, 13, 19, 56, 61, 69, 0, 7, 8},
                     {"rgba8_complex3", GL_RGBA8, 92, 84, 13, 19, 56, 61, 69, 3, 0, 8},
                     {"rgba8_complex4", GL_RGBA8, 92, 84, 13, 19, 56, 61, 69, 3, 7, 8},
                     {"rgba32f_complex", GL_RGBA32F, 92, 84, 13, 19, 56, 61, 69, 3, 7, 8}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cases); ndx++)
            paramGroup->addChild(new TexSubImage2DParamsCase(
                m_context, cases[ndx].name, "", cases[ndx].format, cases[ndx].width, cases[ndx].height, cases[ndx].subX,
                cases[ndx].subY, cases[ndx].subW, cases[ndx].subH, cases[ndx].rowLength, cases[ndx].skipRows,
                cases[ndx].skipPixels, cases[ndx].alignment));
    }

    // glTexSubImage2D() PBO cases.
    {
        tcu::TestCaseGroup *pboGroup =
            new tcu::TestCaseGroup(m_testCtx, "texsubimage2d_pbo", "glTexSubImage2D() pixel buffer object tests");
        addChild(pboGroup);

        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int subX;
            int subY;
            int subW;
            int subH;
            int rowLength;
            int skipRows;
            int skipPixels;
            int alignment;
            int offset;
        } paramCases[] = {{"rgb8_offset", GL_RGB8, 54, 60, 11, 7, 31, 30, 0, 0, 0, 4, 67},
                          {"rgb8_alignment", GL_RGB8, 54, 60, 11, 7, 31, 30, 0, 0, 0, 2, 0},
                          {"rgb8_row_length", GL_RGB8, 54, 60, 11, 7, 31, 30, 50, 0, 0, 4, 0},
                          {"rgb8_skip_rows", GL_RGB8, 54, 60, 11, 7, 31, 30, 0, 3, 0, 4, 0},
                          {"rgb8_skip_pixels", GL_RGB8, 54, 60, 11, 7, 31, 30, 36, 0, 5, 4, 0}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(colorFormats); ndx++)
        {
            pboGroup->addChild(new TexSubImage2DBufferCase(
                m_context, (std::string(colorFormats[ndx].name) + "_2d").c_str(), "", colorFormats[ndx].internalFormat,
                54, // Width
                60, // Height
                11, // Sub X
                7,  // Sub Y
                31, // Sub W
                30, // Sub H
                0,  // Row len
                0,  // Skip rows
                0,  // Skip pixels
                4,  // Alignment
                0 /* offset */));
            pboGroup->addChild(new TexSubImageCubeBufferCase(m_context,
                                                             (std::string(colorFormats[ndx].name) + "_cube").c_str(),
                                                             "", colorFormats[ndx].internalFormat,
                                                             64, // Size
                                                             11, // Sub X
                                                             7,  // Sub Y
                                                             31, // Sub W
                                                             30, // Sub H
                                                             0,  // Row len
                                                             0,  // Skip rows
                                                             0,  // Skip pixels
                                                             4,  // Alignment
                                                             0 /* offset */));
        }

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(paramCases); ndx++)
        {
            pboGroup->addChild(new TexSubImage2DBufferCase(
                m_context, (std::string(paramCases[ndx].name) + "_2d").c_str(), "", paramCases[ndx].format,
                paramCases[ndx].width, paramCases[ndx].height, paramCases[ndx].subX, paramCases[ndx].subY,
                paramCases[ndx].subW, paramCases[ndx].subH, paramCases[ndx].rowLength, paramCases[ndx].skipRows,
                paramCases[ndx].skipPixels, paramCases[ndx].alignment, paramCases[ndx].offset));
            pboGroup->addChild(new TexSubImageCubeBufferCase(
                m_context, (std::string(paramCases[ndx].name) + "_cube").c_str(), "", paramCases[ndx].format,
                paramCases[ndx].width, paramCases[ndx].subX, paramCases[ndx].subY, paramCases[ndx].subW,
                paramCases[ndx].subH, paramCases[ndx].rowLength, paramCases[ndx].skipRows, paramCases[ndx].skipPixels,
                paramCases[ndx].alignment, paramCases[ndx].offset));
        }

        // This test makes sure the last bits from the PBO data can be read without errors.
        pboGroup->addChild(new CopyTexFromPBOCase(m_context, "pbo_bounds_2d",
                                                  "Checks the last bits are read from the PBO without errors"));
    }

    // glTexSubImage2D() depth cases.
    {
        tcu::TestCaseGroup *shadow2dGroup = new tcu::TestCaseGroup(
            m_testCtx, "texsubimage2d_depth", "glTexSubImage2D() with depth or depth/stencil format");
        addChild(shadow2dGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilFormats); ndx++)
        {
            const int tex2DWidth  = 64;
            const int tex2DHeight = 32;

            shadow2dGroup->addChild(new TexSubImage2DDepthCase(m_context, depthStencilFormats[ndx].name, "",
                                                               depthStencilFormats[ndx].internalFormat, tex2DWidth,
                                                               tex2DHeight));
        }
    }

    // Basic glCopyTexImage2D() cases
    {
        tcu::TestCaseGroup *copyTexImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_copyteximage2d", "Basic glCopyTexImage2D() usage");
        addChild(copyTexImageGroup);

        copyTexImageGroup->addChild(new BasicCopyTexImage2DCase(m_context, "2d_alpha", "", GL_ALPHA, 128, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImage2DCase(m_context, "2d_luminance", "", GL_LUMINANCE, 128, 64));
        copyTexImageGroup->addChild(
            new BasicCopyTexImage2DCase(m_context, "2d_luminance_alpha", "", GL_LUMINANCE_ALPHA, 128, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImage2DCase(m_context, "2d_rgb", "", GL_RGB, 128, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImage2DCase(m_context, "2d_rgba", "", GL_RGBA, 128, 64));

        copyTexImageGroup->addChild(new BasicCopyTexImageCubeCase(m_context, "cube_alpha", "", GL_ALPHA, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImageCubeCase(m_context, "cube_luminance", "", GL_LUMINANCE, 64));
        copyTexImageGroup->addChild(
            new BasicCopyTexImageCubeCase(m_context, "cube_luminance_alpha", "", GL_LUMINANCE_ALPHA, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImageCubeCase(m_context, "cube_rgb", "", GL_RGB, 64));
        copyTexImageGroup->addChild(new BasicCopyTexImageCubeCase(m_context, "cube_rgba", "", GL_RGBA, 64));
    }

    // Basic glCopyTexSubImage2D() cases
    {
        tcu::TestCaseGroup *copyTexSubImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_copytexsubimage2d", "Basic glCopyTexSubImage2D() usage");
        addChild(copyTexSubImageGroup);

        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImage2DCase(m_context, "2d_alpha", "", GL_ALPHA, GL_UNSIGNED_BYTE, 128, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImage2DCase(m_context, "2d_luminance", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 128, 64));
        copyTexSubImageGroup->addChild(new BasicCopyTexSubImage2DCase(m_context, "2d_luminance_alpha", "",
                                                                      GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 128, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImage2DCase(m_context, "2d_rgb", "", GL_RGB, GL_UNSIGNED_BYTE, 128, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImage2DCase(m_context, "2d_rgba", "", GL_RGBA, GL_UNSIGNED_BYTE, 128, 64));

        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImageCubeCase(m_context, "cube_alpha", "", GL_ALPHA, GL_UNSIGNED_BYTE, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImageCubeCase(m_context, "cube_luminance", "", GL_LUMINANCE, GL_UNSIGNED_BYTE, 64));
        copyTexSubImageGroup->addChild(new BasicCopyTexSubImageCubeCase(m_context, "cube_luminance_alpha", "",
                                                                        GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImageCubeCase(m_context, "cube_rgb", "", GL_RGB, GL_UNSIGNED_BYTE, 64));
        copyTexSubImageGroup->addChild(
            new BasicCopyTexSubImageCubeCase(m_context, "cube_rgba", "", GL_RGBA, GL_UNSIGNED_BYTE, 64));
    }

    // Basic TexImage3D usage.
    {
        tcu::TestCaseGroup *basicTexImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_teximage3d", "Basic glTexImage3D() usage");
        addChild(basicTexImageGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const char *fmtName        = colorFormats[formatNdx].name;
            uint32_t format            = colorFormats[formatNdx].internalFormat;
            const int tex2DArrayWidth  = 57;
            const int tex2DArrayHeight = 44;
            const int tex2DArrayLevels = 5;
            const int tex3DWidth       = 63;
            const int tex3DHeight      = 29;
            const int tex3DDepth       = 11;

            basicTexImageGroup->addChild(
                new BasicTexImage2DArrayCase(m_context, (string(fmtName) + "_2d_array").c_str(), "", format,
                                             tex2DArrayWidth, tex2DArrayHeight, tex2DArrayLevels));
            basicTexImageGroup->addChild(new BasicTexImage3DCase(m_context, (string(fmtName) + "_3d").c_str(), "",
                                                                 format, tex3DWidth, tex3DHeight, tex3DDepth));
        }
    }

    // glTexImage3D() unpack params cases.
    {
        tcu::TestCaseGroup *paramGroup =
            new tcu::TestCaseGroup(m_testCtx, "teximage3d_unpack_params", "glTexImage3D() unpack parameters");
        addChild(paramGroup);

        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int depth;
            int imageHeight;
            int rowLength;
            int skipImages;
            int skipRows;
            int skipPixels;
            int alignment;
        } cases[] = {{"rgb8_image_height", GL_RGB8, 23, 19, 8, 26, 0, 0, 0, 0, 4},
                     {"rgb8_row_length", GL_RGB8, 23, 19, 8, 0, 27, 0, 0, 0, 4},
                     {"rgb8_skip_images", GL_RGB8, 23, 19, 8, 0, 0, 3, 0, 0, 4},
                     {"rgb8_skip_rows", GL_RGB8, 23, 19, 8, 22, 0, 0, 3, 0, 4},
                     {"rgb8_skip_pixels", GL_RGB8, 23, 19, 8, 0, 25, 0, 0, 2, 4},
                     {"r8_complex1", GL_R8, 13, 17, 11, 23, 15, 2, 3, 1, 1},
                     {"r8_complex2", GL_R8, 13, 17, 11, 23, 15, 2, 3, 1, 2},
                     {"r8_complex3", GL_R8, 13, 17, 11, 23, 15, 2, 3, 1, 4},
                     {"r8_complex4", GL_R8, 13, 17, 11, 23, 15, 2, 3, 1, 8},
                     {"rgba8_complex1", GL_RGBA8, 11, 20, 8, 25, 14, 0, 0, 0, 8},
                     {"rgba8_complex2", GL_RGBA8, 11, 20, 8, 25, 14, 0, 2, 0, 8},
                     {"rgba8_complex3", GL_RGBA8, 11, 20, 8, 25, 14, 0, 0, 3, 8},
                     {"rgba8_complex4", GL_RGBA8, 11, 20, 8, 25, 14, 0, 2, 3, 8},
                     {"rgba32f_complex", GL_RGBA32F, 11, 20, 8, 25, 14, 0, 2, 3, 8}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cases); ndx++)
            paramGroup->addChild(new TexImage3DParamsCase(
                m_context, cases[ndx].name, "", cases[ndx].format, cases[ndx].width, cases[ndx].height,
                cases[ndx].depth, cases[ndx].imageHeight, cases[ndx].rowLength, cases[ndx].skipImages,
                cases[ndx].skipRows, cases[ndx].skipPixels, cases[ndx].alignment));
    }

    // glTexImage3D() pbo cases.
    {
        tcu::TestCaseGroup *pboGroup = new tcu::TestCaseGroup(m_testCtx, "teximage3d_pbo", "glTexImage3D() from PBO");
        addChild(pboGroup);

        // Parameter cases
        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int depth;
            int imageHeight;
            int rowLength;
            int skipImages;
            int skipRows;
            int skipPixels;
            int alignment;
            int offset;
        } parameterCases[] = {{"rgb8_offset", GL_RGB8, 23, 19, 8, 0, 0, 0, 0, 0, 1, 67},
                              {"rgb8_alignment", GL_RGB8, 23, 19, 8, 0, 0, 0, 0, 0, 2, 0},
                              {"rgb8_image_height", GL_RGB8, 23, 19, 8, 26, 0, 0, 0, 0, 4, 0},
                              {"rgb8_row_length", GL_RGB8, 23, 19, 8, 0, 27, 0, 0, 0, 4, 0},
                              {"rgb8_skip_images", GL_RGB8, 23, 19, 8, 0, 0, 3, 0, 0, 4, 0},
                              {"rgb8_skip_rows", GL_RGB8, 23, 19, 8, 22, 0, 0, 3, 0, 4, 0},
                              {"rgb8_skip_pixels", GL_RGB8, 23, 19, 8, 0, 25, 0, 0, 2, 4, 0}};

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const string fmtName  = colorFormats[formatNdx].name;
            const uint32_t format = colorFormats[formatNdx].internalFormat;
            const int tex3DWidth  = 11;
            const int tex3DHeight = 20;
            const int tex3DDepth  = 8;

            pboGroup->addChild(new TexImage2DArrayBufferCase(m_context, (fmtName + "_2d_array").c_str(), "", format,
                                                             tex3DWidth, tex3DHeight, tex3DDepth, 0, 0, 0, 0, 0, 4, 0));
            pboGroup->addChild(new TexImage3DBufferCase(m_context, (fmtName + "_3d").c_str(), "", format, tex3DWidth,
                                                        tex3DHeight, tex3DDepth, 0, 0, 0, 0, 0, 4, 0));
        }

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(parameterCases); ndx++)
        {
            pboGroup->addChild(new TexImage2DArrayBufferCase(
                m_context, (string(parameterCases[ndx].name) + "_2d_array").c_str(), "", parameterCases[ndx].format,
                parameterCases[ndx].width, parameterCases[ndx].depth, parameterCases[ndx].height,
                parameterCases[ndx].imageHeight, parameterCases[ndx].rowLength, parameterCases[ndx].skipImages,
                parameterCases[ndx].skipRows, parameterCases[ndx].skipPixels, parameterCases[ndx].alignment,
                parameterCases[ndx].offset));
            pboGroup->addChild(new TexImage3DBufferCase(
                m_context, (string(parameterCases[ndx].name) + "_3d").c_str(), "", parameterCases[ndx].format,
                parameterCases[ndx].width, parameterCases[ndx].depth, parameterCases[ndx].height,
                parameterCases[ndx].imageHeight, parameterCases[ndx].rowLength, parameterCases[ndx].skipImages,
                parameterCases[ndx].skipRows, parameterCases[ndx].skipPixels, parameterCases[ndx].alignment,
                parameterCases[ndx].offset));
        }
    }

    // glTexImage3D() depth cases.
    {
        tcu::TestCaseGroup *shadow3dGroup =
            new tcu::TestCaseGroup(m_testCtx, "teximage3d_depth", "glTexImage3D() with depth or depth/stencil format");
        addChild(shadow3dGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilFormats); ndx++)
        {
            const int tex3DWidth  = 32;
            const int tex3DHeight = 64;
            const int tex3DDepth  = 8;

            shadow3dGroup->addChild(new TexImage2DArrayDepthCase(
                m_context, (std::string(depthStencilFormats[ndx].name) + "_2d_array").c_str(), "",
                depthStencilFormats[ndx].internalFormat, tex3DWidth, tex3DHeight, tex3DDepth));
        }
    }

    // glTexImage3D() depth cases with pbo.
    {
        tcu::TestCaseGroup *shadow3dGroup = new tcu::TestCaseGroup(
            m_testCtx, "teximage3d_depth_pbo", "glTexImage3D() with depth or depth/stencil format with pbo");
        addChild(shadow3dGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilFormats); ndx++)
        {
            const int tex3DWidth  = 32;
            const int tex3DHeight = 64;
            const int tex3DDepth  = 8;

            shadow3dGroup->addChild(new TexImage2DArrayDepthBufferCase(
                m_context, (std::string(depthStencilFormats[ndx].name) + "_2d_array").c_str(), "",
                depthStencilFormats[ndx].internalFormat, tex3DWidth, tex3DHeight, tex3DDepth));
        }
    }

    // Basic TexSubImage3D usage.
    {
        tcu::TestCaseGroup *basicTexSubImageGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_texsubimage3d", "Basic glTexSubImage3D() usage");
        addChild(basicTexSubImageGroup);
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const char *fmtName   = colorFormats[formatNdx].name;
            uint32_t format       = colorFormats[formatNdx].internalFormat;
            const int tex3DWidth  = 32;
            const int tex3DHeight = 64;
            const int tex3DDepth  = 8;

            basicTexSubImageGroup->addChild(new BasicTexSubImage3DCase(m_context, (string(fmtName) + "_3d").c_str(), "",
                                                                       format, tex3DWidth, tex3DHeight, tex3DDepth));
        }
    }

    // glTexSubImage3D() unpack params cases.
    {
        tcu::TestCaseGroup *paramGroup =
            new tcu::TestCaseGroup(m_testCtx, "texsubimage3d_unpack_params", "glTexSubImage3D() unpack parameters");
        addChild(paramGroup);

        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int depth;
            int subX;
            int subY;
            int subZ;
            int subW;
            int subH;
            int subD;
            int imageHeight;
            int rowLength;
            int skipImages;
            int skipRows;
            int skipPixels;
            int alignment;
        } cases[] = {{"rgb8_image_height", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 26, 0, 0, 0, 0, 4},
                     {"rgb8_row_length", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 0, 27, 0, 0, 0, 4},
                     {"rgb8_skip_images", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 0, 0, 3, 0, 0, 4},
                     {"rgb8_skip_rows", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 22, 0, 0, 3, 0, 4},
                     {"rgb8_skip_pixels", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 0, 25, 0, 0, 2, 4},
                     {"r8_complex1", GL_R8, 15, 20, 11, 1, 1, 0, 13, 17, 11, 23, 15, 2, 3, 1, 1},
                     {"r8_complex2", GL_R8, 15, 20, 11, 1, 1, 0, 13, 17, 11, 23, 15, 2, 3, 1, 2},
                     {"r8_complex3", GL_R8, 15, 20, 11, 1, 1, 0, 13, 17, 11, 23, 15, 2, 3, 1, 4},
                     {"r8_complex4", GL_R8, 15, 20, 11, 1, 1, 0, 13, 17, 11, 23, 15, 2, 3, 1, 8},
                     {"rgba8_complex1", GL_RGBA8, 15, 25, 10, 0, 5, 1, 11, 20, 8, 25, 14, 0, 0, 0, 8},
                     {"rgba8_complex2", GL_RGBA8, 15, 25, 10, 0, 5, 1, 11, 20, 8, 25, 14, 0, 2, 0, 8},
                     {"rgba8_complex3", GL_RGBA8, 15, 25, 10, 0, 5, 1, 11, 20, 8, 25, 14, 0, 0, 3, 8},
                     {"rgba8_complex4", GL_RGBA8, 15, 25, 10, 0, 5, 1, 11, 20, 8, 25, 14, 0, 2, 3, 8},
                     {"rgba32f_complex", GL_RGBA32F, 15, 25, 10, 0, 5, 1, 11, 20, 8, 25, 14, 0, 2, 3, 8}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cases); ndx++)
            paramGroup->addChild(new TexSubImage3DParamsCase(
                m_context, cases[ndx].name, "", cases[ndx].format, cases[ndx].width, cases[ndx].height,
                cases[ndx].depth, cases[ndx].subX, cases[ndx].subY, cases[ndx].subZ, cases[ndx].subW, cases[ndx].subH,
                cases[ndx].subD, cases[ndx].imageHeight, cases[ndx].rowLength, cases[ndx].skipImages,
                cases[ndx].skipRows, cases[ndx].skipPixels, cases[ndx].alignment));
    }

    // glTexSubImage3D() PBO cases.
    {
        tcu::TestCaseGroup *pboGroup =
            new tcu::TestCaseGroup(m_testCtx, "texsubimage3d_pbo", "glTexSubImage3D() pixel buffer object tests");
        addChild(pboGroup);

        static const struct
        {
            const char *name;
            uint32_t format;
            int width;
            int height;
            int depth;
            int subX;
            int subY;
            int subZ;
            int subW;
            int subH;
            int subD;
            int imageHeight;
            int rowLength;
            int skipImages;
            int skipRows;
            int skipPixels;
            int alignment;
            int offset;
        } paramCases[] = {{"rgb8_offset", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 0, 0, 0, 0, 0, 4, 67},
                          {"rgb8_image_height", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 26, 0, 0, 0, 0, 4, 0},
                          {"rgb8_row_length", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 0, 27, 0, 0, 0, 4, 0},
                          {"rgb8_skip_images", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 0, 0, 3, 0, 0, 4, 0},
                          {"rgb8_skip_rows", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 22, 0, 0, 3, 0, 4, 0},
                          {"rgb8_skip_pixels", GL_RGB8, 26, 25, 10, 1, 2, 1, 23, 19, 8, 0, 25, 0, 0, 2, 4, 0}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(colorFormats); ndx++)
        {
            pboGroup->addChild(
                new TexSubImage2DArrayBufferCase(m_context, (std::string(colorFormats[ndx].name) + "_2d_array").c_str(),
                                                 "", colorFormats[ndx].internalFormat,
                                                 26, // Width
                                                 25, // Height
                                                 10, // Depth
                                                 1,  // Sub X
                                                 2,  // Sub Y
                                                 0,  // Sub Z
                                                 23, // Sub W
                                                 19, // Sub H
                                                 8,  // Sub D
                                                 0,  // Image height
                                                 0,  // Row length
                                                 0,  // Skip images
                                                 0,  // Skip rows
                                                 0,  // Skip pixels
                                                 4,  // Alignment
                                                 0 /* offset */));
            pboGroup->addChild(new TexSubImage3DBufferCase(
                m_context, (std::string(colorFormats[ndx].name) + "_3d").c_str(), "", colorFormats[ndx].internalFormat,
                26, // Width
                25, // Height
                10, // Depth
                1,  // Sub X
                2,  // Sub Y
                0,  // Sub Z
                23, // Sub W
                19, // Sub H
                8,  // Sub D
                0,  // Image height
                0,  // Row length
                0,  // Skip images
                0,  // Skip rows
                0,  // Skip pixels
                4,  // Alignment
                0 /* offset */));
        }

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(paramCases); ndx++)
        {
            pboGroup->addChild(new TexSubImage2DArrayBufferCase(
                m_context, (std::string(paramCases[ndx].name) + "_2d_array").c_str(), "", paramCases[ndx].format,
                paramCases[ndx].width, paramCases[ndx].height, paramCases[ndx].depth, paramCases[ndx].subX,
                paramCases[ndx].subY, paramCases[ndx].subZ, paramCases[ndx].subW, paramCases[ndx].subH,
                paramCases[ndx].subD, paramCases[ndx].imageHeight, paramCases[ndx].rowLength,
                paramCases[ndx].skipImages, paramCases[ndx].skipRows, paramCases[ndx].skipPixels,
                paramCases[ndx].alignment, paramCases[ndx].offset));
            pboGroup->addChild(new TexSubImage3DBufferCase(
                m_context, (std::string(paramCases[ndx].name) + "_3d").c_str(), "", paramCases[ndx].format,
                paramCases[ndx].width, paramCases[ndx].height, paramCases[ndx].depth, paramCases[ndx].subX,
                paramCases[ndx].subY, paramCases[ndx].subZ, paramCases[ndx].subW, paramCases[ndx].subH,
                paramCases[ndx].subD, paramCases[ndx].imageHeight, paramCases[ndx].rowLength,
                paramCases[ndx].skipImages, paramCases[ndx].skipRows, paramCases[ndx].skipPixels,
                paramCases[ndx].alignment, paramCases[ndx].offset));
        }
    }

    // glTexSubImage3D() depth cases.
    {
        tcu::TestCaseGroup *shadow3dGroup = new tcu::TestCaseGroup(
            m_testCtx, "texsubimage3d_depth", "glTexSubImage3D() with depth or depth/stencil format");
        addChild(shadow3dGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthStencilFormats); ndx++)
        {
            const int tex2DArrayWidth  = 57;
            const int tex2DArrayHeight = 44;
            const int tex2DArrayLevels = 5;

            shadow3dGroup->addChild(new TexSubImage2DArrayDepthCase(
                m_context, (std::string(depthStencilFormats[ndx].name) + "_2d_array").c_str(), "",
                depthStencilFormats[ndx].internalFormat, tex2DArrayWidth, tex2DArrayHeight, tex2DArrayLevels));
        }
    }

    // glTexStorage2D() cases.
    {
        tcu::TestCaseGroup *texStorageGroup =
            new tcu::TestCaseGroup(m_testCtx, "texstorage2d", "Basic glTexStorage2D() usage");
        addChild(texStorageGroup);

        // All formats.
        tcu::TestCaseGroup *formatGroup =
            new tcu::TestCaseGroup(m_testCtx, "format", "glTexStorage2D() with all formats");
        texStorageGroup->addChild(formatGroup);

        // Color formats.
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const char *fmtName     = colorFormats[formatNdx].name;
            uint32_t internalFormat = colorFormats[formatNdx].internalFormat;
            const int tex2DWidth    = 117;
            const int tex2DHeight   = 97;
            int tex2DLevels         = maxLevelCount(tex2DWidth, tex2DHeight);
            const int cubeSize      = 57;
            int cubeLevels          = maxLevelCount(cubeSize, cubeSize);

            formatGroup->addChild(new BasicTexStorage2DCase(m_context, (string(fmtName) + "_2d").c_str(), "",
                                                            internalFormat, tex2DWidth, tex2DHeight, tex2DLevels));
            formatGroup->addChild(new BasicTexStorageCubeCase(m_context, (string(fmtName) + "_cube").c_str(), "",
                                                              internalFormat, cubeSize, cubeLevels));
        }

        // Depth / stencil formats.
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats); formatNdx++)
        {
            const char *fmtName     = depthStencilFormats[formatNdx].name;
            uint32_t internalFormat = depthStencilFormats[formatNdx].internalFormat;
            const int tex2DWidth    = 117;
            const int tex2DHeight   = 97;
            int tex2DLevels         = maxLevelCount(tex2DWidth, tex2DHeight);
            const int cubeSize      = 57;
            int cubeLevels          = maxLevelCount(cubeSize, cubeSize);

            formatGroup->addChild(new BasicTexStorage2DCase(m_context, (string(fmtName) + "_2d").c_str(), "",
                                                            internalFormat, tex2DWidth, tex2DHeight, tex2DLevels));
            formatGroup->addChild(new BasicTexStorageCubeCase(m_context, (string(fmtName) + "_cube").c_str(), "",
                                                              internalFormat, cubeSize, cubeLevels));
        }

        // Sizes.
        static const struct
        {
            int width;
            int height;
            int levels;
        } tex2DSizes[] = {//    W    H    L
                          {1, 1, 1}, {2, 2, 2}, {64, 32, 7}, {32, 64, 4}, {57, 63, 1}, {57, 63, 2}, {57, 63, 6}};
        static const struct
        {
            int size;
            int levels;
        } cubeSizes[] = {
            //    S    L
            {1, 1}, {2, 2}, {57, 1}, {57, 2}, {57, 6}, {64, 4}, {64, 7},
        };

        tcu::TestCaseGroup *sizeGroup =
            new tcu::TestCaseGroup(m_testCtx, "size", "glTexStorage2D() with various sizes");
        texStorageGroup->addChild(sizeGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(tex2DSizes); ndx++)
        {
            const uint32_t format = GL_RGBA8;
            int width             = tex2DSizes[ndx].width;
            int height            = tex2DSizes[ndx].height;
            int levels            = tex2DSizes[ndx].levels;
            string name           = string("2d_") + de::toString(width) + "x" + de::toString(height) + "_" +
                          de::toString(levels) + "_levels";

            sizeGroup->addChild(new BasicTexStorage2DCase(m_context, name.c_str(), "", format, width, height, levels));
        }

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cubeSizes); ndx++)
        {
            const uint32_t format = GL_RGBA8;
            int size              = cubeSizes[ndx].size;
            int levels            = cubeSizes[ndx].levels;
            string name = string("cube_") + de::toString(size) + "x" + de::toString(size) + "_" + de::toString(levels) +
                          "_levels";

            sizeGroup->addChild(new BasicTexStorageCubeCase(m_context, name.c_str(), "", format, size, levels));
        }
    }

    // glTexStorage3D() cases.
    {
        tcu::TestCaseGroup *texStorageGroup =
            new tcu::TestCaseGroup(m_testCtx, "texstorage3d", "Basic glTexStorage3D() usage");
        addChild(texStorageGroup);

        // All formats.
        tcu::TestCaseGroup *formatGroup =
            new tcu::TestCaseGroup(m_testCtx, "format", "glTexStorage3D() with all formats");
        texStorageGroup->addChild(formatGroup);

        // Color formats.
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(colorFormats); formatNdx++)
        {
            const char *fmtName        = colorFormats[formatNdx].name;
            uint32_t internalFormat    = colorFormats[formatNdx].internalFormat;
            const int tex2DArrayWidth  = 57;
            const int tex2DArrayHeight = 13;
            const int tex2DArrayLayers = 7;
            int tex2DArrayLevels       = maxLevelCount(tex2DArrayWidth, tex2DArrayHeight);
            const int tex3DWidth       = 59;
            const int tex3DHeight      = 37;
            const int tex3DDepth       = 11;
            int tex3DLevels            = maxLevelCount(tex3DWidth, tex3DHeight, tex3DDepth);

            formatGroup->addChild(new BasicTexStorage2DArrayCase(m_context, (string(fmtName) + "_2d_array").c_str(), "",
                                                                 internalFormat, tex2DArrayWidth, tex2DArrayHeight,
                                                                 tex2DArrayLayers, tex2DArrayLevels));
            formatGroup->addChild(new BasicTexStorage3DCase(m_context, (string(fmtName) + "_3d").c_str(), "",
                                                            internalFormat, tex3DWidth, tex3DHeight, tex3DDepth,
                                                            tex3DLevels));
        }

        // Depth/stencil formats (only 2D texture array is supported).
        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats); formatNdx++)
        {
            const char *fmtName        = depthStencilFormats[formatNdx].name;
            uint32_t internalFormat    = depthStencilFormats[formatNdx].internalFormat;
            const int tex2DArrayWidth  = 57;
            const int tex2DArrayHeight = 13;
            const int tex2DArrayLayers = 7;
            int tex2DArrayLevels       = maxLevelCount(tex2DArrayWidth, tex2DArrayHeight);

            formatGroup->addChild(new BasicTexStorage2DArrayCase(m_context, (string(fmtName) + "_2d_array").c_str(), "",
                                                                 internalFormat, tex2DArrayWidth, tex2DArrayHeight,
                                                                 tex2DArrayLayers, tex2DArrayLevels));
        }

        // Sizes.
        static const struct
        {
            int width;
            int height;
            int layers;
            int levels;
        } tex2DArraySizes[] = {//    W    H    La    Le
                               {1, 1, 1, 1},   {2, 2, 2, 2},   {64, 32, 3, 7}, {32, 64, 3, 4},
                               {57, 63, 5, 1}, {57, 63, 5, 2}, {57, 63, 5, 6}};
        static const struct
        {
            int width;
            int height;
            int depth;
            int levels;
        } tex3DSizes[] = {//    W    H    D    L
                          {1, 1, 1, 1},    {2, 2, 2, 2},    {64, 32, 16, 7}, {32, 64, 16, 4},
                          {32, 16, 64, 4}, {57, 63, 11, 1}, {57, 63, 11, 2}, {57, 63, 11, 6}};

        tcu::TestCaseGroup *sizeGroup =
            new tcu::TestCaseGroup(m_testCtx, "size", "glTexStorage2D() with various sizes");
        texStorageGroup->addChild(sizeGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(tex2DArraySizes); ndx++)
        {
            const uint32_t format = GL_RGBA8;
            int width             = tex2DArraySizes[ndx].width;
            int height            = tex2DArraySizes[ndx].height;
            int layers            = tex2DArraySizes[ndx].layers;
            int levels            = tex2DArraySizes[ndx].levels;
            string name           = string("2d_array_") + de::toString(width) + "x" + de::toString(height) + "x" +
                          de::toString(layers) + "_" + de::toString(levels) + "_levels";

            sizeGroup->addChild(
                new BasicTexStorage2DArrayCase(m_context, name.c_str(), "", format, width, height, layers, levels));
        }

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(tex3DSizes); ndx++)
        {
            const uint32_t format = GL_RGBA8;
            int width             = tex3DSizes[ndx].width;
            int height            = tex3DSizes[ndx].height;
            int depth             = tex3DSizes[ndx].depth;
            int levels            = tex3DSizes[ndx].levels;
            string name = string("3d_") + de::toString(width) + "x" + de::toString(height) + "x" + de::toString(depth) +
                          "_" + de::toString(levels) + "_levels";

            sizeGroup->addChild(
                new BasicTexStorage3DCase(m_context, name.c_str(), "", format, width, height, depth, levels));
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
