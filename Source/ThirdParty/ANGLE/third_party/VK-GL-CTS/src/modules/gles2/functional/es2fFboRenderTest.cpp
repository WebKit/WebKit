/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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
 *
 * Notes:
 *   + Like in API tests, tcu::sgl2s::Context class is used.
 *   + ReferenceContext is used to generate reference images.
 *   + API calls can be logged \todo [pyry] Implement.
 *//*--------------------------------------------------------------------*/

#include "es2fFboRenderTest.hpp"
#include "sglrContextUtil.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "deRandom.hpp"
#include "deString.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

using std::string;
using std::vector;
using tcu::RGBA;
using tcu::Surface;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace glw; // GL types

namespace deqp
{
namespace gles2
{
namespace Functional
{

// Shaders.

class FlatColorShader : public sglr::ShaderProgram
{
public:
    FlatColorShader(void)
        : sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                              << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::Uniform("u_color", glu::TYPE_FLOAT_VEC4)
                              << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                                          "void main (void)\n"
                                                          "{\n"
                                                          "    gl_Position = a_position;\n"
                                                          "}\n")
                              << sglr::pdec::FragmentSource("uniform mediump vec4 u_color;\n"
                                                            "void main (void)\n"
                                                            "{\n"
                                                            "    gl_FragColor = u_color;\n"
                                                            "}\n"))
    {
    }

    void setColor(sglr::Context &gl, uint32_t program, const tcu::Vec4 &color)
    {
        gl.useProgram(program);
        gl.uniform4fv(gl.getUniformLocation(program, "u_color"), 1, color.getPtr());
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            packets[packetNdx]->position =
                rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        const tcu::Vec4 color(m_uniforms[0].value.f4);

        DE_UNREF(packets);

        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
    }
};

class SingleTex2DShader : public sglr::ShaderProgram
{
public:
    SingleTex2DShader(void)
        : sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                              << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                              << sglr::pdec::Uniform("u_sampler0", glu::TYPE_SAMPLER_2D)
                              << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                                          "attribute mediump vec2 a_coord;\n"
                                                          "varying mediump vec2 v_coord;\n"
                                                          "void main (void)\n"
                                                          "{\n"
                                                          "    gl_Position = a_position;\n"
                                                          "    v_coord = a_coord;\n"
                                                          "}\n")
                              << sglr::pdec::FragmentSource("uniform sampler2D u_sampler0;\n"
                                                            "varying mediump vec2 v_coord;\n"
                                                            "void main (void)\n"
                                                            "{\n"
                                                            "    gl_FragColor = texture2D(u_sampler0, v_coord);\n"
                                                            "}\n"))
    {
    }

    void setUnit(sglr::Context &gl, uint32_t program, int unitNdx)
    {
        gl.useProgram(program);
        gl.uniform1i(gl.getUniformLocation(program, "u_sampler0"), unitNdx);
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::VertexPacket &packet = *packets[packetNdx];

            packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
            packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
        }
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            {
                const tcu::Vec4 v_coord = rr::readVarying<float>(packets[packetNdx], context, 0, fragNdx);
                const float lod         = 0.0f;

                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0,
                                        this->m_uniforms[0].sampler.tex2D->sample(v_coord.x(), v_coord.y(), lod));
            }
    }
};

class MixTexturesShader : public sglr::ShaderProgram
{
public:
    MixTexturesShader(void)
        : sglr::ShaderProgram(
              sglr::pdec::ShaderProgramDeclaration()
              << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
              << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
              << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
              << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
              << sglr::pdec::Uniform("u_sampler0", glu::TYPE_SAMPLER_2D)
              << sglr::pdec::Uniform("u_sampler1", glu::TYPE_SAMPLER_2D)
              << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                          "attribute mediump vec2 a_coord;\n"
                                          "varying mediump vec2 v_coord;\n"
                                          "void main (void)\n"
                                          "{\n"
                                          "    gl_Position = a_position;\n"
                                          "    v_coord = a_coord;\n"
                                          "}\n")
              << sglr::pdec::FragmentSource(
                     "uniform sampler2D u_sampler0;\n"
                     "uniform sampler2D u_sampler1;\n"
                     "varying mediump vec2 v_coord;\n"
                     "void main (void)\n"
                     "{\n"
                     "    gl_FragColor = texture2D(u_sampler0, v_coord)*0.5 + texture2D(u_sampler1, v_coord)*0.5;\n"
                     "}\n"))
    {
    }

    void setUnits(sglr::Context &gl, uint32_t program, int unit0, int unit1)
    {
        gl.useProgram(program);
        gl.uniform1i(gl.getUniformLocation(program, "u_sampler0"), unit0);
        gl.uniform1i(gl.getUniformLocation(program, "u_sampler1"), unit1);
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::VertexPacket &packet = *packets[packetNdx];

            packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
            packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
        }
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            {
                const tcu::Vec4 v_coord = rr::readVarying<float>(packets[packetNdx], context, 0, fragNdx);
                const float lod         = 0.0f;

                rr::writeFragmentOutput(
                    context, packetNdx, fragNdx, 0,
                    this->m_uniforms[0].sampler.tex2D->sample(v_coord.x(), v_coord.y(), lod) * 0.5f +
                        this->m_uniforms[1].sampler.tex2D->sample(v_coord.x(), v_coord.y(), lod) * 0.5f);
            }
    }
};

// Framebuffer config.

class FboConfig
{
public:
    FboConfig(void)
        : colorbufferType(GL_NONE)
        , colorbufferFormat(GL_NONE)
        , depthbufferType(GL_NONE)
        , depthbufferFormat(GL_NONE)
        , stencilbufferType(GL_NONE)
        , stencilbufferFormat(GL_NONE)
    {
    }

    std::string getName(void) const;

    GLenum colorbufferType;   //!< GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, GL_RENDERBUFFER
    GLenum colorbufferFormat; //!< Internal format for color buffer texture or renderbuffer

    GLenum depthbufferType; //!< GL_RENDERBUFFER
    GLenum depthbufferFormat;

    GLenum stencilbufferType; //!< GL_RENDERBUFFER
    GLenum stencilbufferFormat;

private:
    static const char *getFormatName(GLenum format);
};

