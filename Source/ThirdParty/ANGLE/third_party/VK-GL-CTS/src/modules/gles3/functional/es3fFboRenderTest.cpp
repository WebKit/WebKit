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
 * \brief Framebuffer Object Tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFboRenderTest.hpp"
#include "sglrContextUtil.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "es3fFboTestUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "deRandom.h"
#include "deString.h"
#include "glwDefs.hpp"
#include "glwEnums.hpp"

#include <sstream>

using std::string;
using std::vector;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::RGBA;
using tcu::Surface;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

namespace deqp
{
namespace gles3
{
namespace Functional
{

using glw::GLenum;
using namespace FboTestUtil;

class FboConfig
{
public:
    FboConfig(uint32_t buffers_, uint32_t colorType_, uint32_t colorFormat_, uint32_t depthStencilType_,
              uint32_t depthStencilFormat_, int width_ = 0, int height_ = 0, int samples_ = 0)
        : buffers(buffers_)
        , colorType(colorType_)
        , colorFormat(colorFormat_)
        , depthStencilType(depthStencilType_)
        , depthStencilFormat(depthStencilFormat_)
        , width(width_)
        , height(height_)
        , samples(samples_)
    {
    }

    FboConfig(void)
        : buffers(0)
        , colorType(GL_NONE)
        , colorFormat(GL_NONE)
        , depthStencilType(GL_NONE)
        , depthStencilFormat(GL_NONE)
        , width(0)
        , height(0)
        , samples(0)
    {
    }

    std::string getName(void) const;

    uint32_t buffers; //!< Buffer bit mask (GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|...)

    GLenum colorType;   //!< GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, GL_RENDERBUFFER
    GLenum colorFormat; //!< Internal format for color buffer texture or renderbuffer

    GLenum depthStencilType;
    GLenum depthStencilFormat;

    int width;
    int height;
    int samples;
};

static const char *getTypeName(GLenum type)
{
    switch (type)
    {
    case GL_TEXTURE_2D:
        return "tex2d";
    case GL_RENDERBUFFER:
        return "rbo";
    default:
        TCU_FAIL("Unknown type");
    }
}

std::string FboConfig::getName(void) const
{
    std::ostringstream name;

    DE_ASSERT(buffers & GL_COLOR_BUFFER_BIT);
    name << getTypeName(colorType) << "_" << getFormatName(colorFormat);

    if (buffers & GL_DEPTH_BUFFER_BIT)
        name << "_depth";
    if (buffers & GL_STENCIL_BUFFER_BIT)
        name << "_stencil";

    if (buffers & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
        name << "_" << getTypeName(depthStencilType) << "_" << getFormatName(depthStencilFormat);

    return name.str();
}

class Framebuffer
{
public:
    Framebuffer(sglr::Context &context, const FboConfig &config, int width, int height, uint32_t fbo = 0,
                uint32_t colorBuffer = 0, uint32_t depthStencilBuffer = 0);
    ~Framebuffer(void);

    const FboConfig &getConfig(void) const
    {
        return m_config;
    }
    uint32_t getFramebuffer(void) const
    {
        return m_framebuffer;
    }
    uint32_t getColorBuffer(void) const
    {
        return m_colorBuffer;
    }
    uint32_t getDepthStencilBuffer(void) const
    {
        return m_depthStencilBuffer;
    }

    void checkCompleteness(void);

private:
    uint32_t createTex2D(uint32_t name, GLenum format, int width, int height);
    uint32_t createRbo(uint32_t name, GLenum format, int width, int height);
    void destroyBuffer(uint32_t name, GLenum type);

    FboConfig m_config;
    sglr::Context &m_context;
    uint32_t m_framebuffer;
    uint32_t m_colorBuffer;
    uint32_t m_depthStencilBuffer;
};

static std::vector<std::string> getEnablingExtensions(uint32_t format)
{
    std::vector<std::string> out;

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
        out.push_back("GL_EXT_color_buffer_float");
        break;

    default:
        break;
    }

    return out;
}

static bool isExtensionSupported(sglr::Context &context, const char *name)
{
    std::istringstream extensions(context.getString(GL_EXTENSIONS));
    std::string extension;

    while (std::getline(extensions, extension, ' '))
    {
        if (extension == name)
            return true;
    }

    return false;
}

static bool isAnyExtensionSupported(sglr::Context &context, const std::vector<std::string> &requiredExts)
{
    if (requiredExts.empty())
        return true;

    for (std::vector<std::string>::const_iterator iter = requiredExts.begin(); iter != requiredExts.end(); iter++)
    {
        const std::string &extension = *iter;

        if (isExtensionSupported(context, extension.c_str()))
            return true;
    }

    return false;
}

template <typename T>
static std::string join(const std::vector<T> &list, const std::string &sep)
{
    std::ostringstream out;

    for (typename std::vector<T>::const_iterator iter = list.begin(); iter != list.end(); iter++)
    {
        if (iter != list.begin())
            out << sep;
        out << *iter;
    }

    return out.str();
}

static void checkColorFormatSupport(sglr::Context &context, uint32_t sizedFormat)
{
    const std::vector<std::string> requiredExts = getEnablingExtensions(sizedFormat);

    if (!isAnyExtensionSupported(context, requiredExts))
    {
        std::string errMsg =
            "Format not supported, requires " +
            ((requiredExts.size() == 1) ? requiredExts[0] : " one of the following: " + join(requiredExts, ", "));

        throw tcu::NotSupportedError(errMsg);
    }
}

Framebuffer::Framebuffer(sglr::Context &context, const FboConfig &config, int width, int height, uint32_t fbo,
                         uint32_t colorBufferName, uint32_t depthStencilBufferName)
    : m_config(config)
    , m_context(context)
    , m_framebuffer(fbo)
    , m_colorBuffer(0)
    , m_depthStencilBuffer(0)
{
    // Verify that color format is supported
    checkColorFormatSupport(context, config.colorFormat);

    if (m_framebuffer == 0)
        context.genFramebuffers(1, &m_framebuffer);
    context.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    if (m_config.buffers & (GL_COLOR_BUFFER_BIT))
    {
        switch (m_config.colorType)
        {
        case GL_TEXTURE_2D:
            m_colorBuffer = createTex2D(colorBufferName, m_config.colorFormat, width, height);
            context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorBuffer, 0);
            break;

        case GL_RENDERBUFFER:
            m_colorBuffer = createRbo(colorBufferName, m_config.colorFormat, width, height);
            context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorBuffer);
            break;

        default:
            TCU_FAIL("Unsupported type");
        }
    }