const char *FboConfig::getFormatName(GLenum format)
{
    switch (format)
    {
    case GL_RGB:
        return "rgb";
    case GL_RGBA:
        return "rgba";
    case GL_BGRA:
        return "bgra";
    case GL_ALPHA:
        return "alpha";
    case GL_LUMINANCE:
        return "luminance";
    case GL_LUMINANCE_ALPHA:
        return "luminance_alpha";
    case GL_RGB565:
        return "rgb565";
    case GL_RGB5_A1:
        return "rgb5_a1";
    case GL_RGBA4:
        return "rgba4";
    case GL_RGBA16F:
        return "rgba16f";
    case GL_RGB16F:
        return "rgb16f";
    case GL_DEPTH_COMPONENT16:
        return "depth_component16";
    case GL_STENCIL_INDEX8:
        return "stencil_index8";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

std::string FboConfig::getName(void) const
{
    std::string name = "";

    if (colorbufferType != GL_NONE)
    {
        switch (colorbufferType)
        {
        case GL_TEXTURE_2D:
            name += "tex2d_";
            break;
        case GL_TEXTURE_CUBE_MAP:
            name += "texcube_";
            break;
        case GL_RENDERBUFFER:
            name += "rbo_";
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        name += getFormatName(colorbufferFormat);
    }

    if (depthbufferType != GL_NONE)
    {
        DE_ASSERT(depthbufferType == GL_RENDERBUFFER);
        if (name.length() > 0)
            name += "_";
        name += getFormatName(depthbufferFormat);
    }

    if (stencilbufferType != GL_NONE)
    {
        DE_ASSERT(stencilbufferType == GL_RENDERBUFFER);
        if (name.length() > 0)
            name += "_";
        name += getFormatName(stencilbufferFormat);
    }

    return name;
}

class FboIncompleteException : public tcu::TestError
{
public:
    FboIncompleteException(const FboConfig &config, GLenum reason, const char *file, int line);
    virtual ~FboIncompleteException(void) throw()
    {
    }

    const FboConfig &getConfig(void) const
    {
        return m_config;
    }
    GLenum getReason(void) const
    {
        return m_reason;
    }

private:
    FboConfig m_config;
    GLenum m_reason;
};

static const char *getFboIncompleteReasonName(GLenum reason)
{
    switch (reason)
    {
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
        return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "GL_FRAMEBUFFER_UNSUPPORTED";
    case GL_FRAMEBUFFER_COMPLETE:
        return "GL_FRAMEBUFFER_COMPLETE";
    default:
        return "UNKNOWN";
    }
}

FboIncompleteException::FboIncompleteException(const FboConfig &config, GLenum reason, const char *file, int line)
    : TestError("Framebuffer is not complete", getFboIncompleteReasonName(reason), file, line)
    , m_config(config)
    , m_reason(reason)
{
}

class Framebuffer
{
public:
    Framebuffer(sglr::Context &context, const FboConfig &config, int width, int height, uint32_t fbo = 0,
                uint32_t colorbuffer = 0, uint32_t depthbuffer = 0, uint32_t stencilbuffer = 0);
    ~Framebuffer(void);

    const FboConfig &getConfig(void) const
    {
        return m_config;
    }
    uint32_t getFramebuffer(void) const
    {
        return m_framebuffer;
    }
    uint32_t getColorbuffer(void) const
    {
        return m_colorbuffer;
    }
    uint32_t getDepthbuffer(void) const
    {
        return m_depthbuffer;
    }
    uint32_t getStencilbuffer(void) const
    {
        return m_stencilbuffer;
    }

    void checkCompleteness(void);

private:
    void createRbo(uint32_t &name, GLenum format, int width, int height);
    void destroyBuffer(uint32_t name, GLenum type);

    FboConfig m_config;
    sglr::Context &m_context;
    uint32_t m_framebuffer;
    uint32_t m_colorbuffer;
    uint32_t m_depthbuffer;
    uint32_t m_stencilbuffer;
};

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

static void checkColorFormatSupport(sglr::Context &context, uint32_t sizedFormat)
{
    switch (sizedFormat)
    {
    case GL_RGBA16F:
    case GL_RGB16F:
    case GL_RG16F:
    case GL_R16F:
        if (!isExtensionSupported(context, "GL_EXT_color_buffer_half_float"))
            throw tcu::NotSupportedError("GL_EXT_color_buffer_half_float is not supported");
        break;

    case GL_BGRA:
    case GL_BGRA8_EXT:
        if (!isExtensionSupported(context, "GL_EXT_texture_format_BGRA8888"))
            throw tcu::NotSupportedError("GL_EXT_texture_format_BGRA8888 is not supported");
        break;

    default:
        break;
    }
}

Framebuffer::Framebuffer(sglr::Context &context, const FboConfig &config, int width, int height, uint32_t fbo,
                         uint32_t colorbuffer, uint32_t depthbuffer, uint32_t stencilbuffer)
    : m_config(config)
    , m_context(context)
    , m_framebuffer(fbo)
    , m_colorbuffer(colorbuffer)
    , m_depthbuffer(depthbuffer)
    , m_stencilbuffer(stencilbuffer)
{
    // Verify that color format is supported
    checkColorFormatSupport(context, config.colorbufferFormat);

    if (m_framebuffer == 0)
        context.genFramebuffers(1, &m_framebuffer);
    context.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    switch (m_config.colorbufferType)
    {
    case GL_TEXTURE_2D:
        if (m_colorbuffer == 0)
            context.genTextures(1, &m_colorbuffer);
        context.bindTexture(GL_TEXTURE_2D, m_colorbuffer);
        context.texImage2D(GL_TEXTURE_2D, 0, m_config.colorbufferFormat, width, height);
        context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        if (!deIsPowerOfTwo32(width) || !deIsPowerOfTwo32(height))
        {
            // Set wrap mode to clamp for NPOT FBOs
            context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorbuffer, 0);
        break;

    case GL_TEXTURE_CUBE_MAP:
        DE_FATAL("TODO");
        break;

    case GL_RENDERBUFFER:
        createRbo(m_colorbuffer, m_config.colorbufferFormat, width, height);
        context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorbuffer);
        break;

    default:
        DE_ASSERT(m_config.colorbufferType == GL_NONE);
        break;
    }

    if (m_config.depthbufferType == GL_RENDERBUFFER)
    {
        createRbo(m_depthbuffer, m_config.depthbufferFormat, width, height);
        context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthbuffer);
    }
    else
        DE_ASSERT(m_config.depthbufferType == GL_NONE);

    if (m_config.stencilbufferType == GL_RENDERBUFFER)
    {
        createRbo(m_stencilbuffer, m_config.stencilbufferFormat, width, height);
        context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilbuffer);
    }
    else
        DE_ASSERT(m_config.stencilbufferType == GL_NONE);

    context.bindFramebuffer(GL_FRAMEBUFFER, 0);
}

Framebuffer::~Framebuffer(void)
{
    m_context.deleteFramebuffers(1, &m_framebuffer);
    destroyBuffer(m_colorbuffer, m_config.colorbufferType);
    destroyBuffer(m_depthbuffer, m_config.depthbufferType);
    destroyBuffer(m_stencilbuffer, m_config.stencilbufferType);
}

void Framebuffer::checkCompleteness(void)
{
    m_context.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    GLenum status = m_context.checkFramebufferStatus(GL_FRAMEBUFFER);
    m_context.bindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw FboIncompleteException(m_config, status, __FILE__, __LINE__);
}

void Framebuffer::createRbo(uint32_t &name, GLenum format, int width, int height)
{
    if (name == 0)
        m_context.genRenderbuffers(1, &name);
    m_context.bindRenderbuffer(GL_RENDERBUFFER, name);
    m_context.renderbufferStorage(GL_RENDERBUFFER, format, width, height);
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

    const FboConfig &getConfig(void) const
    {
        return m_config;
    }

    static bool isConfigSupported(const FboConfig &config)
    {
        DE_UNREF(config);
        return true;
    }

private:
    FboConfig m_config;
};

FboRenderCase::FboRenderCase(Context &context, const char *name, const char *description, const FboConfig &config)
    : TestCase(context, name, description)
    , m_config(config)
{
}

TestCase::IterateResult FboRenderCase::iterate(void)
{
    Vec4 clearColor(0.125f, 0.25f, 0.5f, 1.0f);
    glu::RenderContext &renderCtx         = m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
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

    tcu::Surface gles2Frame(width, height);
    tcu::Surface refFrame(width, height);
    GLenum gles2Error;
    GLenum refError;

    // Render using GLES2
    try
    {
        sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS, tcu::IVec4(x, y, width, height));

        context.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
        context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        render(context, gles2Frame); // Call actual render func
        gles2Error = context.getError();
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
    bool errorCodesOk = (gles2Error == refError);

    if (!errorCodesOk)
    {
        log << tcu::TestLog::Message << "Error code mismatch: got " << glu::getErrorStr(gles2Error) << ", expected "
            << glu::getErrorStr(refError) << tcu::TestLog::EndMessage;
        failReason = "Got unexpected error";
    }

    // Compare images
    const float threshold = 0.05f;
    bool imagesOk         = tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, gles2Frame,
                                              threshold, tcu::COMPARE_LOG_RESULT);

    if (!imagesOk && !failReason)
        failReason = "Image comparison failed";

    // Store test result
    bool isOk = errorCodesOk && imagesOk;
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, isOk ? "Pass" : failReason);

    return STOP;
}

namespace FboCases
{

class ColorClearsTest : public FboRenderCase
{
public:
    ColorClearsTest(Context &context, const FboConfig &config);
    ~ColorClearsTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);
};

ColorClearsTest::ColorClearsTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Color buffer clears", config)
{
}

void ColorClearsTest::render(sglr::Context &context, Surface &dst)
{
    int width  = 128;
    int height = 128;
    deRandom rnd;

    deRandom_init(&rnd, 0);

    // Create framebuffer
    Framebuffer fbo(context, getConfig(), width, height);
    fbo.checkCompleteness();

    // Clear fbo
    context.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    context.viewport(0, 0, width, height);
    context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Enable scissor test.
    context.enable(GL_SCISSOR_TEST);

    // Do 10 random color clears
    for (int i = 0; i < 15; i++)
    {
        int cX      = (int)(deRandom_getUint32(&rnd) & 0x7fffffff) % width;
        int cY      = (int)(deRandom_getUint32(&rnd) & 0x7fffffff) % height;
        int cWidth  = (int)(deRandom_getUint32(&rnd) & 0x7fffffff) % (width - cX);
        int cHeight = (int)(deRandom_getUint32(&rnd) & 0x7fffffff) % (height - cY);
        Vec4 color  = RGBA(deRandom_getUint32(&rnd)).toVec();

        context.scissor(cX, cY, cWidth, cHeight);
        context.clearColor(color.x(), color.y(), color.z(), color.w());
        context.clear(GL_COLOR_BUFFER_BIT);
    }

    // Disable scissor.
    context.disable(GL_SCISSOR_TEST);

    if (fbo.getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        // Unbind fbo
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);

        // Draw to screen
        SingleTex2DShader shader;
        uint32_t shaderID = context.createProgram(&shader);

        shader.setUnit(context, shaderID, 0);

        context.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Read from screen
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
    {
        // clear alpha channel for GL_RGB5_A1 format because test
        // thresholds for the alpha channel do not account for dithering
        if (getConfig().colorbufferFormat == GL_RGB5_A1)
        {
            context.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
            context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            context.clear(GL_COLOR_BUFFER_BIT);
            context.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }

        // Read from fbo
        context.readPixels(dst, 0, 0, width, height);
    }
}

class IntersectingQuadsTest : public FboRenderCase
{
public:
    IntersectingQuadsTest(Context &context, const FboConfig &config, bool npot = false);
    virtual ~IntersectingQuadsTest(void)
    {
    }

    virtual void render(sglr::Context &context, Surface &dst);

    static bool isConfigSupported(const FboConfig &config);

private:
    int m_fboWidth;
    int m_fboHeight;
};

class IntersectingQuadsNpotTest : public IntersectingQuadsTest
{
public:
    IntersectingQuadsNpotTest(Context &context, const FboConfig &config) : IntersectingQuadsTest(context, config, true)
    {
    }
};

IntersectingQuadsTest::IntersectingQuadsTest(Context &context, const FboConfig &config, bool npot)
    : FboRenderCase(context, (string(npot ? "npot_" : "") + config.getName()).c_str(), "Intersecting textured quads",
                    config)
    , m_fboWidth(npot ? 127 : 128)
    , m_fboHeight(npot ? 95 : 128)
{
}

bool IntersectingQuadsTest::isConfigSupported(const FboConfig &config)
{
    // \note Disabled for stencil configurations since doesn't exercise stencil buffer
    return config.depthbufferType != GL_NONE && config.stencilbufferType == GL_NONE;
}

void IntersectingQuadsTest::render(sglr::Context &ctx, Surface &dst)
{
    SingleTex2DShader texShader;
    uint32_t texShaderID = ctx.createProgram(&texShader);

    uint32_t metaballsTex = 1;
    uint32_t quadsTex     = 2;

    createMetaballsTex2D(ctx, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createQuadsTex2D(ctx, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);

    int width  = m_fboWidth;
    int height = m_fboHeight;
    Framebuffer fbo(ctx, getConfig(), width, height);
    fbo.checkCompleteness();

    // Setup shaders
    texShader.setUnit(ctx, texShaderID, 0);

    // Draw scene
    ctx.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    ctx.viewport(0, 0, width, height);
    ctx.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
    ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ctx.enable(GL_DEPTH_TEST);

    ctx.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    ctx.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, 1.0f), Vec3(1.0f, 1.0f, -1.0f));

    ctx.disable(GL_DEPTH_TEST);

    if (fbo.getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        // Unbind fbo
        ctx.bindFramebuffer(GL_FRAMEBUFFER, 0);

        // Draw to screen
        ctx.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        ctx.viewport(0, 0, ctx.getWidth(), ctx.getHeight());
        sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Read from screen
        ctx.readPixels(dst, 0, 0, ctx.getWidth(), ctx.getHeight());
    }
    else
    {
        // Read from fbo
        ctx.readPixels(dst, 0, 0, width, height);
    }
}

class MixTest : public FboRenderCase
{
public:
    MixTest(Context &context, const FboConfig &config, bool npot = false);
    virtual ~MixTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);

    static bool isConfigSupported(const FboConfig &config);

private:
    int m_fboAWidth;
    int m_fboAHeight;
    int m_fboBWidth;
    int m_fboBHeight;
};

class MixNpotTest : public MixTest
{
public:
    MixNpotTest(Context &context, const FboConfig &config) : MixTest(context, config, true)
    {
    }
};

MixTest::MixTest(Context &context, const FboConfig &config, bool npot)
    : FboRenderCase(context, (string(npot ? "mix_npot_" : "mix_") + config.getName()).c_str(),
                    "Use two fbos as sources in draw operation", config)
    , m_fboAWidth(npot ? 127 : 128)
    , m_fboAHeight(npot ? 95 : 128)
    , m_fboBWidth(npot ? 55 : 64)
    , m_fboBHeight(npot ? 63 : 64)
{
}

bool MixTest::isConfigSupported(const FboConfig &config)
{
    // \note Disabled for stencil configurations since doesn't exercise stencil buffer
    return config.colorbufferType == GL_TEXTURE_2D && config.stencilbufferType == GL_NONE;
}

void MixTest::render(sglr::Context &context, Surface &dst)
{
    SingleTex2DShader singleTexShader;
    MixTexturesShader mixShader;

    uint32_t singleTexShaderID = context.createProgram(&singleTexShader);
    uint32_t mixShaderID       = context.createProgram(&mixShader);

    // Texture with metaballs
    uint32_t metaballsTex = 1;
    context.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    createMetaballsTex2D(context, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);

    // Setup shaders
    singleTexShader.setUnit(context, singleTexShaderID, 0);
    mixShader.setUnits(context, mixShaderID, 0, 1);

    // Fbo, quad with metaballs texture
    Framebuffer fboA(context, getConfig(), m_fboAWidth, m_fboAHeight);
    fboA.checkCompleteness();
    context.bindFramebuffer(GL_FRAMEBUFFER, fboA.getFramebuffer());
    context.viewport(0, 0, m_fboAWidth, m_fboAHeight);
    context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(context, singleTexShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    // Fbo, color clears
    Framebuffer fboB(context, getConfig(), m_fboBWidth, m_fboBHeight);
    fboB.checkCompleteness();
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    context.viewport(0, 0, m_fboBWidth, m_fboBHeight);
    context.enable(GL_SCISSOR_TEST);
    context.scissor(0, 0, 32, 64);
    context.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT);
    context.scissor(32, 0, 32, 64);
    context.clearColor(0.0f, 1.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT);
    context.disable(GL_SCISSOR_TEST);

    // Final mix op
    context.activeTexture(GL_TEXTURE0);
    context.bindTexture(GL_TEXTURE_2D, fboA.getColorbuffer());
    context.activeTexture(GL_TEXTURE1);
    context.bindTexture(GL_TEXTURE_2D, fboB.getColorbuffer());
    context.bindFramebuffer(GL_FRAMEBUFFER, 0);
    context.viewport(0, 0, context.getWidth(), context.getHeight());
    sglr::drawQuad(context, mixShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
}

class BlendTest : public FboRenderCase
{
public:
    BlendTest(Context &context, const FboConfig &config, bool npot = false);
    virtual ~BlendTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);

    static bool isConfigSupported(const FboConfig &config);

private:
    int m_fboWidth;
    int m_fboHeight;
};