    if (m_config.buffers & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
    {
        switch (m_config.depthStencilType)
        {
        case GL_TEXTURE_2D:
            m_depthStencilBuffer = createTex2D(depthStencilBufferName, m_config.depthStencilFormat, width, height);
            break;
        case GL_RENDERBUFFER:
            m_depthStencilBuffer = createRbo(depthStencilBufferName, m_config.depthStencilFormat, width, height);
            break;
        default:
            TCU_FAIL("Unsupported type");
        }
    }

    for (int ndx = 0; ndx < 2; ndx++)
    {
        uint32_t bit   = ndx ? GL_STENCIL_BUFFER_BIT : GL_DEPTH_BUFFER_BIT;
        uint32_t point = ndx ? GL_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;

        if ((m_config.buffers & bit) == 0)
            continue; /* Not used. */

        switch (m_config.depthStencilType)
        {
        case GL_TEXTURE_2D:
            context.framebufferTexture2D(GL_FRAMEBUFFER, point, GL_TEXTURE_2D, m_depthStencilBuffer, 0);
            break;
        case GL_RENDERBUFFER:
            context.framebufferRenderbuffer(GL_FRAMEBUFFER, point, GL_RENDERBUFFER, m_depthStencilBuffer);
            break;
        default:
            DE_ASSERT(false);
        }
    }

    GLenum err = m_context.getError();
    if (err != GL_NO_ERROR)
        throw glu::Error(err, glu::getErrorStr(err).toString().c_str(), "", __FILE__, __LINE__);

    context.bindFramebuffer(GL_FRAMEBUFFER, 0);
}

Framebuffer::~Framebuffer(void)
{
    m_context.deleteFramebuffers(1, &m_framebuffer);
    destroyBuffer(m_colorBuffer, m_config.colorType);
    destroyBuffer(m_depthStencilBuffer, m_config.depthStencilType);
}

void Framebuffer::checkCompleteness(void)
{
    m_context.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    GLenum status = m_context.checkFramebufferStatus(GL_FRAMEBUFFER);
    m_context.bindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw FboIncompleteException(status, __FILE__, __LINE__);
}

uint32_t Framebuffer::createTex2D(uint32_t name, GLenum format, int width, int height)
{
    if (name == 0)
        m_context.genTextures(1, &name);

    m_context.bindTexture(GL_TEXTURE_2D, name);
    m_context.texImage2D(GL_TEXTURE_2D, 0, format, width, height);

    if (!deIsPowerOfTwo32(width) || !deIsPowerOfTwo32(height))
    {
        // Set wrap mode to clamp for NPOT FBOs
        m_context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        m_context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    m_context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return name;
}

uint32_t Framebuffer::createRbo(uint32_t name, GLenum format, int width, int height)
{
    if (name == 0)
        m_context.genRenderbuffers(1, &name);

    m_context.bindRenderbuffer(GL_RENDERBUFFER, name);
    m_context.renderbufferStorage(GL_RENDERBUFFER, format, width, height);

    return name;
}

void Framebuffer::destroyBuffer(uint32_t name, GLenum type)
{
    if (type == GL_TEXTURE_2D || type == GL_TEXTURE_CUBE_MAP)
        m_context.deleteTextures(1, &name);
    else if (type == GL_RENDERBUFFER)
        m_context.deleteRenderbuffers(1, &name);
    else
        DE_ASSERT(type == GL_NONE);
}

static void createMetaballsTex2D(sglr::Context &context, uint32_t name, GLenum format, GLenum dataType, int width,
                                 int height)
{
    tcu::TextureFormat texFormat = glu::mapGLTransferFormat(format, dataType);
    tcu::TextureLevel level(texFormat, width, height);

    tcu::fillWithMetaballs(level.getAccess(), 5, name ^ width ^ height);

    context.bindTexture(GL_TEXTURE_2D, name);
    context.texImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, dataType, level.getAccess().getDataPtr());
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

static void createQuadsTex2D(sglr::Context &context, uint32_t name, GLenum format, GLenum dataType, int width,
                             int height)
{
    tcu::TextureFormat texFormat = glu::mapGLTransferFormat(format, dataType);
    tcu::TextureLevel level(texFormat, width, height);

    tcu::fillWithRGBAQuads(level.getAccess());

    context.bindTexture(GL_TEXTURE_2D, name);
    context.texImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, dataType, level.getAccess().getDataPtr());
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

class FboRenderCase : public TestCase
{
public:
    FboRenderCase(Context &context, const char *name, const char *description, const FboConfig &config);
    virtual ~FboRenderCase(void)
    {
    }

    virtual IterateResult iterate(void);
    virtual void render(sglr::Context &fboContext, Surface &dst) = 0;

    bool compare(const tcu::Surface &reference, const tcu::Surface &result);

protected:
    const FboConfig m_config;
};

FboRenderCase::FboRenderCase(Context &context, const char *name, const char *description, const FboConfig &config)
    : TestCase(context, name, description)
    , m_config(config)
{
}

TestCase::IterateResult FboRenderCase::iterate(void)
{
    tcu::Vec4 clearColor                  = tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f);
    glu::RenderContext &renderCtx         = m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();
    tcu::TestLog &log                     = m_testCtx.getLog();
    const char *failReason                = nullptr;

    // Position & size for context
    deRandom rnd;
    deRandom_init(&rnd, deStringHash(getName()));

    int width  = deMin32(renderTarget.getWidth(), 128);
    int height = deMin32(renderTarget.getHeight(), 128);
    int xMax   = renderTarget.getWidth() - width + 1;
    int yMax   = renderTarget.getHeight() - height + 1;
    int x      = deRandom_getUint32(&rnd) % xMax;
    int y      = deRandom_getUint32(&rnd) % yMax;

    tcu::Surface gles3Frame(width, height);
    tcu::Surface refFrame(width, height);
    GLenum gles3Error;
    GLenum refError;

    // Render using GLES3
    try
    {
        sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(x, y, width, height));

        context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
        context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        render(context, gles3Frame); // Call actual render func
        gles3Error = context.getError();
    }
    catch (const FboIncompleteException &e)
    {
        if (e.getReason() == GL_FRAMEBUFFER_UNSUPPORTED)
        {
            // Mark test case as unsupported
            log << e;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
            return STOP;
        }
        else
            throw; // Propagate error
    }

    // Render reference image
    {
        sglr::ReferenceContextBuffers buffers(
            tcu::PixelFormat(8, 8, 8, renderTarget.getPixelFormat().alphaBits ? 8 : 0), renderTarget.getDepthBits(),
            renderTarget.getStencilBits(), width, height);
        sglr::ReferenceContext context(sglr::ReferenceContextLimits(renderCtx), buffers.getColorbuffer(),
                                       buffers.getDepthbuffer(), buffers.getStencilbuffer());

        context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
        context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        render(context, refFrame);
        refError = context.getError();
    }

    // Compare error codes
    bool errorCodesOk = (gles3Error == refError);

    if (!errorCodesOk)
    {
        log << tcu::TestLog::Message << "Error code mismatch: got " << glu::getErrorStr(gles3Error) << ", expected "
            << glu::getErrorStr(refError) << tcu::TestLog::EndMessage;
        failReason = "Got unexpected error";
    }

    // Compare images
    bool imagesOk = compare(refFrame, gles3Frame);

    if (!imagesOk && !failReason)
        failReason = "Image comparison failed";

    // Store test result
    bool isOk = errorCodesOk && imagesOk;
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, isOk ? "Pass" : failReason);

    return STOP;
}