class BlendNpotTest : public BlendTest
{
public:
    BlendNpotTest(Context &context, const FboConfig &config) : BlendTest(context, config, true)
    {
    }
};

BlendTest::BlendTest(Context &context, const FboConfig &config, bool npot)
    : FboRenderCase(context, (string(npot ? "blend_npot_" : "blend_") + config.getName()).c_str(), "Blend to fbo",
                    config)
    , m_fboWidth(npot ? 111 : 128)
    , m_fboHeight(npot ? 122 : 128)
{
}

bool BlendTest::isConfigSupported(const FboConfig &config)
{
    // \note Disabled for stencil configurations since doesn't exercise stencil buffer
    return config.stencilbufferType == GL_NONE;
}

void BlendTest::render(sglr::Context &context, Surface &dst)
{
    SingleTex2DShader shader;
    uint32_t shaderID     = context.createProgram(&shader);
    int width             = m_fboWidth;
    int height            = m_fboHeight;
    uint32_t metaballsTex = 1;

    createMetaballsTex2D(context, metaballsTex, GL_RGBA, GL_UNSIGNED_BYTE, 64, 64);

    Framebuffer fbo(context, getConfig(), width, height);
    fbo.checkCompleteness();

    shader.setUnit(context, shaderID, 0);

    context.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    context.viewport(0, 0, width, height);
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.clearColor(0.6f, 0.0f, 0.6f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    context.enable(GL_BLEND);
    context.blendEquation(GL_FUNC_ADD);
    context.blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
    context.disable(GL_BLEND);

    if (fbo.getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        context.readPixels(dst, 0, 0, width, height);
}

class StencilClearsTest : public FboRenderCase
{
public:
    StencilClearsTest(Context &context, const FboConfig &config);
    virtual ~StencilClearsTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);

    static bool isConfigSupported(const FboConfig &config);
};

StencilClearsTest::StencilClearsTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Stencil clears", config)
{
}

void StencilClearsTest::render(sglr::Context &context, Surface &dst)
{
    SingleTex2DShader shader;
    uint32_t shaderID     = context.createProgram(&shader);
    int width             = 128;
    int height            = 128;
    uint32_t quadsTex     = 1;
    uint32_t metaballsTex = 2;

    createQuadsTex2D(context, quadsTex, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
    createMetaballsTex2D(context, metaballsTex, GL_RGBA, GL_UNSIGNED_BYTE, width, height);

    Framebuffer fbo(context, getConfig(), width, height);
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
    context.activeTexture(GL_TEXTURE0);
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    context.activeTexture(GL_TEXTURE1);
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);

    context.enable(GL_STENCIL_TEST);
    context.stencilFunc(GL_EQUAL, 1, 0xffffffffu);
    shader.setUnit(context, shaderID, 0);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    context.stencilFunc(GL_EQUAL, 2, 0xffffffffu);
    shader.setUnit(context, shaderID, 1);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    context.disable(GL_STENCIL_TEST);

    if (fbo.getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.activeTexture(GL_TEXTURE0);
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        shader.setUnit(context, shaderID, 0);
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
    {
        // clear alpha channel for GL_RGB5_A1 format because test
        // thresholds for the alpha channel do not account for dithering
        if (getConfig().colorbufferFormat == GL_RGB5_A1)
        {
            context.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
            context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            context.clear(GL_COLOR_BUFFER_BIT);
            context.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }

        context.readPixels(dst, 0, 0, width, height);
    }
}

bool StencilClearsTest::isConfigSupported(const FboConfig &config)
{
    return config.stencilbufferType != GL_NONE;
}

class StencilTest : public FboRenderCase
{
public:
    StencilTest(Context &context, const FboConfig &config, bool npot = false);
    virtual ~StencilTest(void)
    {
    }

    void render(sglr::Context &context, Surface &dst);

    static bool isConfigSupported(const FboConfig &config);

private:
    int m_fboWidth;
    int m_fboHeight;
};

class StencilNpotTest : public StencilTest
{
public:
    StencilNpotTest(Context &context, const FboConfig &config) : StencilTest(context, config, true)
    {
    }
};

StencilTest::StencilTest(Context &context, const FboConfig &config, bool npot)
    : FboRenderCase(context, (string(npot ? "npot_" : "") + config.getName()).c_str(), "Stencil ops", config)
    , m_fboWidth(npot ? 99 : 128)
    , m_fboHeight(npot ? 110 : 128)
{
}

bool StencilTest::isConfigSupported(const FboConfig &config)
{
    return config.stencilbufferType != GL_NONE;
}

void StencilTest::render(sglr::Context &ctx, Surface &dst)
{
    FlatColorShader colorShader;
    SingleTex2DShader texShader;
    uint32_t colorShaderID = ctx.createProgram(&colorShader);
    uint32_t texShaderID   = ctx.createProgram(&texShader);
    int width              = m_fboWidth;
    int height             = m_fboHeight;
    int texWidth           = 64;
    int texHeight          = 64;
    uint32_t quadsTex      = 1;
    uint32_t metaballsTex  = 2;
    bool depth             = getConfig().depthbufferType != GL_NONE;

    createQuadsTex2D(ctx, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, texWidth, texHeight);
    createMetaballsTex2D(ctx, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, texWidth, texHeight);

    Framebuffer fbo(ctx, getConfig(), width, height);
    fbo.checkCompleteness();

    // Bind framebuffer and clear
    ctx.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    ctx.viewport(0, 0, width, height);
    ctx.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Render intersecting quads - increment stencil on depth pass
    ctx.enable(GL_DEPTH_TEST);
    ctx.enable(GL_STENCIL_TEST);
    ctx.stencilFunc(GL_ALWAYS, 0, 0xffu);
    ctx.stencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    colorShader.setColor(ctx, colorShaderID, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
    sglr::drawQuad(ctx, colorShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

    ctx.bindTexture(GL_TEXTURE_2D, quadsTex);
    texShader.setUnit(ctx, texShaderID, 0);
    sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(+1.0f, +1.0f, +1.0f));

    // Draw quad with stencil test (stencil == 1 or 2 depending on depth) - decrement on stencil failure
    ctx.disable(GL_DEPTH_TEST);
    ctx.stencilFunc(GL_EQUAL, depth ? 2 : 1, 0xffu);
    ctx.stencilOp(GL_DECR, GL_KEEP, GL_KEEP);
    colorShader.setColor(ctx, colorShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));
    sglr::drawQuad(ctx, colorShaderID, Vec3(-0.5f, -0.5f, 0.0f), Vec3(+0.5f, +0.5f, 0.0f));

    // Draw metaballs with stencil test where stencil > 1 or 2 depending on depth buffer
    ctx.bindTexture(GL_TEXTURE_2D, metaballsTex);
    ctx.stencilFunc(GL_GREATER, depth ? 1 : 2, 0xffu);
    ctx.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

    ctx.disable(GL_STENCIL_TEST);

    if (fbo.getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        ctx.bindFramebuffer(GL_FRAMEBUFFER, 0);
        ctx.activeTexture(GL_TEXTURE0);
        ctx.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        ctx.viewport(0, 0, ctx.getWidth(), ctx.getHeight());
        sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        ctx.readPixels(dst, 0, 0, ctx.getWidth(), ctx.getHeight());
    }
    else
        ctx.readPixels(dst, 0, 0, width, height);
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
    SingleTex2DShader shader;
    uint32_t shaderID = context.createProgram(&shader);
    int width         = 128;
    int height        = 128;
    // bool depth = getConfig().depthbufferFormat != GL_NONE;
    bool stencil = getConfig().stencilbufferFormat != GL_NONE;

    // Textures
    uint32_t quadsTex     = 1;
    uint32_t metaballsTex = 2;
    createQuadsTex2D(context, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createMetaballsTex2D(context, metaballsTex, GL_RGBA, GL_UNSIGNED_BYTE, 64, 64);

    context.viewport(0, 0, width, height);

    shader.setUnit(context, shaderID, 0);

    // Fbo A
    Framebuffer fboA(context, getConfig(), width, height);
    fboA.checkCompleteness();

    // Fbo B - don't create colorbuffer
    FboConfig cfg         = getConfig();
    cfg.colorbufferType   = GL_NONE;
    cfg.colorbufferFormat = GL_NONE;
    Framebuffer fboB(context, cfg, width, height);

    // Attach color buffer from fbo A
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    switch (getConfig().colorbufferType)
    {
    case GL_TEXTURE_2D:
        context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboA.getColorbuffer(), 0);
        break;

    case GL_RENDERBUFFER:
        context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fboA.getColorbuffer());
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

    context.enable(GL_DEPTH_TEST);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
    context.disable(GL_DEPTH_TEST);

    // Blend metaballs to fbo 2
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.enable(GL_BLEND);
    context.blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    // Render small quad that is only visible if depth buffer is not shared with fbo A - or there is no depth bits
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    context.enable(GL_DEPTH_TEST);
    sglr::drawQuad(context, shaderID, Vec3(0.5f, 0.5f, 0.5f), Vec3(1.0f, 1.0f, 0.5f));
    context.disable(GL_DEPTH_TEST);

    if (stencil)
    {
        FlatColorShader flatShader;
        uint32_t flatShaderID = context.createProgram(&flatShader);

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
    if (fboA.getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.bindTexture(GL_TEXTURE_2D, fboA.getColorbuffer());
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        context.readPixels(dst, 0, 0, width, height);
}

class SharedColorbufferClearsTest : public FboRenderCase
{
public:
    SharedColorbufferClearsTest(Context &context, const FboConfig &config);
    virtual ~SharedColorbufferClearsTest(void)
    {
    }

    static bool isConfigSupported(const FboConfig &config);
    void render(sglr::Context &context, Surface &dst);
};

SharedColorbufferClearsTest::SharedColorbufferClearsTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Shared colorbuffer clears", config)
{
}

bool SharedColorbufferClearsTest::isConfigSupported(const FboConfig &config)
{
    return config.colorbufferType != GL_NONE && config.depthbufferType == GL_NONE &&
           config.stencilbufferType == GL_NONE;
}

void SharedColorbufferClearsTest::render(sglr::Context &context, Surface &dst)
{
    int width            = 128;
    int height           = 128;
    uint32_t colorbuffer = 1;

    checkColorFormatSupport(context, getConfig().colorbufferFormat);

    // Single colorbuffer
    if (getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        context.bindTexture(GL_TEXTURE_2D, colorbuffer);
        context.texImage2D(GL_TEXTURE_2D, 0, getConfig().colorbufferFormat, width, height);
        context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    else
    {
        DE_ASSERT(getConfig().colorbufferType == GL_RENDERBUFFER);
        context.bindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
        context.renderbufferStorage(GL_RENDERBUFFER, getConfig().colorbufferFormat, width, height);
    }

    // Multiple framebuffers sharing the colorbuffer
    for (int fbo = 1; fbo <= 3; fbo++)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, fbo);

        if (getConfig().colorbufferType == GL_TEXTURE_2D)
            context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer, 0);
        else
            context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuffer);
    }

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    // Check completeness
    {
        GLenum status = context.checkFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            throw FboIncompleteException(getConfig(), status, __FILE__, __LINE__);
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

    if (getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        SingleTex2DShader shader;
        uint32_t shaderID = context.createProgram(&shader);

        shader.setUnit(context, shaderID, 0);

        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        sglr::drawQuad(context, shaderID, Vec3(-0.9f, -0.9f, 0.0f), Vec3(0.9f, 0.9f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        context.readPixels(dst, 0, 0, width, height);
}

class SharedDepthbufferTest : public FboRenderCase
{
public:
    SharedDepthbufferTest(Context &context, const FboConfig &config);
    virtual ~SharedDepthbufferTest(void)
    {
    }

    static bool isConfigSupported(const FboConfig &config);
    void render(sglr::Context &context, Surface &dst);
};

SharedDepthbufferTest::SharedDepthbufferTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, config.getName().c_str(), "Shared depthbuffer", config)
{
}

bool SharedDepthbufferTest::isConfigSupported(const FboConfig &config)
{
    return config.depthbufferType == GL_RENDERBUFFER;
}

void SharedDepthbufferTest::render(sglr::Context &context, Surface &dst)
{
    SingleTex2DShader texShader;
    FlatColorShader colorShader;
    uint32_t texShaderID   = context.createProgram(&texShader);
    uint32_t colorShaderID = context.createProgram(&colorShader);
    int width              = 128;
    int height             = 128;
    bool stencil           = getConfig().stencilbufferType != GL_NONE;

    // Setup shaders
    texShader.setUnit(context, texShaderID, 0);
    colorShader.setColor(context, colorShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // Textures
    uint32_t metaballsTex = 5;
    uint32_t quadsTex     = 6;
    createMetaballsTex2D(context, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createQuadsTex2D(context, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);

    context.viewport(0, 0, width, height);

    // Fbo A
    Framebuffer fboA(context, getConfig(), width, height);
    fboA.checkCompleteness();

    // Fbo B
    FboConfig cfg         = getConfig();
    cfg.depthbufferType   = GL_NONE;
    cfg.depthbufferFormat = GL_NONE;
    Framebuffer fboB(context, cfg, width, height);

    // Bind depth buffer from fbo A to fbo B
    DE_ASSERT(fboA.getConfig().depthbufferType == GL_RENDERBUFFER);
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboA.getDepthbuffer());

    // Clear fbo B color to red and stencil to 1
    context.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
    context.clearStencil(1);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Enable depth test.
    context.enable(GL_DEPTH_TEST);

    // Render quad to fbo A
    context.bindFramebuffer(GL_FRAMEBUFFER, fboA.getFramebuffer());
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    // Render metaballs to fbo B
    context.bindFramebuffer(GL_FRAMEBUFFER, fboB.getFramebuffer());
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    context.disable(GL_DEPTH_TEST);

    if (stencil)
    {
        // Clear subset of stencil buffer to 0
        context.enable(GL_SCISSOR_TEST);
        context.scissor(10, 10, 12, 25);
        context.clearStencil(0);
        context.clear(GL_STENCIL_BUFFER_BIT);
        context.disable(GL_SCISSOR_TEST);

        // Render quad with stencil mask == 0
        context.enable(GL_STENCIL_TEST);
        context.stencilFunc(GL_EQUAL, 0, 0xffu);
        sglr::drawQuad(context, colorShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        context.disable(GL_STENCIL_TEST);
    }

    if (getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        // Render both to screen
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        context.bindTexture(GL_TEXTURE_2D, fboA.getColorbuffer());
        sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        context.bindTexture(GL_TEXTURE_2D, fboB.getColorbuffer());
        sglr::drawQuad(context, texShaderID, Vec3(0.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
    {
        // Read results from fbo B
        context.readPixels(dst, 0, 0, width, height);
    }
}

class TexSubImageAfterRenderTest : public FboRenderCase
{
public:
    TexSubImageAfterRenderTest(Context &context, const FboConfig &config);
    virtual ~TexSubImageAfterRenderTest(void)
    {
    }

    static bool isConfigSupported(const FboConfig &config);
    void render(sglr::Context &context, Surface &dst);
};

TexSubImageAfterRenderTest::TexSubImageAfterRenderTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, (string("after_render_") + config.getName()).c_str(),
                    "TexSubImage after rendering to texture", config)
{
}

bool TexSubImageAfterRenderTest::isConfigSupported(const FboConfig &config)
{
    return config.colorbufferType == GL_TEXTURE_2D &&
           (config.colorbufferFormat == GL_RGB || config.colorbufferFormat == GL_RGBA) &&
           config.depthbufferType == GL_NONE && config.stencilbufferType == GL_NONE;
}

void TexSubImageAfterRenderTest::render(sglr::Context &context, Surface &dst)
{
    SingleTex2DShader shader;
    uint32_t shaderID = context.createProgram(&shader);
    bool isRGBA       = getConfig().colorbufferFormat == GL_RGBA;

    tcu::TextureLevel fourQuads(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), 64, 64);
    tcu::fillWithRGBAQuads(fourQuads.getAccess());

    tcu::TextureLevel metaballs(
        tcu::TextureFormat(isRGBA ? tcu::TextureFormat::RGBA : tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
        64, 64);
    tcu::fillWithMetaballs(metaballs.getAccess(), 5, 3);

    shader.setUnit(context, shaderID, 0);

    uint32_t fourQuadsTex = 1;
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE,
                       fourQuads.getAccess().getDataPtr());

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t fboTex = 2;
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texImage2D(GL_TEXTURE_2D, 0, isRGBA ? GL_RGBA : GL_RGB, 128, 128);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);

    // Render to fbo
    context.viewport(0, 0, 128, 128);
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    // Update texture using TexSubImage2D
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texSubImage2D(GL_TEXTURE_2D, 0, 32, 32, 64, 64, isRGBA ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                          metaballs.getAccess().getDataPtr());

    // Draw to screen
    context.bindFramebuffer(GL_FRAMEBUFFER, 0);
    context.viewport(0, 0, context.getWidth(), context.getHeight());
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
    context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
}

class TexSubImageBetweenRenderTest : public FboRenderCase
{
public:
    TexSubImageBetweenRenderTest(Context &context, const FboConfig &config);
    virtual ~TexSubImageBetweenRenderTest(void)
    {
    }

    static bool isConfigSupported(const FboConfig &config);
    void render(sglr::Context &context, Surface &dst);
};

TexSubImageBetweenRenderTest::TexSubImageBetweenRenderTest(Context &context, const FboConfig &config)
    : FboRenderCase(context, (string("between_render_") + config.getName()).c_str(),
                    "TexSubImage between rendering calls", config)
{
}

bool TexSubImageBetweenRenderTest::isConfigSupported(const FboConfig &config)
{
    return config.colorbufferType == GL_TEXTURE_2D &&
           (config.colorbufferFormat == GL_RGB || config.colorbufferFormat == GL_RGBA) &&
           config.depthbufferType == GL_NONE && config.stencilbufferType == GL_NONE;
}

void TexSubImageBetweenRenderTest::render(sglr::Context &context, Surface &dst)
{
    SingleTex2DShader shader;
    uint32_t shaderID = context.createProgram(&shader);
    bool isRGBA       = getConfig().colorbufferFormat == GL_RGBA;

    tcu::TextureLevel fourQuads(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), 64, 64);
    tcu::fillWithRGBAQuads(fourQuads.getAccess());

    tcu::TextureLevel metaballs(
        tcu::TextureFormat(isRGBA ? tcu::TextureFormat::RGBA : tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
        64, 64);
    tcu::fillWithMetaballs(metaballs.getAccess(), 5, 3);

    tcu::TextureLevel metaballs2(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 64, 64);
    tcu::fillWithMetaballs(metaballs2.getAccess(), 5, 4);

    uint32_t metaballsTex = 3;
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                       metaballs2.getAccess().getDataPtr());

    uint32_t fourQuadsTex = 1;
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE,
                       fourQuads.getAccess().getDataPtr());

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t fboTex = 2;
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texImage2D(GL_TEXTURE_2D, 0, isRGBA ? GL_RGBA : GL_RGB, 128, 128);
    context.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);

    shader.setUnit(context, shaderID, 0);

    // Render to fbo
    context.viewport(0, 0, 128, 128);
    context.bindTexture(GL_TEXTURE_2D, fourQuadsTex);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    // Update texture using TexSubImage2D
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    context.texSubImage2D(GL_TEXTURE_2D, 0, 32, 32, 64, 64, isRGBA ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                          metaballs.getAccess().getDataPtr());

    // Render again to fbo
    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    context.enable(GL_BLEND);
    context.blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
    context.disable(GL_BLEND);

    // Draw to screen
    context.bindFramebuffer(GL_FRAMEBUFFER, 0);
    context.viewport(0, 0, context.getWidth(), context.getHeight());
    context.bindTexture(GL_TEXTURE_2D, fboTex);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
}

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
    SingleTex2DShader texShader;
    FlatColorShader colorShader;
    uint32_t texShaderID   = context.createProgram(&texShader);
    uint32_t colorShaderID = context.createProgram(&colorShader);
    uint32_t quadsTex      = 1;
    uint32_t metaballsTex  = 2;
    bool depth             = getConfig().depthbufferType != GL_NONE;
    bool stencil           = getConfig().stencilbufferType != GL_NONE;

    createQuadsTex2D(context, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createMetaballsTex2D(context, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 32, 32);

    Framebuffer fbo(context, getConfig(), 128, 128);
    fbo.checkCompleteness();

    // Setup shaders
    texShader.setUnit(context, texShaderID, 0);
    colorShader.setColor(context, colorShaderID, Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // Render quads
    context.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    context.viewport(0, 0, 128, 128);
    context.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

    if (fbo.getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        // Render fbo to screen
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        // Restore binding
        context.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    }

    int newWidth  = 64;
    int newHeight = 32;

    // Resize buffers
    switch (fbo.getConfig().colorbufferType)
    {
    case GL_TEXTURE_2D:
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        context.texImage2D(GL_TEXTURE_2D, 0, fbo.getConfig().colorbufferFormat, newWidth, newHeight);
        break;

    case GL_RENDERBUFFER:
        context.bindRenderbuffer(GL_RENDERBUFFER, fbo.getColorbuffer());
        context.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().colorbufferFormat, newWidth, newHeight);
        break;

    default:
        DE_ASSERT(false);
    }

    if (depth)
    {
        DE_ASSERT(fbo.getConfig().depthbufferType == GL_RENDERBUFFER);
        context.bindRenderbuffer(GL_RENDERBUFFER, fbo.getDepthbuffer());
        context.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().depthbufferFormat, newWidth, newHeight);
    }

    if (stencil)
    {
        DE_ASSERT(fbo.getConfig().stencilbufferType == GL_RENDERBUFFER);
        context.bindRenderbuffer(GL_RENDERBUFFER, fbo.getStencilbuffer());
        context.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().stencilbufferFormat, newWidth, newHeight);
    }

    // Render to resized fbo
    context.viewport(0, 0, newWidth, newHeight);
    context.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    context.enable(GL_DEPTH_TEST);

    context.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(context, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));

    context.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(context, texShaderID, Vec3(0.0f, 0.0f, -1.0f), Vec3(+1.0f, +1.0f, 1.0f));

    context.disable(GL_DEPTH_TEST);

    if (stencil)
    {
        context.enable(GL_SCISSOR_TEST);
        context.scissor(10, 10, 5, 15);
        context.clearStencil(1);
        context.clear(GL_STENCIL_BUFFER_BIT);
        context.disable(GL_SCISSOR_TEST);

        context.enable(GL_STENCIL_TEST);
        context.stencilFunc(GL_EQUAL, 1, 0xffu);
        sglr::drawQuad(context, colorShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(+1.0f, +1.0f, 0.0f));
        context.disable(GL_STENCIL_TEST);
    }

    if (getConfig().colorbufferType == GL_TEXTURE_2D)
    {
        context.bindFramebuffer(GL_FRAMEBUFFER, 0);
        context.viewport(0, 0, context.getWidth(), context.getHeight());
        context.bindTexture(GL_TEXTURE_2D, fbo.getColorbuffer());
        sglr::drawQuad(context, texShaderID, Vec3(-0.5f, -0.5f, 0.0f), Vec3(0.5f, 0.5f, 0.0f));
        context.readPixels(dst, 0, 0, context.getWidth(), context.getHeight());
    }
    else
        context.readPixels(dst, 0, 0, newWidth, newHeight);
}

template <GLenum Buffers>
class RecreateBuffersTest : public FboRenderCase
{
public:
    RecreateBuffersTest(Context &context, const FboConfig &config, bool rebind);
    virtual ~RecreateBuffersTest(void)
    {
    }

    static bool isConfigSupported(const FboConfig &config);
    void render(sglr::Context &context, Surface &dst);

private:
    bool m_rebind;
};

template <GLenum Buffers>
class RecreateBuffersNoRebindTest : public RecreateBuffersTest<Buffers>
{
public:
    RecreateBuffersNoRebindTest(Context &context, const FboConfig &config)
        : RecreateBuffersTest<Buffers>(context, config, false)
    {
    }
};

template <GLenum Buffers>
class RecreateBuffersRebindTest : public RecreateBuffersTest<Buffers>
{
public:
    RecreateBuffersRebindTest(Context &context, const FboConfig &config)
        : RecreateBuffersTest<Buffers>(context, config, true)
    {
    }
};

template <GLenum Buffers>
RecreateBuffersTest<Buffers>::RecreateBuffersTest(Context &context, const FboConfig &config, bool rebind)
    : FboRenderCase(context, (string(rebind ? "rebind_" : "no_rebind_") + config.getName()).c_str(), "Recreate buffers",
                    config)
    , m_rebind(rebind)
{
}