bool FboRenderCase::compare(const tcu::Surface &reference, const tcu::Surface &result)
{
    const tcu::RGBA threshold(tcu::max(getFormatThreshold(m_config.colorFormat), tcu::RGBA(12, 12, 12, 12)));

    return tcu::bilinearCompare(m_testCtx.getLog(), "ComparisonResult", "Image comparison result",
                                reference.getAccess(), result.getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
}

namespace FboCases
{

class StencilClearsTest : public FboRenderCase
{
public:
    StencilClearsTest(Context &context, const FboConfig &config);
    virtual ~StencilClearsTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);
};

StencilClearsTest::StencilClearsTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Stencil clears", config)
{
}

void StencilClearsTest::render(sglr::Context &context, Surface &dst)
{
    tcu::TextureFormat colorFormat      = glu::mapGLInternalFormat(m_config.colorFormat);
    glu::DataType fboSamplerType        = glu::getSampler2DType(colorFormat);
    glu::DataType fboOutputType         = getFragmentOutputType(colorFormat);
    tcu::TextureFormatInfo fboRangeInfo = tcu::getTextureFormatInfo(colorFormat);
    Vec4 fboOutScale                    = fboRangeInfo.valueMax - fboRangeInfo.valueMin;
    Vec4 fboOutBias                     = fboRangeInfo.valueMin;

    Texture2DShader texToFboShader(DataTypes() << glu::TYPE_SAMPLER_2D, fboOutputType);
    Texture2DShader texFromFboShader(DataTypes() << fboSamplerType, glu::TYPE_FLOAT_VEC4);

    uint32_t texToFboShaderID   = context.createProgram(&texToFboShader);
    uint32_t texFromFboShaderID = context.createProgram(&texFromFboShader);

    uint32_t metaballsTex = 1;
    uint32_t quadsTex     = 2;
    int width             = 128;
    int height            = 128;

    texToFboShader.setOutScaleBias(fboOutScale, fboOutBias);
    texFromFboShader.setTexScaleBias(0, fboRangeInfo.lookupScale, fboRangeInfo.lookupBias);

    createQuadsTex2D(context, quadsTex, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
    createMetaballsTex2D(context, metaballsTex, GL_RGBA, GL_UNSIGNED_BYTE, width, height);

    Framebuffer fbo(context, m_config, width, height);
    fbo.checkCompleteness();

    // Bind framebuffer and clear
    context.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    context.viewport(0, 0, width, height);
    context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Do stencil clears
    context.enable(GL_SCISSOR_TEST);
    context.scissor(10, 16, 32, 120);
    context.clearStencil(1);
    context.clear(GL_STENCIL_BUFFER_BIT);
    context.scissor(16, 32, 100, 64);
    context.clearStencil(2);
    context.clear(GL_STENCIL_BUFFER_BIT);
    context.disable(GL_SCISSOR_TEST);

    // Draw 2 textures with stecil tests
    context.enable(GL_STENCIL_TEST);

    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    context.stencilFunc(GL_EQUAL, 1, 0xffu);

    texToFboShader.setUniforms(context, texToFboShaderID);
    sglr::drawQuad(context, texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.stencilFunc(GL_EQUAL, 2, 0xffu);

    texToFboShader.setUniforms(context, texToFboShaderID);
    sglr::drawQuad(context, texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

    context.disable(GL_STENCIL_TEST);

    if (fbo.getConfig().colorType == GL_TEXTURE_2D)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorBuffer());
        context.viewport(0, 0, context.getWidth(), context.getHeight());

        texFromFboShader.setUniforms(context, texFromFboShaderID);
        sglr::drawQuad(context, texFromFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        readPixels(context, dst, 0, 0, width, height, colorFormat, fboRangeInfo.lookupScale, fboRangeInfo.lookupBias);
}

class SharedColorbufferTest : public FboRenderCase
{
public:
    SharedColorbufferTest(Context &context, const FboConfig &config);
    virtual ~SharedColorbufferTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);
};

SharedColorbufferTest::SharedColorbufferTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Shared colorbuffer", config)
{
}

void SharedColorbufferTest::render(sglr::Context &context, Surface &dst)
{
    Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
    FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
    uint32_t texShaderID  = context.createProgram(&texShader);
    uint32_t flatShaderID = context.createProgram(&flatShader);

    int width             = 128;
    int height            = 128;
    uint32_t quadsTex     = 1;
    uint32_t metaballsTex = 2;
    bool stencil          = (m_config.buffers & GL_STENCIL_BUFFER_BIT) != 0;

    context.disable(GL_DITHER);

    // Textures
    createQuadsTex2D(context, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createMetaballsTex2D(context, metaballsTex, GL_RGBA, GL_UNSIGNED_BYTE, 64, 64);

    context.viewport(0, 0, width, height);

    // Fbo A
    Framebuffer fboA(context, m_config, width, height);
    fboA.checkCompleteness();

    // Fbo B - don't create colorbuffer
    FboConfig cfg = m_config;
    cfg.buffers &= ~GL_COLOR_BUFFER_BIT;
    cfg.colorType   = GL_NONE;
    cfg.colorFormat = GL_NONE;
    Framebuffer fboB(context, cfg, width, height);

    // Attach color buffer from fbo A
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    switch (m_config.colorType)
    {
    case GL_TEXTURE_2D:
        context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboA.getColorBuffer(), 0);
        break;

    case GL_RENDERBUFFER:
        context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fboA.getColorBuffer());
        break;

    default:
        DE_ASSERT(false);
    }

    // Clear depth and stencil in fbo B
    context.clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Render quads to fbo 1, with depth 0.0
    context.bindFramebuffer(GL_FRAMEBUFFER, fboA.getFramebuffer());
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (stencil)
    {
        // Stencil to 1 in fbo A
        context.clearStencil(1);
        context.clear(GL_STENCIL_BUFFER_BIT);
    }

    texShader.setUniforms(context, texShaderID);

    context.enable(GL_DEPTH_TEST);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
    context.disable(GL_DEPTH_TEST);

    // Blend metaballs to fbo 2
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.enable(GL_BLEND);
    context.blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    // Render small quad that is only visible if depth buffer is not shared with fbo A - or there is no depth bits
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    context.enable(GL_DEPTH_TEST);
    sglr::drawQuad(context, texShaderID, Vec3(0.5f, 0.5f, 0.5f), Vec3(1.0f, 1.0f, 0.5f));
    context.disable(GL_DEPTH_TEST);

    if (stencil)
    {
        flatShader.setColor(context, flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));

        // Clear subset of stencil buffer to 1
        context.enable(GL_SCISSOR_TEST);
        context.scissor(10, 10, 12, 25);
        context.clearStencil(1);
        context.clear(GL_STENCIL_BUFFER_BIT);
        context.disable(GL_SCISSOR_TEST);

        // Render quad with stencil mask == 1
        context.enable(GL_STENCIL_TEST);
        context.stencilFunc(GL_EQUAL, 1, 0xffu);
        sglr::drawQuad(context, flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        context.disable(GL_STENCIL_TEST);
    }

    // Get results
    if (fboA.getConfig().colorType == GL_TEXTURE_2D)
    {
        texShader.setUniforms(context, texShaderID);

        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.bindTexture(GL_TEXTURE_2D, fboA.getColorBuffer());
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        readPixels(context, dst, 0, 0, width, height, glu::mapGLInternalFormat(fboA.getConfig().colorFormat),
                   Vec4(1.0f), Vec4(0.0f));
}

class SharedColorbufferClearsTest : public FboRenderCase
{
public:
    SharedColorbufferClearsTest(Context &context, const FboConfig &config);
    virtual ~SharedColorbufferClearsTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);
};

SharedColorbufferClearsTest::SharedColorbufferClearsTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Shared colorbuffer clears", config)
{
}

void SharedColorbufferClearsTest::render(sglr::Context &context, Surface &dst)
{
    tcu::TextureFormat colorFormat = glu::mapGLInternalFormat(m_config.colorFormat);
    glu::DataType fboSamplerType   = glu::getSampler2DType(colorFormat);
    int width                      = 128;
    int height                     = 128;
    uint32_t colorbuffer           = 1;

    // Check for format support.
    checkColorFormatSupport(context, m_config.colorFormat);

    // Single colorbuffer
    if (m_config.colorType == GL_TEXTURE_2D)
    {
        context.bindTexture(GL_TEXTURE_2D, colorbuffer);
        context.texImage2D(GL_TEXTURE_2D, 0, m_config.colorFormat, width, height);
        context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        DE_ASSERT(m_config.colorType == GL_RENDERBUFFER);
        context.bindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
        context.renderbufferStorage(GL_RENDERBUFFER, m_config.colorFormat, width, height);
    }

    // Multiple framebuffers sharing the colorbuffer
    for (int fbo = 1; fbo <= 3; fbo++)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, fbo);

        if (m_config.colorType == GL_TEXTURE_2D)
            context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer, 0);
        else
            context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuffer);
    }

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    // Check completeness
    {
        GLenum status = context.checkFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            throw FboIncompleteException(status, __FILE__, __LINE__);
    }

    // Render to them
    context.viewport(0, 0, width, height);
    context.clearColor(0.0f, 0.0f, 1.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT);

    context.enable(GL_SCISSOR_TEST);

    context.bindFramebuffer(GL_FRAMEBUFFER, 2);
    context.clearColor(0.6f, 0.0f, 0.0f, 1.0f);
    context.scissor(10, 10, 64, 64);
    context.clear(GL_COLOR_BUFFER_BIT);
    context.clearColor(0.0f, 0.6f, 0.0f, 1.0f);
    context.scissor(60, 60, 40, 20);
    context.clear(GL_COLOR_BUFFER_BIT);

    context.bindFramebuffer(GL_FRAMEBUFFER, 3);
    context.clearColor(0.0f, 0.0f, 0.6f, 1.0f);
    context.scissor(20, 20, 100, 10);
    context.clear(GL_COLOR_BUFFER_BIT);

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);
    context.clearColor(0.6f, 0.0f, 0.6f, 1.0f);
    context.scissor(20, 20, 5, 100);
    context.clear(GL_COLOR_BUFFER_BIT);

    context.disable(GL_SCISSOR_TEST);

    if (m_config.colorType == GL_TEXTURE_2D)
    {
        Texture2DShader shader(DataTypes() << fboSamplerType, glu::TYPE_FLOAT_VEC4);
        uint32_t shaderID = context.createProgram(&shader);

        shader.setUniforms(context, shaderID);

        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        sglr::drawQuad(context, shaderID, Vec3(-0.9f, -0.9f, 0.0f), Vec3(0.9f, 0.9f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        readPixels(context, dst, 0, 0, width, height, colorFormat, Vec4(1.0f), Vec4(0.0f));
}

class SharedDepthStencilTest : public FboRenderCase
{
public:
    SharedDepthStencilTest(Context &context, const FboConfig &config);
    virtual ~SharedDepthStencilTest(void)
    {
    }

    static bool isConfigSupported(const FboConfig &config);
    void render(sglr::Context &context, Surface &dst);
};

SharedDepthStencilTest::SharedDepthStencilTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Shared depth/stencilbuffer", config)
{
}

bool SharedDepthStencilTest::isConfigSupported(const FboConfig &config)
{
    return (config.buffers & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0;
}

void SharedDepthStencilTest::render(sglr::Context &context, Surface &dst)
{
    Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
    FlatColorShader flatShader(glu::TYPE_FLOAT_VEC4);
    uint32_t texShaderID  = context.createProgram(&texShader);
    uint32_t flatShaderID = context.createProgram(&flatShader);
    int width             = 128;
    int height            = 128;
    // bool depth = (m_config.buffers & GL_DEPTH_BUFFER_BIT) != 0;
    bool stencil = (m_config.buffers & GL_STENCIL_BUFFER_BIT) != 0;

    // Textures
    uint32_t metaballsTex = 5;
    uint32_t quadsTex     = 6;
    createMetaballsTex2D(context, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createQuadsTex2D(context, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);

    context.viewport(0, 0, width, height);

    // Fbo A
    Framebuffer fboA(context, m_config, width, height);
    fboA.checkCompleteness();

    // Fbo B
    FboConfig cfg = m_config;
    cfg.buffers &= ~(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    cfg.depthStencilType   = GL_NONE;
    cfg.depthStencilFormat = GL_NONE;
    Framebuffer fboB(context, cfg, width, height);

    // Bind depth/stencil buffers from fbo A to fbo B
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    for (int ndx = 0; ndx < 2; ndx++)
    {
        uint32_t bit   = ndx ? GL_STENCIL_BUFFER_BIT : GL_DEPTH_BUFFER_BIT;
        uint32_t point = ndx ? GL_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;

        if ((m_config.buffers & bit) == 0)
            continue;

        switch (m_config.depthStencilType)
        {
        case GL_TEXTURE_2D:
            context.framebufferTexture2D(GL_FRAMEBUFFER, point, GL_TEXTURE_2D, fboA.getDepthStencilBuffer(), 0);
            break;
        case GL_RENDERBUFFER:
            context.framebufferRenderbuffer(GL_FRAMEBUFFER, point, GL_RENDERBUFFER, fboA.getDepthStencilBuffer());
            break;
        default:
            TCU_FAIL("Not implemented");
        }
    }

    // Setup uniforms
    texShader.setUniforms(context, texShaderID);

    // Clear color to red and stencil to 1 in fbo B.
    context.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
    context.clearStencil(1);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    context.enable(GL_DEPTH_TEST);

    // Render quad to fbo A
    context.bindFramebuffer(GL_FRAMEBUFFER, fboA.getFramebuffer());
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    if (stencil)
    {
        // Clear subset of stencil buffer to 0 in fbo A
        context.enable(GL_SCISSOR_TEST);
        context.scissor(10, 10, 12, 25);
        context.clearStencil(0);
        context.clear(GL_STENCIL_BUFFER_BIT);
        context.disable(GL_SCISSOR_TEST);
    }

    // Render metaballs to fbo B
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    context.disable(GL_DEPTH_TEST);

    if (stencil)
    {
        // Render quad with stencil mask == 0
        context.enable(GL_STENCIL_TEST);
        context.stencilFunc(GL_EQUAL, 0, 0xffu);
        context.useProgram(flatShaderID);
        flatShader.setColor(context, flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));
        sglr::drawQuad(context, flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));
        context.disable(GL_STENCIL_TEST);
    }

    if (m_config.colorType == GL_TEXTURE_2D)
    {
        // Render both to screen
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        context.bindTexture(GL_TEXTURE_2D, fboA.getColorBuffer());
        sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        context.bindTexture(GL_TEXTURE_2D, fboB.getColorBuffer());
        sglr::drawQuad(context, texShaderID, Vec3(0.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
    {
        // Read results from fbo B
        readPixels(context, dst, 0, 0, width, height, glu::mapGLInternalFormat(m_config.colorFormat), Vec4(1.0f),
                   Vec4(0.0f));
    }
}

#if 0
class TexSubImageAfterRenderTest : public FboRenderCase
{
public:
                    TexSubImageAfterRenderTest        (Context& context, const FboConfig& config);
    virtual            ~TexSubImageAfterRenderTest        (void) {}

    void            render                            (sglr::Context& context, Surface& dst);
};

TexSubImageAfterRenderTest::TexSubImageAfterRenderTest (Context& context, const FboConfig& config)
    : FboRenderCase(context, (string("after_render_") + config.getName()).c_str(), "TexSubImage after rendering to texture", config)
{
}

void TexSubImageAfterRenderTest::render (sglr::Context& context, Surface& dst)
{
    using sglr::TexturedQuadOp;

    bool isRGBA = true;

    Surface fourQuads(Surface::PIXELFORMAT_RGB, 64, 64);
    tcu::SurfaceUtil::fillWithFourQuads(fourQuads);

    Surface metaballs(isRGBA ? Surface::PIXELFORMAT_RGBA : Surface::PIXELFORMAT_RGB, 64, 64);
    tcu::SurfaceUtil::fillWithMetaballs(metaballs, 5, 3);

    uint32_t fourQuadsTex = 1;
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGB, fourQuads);

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t fboTex = 2;
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texImage2D(GL_TEXTURE_2D, 0, isRGBA ? GL_RGBA : GL_RGB, 128, 128);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);

    // Render to fbo
    context.viewport(0, 0, 128, 128);
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    context.draw(TexturedQuadOp(-1.0f, -1.0f, 1.0f, 1.0f, 0));

    // Update texture using TexSubImage2D
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texSubImage2D(GL_TEXTURE_2D, 0, 32, 32, metaballs);

    // Draw to screen
    context.bindFramebuffer(GL_FRAMEBUFFER, 0);
    context.viewport(0, 0, context.getWidth(), context.getHeight());
    context.draw(TexturedQuadOp(-1.0f, -1.0f, 1.0f, 1.0f, 0));
    context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
}

class TexSubImageBetweenRenderTest : public FboRenderCase
{
public:
                    TexSubImageBetweenRenderTest        (Context& context, const FboConfig& config);
    virtual            ~TexSubImageBetweenRenderTest        (void) {}

    void            render                                (sglr::Context& context, Surface& dst);
};

TexSubImageBetweenRenderTest::TexSubImageBetweenRenderTest (Context& context, const FboConfig& config)
    : FboRenderCase(context, (string("between_render_") + config.getName()).c_str(), "TexSubImage between rendering calls", config)
{
}

void TexSubImageBetweenRenderTest::render (sglr::Context& context, Surface& dst)
{
    using sglr::TexturedQuadOp;
    using sglr::BlendTextureOp;

    bool isRGBA = true;

    Surface fourQuads(Surface::PIXELFORMAT_RGB, 64, 64);
    tcu::SurfaceUtil::fillWithFourQuads(fourQuads);

    Surface metaballs(isRGBA ? Surface::PIXELFORMAT_RGBA : Surface::PIXELFORMAT_RGB, 64, 64);
    tcu::SurfaceUtil::fillWithMetaballs(metaballs, 5, 3);

    Surface metaballs2(Surface::PIXELFORMAT_RGBA, 64, 64);
    tcu::SurfaceUtil::fillWithMetaballs(metaballs2, 5, 4);

    uint32_t metaballsTex = 3;
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, metaballs2);

    uint32_t fourQuadsTex = 1;
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGB, fourQuads);

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t fboTex = 2;
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texImage2D(GL_TEXTURE_2D, 0, isRGBA ? GL_RGBA : GL_RGB, 128, 128);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);

    // Render to fbo
    context.viewport(0, 0, 128, 128);
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    context.draw(TexturedQuadOp(-1.0f, -1.0f, 1.0f, 1.0f, 0));

    // Update texture using TexSubImage2D
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texSubImage2D(GL_TEXTURE_2D, 0, 32, 32, metaballs);

    // Render again to fbo
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.draw(BlendTextureOp(0));

    // Draw to screen
    context.bindFramebuffer(GL_FRAMEBUFFER, 0);
    context.viewport(0, 0, context.getWidth(), context.getHeight());
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.draw(TexturedQuadOp(-1.0f, -1.0f, 1.0f, 1.0f, 0));

    context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
}
#endif

class ResizeTest : public FboRenderCase
{
public:
    ResizeTest(Context &context, const FboConfig &config);
    virtual ~ResizeTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);
};