template <GLenum Buffers>
bool RecreateBuffersTest<Buffers>::isConfigSupported(const FboConfig &config)
{
    if ((Buffers & GL_COLOR_BUFFER_BIT) && config.colorbufferType == GL_NONE)
        return false;
    if ((Buffers & GL_DEPTH_BUFFER_BIT) && config.depthbufferType == GL_NONE)
        return false;
    if ((Buffers & GL_STENCIL_BUFFER_BIT) && config.stencilbufferType == GL_NONE)
        return false;
    return true;
}

template <GLenum Buffers>
void RecreateBuffersTest<Buffers>::render(sglr::Context &ctx, Surface &dst)
{
    SingleTex2DShader texShader;
    uint32_t texShaderID  = ctx.createProgram(&texShader);
    int width             = 128;
    int height            = 128;
    uint32_t metaballsTex = 1;
    uint32_t quadsTex     = 2;
    bool stencil          = getConfig().stencilbufferType != GL_NONE;

    createQuadsTex2D(ctx, quadsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);
    createMetaballsTex2D(ctx, metaballsTex, GL_RGB, GL_UNSIGNED_BYTE, 64, 64);

    Framebuffer fbo(ctx, getConfig(), width, height);
    fbo.checkCompleteness();

    // Setup shader
    texShader.setUnit(ctx, texShaderID, 0);

    // Draw scene
    ctx.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());
    ctx.viewport(0, 0, width, height);
    ctx.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
    ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ctx.enable(GL_DEPTH_TEST);

    ctx.bindTexture(GL_TEXTURE_2D, quadsTex);
    sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

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

    if (Buffers & GL_COLOR_BUFFER_BIT)
    {
        uint32_t colorbuf = fbo.getColorbuffer();
        switch (fbo.getConfig().colorbufferType)
        {
        case GL_TEXTURE_2D:
            ctx.deleteTextures(1, &colorbuf);
            ctx.bindTexture(GL_TEXTURE_2D, colorbuf);
            ctx.texImage2D(GL_TEXTURE_2D, 0, fbo.getConfig().colorbufferFormat, width, height);
            ctx.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            if (m_rebind)
                ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuf, 0);
            break;

        case GL_RENDERBUFFER:
            ctx.deleteRenderbuffers(1, &colorbuf);
            ctx.bindRenderbuffer(GL_RENDERBUFFER, colorbuf);
            ctx.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().colorbufferFormat, width, height);

            if (m_rebind)
                ctx.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuf);
            break;

        default:
            DE_ASSERT(false);
        }
    }

    if (Buffers & GL_DEPTH_BUFFER_BIT)
    {
        uint32_t depthbuf = fbo.getDepthbuffer();
        DE_ASSERT(fbo.getConfig().depthbufferType == GL_RENDERBUFFER);

        ctx.deleteRenderbuffers(1, &depthbuf);
        ctx.bindRenderbuffer(GL_RENDERBUFFER, depthbuf);
        ctx.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().depthbufferFormat, width, height);

        if (m_rebind)
            ctx.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuf);
    }

    if (Buffers & GL_STENCIL_BUFFER_BIT)
    {
        uint32_t stencilbuf = fbo.getStencilbuffer();
        DE_ASSERT(fbo.getConfig().stencilbufferType == GL_RENDERBUFFER);

        ctx.deleteRenderbuffers(1, &stencilbuf);
        ctx.bindRenderbuffer(GL_RENDERBUFFER, stencilbuf);
        ctx.renderbufferStorage(GL_RENDERBUFFER, fbo.getConfig().stencilbufferFormat, width, height);

        if (m_rebind)
            ctx.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilbuf);
    }

    if (!m_rebind)
        ctx.bindFramebuffer(GL_FRAMEBUFFER, fbo.getFramebuffer());

    ctx.clearColor(0.0f, 0.0f, 1.0f, 0.0f);
    ctx.clearStencil(0);
    ctx.clear(Buffers); // \note Clear only buffers that were re-created

    if (stencil)
    {
        // \note Stencil test enabled only if we have stencil buffer
        ctx.enable(GL_STENCIL_TEST);
        ctx.stencilFunc(GL_EQUAL, 0, 0xffu);
    }
    ctx.bindTexture(GL_TEXTURE_2D, metaballsTex);
    sglr::drawQuad(ctx, texShaderID, Vec3(-1.0f, -1.0f, 1.0f), Vec3(1.0f, 1.0f, -1.0f));
    if (stencil)
        ctx.disable(GL_STENCIL_TEST);

    ctx.disable(GL_DEPTH_TEST);

    // Read from fbo
    ctx.readPixels(dst, 0, 0, width, height);
}