ResizeTest::ResizeTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Resize framebuffer", config)
{
}

void ResizeTest::render(sglr::Context &context, Surface &dst)
{
    tcu::TextureFormat colorFormat      = glu::mapGLInternalFormat(m_config.colorFormat);
    glu::DataType fboSamplerType        = glu::getSampler2DType(colorFormat);
    glu::DataType fboOutputType         = getFragmentOutputType(colorFormat);
    tcu::TextureFormatInfo fboRangeInfo = tcu::getTextureFormatInfo(colorFormat);
    Vec4 fboOutScale                    = fboRangeInfo.valueMax - fboRangeInfo.valueMin;
    Vec4 fboOutBias                     = fboRangeInfo.valueMin;

    Texture2DShader texToFboShader(DataTypes() << glu::TYPE_SAMPLER_2D, fboOutputType);
    Texture2DShader texFromFboShader(DataTypes() << fboSamplerType, glu::TYPE_FLOAT_VEC4);
    FlatColorShader flatShader(fboOutputType);
    uint32_t texToFboShaderID   = context.createProgram(&texToFboShader);
    uint32_t texFromFboShaderID = context.createProgram(&texFromFboShader);
    uint32_t flatShaderID       = context.createProgram(&flatShader);

    uint32_t quadsTex     = 1;
    uint32_t metaballsTex = 2;
    bool depth            = (m_config.buffers & GL_DEPTH_BUFFER_BIT) != 0;
    bool stencil          = (m_config.buffers & GL_STENCIL_BUFFER_BIT) != 0;
    int initialWidth      = 128;
    int initialHeight     = 128;
    int newWidth          = 64;
    int newHeight         = 32;

    texToFboShader.setOutScaleBias(fboOutScale, fboOutBias);
    texFromFboShader.setTexScaleBias(0, fboRangeInfo.lookupScale, fboRangeInfo.lookupBias);

    createQuadsTex2D(context, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createMetaballsTex2D(context, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 32, 32);

    Framebuffer fbo(context, m_config, initialWidth, initialHeight);
    fbo.checkCompleteness();

    // Setup shaders
    texToFboShader.setUniforms(context, texToFboShaderID);
    texFromFboShader.setUniforms(context, texFromFboShaderID);
    flatShader.setColor(context, flatShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f) * fboOutScale + fboOutBias);

    // Render quads
    context.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    context.viewport(0, 0, initialWidth, initialHeight);
    clearColorBuffer(context, colorFormat, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    context.clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(context, texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    if (fbo.getConfig().colorType == GL_TEXTURE_2D)
    {
        // Render fbo to screen
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorBuffer());
        sglr::drawQuad(context, texFromFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Restore binding
        context.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    }

    // Resize buffers
    switch (fbo.getConfig().colorType)
    {
    case GL_TEXTURE_2D:
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorBuffer());
        context.texImage2D(GL_TEXTURE_2D, 0, fbo.getConfig().colorFormat, newWidth, newHeight);
        break;

    case GL_RENDERBUFFER:
        context.bindRenderbuffer(GL_RENDERBUFFER, fbo.getColorBuffer());
        context.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().colorFormat, newWidth, newHeight);
        break;

    default:
        DE_ASSERT(false);
    }

    if (depth || stencil)
    {
        switch (fbo.getConfig().depthStencilType)
        {
        case GL_TEXTURE_2D:
            context.bindTexture(GL_TEXTURE_2D, fbo.getDepthStencilBuffer());
            context.texImage2D(GL_TEXTURE_2D, 0, fbo.getConfig().depthStencilFormat, newWidth, newHeight);
            break;

        case GL_RENDERBUFFER:
            context.bindRenderbuffer(GL_RENDERBUFFER, fbo.getDepthStencilBuffer());
            context.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().depthStencilFormat, newWidth, newHeight);
            break;

        default:
            DE_ASSERT(false);
        }
    }

    // Render to resized fbo
    context.viewport(0, 0, newWidth, newHeight);
    clearColorBuffer(context, colorFormat, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    context.clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    context.enable(GL_DEPTH_TEST);

    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(context, texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(context, texToFboShaderID, Vec3(0.0f, 0.0f, -1.0f), Vec3(+1.0f, +1.0f, 1.0f));

    context.disable(GL_DEPTH_TEST);

    if (stencil)
    {
        context.enable(GL_SCISSOR_TEST);
        context.clearStencil(1);
        context.scissor(10, 10, 5, 15);
        context.clear(GL_STENCIL_BUFFER_BIT);
        context.disable(GL_SCISSOR_TEST);

        context.enable(GL_STENCIL_TEST);
        context.stencilFunc(GL_EQUAL, 1, 0xffu);
        sglr::drawQuad(context, flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));
        context.disable(GL_STENCIL_TEST);
    }

    if (m_config.colorType == GL_TEXTURE_2D)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorBuffer());
        sglr::drawQuad(context, texFromFboShaderID, Vec3(-0.5f, -0.5f, 0.0f), Vec3(0.5f, 0.5f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        readPixels(context, dst, 0, 0, newWidth, newHeight, colorFormat, fboRangeInfo.lookupScale,
                   fboRangeInfo.lookupBias);
}

class RecreateBuffersTest : public FboRenderCase
{
public:
    RecreateBuffersTest(Context &context, const FboConfig &config, uint32_t buffers, bool rebind);
    virtual ~RecreateBuffersTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);

private:
    uint32_t m_buffers;
    bool m_rebind;
};

RecreateBuffersTest::RecreateBuffersTest(Context &context, const FboConfig &config, uint32_t buffers, bool rebind)
    : FboRenderCase(context, (string(config.getName()) + (rebind ? "" : "_no_rebind")).c_str(), "Recreate buffers",
                    config)
    , m_buffers(buffers)
    , m_rebind(rebind)
{
}

void RecreateBuffersTest::render(sglr::Context &ctx, Surface &dst)
{
    tcu::TextureFormat colorFormat      = glu::mapGLInternalFormat(m_config.colorFormat);
    glu::DataType fboSamplerType        = glu::getSampler2DType(colorFormat);
    glu::DataType fboOutputType         = getFragmentOutputType(colorFormat);
    tcu::TextureFormatInfo fboRangeInfo = tcu::getTextureFormatInfo(colorFormat);
    Vec4 fboOutScale                    = fboRangeInfo.valueMax - fboRangeInfo.valueMin;
    Vec4 fboOutBias                     = fboRangeInfo.valueMin;

    Texture2DShader texToFboShader(DataTypes() << glu::TYPE_SAMPLER_2D, fboOutputType);
    Texture2DShader texFromFboShader(DataTypes() << fboSamplerType, glu::TYPE_FLOAT_VEC4);
    FlatColorShader flatShader(fboOutputType);
    uint32_t texToFboShaderID   = ctx.createProgram(&texToFboShader);
    uint32_t texFromFboShaderID = ctx.createProgram(&texFromFboShader);
    uint32_t flatShaderID       = ctx.createProgram(&flatShader);

    int width             = 128;
    int height            = 128;
    uint32_t metaballsTex = 1;
    uint32_t quadsTex     = 2;
    bool stencil          = (m_config.buffers & GL_STENCIL_BUFFER_BIT) != 0;

    createQuadsTex2D(ctx, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createMetaballsTex2D(ctx, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);

    Framebuffer fbo(ctx, m_config, width, height);
    fbo.checkCompleteness();

    // Setup shaders
    texToFboShader.setOutScaleBias(fboOutScale, fboOutBias);
    texFromFboShader.setTexScaleBias(0, fboRangeInfo.lookupScale, fboRangeInfo.lookupBias);
    texToFboShader.setUniforms(ctx, texToFboShaderID);
    texFromFboShader.setUniforms(ctx, texFromFboShaderID);
    flatShader.setColor(ctx, flatShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f) * fboOutScale + fboOutBias);

    // Draw scene
    ctx.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    ctx.viewport(0, 0, width, height);
    clearColorBuffer(ctx, colorFormat, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    ctx.clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ctx.enable(GL_DEPTH_TEST);

    ctx.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(ctx, texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    ctx.disable(GL_DEPTH_TEST);

    if (stencil)
    {
        ctx.enable(GL_SCISSOR_TEST);
        ctx.scissor(width / 4, height / 4, width / 2, height / 2);
        ctx.clearStencil(1);
        ctx.clear(GL_STENCIL_BUFFER_BIT);
        ctx.disable(GL_SCISSOR_TEST);
    }

    // Recreate buffers
    if (!m_rebind)
        ctx.bindFramebuffer(GL_FRAMEBUFFER, 0);

    DE_ASSERT((m_buffers & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) == 0 ||
              (m_buffers & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) ==
                  (m_config.buffers & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)));

    // Recreate.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        uint32_t bit    = ndx == 0 ? GL_COLOR_BUFFER_BIT : (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        uint32_t type   = ndx == 0 ? fbo.getConfig().colorType : fbo.getConfig().depthStencilType;
        uint32_t format = ndx == 0 ? fbo.getConfig().colorFormat : fbo.getConfig().depthStencilFormat;
        uint32_t buf    = ndx == 0 ? fbo.getColorBuffer() : fbo.getDepthStencilBuffer();

        if ((m_buffers & bit) == 0)
            continue;

        switch (type)
        {
        case GL_TEXTURE_2D:
            ctx.deleteTextures(1, &buf);
            ctx.bindTexture(GL_TEXTURE_2D, buf);
            ctx.texImage2D(GL_TEXTURE_2D, 0, format, width, height);
            ctx.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            ctx.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;

        case GL_RENDERBUFFER:
            ctx.deleteRenderbuffers(1, &buf);
            ctx.bindRenderbuffer(GL_RENDERBUFFER, buf);
            ctx.renderbufferStorage(GL_RENDERBUFFER, format, width, height);
            break;

        default:
            DE_ASSERT(false);
        }
    }

    // Rebind.
    if (m_rebind)
    {
        for (int ndx = 0; ndx < 3; ndx++)
        {
            uint32_t bit   = ndx == 0 ? GL_COLOR_BUFFER_BIT :
                             ndx == 1 ? GL_DEPTH_BUFFER_BIT :
                             ndx == 2 ? GL_STENCIL_BUFFER_BIT :
                                        0;
            uint32_t point = ndx == 0 ? GL_COLOR_ATTACHMENT0 :
                             ndx == 1 ? GL_DEPTH_ATTACHMENT :
                             ndx == 2 ? GL_STENCIL_ATTACHMENT :
                                        0;
            uint32_t type  = ndx == 0 ? fbo.getConfig().colorType : fbo.getConfig().depthStencilType;
            uint32_t buf   = ndx == 0 ? fbo.getColorBuffer() : fbo.getDepthStencilBuffer();

            if ((m_buffers & bit) == 0)
                continue;

            switch (type)
            {
            case GL_TEXTURE_2D:
                ctx.framebufferTexture2D(GL_FRAMEBUFFER, point, GL_TEXTURE_2D, buf, 0);
                break;

            case GL_RENDERBUFFER:
                ctx.framebufferRenderbuffer(GL_FRAMEBUFFER, point, GL_RENDERBUFFER, buf);
                break;

            default:
                DE_ASSERT(false);
            }
        }
    }

    if (!m_rebind)
        ctx.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());

    ctx.clearStencil(0);
    ctx.clear(m_buffers &
              (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)); // \note Clear only buffers that were re-created
    if (m_buffers & GL_COLOR_BUFFER_BIT)
    {
        // Clearing of integer buffers is undefined so do clearing by rendering flat color.
        sglr::drawQuad(ctx, flatShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
    }

    ctx.enable(GL_DEPTH_TEST);

    if (stencil)
    {
        // \note Stencil test enabled only if we have stencil buffer
        ctx.enable(GL_STENCIL_TEST);
        ctx.stencilFunc(GL_EQUAL, 0, 0xffu);
    }
    ctx.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(ctx, texToFboShaderID, Vec3(-1.0f, -1.0f, 1.0f), Vec3(1.0f, 1.0f, -1.0f));
    if (stencil)
        ctx.disable(GL_STENCIL_TEST);

    ctx.disable(GL_DEPTH_TEST);

    if (fbo.getConfig().colorType == GL_TEXTURE_2D)
    {
        // Unbind fbo
        ctx.bindFramebuffer(GL_FRAMEBUFFER, 0);

        // Draw to screen
        ctx.bindTexture(GL_TEXTURE_2D, fbo.getColorBuffer());
        ctx.viewport(0, 0, ctx.getWidth(), ctx.getHeight());
        sglr::drawQuad(ctx, texFromFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Read from screen
        ctx.readPixels(dst, 0, 0, ctx.getWidth(), ctx.getHeight());
    }
    else
    {
        // Read from fbo
        readPixels(ctx, dst, 0, 0, width, height, colorFormat, fboRangeInfo.lookupScale, fboRangeInfo.lookupBias);
    }
}

} // namespace FboCases