class RepeatedClearCase : public FboRenderCase
{
private:
    static FboConfig makeConfig(uint32_t format)
    {
        FboConfig cfg;
        cfg.colorbufferType   = GL_TEXTURE_2D;
        cfg.colorbufferFormat = format;
        cfg.depthbufferType   = GL_NONE;
        cfg.stencilbufferType = GL_NONE;
        return cfg;
    }

public:
    RepeatedClearCase(Context &context, uint32_t format)
        : FboRenderCase(context, makeConfig(format).getName().c_str(), "Repeated clears", makeConfig(format))
    {
    }

protected:
    void render(sglr::Context &ctx, Surface &dst)
    {
        const int numRowsCols = 4;
        const int cellSize    = 16;
        const int fboSizes[]  = {cellSize, cellSize * numRowsCols};

        SingleTex2DShader fboBlitShader;
        const uint32_t fboBlitShaderID = ctx.createProgram(&fboBlitShader);

        de::Random rnd(18169662);
        uint32_t fbos[]     = {0, 0};
        uint32_t textures[] = {0, 0};

        ctx.genFramebuffers(2, &fbos[0]);
        ctx.genTextures(2, &textures[0]);

        for (int fboNdx = 0; fboNdx < DE_LENGTH_OF_ARRAY(fbos); fboNdx++)
        {
            ctx.bindTexture(GL_TEXTURE_2D, textures[fboNdx]);
            ctx.texImage2D(GL_TEXTURE_2D, 0, getConfig().colorbufferFormat, fboSizes[fboNdx], fboSizes[fboNdx], 0,
                           getConfig().colorbufferFormat, GL_UNSIGNED_BYTE, nullptr);
            ctx.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            ctx.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            ctx.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            ctx.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            ctx.bindFramebuffer(GL_FRAMEBUFFER, fbos[fboNdx]);
            ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[fboNdx], 0);

            {
                const GLenum status = ctx.checkFramebufferStatus(GL_FRAMEBUFFER);
                if (status != GL_FRAMEBUFFER_COMPLETE)
                    throw FboIncompleteException(getConfig(), status, __FILE__, __LINE__);
            }
        }

        // larger fbo bound -- clear to transparent black
        ctx.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
        ctx.clear(GL_COLOR_BUFFER_BIT);

        fboBlitShader.setUnit(ctx, fboBlitShaderID, 0);
        ctx.bindTexture(GL_TEXTURE_2D, textures[0]);

        for (int cellY = 0; cellY < numRowsCols; cellY++)
            for (int cellX = 0; cellX < numRowsCols; cellX++)
            {
                const float r = rnd.getFloat();
                const float g = rnd.getFloat();
                const float b = rnd.getFloat();
                const float a = rnd.getFloat();

                ctx.bindFramebuffer(GL_FRAMEBUFFER, fbos[0]);
                ctx.clearColor(r, g, b, a);
                ctx.clear(GL_COLOR_BUFFER_BIT);

                ctx.bindFramebuffer(GL_FRAMEBUFFER, fbos[1]);
                ctx.viewport(cellX * cellSize, cellY * cellSize, cellSize, cellSize);
                sglr::drawQuad(ctx, fboBlitShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            }

        ctx.readPixels(dst, 0, 0, fboSizes[1], fboSizes[1]);
    }
};

} // namespace FboCases

FboRenderTestGroup::FboRenderTestGroup(Context &context) : TestCaseGroup(context, "render", "Rendering Tests")
{
}

FboRenderTestGroup::~FboRenderTestGroup(void)
{
}

namespace
{

struct TypeFormatPair
{
    GLenum type;
    GLenum format;
};

template <typename CaseType>
void addChildVariants(deqp::gles2::TestCaseGroup *group)
{
    TypeFormatPair colorbufferConfigs[] = {
        //        { GL_TEXTURE_2D, GL_ALPHA },
        //        { GL_TEXTURE_2D, GL_LUMINANCE },
        //        { GL_TEXTURE_2D, GL_LUMINANCE_ALPHA },
        {GL_TEXTURE_2D, GL_RGB},
        {GL_TEXTURE_2D, GL_RGBA},
        {GL_TEXTURE_2D, GL_BGRA},
        {GL_RENDERBUFFER, GL_RGB565},
        {GL_RENDERBUFFER, GL_RGB5_A1},
        {GL_RENDERBUFFER, GL_RGBA4},
        //        { GL_RENDERBUFFER, GL_RGBA16F },
        //        { GL_RENDERBUFFER, GL_RGB16F }
        {GL_RENDERBUFFER, GL_BGRA},
    };
    TypeFormatPair depthbufferConfigs[]   = {{GL_NONE, GL_NONE}, {GL_RENDERBUFFER, GL_DEPTH_COMPONENT16}};
    TypeFormatPair stencilbufferConfigs[] = {{GL_NONE, GL_NONE}, {GL_RENDERBUFFER, GL_STENCIL_INDEX8}};

    for (int colorbufferNdx = 0; colorbufferNdx < DE_LENGTH_OF_ARRAY(colorbufferConfigs); colorbufferNdx++)
        for (int depthbufferNdx = 0; depthbufferNdx < DE_LENGTH_OF_ARRAY(depthbufferConfigs); depthbufferNdx++)
            for (int stencilbufferNdx = 0; stencilbufferNdx < DE_LENGTH_OF_ARRAY(stencilbufferConfigs);
                 stencilbufferNdx++)
            {
                FboConfig config;
                config.colorbufferType     = colorbufferConfigs[colorbufferNdx].type;
                config.colorbufferFormat   = colorbufferConfigs[colorbufferNdx].format;
                config.depthbufferType     = depthbufferConfigs[depthbufferNdx].type;
                config.depthbufferFormat   = depthbufferConfigs[depthbufferNdx].format;
                config.stencilbufferType   = stencilbufferConfigs[stencilbufferNdx].type;
                config.stencilbufferFormat = stencilbufferConfigs[stencilbufferNdx].format;

                if (CaseType::isConfigSupported(config))
                    group->addChild(new CaseType(group->getContext(), config));
            }
}

template <typename CaseType>
void createChildGroup(deqp::gles2::TestCaseGroup *parent, const char *name, const char *description)
{
    deqp::gles2::TestCaseGroup *tmpGroup = new deqp::gles2::TestCaseGroup(parent->getContext(), name, description);
    parent->addChild(tmpGroup);
    addChildVariants<CaseType>(tmpGroup);
}

template <GLbitfield Buffers>
void createRecreateBuffersGroup(deqp::gles2::TestCaseGroup *parent, const char *name, const char *description)
{
    deqp::gles2::TestCaseGroup *tmpGroup = new deqp::gles2::TestCaseGroup(parent->getContext(), name, description);
    parent->addChild(tmpGroup);
    addChildVariants<FboCases::RecreateBuffersRebindTest<Buffers>>(tmpGroup);
    addChildVariants<FboCases::RecreateBuffersNoRebindTest<Buffers>>(tmpGroup);
}

} // namespace

void FboRenderTestGroup::init(void)
{
    createChildGroup<FboCases::ColorClearsTest>(this, "color_clear", "Color buffer clears");
    createChildGroup<FboCases::StencilClearsTest>(this, "stencil_clear", "Stencil buffer clears");

    deqp::gles2::TestCaseGroup *colorGroup = new deqp::gles2::TestCaseGroup(m_context, "color", "Color buffer tests");
    addChild(colorGroup);
    addChildVariants<FboCases::MixTest>(colorGroup);
    addChildVariants<FboCases::MixNpotTest>(colorGroup);
    addChildVariants<FboCases::BlendTest>(colorGroup);
    addChildVariants<FboCases::BlendNpotTest>(colorGroup);

    deqp::gles2::TestCaseGroup *depthGroup = new deqp::gles2::TestCaseGroup(m_context, "depth", "Depth bufer tests");
    addChild(depthGroup);
    addChildVariants<FboCases::IntersectingQuadsTest>(depthGroup);
    addChildVariants<FboCases::IntersectingQuadsNpotTest>(depthGroup);

    deqp::gles2::TestCaseGroup *stencilGroup =
        new deqp::gles2::TestCaseGroup(m_context, "stencil", "Stencil buffer tests");
    addChild(stencilGroup);
    addChildVariants<FboCases::StencilTest>(stencilGroup);
    addChildVariants<FboCases::StencilNpotTest>(stencilGroup);

    createChildGroup<FboCases::SharedColorbufferClearsTest>(this, "shared_colorbuffer_clear",
                                                            "Shared colorbuffer clears");
    createChildGroup<FboCases::SharedColorbufferTest>(this, "shared_colorbuffer", "Shared colorbuffer tests");
    createChildGroup<FboCases::SharedDepthbufferTest>(this, "shared_depthbuffer", "Shared depthbuffer tests");
    createChildGroup<FboCases::ResizeTest>(this, "resize", "FBO resize tests");

    createRecreateBuffersGroup<GL_COLOR_BUFFER_BIT>(this, "recreate_colorbuffer", "Recreate colorbuffer tests");
    createRecreateBuffersGroup<GL_DEPTH_BUFFER_BIT>(this, "recreate_depthbuffer", "Recreate depthbuffer tests");
    createRecreateBuffersGroup<GL_STENCIL_BUFFER_BIT>(this, "recreate_stencilbuffer", "Recreate stencilbuffer tests");

    deqp::gles2::TestCaseGroup *texSubImageGroup =
        new deqp::gles2::TestCaseGroup(m_context, "texsubimage", "TexSubImage interop with FBO colorbuffer texture");
    addChild(texSubImageGroup);
    addChildVariants<FboCases::TexSubImageAfterRenderTest>(texSubImageGroup);
    addChildVariants<FboCases::TexSubImageBetweenRenderTest>(texSubImageGroup);

    {
        tcu::TestCaseGroup *const repeatedClearGroup =
            new tcu::TestCaseGroup(m_testCtx, "repeated_clear", "Repeated FBO clears");
        addChild(repeatedClearGroup);

        repeatedClearGroup->addChild(new FboCases::RepeatedClearCase(m_context, GL_RGB));
        repeatedClearGroup->addChild(new FboCases::RepeatedClearCase(m_context, GL_RGBA));
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