FboRenderTestGroup::FboRenderTestGroup(Context &context) : TestCaseGroup(context, "render", "Rendering Tests")
{
}

FboRenderTestGroup::~FboRenderTestGroup(void)
{
}

void FboRenderTestGroup::init(void)
{
    static const uint32_t objectTypes[] = {GL_TEXTURE_2D, GL_RENDERBUFFER};

    enum FormatType
    {
        FORMATTYPE_FLOAT = 0,
        FORMATTYPE_FIXED,
        FORMATTYPE_INT,
        FORMATTYPE_UINT,

        FORMATTYPE_LAST
    };

    // Required by specification.
    static const struct
    {
        uint32_t format;
        FormatType type;
    } colorFormats[] = {{GL_RGBA32F, FORMATTYPE_FLOAT},
                        {GL_RGBA32I, FORMATTYPE_INT},
                        {GL_RGBA32UI, FORMATTYPE_UINT},
                        {GL_RGBA16F, FORMATTYPE_FLOAT},
                        {GL_RGBA16I, FORMATTYPE_INT},
                        {GL_RGBA16UI, FORMATTYPE_UINT},
                        {GL_RGB16F, FORMATTYPE_FLOAT},
                        {GL_RGBA8, FORMATTYPE_FIXED},
                        {GL_RGBA8I, FORMATTYPE_INT},
                        {GL_RGBA8UI, FORMATTYPE_UINT},
                        {GL_SRGB8_ALPHA8, FORMATTYPE_FIXED},
                        {GL_RGB10_A2, FORMATTYPE_FIXED},
                        {GL_RGB10_A2UI, FORMATTYPE_UINT},
                        {GL_RGBA4, FORMATTYPE_FIXED},
                        {GL_RGB5_A1, FORMATTYPE_FIXED},
                        {GL_RGB8, FORMATTYPE_FIXED},
                        {GL_RGB565, FORMATTYPE_FIXED},
                        {GL_R11F_G11F_B10F, FORMATTYPE_FLOAT},
                        {GL_RG32F, FORMATTYPE_FLOAT},
                        {GL_RG32I, FORMATTYPE_INT},
                        {GL_RG32UI, FORMATTYPE_UINT},
                        {GL_RG16F, FORMATTYPE_FLOAT},
                        {GL_RG16I, FORMATTYPE_INT},
                        {GL_RG16UI, FORMATTYPE_UINT},
                        {GL_RG8, FORMATTYPE_FLOAT},
                        {GL_RG8I, FORMATTYPE_INT},
                        {GL_RG8UI, FORMATTYPE_UINT},
                        {GL_R32F, FORMATTYPE_FLOAT},
                        {GL_R32I, FORMATTYPE_INT},
                        {GL_R32UI, FORMATTYPE_UINT},
                        {GL_R16F, FORMATTYPE_FLOAT},
                        {GL_R16I, FORMATTYPE_INT},
                        {GL_R16UI, FORMATTYPE_UINT},
                        {GL_R8, FORMATTYPE_FLOAT},
                        {GL_R8I, FORMATTYPE_INT},
                        {GL_R8UI, FORMATTYPE_UINT}};

    static const struct
    {
        uint32_t format;
        bool depth;
        bool stencil;
    } depthStencilFormats[] = {{GL_DEPTH_COMPONENT32F, true, false}, {GL_DEPTH_COMPONENT24, true, false},
                               {GL_DEPTH_COMPONENT16, true, false},  {GL_DEPTH32F_STENCIL8, true, true},
                               {GL_DEPTH24_STENCIL8, true, true},    {GL_STENCIL_INDEX8, false, true}};

    using namespace FboCases;

    // .stencil_clear
    tcu::TestCaseGroup *stencilClearGroup = new tcu::TestCaseGroup(m_testCtx, "stencil_clear", "Stencil buffer clears");
    addChild(stencilClearGroup);
    for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats); fmtNdx++)
    {
        uint32_t colorType   = GL_TEXTURE_2D;
        uint32_t stencilType = GL_RENDERBUFFER;
        uint32_t colorFmt    = GL_RGBA8;

        if (!depthStencilFormats[fmtNdx].stencil)
            continue;

        FboConfig config(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, colorType, colorFmt, stencilType,
                         depthStencilFormats[fmtNdx].format);
        stencilClearGroup->addChild(new StencilClearsTest(m_context, config));
    }

    // .shared_colorbuffer_clear
    tcu::TestCaseGroup *sharedColorbufferClearGroup =
        new tcu::TestCaseGroup(m_testCtx, "shared_colorbuffer_clear", "Shader colorbuffer clears");
    addChild(sharedColorbufferClearGroup);
    for (int colorFmtNdx = 0; colorFmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); colorFmtNdx++)
    {
        // Clearing of integer buffers is undefined.
        if (colorFormats[colorFmtNdx].type == FORMATTYPE_INT || colorFormats[colorFmtNdx].type == FORMATTYPE_UINT)
            continue;

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(objectTypes); typeNdx++)
        {
            FboConfig config(GL_COLOR_BUFFER_BIT, objectTypes[typeNdx], colorFormats[colorFmtNdx].format, GL_NONE,
                             GL_NONE);
            sharedColorbufferClearGroup->addChild(new SharedColorbufferClearsTest(m_context, config));
        }
    }

    // .shared_colorbuffer
    tcu::TestCaseGroup *sharedColorbufferGroup =
        new tcu::TestCaseGroup(m_testCtx, "shared_colorbuffer", "Shared colorbuffer tests");
    addChild(sharedColorbufferGroup);
    for (int colorFmtNdx = 0; colorFmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); colorFmtNdx++)
    {
        uint32_t depthStencilType   = GL_RENDERBUFFER;
        uint32_t depthStencilFormat = GL_DEPTH24_STENCIL8;

        // Blending with integer buffers and fp32 targets is not supported.
        if (colorFormats[colorFmtNdx].type == FORMATTYPE_INT || colorFormats[colorFmtNdx].type == FORMATTYPE_UINT ||
            colorFormats[colorFmtNdx].format == GL_RGBA32F || colorFormats[colorFmtNdx].format == GL_RGB32F ||
            colorFormats[colorFmtNdx].format == GL_RG32F || colorFormats[colorFmtNdx].format == GL_R32F)
            continue;

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(objectTypes); typeNdx++)
        {
            FboConfig colorOnlyConfig(GL_COLOR_BUFFER_BIT, objectTypes[typeNdx], colorFormats[colorFmtNdx].format,
                                      GL_NONE, GL_NONE);
            FboConfig colorDepthConfig(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, objectTypes[typeNdx],
                                       colorFormats[colorFmtNdx].format, depthStencilType, depthStencilFormat);
            FboConfig colorDepthStencilConfig(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                                              objectTypes[typeNdx], colorFormats[colorFmtNdx].format, depthStencilType,
                                              depthStencilFormat);

            sharedColorbufferGroup->addChild(new SharedColorbufferTest(m_context, colorOnlyConfig));
            sharedColorbufferGroup->addChild(new SharedColorbufferTest(m_context, colorDepthConfig));
            sharedColorbufferGroup->addChild(new SharedColorbufferTest(m_context, colorDepthStencilConfig));
        }
    }

    // .shared_depth_stencil
    tcu::TestCaseGroup *sharedDepthStencilGroup =
        new tcu::TestCaseGroup(m_testCtx, "shared_depth_stencil", "Shared depth and stencil buffers");
    addChild(sharedDepthStencilGroup);
    for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats); fmtNdx++)
    {
        uint32_t colorType = GL_TEXTURE_2D;
        uint32_t colorFmt  = GL_RGBA8;
        bool depth         = depthStencilFormats[fmtNdx].depth;
        bool stencil       = depthStencilFormats[fmtNdx].stencil;

        if (!depth)
            continue; // Not verified.

        // Depth and stencil: both rbo and textures
        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(objectTypes); typeNdx++)
        {
            FboConfig config(GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0) |
                                 (stencil ? GL_STENCIL_BUFFER_BIT : 0),
                             colorType, colorFmt, objectTypes[typeNdx], depthStencilFormats[fmtNdx].format);
            sharedDepthStencilGroup->addChild(new SharedDepthStencilTest(m_context, config));
        }
    }

    // .resize
    tcu::TestCaseGroup *resizeGroup = new tcu::TestCaseGroup(m_testCtx, "resize", "FBO resize tests");
    addChild(resizeGroup);
    for (int colorFmtNdx = 0; colorFmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); colorFmtNdx++)
    {
        uint32_t colorFormat = colorFormats[colorFmtNdx].format;

        // Color-only.
        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(objectTypes); typeNdx++)
        {
            FboConfig config(GL_COLOR_BUFFER_BIT, objectTypes[typeNdx], colorFormat, GL_NONE, GL_NONE);
            resizeGroup->addChild(new ResizeTest(m_context, config));
        }

        // For selected color formats tests depth & stencil variants.
        if (colorFormat == GL_RGBA8 || colorFormat == GL_RGBA16F)
        {
            for (int depthStencilFmtNdx = 0; depthStencilFmtNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats);
                 depthStencilFmtNdx++)
            {
                uint32_t colorType = GL_TEXTURE_2D;
                bool depth         = depthStencilFormats[depthStencilFmtNdx].depth;
                bool stencil       = depthStencilFormats[depthStencilFmtNdx].stencil;

                // Depth and stencil: both rbo and textures
                for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(objectTypes); typeNdx++)
                {
                    if (!depth && objectTypes[typeNdx] != GL_RENDERBUFFER)
                        continue; // Not supported.

                    FboConfig config(
                        GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0) | (stencil ? GL_STENCIL_BUFFER_BIT : 0),
                        colorType, colorFormat, objectTypes[typeNdx], depthStencilFormats[depthStencilFmtNdx].format);
                    resizeGroup->addChild(new ResizeTest(m_context, config));
                }
            }
        }
    }

    // .recreate_color
    tcu::TestCaseGroup *recreateColorGroup =
        new tcu::TestCaseGroup(m_testCtx, "recreate_color", "Recreate colorbuffer tests");
    addChild(recreateColorGroup);
    for (int colorFmtNdx = 0; colorFmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); colorFmtNdx++)
    {
        uint32_t colorFormat        = colorFormats[colorFmtNdx].format;
        uint32_t depthStencilFormat = GL_DEPTH24_STENCIL8;
        uint32_t depthStencilType   = GL_RENDERBUFFER;

        // Color-only.
        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(objectTypes); typeNdx++)
        {
            FboConfig config(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, objectTypes[typeNdx],
                             colorFormat, depthStencilType, depthStencilFormat);
            recreateColorGroup->addChild(
                new RecreateBuffersTest(m_context, config, GL_COLOR_BUFFER_BIT, true /* rebind */));
        }
    }

    // .recreate_depth_stencil
    tcu::TestCaseGroup *recreateDepthStencilGroup =
        new tcu::TestCaseGroup(m_testCtx, "recreate_depth_stencil", "Recreate depth and stencil buffers");
    addChild(recreateDepthStencilGroup);
    for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(depthStencilFormats); fmtNdx++)
    {
        uint32_t colorType = GL_TEXTURE_2D;
        uint32_t colorFmt  = GL_RGBA8;
        bool depth         = depthStencilFormats[fmtNdx].depth;
        bool stencil       = depthStencilFormats[fmtNdx].stencil;

        // Depth and stencil: both rbo and textures
        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(objectTypes); typeNdx++)
        {
            if (!depth && objectTypes[typeNdx] != GL_RENDERBUFFER)
                continue;

            FboConfig config(GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0) |
                                 (stencil ? GL_STENCIL_BUFFER_BIT : 0),
                             colorType, colorFmt, objectTypes[typeNdx], depthStencilFormats[fmtNdx].format);
            recreateDepthStencilGroup->addChild(new RecreateBuffersTest(
                m_context, config, (depth ? GL_DEPTH_BUFFER_BIT : 0) | (stencil ? GL_STENCIL_BUFFER_BIT : 0),
                true /* rebind */));
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
